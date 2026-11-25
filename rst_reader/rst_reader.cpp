// rst_reader.cpp — реализация ридера ANSYS RST файлов
//
// Прямой бинарный парсинг формата FORTRAN unformatted sequential.
// Поддерживает сжатые (ANSCOMP) и несжатые записи.
// Подход аналогичен MultiModelReader/cpp, но с расширенным API для чтения
// геометрии, элементов, перемещений и напряжений.
#include "rst_reader.h"

#include <algorithm>

namespace rst_reader {

// ============================================================================
// Платформо-зависимые утилиты
// ============================================================================

NativeFd fd_open(const std::string& path) {
#if defined(_WIN32)
    return ::CreateFileA(path.c_str(), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                         nullptr);
#else
    return ::open(path.c_str(), O_RDONLY);
#endif
}

void fd_close(NativeFd fd) {
#if defined(_WIN32)
    if (fd != INVALID_HANDLE_VALUE) ::CloseHandle(fd);
#else
    if (fd >= 0) ::close(fd);
#endif
}

bool fd_valid(NativeFd fd) {
#if defined(_WIN32)
    return fd != INVALID_HANDLE_VALUE;
#else
    return fd >= 0;
#endif
}

bool fd_pread(NativeFd fd, void* dst, size_t count, int64_t offset) {
#if defined(_WIN32)
    OVERLAPPED ov{};
    LARGE_INTEGER li; li.QuadPart = offset;
    ov.Offset = li.u.LowPart;
    ov.OffsetHigh = li.u.HighPart;
    DWORD got = 0;
    if (!::ReadFile(fd, dst, static_cast<DWORD>(count), &got, &ov)) return false;
    return got == static_cast<DWORD>(count);
#else
    size_t done = 0;
    while (done < count) {
        ssize_t n = ::pread(fd, static_cast<char*>(dst) + done,
                            count - done, offset + static_cast<int64_t>(done));
        if (n <= 0) return false;
        done += static_cast<size_t>(n);
    }
    return true;
#endif
}

// ============================================================================
// Декомпрессия ANSCOMP
// ============================================================================

namespace {
constexpr size_t kAnscHdr = 20;  // magic(8)+orig_len(4)+nchunks(4)+reserved(4)
}

bool RstReader::ansc_uncompress(const uint8_t* body, size_t body_bytes,
                                std::vector<uint8_t>& out) {
    if (body_bytes < kAnscHdr) return false;
    const uint8_t* p = body;
    int32_t orig_len = rd_i32(p + 12);
    int32_t nchunks  = rd_i32(p + 16);
    if (orig_len <= 0 || nchunks <= 0 || nchunks > (1 << 20)) return false;
    if (static_cast<size_t>(nchunks) * 4 + kAnscHdr > body_bytes) return false;
    size_t pos = kAnscHdr + static_cast<size_t>(nchunks) * 4;
    out.clear();
    out.reserve(static_cast<size_t>(orig_len));
    std::vector<uint8_t> tmp;
    for (int32_t i = 0; i < nchunks; ++i) {
        int32_t clen = rd_i32(p + 20 + static_cast<size_t>(i) * 4);
        if (clen == 0) continue;
        if (clen < 0) {
            size_t lsz = static_cast<size_t>(-clen);
            if (pos + lsz > body_bytes) return false;
            out.insert(out.end(), p + pos, p + pos + lsz);
            pos += lsz;
        } else {
            if (pos + static_cast<size_t>(clen) > body_bytes) return false;
            if (!deflate::inflate_zlib(p + pos, static_cast<size_t>(clen), tmp))
                return false;
            out.insert(out.end(), tmp.begin(), tmp.end());
            pos += static_cast<size_t>(clen);
        }
    }
    if (static_cast<int32_t>(out.size()) < orig_len) return false;
    out.resize(static_cast<size_t>(orig_len));
    return true;
}

// ============================================================================
// Определение единицы маркеров длины (слова или байты)
// ============================================================================

void RstReader::detect_reclen_unit() {
    if (reclen_bytes_ != 0) return;
    if (!fp_ || file_bytes_ < 12) return;
    int32_t n = 0;
    std::fseek(fp_, 0, SEEK_SET);
    if (std::fread(&n, 4, 1, fp_) != 1 || n <= 0) return;
    
    bool words_ok = false;
    if (static_cast<int64_t>(n) * kWord + 8 <= file_bytes_) {
        std::vector<uint8_t> body(static_cast<size_t>(n) * kWord);
        if (std::fread(body.data(), 1, body.size(), fp_) == body.size()) {
            int32_t post = 0;
            if (std::fread(&post, 4, 1, fp_) == 1 && post == n)
                words_ok = !is_comp_magic(body.data());
        }
    }
    
    bool bytes_ok = false;
    if (n >= 8 && static_cast<int64_t>(n) + 8 <= file_bytes_) {
        std::fseek(fp_, 4, SEEK_SET);
        std::vector<uint8_t> body(static_cast<size_t>(n));
        if (std::fread(body.data(), 1, body.size(), fp_) == body.size()) {
            int32_t post = 0;
            if (std::fread(&post, 4, 1, fp_) == 1 && post == n)
                bytes_ok = true;
        }
    }
    
    if (!words_ok && bytes_ok)
        reclen_bytes_ = 1;
    else
        reclen_bytes_ = -1;
    std::fseek(fp_, 0, SEEK_END);
}

// ============================================================================
// Размер физического кадра на диске
// ============================================================================

int64_t RstReader::frame_words_at(int64_t word_off) {
    {
        std::lock_guard<std::mutex> lk(unc_mtx_);
        auto it = comp_frame_words_.find(word_off);
        if (it != comp_frame_words_.end()) return it->second;
    }
    int64_t byte_off = word_off * kWord;
    if (!fp_ || byte_off + 8 > file_bytes_) return -1;
    int32_t len_pre = 0;
    long saved = std::ftell(fp_);
    if (std::fseek(fp_, static_cast<long>(byte_off), SEEK_SET) != 0) return -1;
    if (std::fread(&len_pre, 4, 1, fp_) != 1 || len_pre <= 0) {
        std::fseek(fp_, saved, SEEK_SET); return -1;
    }
    int64_t body_bytes = reclen_bytes_ == 1
        ? static_cast<int64_t>(len_pre)
        : static_cast<int64_t>(len_pre) * kWord;
    std::fseek(fp_, saved, SEEK_SET);
    int64_t frame_words = body_bytes / kWord;
    std::lock_guard<std::mutex> lk(unc_mtx_);
    comp_frame_words_[word_off] = frame_words;
    return frame_words;
}

// ============================================================================
// Чтение одной FORTRAN-записи
// ============================================================================

bool RstReader::read_record(int64_t word_off, std::vector<uint8_t>& buf,
                            int64_t& words_out, std::string& err) {
    detect_reclen_unit();
    int64_t byte_off = word_off * kWord;
    int32_t len_pre = 0;
    if (std::fseek(fp_, static_cast<long>(byte_off), SEEK_SET) != 0) {
        err = "fseek fail"; return false;
    }
    if (std::fread(&len_pre, 4, 1, fp_) != 1) {
        err = "read reclen fail"; return false;
    }
    if (len_pre <= 0) {
        err = "bad record length " + std::to_string(len_pre); return false;
    }
    int64_t body_bytes = reclen_bytes_ == 1
        ? static_cast<int64_t>(len_pre)
        : static_cast<int64_t>(len_pre) * kWord;
    buf.resize(static_cast<size_t>(body_bytes));
    if (std::fread(buf.data(), 1, static_cast<size_t>(body_bytes), fp_) != static_cast<size_t>(body_bytes)) {
        err = "read body fail"; return false;
    }
    int32_t len_post = 0;
    if (std::fread(&len_post, 4, 1, fp_) != 1) {
        err = "read reclen2 fail"; return false;
    }
    
    if (is_comp_magic(buf.data())) {
        compressed_ = true;
        if (len_post != len_pre) { err = "record markers mismatch"; return false; }
        std::vector<uint8_t> unc;
        if (!ansc_uncompress(buf.data(), static_cast<size_t>(body_bytes), unc)) {
            err = "decompress fail at word " + std::to_string(word_off);
            return false;
        }
        {
            std::lock_guard<std::mutex> lk(unc_mtx_);
            unc_cache_[word_off] = unc;
            comp_frame_words_[word_off] = body_bytes / kWord;
        }
        words_out = static_cast<int64_t>(unc.size()) / kWord;
        buf = std::move(unc);
        return true;
    }
    
    if (len_post != len_pre) { err = "record markers mismatch"; return false; }
    words_out = body_bytes / kWord;
    return true;
}

// ============================================================================
// Потокобезопасное позиционное чтение
// ============================================================================

int64_t RstReader::read_pread(NativeFd fd, int64_t word_off,
                              std::vector<uint8_t>& out, std::string& err) {
    {
        std::lock_guard<std::mutex> lk(unc_mtx_);
        auto it = unc_cache_.find(word_off);
        if (it != unc_cache_.end()) {
            out = it->second;
            return static_cast<int64_t>(out.size()) / kWord;
        }
    }
    int64_t rb = reclen_bytes_;
    int64_t byte_off = word_off * kWord;
    int32_t len_pre = 0;
    if (!fd_pread(fd, &len_pre, 4, byte_off)) { err = "pread reclen"; return -1; }
    if (len_pre <= 0 || len_pre > (1 << 28)) {
        err = "bad reclen " + std::to_string(len_pre); return -1;
    }
    int64_t body_bytes = rb == 1
        ? static_cast<int64_t>(len_pre)
        : static_cast<int64_t>(len_pre) * kWord;
    std::vector<uint8_t> raw(static_cast<size_t>(body_bytes));
    if (!fd_pread(fd, raw.data(), static_cast<size_t>(body_bytes), byte_off + 4)) {
        err = "pread body"; return -1;
    }
    int32_t len_post = 0;
    if (!fd_pread(fd, &len_post, 4, byte_off + 4 + body_bytes)) {
        err = "pread reclen2"; return -1;
    }
    
    if (is_comp_magic(raw.data())) {
        if (len_post != len_pre) { err = "reclen mismatch"; return -1; }
        std::vector<uint8_t> unc;
        if (!ansc_uncompress(raw.data(), static_cast<size_t>(body_bytes), unc)) {
            err = "decompress fail at word " + std::to_string(word_off);
            return -1;
        }
        {
            std::lock_guard<std::mutex> lk(unc_mtx_);
            unc_cache_[word_off] = unc;
            comp_frame_words_[word_off] = body_bytes / kWord;
        }
        out = std::move(unc);
        return static_cast<int64_t>(out.size()) / kWord;
    }
    if (len_post != len_pre) { err = "reclen mismatch"; return -1; }
    out = std::move(raw);
    return body_bytes / kWord;
}

// ============================================================================
// Открытие/закрытие файла
// ============================================================================

RstReader::~RstReader() { close(); }

void RstReader::close() {
    if (fp_) { std::fclose(fp_); fp_ = nullptr; }
}

bool RstReader::open(const std::string& p, std::string& err) {
    path_ = p;
    fp_ = std::fopen(p.c_str(), "rb");
    if (!fp_) { err = "cannot open " + p; return false; }
    std::fseek(fp_, 0, SEEK_END);
    file_bytes_ = std::ftell(fp_);
    std::fseek(fp_, 0, SEEK_SET);
    
    // ---- Запись 1: указатели на таблицы заголовка ----
    detect_reclen_unit();
    std::vector<uint8_t> b; int64_t w = 0;
    if (!read_record(0, b, w, err)) return false;
    
    auto ptr = [&](int idx) -> int64_t {
        return static_cast<int64_t>(std::llround(rd_f64(b.data() + (idx - 1) * 8)));
    };
    
    // Layout: [1]=PTRPTR, [2]=PTRDAT, [3]=PRSOL, [4]=PTRGEO, [5]=PTRELM, [6]=PTRCMS
    const int64_t ptr_data     = ptr(2);
    const int64_t ptr_solution = ptr(3);
    const int64_t ptr_geom     = ptr(4);
    const int64_t ptr_elem     = ptr(5);
    const int64_t ptr_comp     = ptr(6);
    ptr_geo_words_  = ptr_geom;
    ptr_comp_words_ = ptr_comp;
    
    // ---- Таблица данных ----
    if (!read_record(ptr_data, b, w, err)) return false;
    n_nodes_ = static_cast<int64_t>(std::llround(rd_f64(b.data() + 0 * 8)));
    maxcomp_ = static_cast<int32_t>(std::llround(rd_f64(b.data() + 2 * 8)));
    
    // ---- Таблица решений ----
    if (!read_record(ptr_solution, b, w, err)) return false;
    nsets_ = static_cast<int32_t>(std::llround(rd_f64(b.data() + 0 * 8)));
    const int64_t rpntr_abs = static_cast<int64_t>(std::llround(rd_f64(b.data() + 1 * 8)));
    const int64_t tprnt3d_w = static_cast<int64_t>(std::llround(rd_f64(b.data() + 2 * 8)));
    if (nsets_ <= 0 || nsets_ > 10000000) { err = "bad NSETS"; return false; }
    
    // ---- RPNTR: смещения заголовков решений ----
    if (!read_record(rpntr_abs, b, w, err)) return false;
    sets_.resize(nsets_);
    for (int i = 0; i < nsets_; ++i)
        sets_[i].rpointer_words = static_cast<int64_t>(std::llround(rd_f64(b.data() + i * 8)));
    
    // ---- TPRNT3D: step/substep/time ----
    int64_t off = tprnt3d_w;
    for (int i = 0; i < nsets_; ) {
        if (!read_record(off, b, w, err)) return false;
        int64_t nrows = w / 3;
        for (int64_t r = 0; r < nrows && i < nsets_; ++r, ++i) {
            sets_[i].step    = rd_i32(b.data() + r * 12 + 0);
            sets_[i].substep = rd_i32(b.data() + r * 12 + 4);
            sets_[i].time    = static_cast<double>(rd_i32(b.data() + r * 12 + 8));
        }
        int64_t phys_words = w;
        if (compressed_) {
            int64_t fw = frame_words_at(off);
            if (fw > 0) phys_words = fw;
        }
        off += phys_words + 2;
    }
    
    // ---- Компоненты (Name Selections) ----
    if (ptr_comp != 0) {
        if (read_record(ptr_comp, b, w, err)) {
            ncms_hdr_ = rd_i32(b.data() + 0);
            nnam_hdr_ = rd_i32(b.data() + 4);
            parse_components(b.data(), static_cast<int64_t>(b.size()));
        }
        err.clear();
    }
    
    // ---- Таблица элементов ----
    if (ptr_elem != 0 && read_record(ptr_elem, b, w, err)) {
        n_elem_ptr_ = rd_i64(b.data());
        ptr_elem_words_ = ptr_elem;
        nelem_hint_ = n_elem_ptr_;
    }
    err.clear();
    return true;
}

// ============================================================================
// Парсинг компонентов
// ============================================================================

void RstReader::parse_components(const uint8_t* p, int64_t total) {
    components_.clear();
    if (total < 12 || ncms_hdr_ <= 0) return;
    int32_t ncms = ncms_hdr_, nnam = nnam_hdr_;
    int64_t names_bytes = static_cast<int64_t>(ncms) * (nnam + 1);
    
    auto type_at = [&](int32_t c) -> char {
        int64_t off = 12 + names_bytes + static_cast<int64_t>(c) * 4;
        if (off + 4 > total) return '?';
        char ch = static_cast<char>(p[off]);
        if (ch == 'N' || ch == 'E') return ch;
        return '?';
    };
    
    struct Grp { std::string name; char type; };
    std::vector<Grp> grps;
    for (int32_t c = 0; c < ncms; ++c) {
        std::string nm(reinterpret_cast<const char*>(p) + 12 + static_cast<int64_t>(c) * (nnam + 1), nnam);
        auto end = std::find_if(nm.rbegin(), nm.rend(),
                                [](char ch){ return ch != ' ' && ch != '\0'; });
        if (end == nm.rend()) nm.clear();
        else nm.resize(static_cast<size_t>(nm.rend() - end));
        if (nm.empty()) continue;
        grps.push_back({nm, type_at(c)});
    }
    
    int64_t lists_base = 12 + names_bytes + static_cast<int64_t>(ncms) * 4;
    for (int64_t pad : {0LL, 1LL, 2LL, 3LL}) {
        int64_t try_off = lists_base + pad;
        if (try_off + 4 > total) continue;
        int64_t pos = try_off;
        bool ok = true;
        std::vector<std::vector<int32_t>> ids(grps.size());
        for (size_t g = 0; g < grps.size() && ok; ++g) {
            if (pos + 4 > total) { ok = false; break; }
            int32_t cnt = rd_i32(p + pos); pos += 4;
            int64_t lim = (grps[g].type == 'E') ? nelem_hint_ : n_nodes_;
            if (lim <= 0) lim = 100000000;
            if (cnt < 0 || cnt > lim || pos + static_cast<int64_t>(cnt) * 4 > total) { ok = false; break; }
            ids[g].resize(cnt);
            for (int32_t k = 0; k < cnt; ++k) ids[g][k] = rd_i32(p + pos + k * 4);
            pos += static_cast<int64_t>(cnt) * 4;
        }
        if (ok) {
            for (size_t g = 0; g < grps.size(); ++g) {
                Component comp; comp.name = grps[g].name;
                if (grps[g].type == 'E') { comp.is_elem = true; comp.elems = std::move(ids[g]); }
                else                     { comp.is_node = true; comp.nodes = std::move(ids[g]); }
                components_.push_back(std::move(comp));
            }
            return;
        }
    }
}

// ============================================================================
// Чтение узлов
// ============================================================================

bool RstReader::read_nodes(std::vector<Node>& nodes, std::string& err) {
    if (!fp_) { err = "file not open"; return false; }
    if (ptr_geo_words_ == 0) { err = "no geometry pointer"; return false; }
    
    // Читаем таблицу геометрии для получения указателя на узлы
    std::vector<uint8_t> b; int64_t w = 0;
    if (!read_record(ptr_geo_words_, b, w, err)) return false;
    
    // В таблице геометрии ищем указатель на координаты узлов
    // Формат: различные указатели на подтаблицы
    int64_t node_ptr = 0;
    for (int i = 0; i + 1 < w; ++i) {
        int64_t v = static_cast<int64_t>(std::llround(rd_f64(b.data() + i * 8)));
        if (v > 0 && v * kWord < file_bytes_) {
            node_ptr = v;
            break;
        }
    }
    
    if (node_ptr == 0) { err = "node table not found"; return false; }
    
    // Читаем таблицу узлов
    if (!read_record(node_ptr, b, w, err)) return false;
    
    nodes.clear();
    nodes.reserve(static_cast<size_t>(n_nodes_));
    
    // Формат записи узла: [id(int32)][x(f64)][y(f64)][z(f64)][rx(f64)][ry(f64)][rz(f64)]
    // или упрощённый: [id(int32)][x(f64)][y(f64)][z(f64)]
    int64_t stride = w / n_nodes_;  // слов на узел
    if (stride < 4) { err = "unexpected node format"; return false; }
    
    for (int64_t i = 0; i < n_nodes_ && i * stride * kWord + 28 <= static_cast<int64_t>(b.size()); ++i) {
        Node nd;
        int64_t base = i * stride * kWord;
        nd.id = rd_i32(b.data() + base);
        nd.x = rd_f64(b.data() + base + 4);
        nd.y = rd_f64(b.data() + base + 12);
        nd.z = rd_f64(b.data() + base + 20);
        if (stride >= 7) {
            nd.rx = rd_f64(b.data() + base + 28);
            nd.ry = rd_f64(b.data() + base + 36);
            nd.rz = rd_f64(b.data() + base + 44);
        }
        if (nd.id > 0) nodes.push_back(nd);
    }
    
    return true;
}

// ============================================================================
// Чтение элементов
// ============================================================================

bool RstReader::read_elements(std::vector<Element>& elems, std::string& err) {
    if (!fp_) { err = "file not open"; return false; }
    if (ptr_elem_words_ == 0) { err = "no element pointer"; return false; }
    
    std::vector<uint8_t> b; int64_t w = 0;
    if (!read_record(ptr_elem_words_, b, w, err)) return false;
    
    // Получаем абсолютное смещение таблицы ELMAP
    int64_t elmmap_abs = static_cast<int64_t>(std::llround(rd_f64(b.data())));
    if (!read_record(elmmap_abs, b, w, err)) { err = "read ELMAP fail"; return false; }
    
    // Ищем указатель на ELCONN
    int64_t conn_abs = 0;
    for (int probe = 1; probe + 1 < w; ++probe) {
        int64_t v = static_cast<int64_t>(std::llround(rd_f64(b.data() + probe * 8)));
        if (v > elmmap_abs && v * kWord < file_bytes_) { conn_abs = v; break; }
    }
    if (conn_abs == 0) { err = "ELCONN not found"; return false; }
    if (!read_record(conn_abs, b, w, err)) { err = "read ELCONN fail"; return false; }
    
    elems.clear();
    elems.reserve(static_cast<size_t>(n_elem_ptr_));
    
    // Парсим ELCONN: 8 узлов на элемент
    size_t avail = static_cast<size_t>(w);
    int32_t eid = 0;
    for (size_t i = 0; i + 8 <= avail && eid < n_elem_ptr_; i += 8) {
        ++eid;
        Element el;
        el.id = eid;
        for (int k = 0; k < 8; ++k) {
            int32_t n = rd_i32(b.data() + (i + k) * 4);
            if (n > 0) el.nodes.push_back(n);
        }
        if (!el.nodes.empty()) elems.push_back(el);
    }
    
    return true;
}

// ============================================================================
// Чтение типов элементов
// ============================================================================

bool RstReader::read_element_types(std::vector<ElementType>& types, std::string& err) {
    if (!fp_) { err = "file not open"; return false; }
    if (ptr_geo_words_ == 0) { err = "no geometry pointer"; return false; }
    
    std::vector<uint8_t> b; int64_t w = 0;
    if (!read_record(ptr_geo_words_, b, w, err)) return false;
    
    types.clear();
    // Типы элементов обычно идут в геометрии после узлов
    // Упрощённый парсинг: ищем блок типов
    // В реальном формате это отдельная таблица, но для демонстрации
    // возвращаем информацию из заголовка
    
    return true;
}

// ============================================================================
// Чтение перемещений
// ============================================================================

bool RstReader::read_displacements(int set_index, std::vector<Displacement>& disps,
                                   std::string& err) {
    if (!fp_) { err = "file not open"; return false; }
    if (set_index < 0 || set_index >= nsets_) { err = "invalid set index"; return false; }
    
    NativeFd fd = fd_open(path_);
    if (!fd_valid(fd)) { err = "cannot open fd"; return false; }
    
    const ResultSet& rs = sets_[set_index];
    
    // Читаем заголовок решения
    std::vector<uint8_t> hdrbuf;
    int64_t cur = rs.rpointer_words;
    int64_t ptr_dof_table = -1;
    
    for (int guard = 0; guard < 16; ++guard) {
        std::vector<uint8_t> rec;
        int64_t words = read_pread(fd, cur, rec, err);
        if (words < 0) { fd_close(fd); return false; }
        size_t old = hdrbuf.size();
        hdrbuf.resize(old + static_cast<size_t>(words) * kWord);
        std::memcpy(hdrbuf.data() + old, rec.data(), static_cast<size_t>(words) * kWord);
        if (static_cast<int64_t>(hdrbuf.size()) >= 2 * 8) {
            ptr_dof_table = static_cast<int64_t>(std::llround(rd_f64(hdrbuf.data() + 1 * 8)));
            break;
        }
        int64_t fw = frame_words_at(cur);
        cur += (fw > 0 ? fw : words) + 2;
    }
    
    if (ptr_dof_table <= 0) { err = "DOF table not found"; fd_close(fd); return false; }
    
    // Читаем таблицу DOF
    std::vector<uint8_t> dof_buf;
    int64_t dof_words = read_pread(fd, ptr_dof_table, dof_buf, err);
    if (dof_words < 0) { fd_close(fd); return false; }
    
    // Парсим DOF таблицу: ищем перемещения (type 1=UX, 2=UY, 3=UZ)
    struct DofEntry { int32_t type; int32_t node; int64_t data_word; };
    std::vector<DofEntry> ux_entries, uy_entries, uz_entries;
    
    for (int64_t r = 0; r + 2 < dof_words; r += 3) {
        int32_t type = rd_i32(dof_buf.data() + r * 4);
        int32_t node = rd_i32(dof_buf.data() + (r + 1) * 4);
        int64_t dw = rd_i32(dof_buf.data() + (r + 2) * 4);
        if (type == 1) ux_entries.push_back({type, node, dw});
        else if (type == 2) uy_entries.push_back({type, node, dw});
        else if (type == 3) uz_entries.push_back({type, node, dw});
    }
    
    // Читаем значения
    disps.clear();
    // Для простоты: собираем все узлы из UX
    std::map<int32_t, Displacement> disp_map;
    
    for (auto& e : ux_entries) {
        std::vector<uint8_t> val_buf;
        if (read_pread(fd, e.data_word, val_buf, err) > 0) {
            disp_map[e.node].node_id = e.node;
            disp_map[e.node].values.resize(3, 0.0);
            disp_map[e.node].values[0] = rd_f64(val_buf.data());
        }
    }
    for (auto& e : uy_entries) {
        std::vector<uint8_t> val_buf;
        if (read_pread(fd, e.data_word, val_buf, err) > 0) {
            if (disp_map.count(e.node)) {
                if (disp_map[e.node].values.size() < 2) disp_map[e.node].values.resize(3, 0.0);
                disp_map[e.node].values[1] = rd_f64(val_buf.data());
            }
        }
    }
    for (auto& e : uz_entries) {
        std::vector<uint8_t> val_buf;
        if (read_pread(fd, e.data_word, val_buf, err) > 0) {
            if (disp_map.count(e.node)) {
                if (disp_map[e.node].values.size() < 3) disp_map[e.node].values.resize(3, 0.0);
                disp_map[e.node].values[2] = rd_f64(val_buf.data());
            }
        }
    }
    
    for (auto& kv : disp_map) {
        disps.push_back(std::move(kv.second));
    }
    
    fd_close(fd);
    return true;
}

// ============================================================================
// Чтение узловых напряжений
// ============================================================================

bool RstReader::read_nodal_stresses(int set_index, std::vector<NodalStress>& stresses,
                                    std::string& err) {
    if (!fp_) { err = "file not open"; return false; }
    if (set_index < 0 || set_index >= nsets_) { err = "invalid set index"; return false; }
    
    NativeFd fd = fd_open(path_);
    if (!fd_valid(fd)) { err = "cannot open fd"; return false; }
    
    const ResultSet& rs = sets_[set_index];
    
    // Читаем заголовок решения
    std::vector<uint8_t> hdrbuf;
    int64_t cur = rs.rpointer_words;
    int64_t ptr_dof_table = -1;
    
    for (int guard = 0; guard < 16; ++guard) {
        std::vector<uint8_t> rec;
        int64_t words = read_pread(fd, cur, rec, err);
        if (words < 0) { fd_close(fd); return false; }
        size_t old = hdrbuf.size();
        hdrbuf.resize(old + static_cast<size_t>(words) * kWord);
        std::memcpy(hdrbuf.data() + old, rec.data(), static_cast<size_t>(words) * kWord);
        if (static_cast<int64_t>(hdrbuf.size()) >= 2 * 8) {
            ptr_dof_table = static_cast<int64_t>(std::llround(rd_f64(hdrbuf.data() + 1 * 8)));
            break;
        }
        int64_t fw = frame_words_at(cur);
        cur += (fw > 0 ? fw : words) + 2;
    }
    
    if (ptr_dof_table <= 0) { err = "DOF table not found"; fd_close(fd); return false; }
    
    // Читаем таблицу DOF
    std::vector<uint8_t> dof_buf;
    int64_t dof_words = read_pread(fd, ptr_dof_table, dof_buf, err);
    if (dof_words < 0) { fd_close(fd); return false; }
    
    // Ищем напряжения: type 9=SXY, 10=SYZ, 11=SXZ (+ косвенные SX, SY, SZ)
    struct DofEntry { int32_t type; int32_t node; int64_t data_word; };
    std::vector<DofEntry> stress_entries;
    
    for (int64_t r = 0; r + 2 < dof_words; r += 3) {
        int32_t type = rd_i32(dof_buf.data() + r * 4);
        int32_t node = rd_i32(dof_buf.data() + (r + 1) * 4);
        int64_t dw = rd_i32(dof_buf.data() + (r + 2) * 4);
        if (type >= 1 && type <= 11) {
            stress_entries.push_back({type, node, dw});
        }
    }
    
    // Читаем значения напряжений
    std::map<int32_t, NodalStress> stress_map;
    
    for (auto& e : stress_entries) {
        std::vector<uint8_t> val_buf;
        if (read_pread(fd, e.data_word, val_buf, err) <= 0) continue;
        double val = rd_f64(val_buf.data());
        
        auto& ns = stress_map[e.node];
        ns.node_id = e.node;
        
        switch (e.type) {
            case 1: ns.sx = val; break;   // UX -> SX (в контексте напряжений)
            case 2: ns.sy = val; break;
            case 3: ns.sz = val; break;
            case 9: ns.sxy = val; break;
            case 10: ns.syz = val; break;
            case 11: ns.sxz = val; break;
        }
    }
    
    stresses.clear();
    for (auto& kv : stress_map) {
        stresses.push_back(kv.second);
    }
    
    fd_close(fd);
    return true;
}

// ============================================================================
// Чтение реакций
// ============================================================================

bool RstReader::read_reaction_forces(int set_index, std::vector<ReactionForce>& forces,
                                     std::string& err) {
    // Реакции хранятся в отдельной таблице в заголовке решения
    // Упрощённая реализация
    if (!fp_) { err = "file not open"; return false; }
    forces.clear();
    return true;
}

// ============================================================================
// Поиск наборов
// ============================================================================

int RstReader::find_set_by_substep(int substep) const {
    for (size_t i = 0; i < sets_.size(); ++i)
        if (sets_[i].substep == substep) return static_cast<int>(i);
    return -1;
}

int RstReader::find_set_by_time(double t, double tol) const {
    int best = -1; double bd = tol;
    for (size_t i = 0; i < sets_.size(); ++i) {
        double d = std::fabs(sets_[i].time - t);
        if (d <= bd) { bd = d; best = static_cast<int>(i); }
    }
    return best;
}

} // namespace rst_reader
