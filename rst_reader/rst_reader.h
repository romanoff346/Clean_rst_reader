// rst_reader.h — заголовочный файл ридера ANSYS RST файлов
//
// Формат: FORTRAN unformatted sequential, little-endian, double precision.
// Все смещения в заголовках указаны в 4-байтных словах (word = 4 байта).
// Запись на диске: [int32 reclen_words][body][int32 reclen_words].
//
// Поддерживаются:
//   - Несжатые RST (OUTRES,...,NOCOMPRESS / COMPRESS,OFF)
//   - Сжатые записи ANSCOMP (OUTRES,...,,ANSYS; по умолчанию в MAPDL >= 2023 R1)
//     Сжатая запись помечается магическим словом 'ANS-' и распаковывается
//     встроенным DEFLATE-декодером.
//
// Подход: прямой бинарный парсинг без зависимостей от ANSYS библиотек.
// Аналогично реализации в MultiModelReader/cpp, но с расширенным API.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

namespace rst_reader {

// ============================================================================
// Константы
// ============================================================================
constexpr int kWord = 4;                          // размер слова в байтах
constexpr int kMaxDofPerNode = 32;                // макс. степеней свободы на узел
constexpr int kMaxNodesPerElem = 24;              // макс. узлов на элемент
constexpr int kNumElementDataSets = 26;           // число типов элементных данных

// Метки элементных данных (аналогично ResRdDemo.F)
inline const char* element_data_labels() {
    static const char* labels[kNumElementDataSets] = {
        "EMS","ENF","ENS","ENG","EGR",
        "EEL","EPL","ECR","ETH","EUL",
        "EFX","ELF","EMN","ECD","ENL",
        "EHC","EPT","ESF","UNK","ETB",
        "ECT","EXY","EBA","ESV","UNK",
        "SRS"
    };
    return *labels;
}

// ============================================================================
// Платформо-зависимые утилиты ввода-вывода
// ============================================================================
using NativeFd =
#if defined(_WIN32)
    HANDLE;
#else
    int;
#endif

NativeFd fd_open(const std::string& path);
void fd_close(NativeFd fd);
bool fd_valid(NativeFd fd);
bool fd_pread(NativeFd fd, void* dst, size_t count, int64_t offset);

// ============================================================================
// Утилиты чтения бинарных данных
// ============================================================================
inline int64_t rd_i64(const uint8_t* p) {
    int64_t v; std::memcpy(&v, p, 8); return v;
}
inline int32_t rd_i32(const uint8_t* p) {
    int32_t v; std::memcpy(&v, p, 4); return v;
}
inline double rd_f64(const uint8_t* p) {
    double v; std::memcpy(&v, p, 8); return v;
}

// Проверка магического слова сжатой записи ANSCOMP
inline bool is_comp_magic(const void* p) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return std::memcmp(b, "ANS-", 4) == 0 && b[4] >= '1' && b[4] <= '9';
}

// ============================================================================
// Структуры данных
// ============================================================================

// Один набор результатов (load step / substep)
struct ResultSet {
    int32_t step = 0;           // номер шага нагружения
    int32_t substep = 0;        // номер подшага
    double  time = 0.0;         // время
    int64_t rpointer_words = 0; // абсолютное смещение заголовка решения (в словах)
};

// Компонент (Name Selection)
struct Component {
    std::string name;
    std::vector<int32_t> nodes;     // узлы (для узлового компонента)
    std::vector<int32_t> elems;     // элементы (для элементного компонента)
    bool is_node = false;
    bool is_elem = false;
};

// Узел модели
struct Node {
    int32_t id = 0;
    double x = 0.0, y = 0.0, z = 0.0;
    //Rotations (если есть)
    double rx = 0.0, ry = 0.0, rz = 0.0;
};

// Элемент модели
struct Element {
    int32_t id = 0;
    int32_t type_id = 0;        // тип элемента
    int32_t real_id = 0;        // номер набора real constants
    int32_t material_id = 0;    // номер материала
    std::vector<int32_t> nodes; // узлы элемента (1-based)
};

// Тип элемента
struct ElementType {
    int32_t id = 0;
    std::vector<int32_t> data;  // параметры типа
};

// Перемещения узла
struct Displacement {
    int32_t node_id = 0;
    std::vector<double> values; // компоненты перемещений (UX, UY, UZ, ROTX, ROTY, ROTZ, ...)
};

// Реакция в узле
struct ReactionForce {
    int32_t node_id = 0;
    int32_t dof = 0;
    double value = 0.0;
};

// Напряжения в узле (6 компонентов: SX, SY, SZ, SXY, SYZ, SXZ)
struct NodalStress {
    int32_t node_id = 0;
    double sx = 0, sy = 0, sz = 0;
    double sxy = 0, syz = 0, sxz = 0;
    
    double von_mises() const {
        return std::sqrt(0.5 * ((sx-sy)*(sx-sy) + (sy-sz)*(sy-sz) + (sx-sz)*(sx-sz))
                         + 3.0 * (sxy*sxy + syz*syz + sxz*sxz));
    }
};

// Заголовок файла результатов
struct FileHeader {
    std::string title;
    std::string subtitle;
    std::string jobname;
    int32_t units = 0;          // единицы измерения
    int32_t num_dof = 0;        // число степеней свободы на узел
    std::vector<int32_t> dof_list;
    int32_t user_code = 0;
};

// ============================================================================
// Основной класс ридера RST файлов
// ============================================================================
class RstReader {
public:
    RstReader() = default;
    ~RstReader();
    
    // Открытие/закрытие файла
    bool open(const std::string& path, std::string& err);
    void close();
    bool is_open() const { return fp_ != nullptr; }
    
    // ---- Метаданные ----
    const FileHeader& header() const { return header_; }
    int num_sets() const { return nsets_; }
    int64_t num_nodes() const { return n_nodes_; }
    int64_t num_elements() const { return n_elem_ptr_; }
    const std::vector<ResultSet>& result_sets() const { return sets_; }
    const std::vector<Component>& components() const { return components_; }
    
    // ---- Геометрия ----
    bool read_nodes(std::vector<Node>& nodes, std::string& err);
    bool read_elements(std::vector<Element>& elems, std::string& err);
    bool read_element_types(std::vector<ElementType>& types, std::string& err);
    
    // ---- Результаты ----
    bool read_displacements(int set_index, std::vector<Displacement>& disps, std::string& err);
    bool read_nodal_stresses(int set_index, std::vector<NodalStress>& stresses, std::string& err);
    bool read_reaction_forces(int set_index, std::vector<ReactionForce>& forces, std::string& err);
    
    // ---- Поиск ----
    int find_set_by_substep(int substep) const;
    int find_set_by_time(double time, double tol = 1e-9) const;
    
    // ---- Информация о файле ----
    bool is_compressed() const { return compressed_; }
    const std::string& path() const { return path_; }

private:
    // Внутренние методы чтения записей
    bool read_record(int64_t word_off, std::vector<uint8_t>& buf,
                     int64_t& words_out, std::string& err);
    int64_t read_pread(NativeFd fd, int64_t word_off, std::vector<uint8_t>& out,
                       std::string& err);
    int64_t frame_words_at(int64_t word_off);
    void detect_reclen_unit();
    
    // Парсинг компонентов
    void parse_components(const uint8_t* p, int64_t total);
    
    // Декомпрессия ANSCOMP
    bool ansc_uncompress(const uint8_t* body, size_t body_bytes,
                         std::vector<uint8_t>& out);
    
    // Данные файла
    FILE* fp_ = nullptr;
    std::string path_;
    int64_t file_bytes_ = 0;
    
    // Заголовок
    FileHeader header_;
    int nsets_ = 0;
    int64_t n_nodes_ = 0;
    int64_t n_elem_ptr_ = 0;
    int32_t maxcomp_ = 0;
    
    // Таблицы
    std::vector<ResultSet> sets_;
    std::vector<Component> components_;
    
    // Указатели на таблицы
    int64_t ptr_geo_words_ = 0;
    int64_t ptr_elem_words_ = 0;
    int64_t ptr_comp_words_ = 0;
    
    // Поддержка сжатых записей
    bool compressed_ = false;
    int64_t reclen_bytes_ = 0;    // 1 = байты, -1 = слова
    bool reclen_64_ = false;      // true = 8-байтные маркеры (int64)
    int64_t nelem_hint_ = 0;
    
    // Кэш распакованных записей
    std::map<int64_t, std::vector<uint8_t>> unc_cache_;
    std::map<int64_t, int64_t> comp_frame_words_;
    mutable std::mutex unc_mtx_;
    
    int32_t ncms_hdr_ = 0, nnam_hdr_ = 0;
};

// ============================================================================
// DEFLATE декодер (минимальная реализация RFC 1951 + zlib обёртка RFC 1950)
// ============================================================================
namespace deflate {

bool inflate_raw(const uint8_t* src, size_t src_len, std::vector<uint8_t>& dst);
bool inflate_zlib(const uint8_t* src, size_t src_len, std::vector<uint8_t>& dst);

} // namespace deflate

} // namespace rst_reader
