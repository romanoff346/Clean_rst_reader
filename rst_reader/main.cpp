// main.cpp — демонстрационная программа чтения ANSYS RST файлов
//
// Использование:
//   ./rst_reader_demo <file.rst> [опции]
//
// Опции:
//   --list              показать информацию о файле (наборы, компоненты)
//   --set N             прочитать набор с индексом N (0-based)
//   --substep N         найти набор по номеру подшага
//   --nodes             вывести координаты узлов
//   --elements          вывести элементы
//   --displacements     вывести перемещения
//   --stresses          вывести узловые напряжения
//   --mises             добавить напряжения Мизеса
//   --component NAME    фильтр по Name Selection
//   --csv FILE          вывод в CSV файл
//
// Примеры:
//   ./rst_reader_demo model.rst --list
//   ./rst_reader_demo model.rst --set 0 --stresses --mises
//   ./rst_reader_demo model.rst --substep 5 --component FIX_NODES --displacements --csv out.csv
//
// Подход: прямой бинарный парсинг FORTRAN unformatted sequential формата.
// Аналогично ResRdDemo.F (Fortran) и MultiModelReader/cpp (C++), но
// реализовано с нуля на C++20 без внешних зависимостей.

#include "rst_reader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

using namespace rst_reader;

// ============================================================================
// Вспомогательные функции вывода
// ============================================================================

static void print_separator(char c = '-', int len = 70) {
    for (int i = 0; i < len; ++i) std::putchar(c);
    std::putchar('\n');
}

static void print_header_info(const RstReader& reader) {
    const auto& hdr = reader.header();
    std::printf("=== Информация о файле ===\n");
    std::printf("Файл: %s\n", reader.path().c_str());
    std::printf("Сжатие: %s\n", reader.is_compressed() ? "ANSCOMP (DEFLATE)" : "нет");
    std::printf("Наборов результатов: %d\n", reader.num_sets());
    std::printf("Узлов: %lld\n", (long long)reader.num_nodes());
    std::printf("Элементов: %lld\n", (long long)reader.num_elements());
    std::printf("Компонентов (Name Selections): %zu\n", reader.components().size());
}

static void print_result_sets(const RstReader& reader) {
    std::printf("\n=== Наборы результатов ===\n");
    std::printf("%-6s %-8s %-10s %-15s\n", "Index", "Step", "Substep", "Time");
    print_separator();
    for (size_t i = 0; i < reader.result_sets().size(); ++i) {
        const auto& rs = reader.result_sets()[i];
        std::printf("%-6zu %-8d %-10d %-15.6g\n", i, rs.step, rs.substep, rs.time);
    }
}

static void print_components(const RstReader& reader) {
    std::printf("\n=== Name Selections (компоненты) ===\n");
    if (reader.components().empty()) {
        std::printf("  (нет компонентов)\n");
        return;
    }
    for (const auto& c : reader.components()) {
        if (c.is_elem)
            std::printf("  %-32s [ELEM] %zu элементов\n", c.name.c_str(), c.elems.size());
        else
            std::printf("  %-32s [NODE] %zu узлов\n", c.name.c_str(), c.nodes.size());
    }
}

static void print_nodes(const std::vector<Node>& nodes, int max_print = 20) {
    std::printf("\n=== Узлы (показано %d из %zu) ===\n",
                std::min(max_print, static_cast<int>(nodes.size())), nodes.size());
    std::printf("%-10s %-15s %-15s %-15s\n", "ID", "X", "Y", "Z");
    print_separator();
    int count = 0;
    for (const auto& nd : nodes) {
        if (count++ >= max_print) break;
        std::printf("%-10d %-15.6g %-15.6g %-15.6g\n", nd.id, nd.x, nd.y, nd.z);
    }
    if (static_cast<int>(nodes.size()) > max_print)
        std::printf("  ... ещё %zu узлов\n", nodes.size() - max_print);
}

static void print_elements(const std::vector<Element>& elems, int max_print = 20) {
    std::printf("\n=== Элементы (показано %d из %zu) ===\n",
                std::min(max_print, static_cast<int>(elems.size())), elems.size());
    std::printf("%-10s %-8s %s\n", "ID", "Nodes", "Connectivity");
    print_separator();
    int count = 0;
    for (const auto& el : elems) {
        if (count++ >= max_print) break;
        std::printf("%-10d %-8zu ", el.id, el.nodes.size());
        for (size_t i = 0; i < el.nodes.size() && i < 8; ++i) {
            std::printf("%d ", el.nodes[i]);
        }
        if (el.nodes.size() > 8) std::printf("...");
        std::printf("\n");
    }
    if (static_cast<int>(elems.size()) > max_print)
        std::printf("  ... ещё %zu элементов\n", elems.size() - max_print);
}

static void print_displacements(const std::vector<Displacement>& disps, int max_print = 20) {
    std::printf("\n=== Перемещения (показано %d из %zu) ===\n",
                std::min(max_print, static_cast<int>(disps.size())), disps.size());
    std::printf("%-10s %-15s %-15s %-15s\n", "Node", "UX", "UY", "UZ");
    print_separator();
    int count = 0;
    for (const auto& d : disps) {
        if (count++ >= max_print) break;
        std::printf("%-10d", d.node_id);
        for (int i = 0; i < 3; ++i) {
            if (i < static_cast<int>(d.values.size()))
                std::printf(" %-15.6e", d.values[i]);
            else
                std::printf(" %-15s", "0.0");
        }
        std::printf("\n");
    }
    if (static_cast<int>(disps.size()) > max_print)
        std::printf("  ... ещё %zu записей\n", disps.size() - max_print);
}

static void print_stresses(const std::vector<NodalStress>& stresses, bool show_mises,
                           int max_print = 20) {
    std::printf("\n=== Узловые напряжения (показано %d из %zu) ===\n",
                std::min(max_print, static_cast<int>(stresses.size())), stresses.size());
    if (show_mises)
        std::printf("%-10s %-12s %-12s %-12s %-12s %-12s %-12s %-12s\n",
                    "Node", "SX", "SY", "SZ", "SXY", "SYZ", "SXZ", "MISES");
    else
        std::printf("%-10s %-12s %-12s %-12s %-12s %-12s %-12s\n",
                    "Node", "SX", "SY", "SZ", "SXY", "SYZ", "SXZ");
    print_separator();
    int count = 0;
    for (const auto& s : stresses) {
        if (count++ >= max_print) break;
        std::printf("%-10d %-12.5e %-12.5e %-12.5e %-12.5e %-12.5e %-12.5e",
                    s.node_id, s.sx, s.sy, s.sz, s.sxy, s.syz, s.sxz);
        if (show_mises)
            std::printf(" %-12.5e", s.von_mises());
        std::printf("\n");
    }
    if (static_cast<int>(stresses.size()) > max_print)
        std::printf("  ... ещё %zu записей\n", stresses.size() - max_print);
}

// ============================================================================
// Запись в CSV
// ============================================================================

static bool write_csv(const std::string& path, const std::vector<NodalStress>& stresses,
                      bool show_mises) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    
    if (show_mises)
        std::fprintf(f, "Node,SX,SY,SZ,SXY,SYZ,SXZ,MISES\n");
    else
        std::fprintf(f, "Node,SX,SY,SZ,SXY,SYZ,SXZ\n");
    
    for (const auto& s : stresses) {
        std::fprintf(f, "%d,%.8e,%.8e,%.8e,%.8e,%.8e,%.8e",
                     s.node_id, s.sx, s.sy, s.sz, s.sxy, s.syz, s.sxz);
        if (show_mises)
            std::fprintf(f, ",%.8e", s.von_mises());
        std::fprintf(f, "\n");
    }
    
    std::fclose(f);
    return true;
}

// ============================================================================
// Фильтрация по компоненту
// ============================================================================

static std::vector<NodalStress> filter_by_component(
    const std::vector<NodalStress>& stresses,
    const Component& comp) {
    
    std::set<int32_t> node_set(comp.nodes.begin(), comp.nodes.end());
    std::vector<NodalStress> filtered;
    for (const auto& s : stresses) {
        if (node_set.count(s.node_id))
            filtered.push_back(s);
    }
    return filtered;
}

// ============================================================================
// Использование
// ============================================================================

static void usage() {
    std::printf(
        "ANSYS RST Reader Demo — чтение файлов результатов ANSYS MAPDL\n"
        "=============================================================\n\n"
        "Использование: rst_reader_demo <file.rst> [опции]\n\n"
        "Опции:\n"
        "  --list              показать информацию о файле\n"
        "  --set N             прочитать набор с индексом N (0-based)\n"
        "  --substep N         найти набор по номеру подшага\n"
        "  --nodes             вывести координаты узлов\n"
        "  --elements          вывести элементы\n"
        "  --displacements     вывести перемещения для выбранного набора\n"
        "  --stresses          вывести узловые напряжения\n"
        "  --mises             добавить напряжения Мизеса\n"
        "  --component NAME    фильтр по Name Selection\n"
        "  --csv FILE          сохранить результат в CSV\n"
        "  --help              показать эту справку\n\n"
        "Примеры:\n"
        "  rst_reader_demo model.rst --list\n"
        "  rst_reader_demo model.rst --set 0 --stresses --mises\n"
        "  rst_reader_demo model.rst --substep 5 --component FIX_NODES --displacements\n"
        "  rst_reader_demo model.rst --set 0 --stresses --mises --csv output.csv\n\n"
        "Подход: прямой бинарный парсинг FORTRAN unformatted sequential формата.\n"
        "Поддерживаются сжатые (ANSCOMP) и несжатые RST файлы.\n"
        "Аналогично ResRdDemo.F (binlib.dll) и MultiModelReader/cpp.\n"
    );
}

// ============================================================================
// Главная функция
// ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    
    // Парсинг аргументов
    std::string rst_path = argv[1];
    bool do_list = false;
    bool do_nodes = false;
    bool do_elements = false;
    bool do_displacements = false;
    bool do_stresses = false;
    bool do_mises = false;
    int set_index = -1;
    int find_substep = 0;
    std::string component_name;
    std::string csv_path;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        auto next_arg = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Ошибка: нужен аргумент после %s\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        
        if (arg == "--list") do_list = true;
        else if (arg == "--set") set_index = std::atoi(next_arg());
        else if (arg == "--substep") find_substep = std::atoi(next_arg());
        else if (arg == "--nodes") do_nodes = true;
        else if (arg == "--elements") do_elements = true;
        else if (arg == "--displacements") do_displacements = true;
        else if (arg == "--stresses") do_stresses = true;
        else if (arg == "--mises") do_mises = true;
        else if (arg == "--component") component_name = next_arg();
        else if (arg == "--csv") csv_path = next_arg();
        else if (arg == "--help") { usage(); return 0; }
        else {
            std::fprintf(stderr, "Неизвестная опция: %s\n", arg.c_str());
            usage();
            return 2;
        }
    }
    
    // Открытие файла
    RstReader reader;
    std::string err;
    if (!reader.open(rst_path, err)) {
        std::fprintf(stderr, "Ошибка открытия RST: %s\n", err.c_str());
        return 1;
    }
    
    // Вывод информации
    print_header_info(reader);
    
    if (do_list || (set_index < 0 && find_substep == 0 && !do_nodes && !do_elements)) {
        print_result_sets(reader);
        print_components(reader);
        if (do_list) { reader.close(); return 0; }
        if (set_index < 0 && find_substep == 0 && !do_nodes && !do_elements) {
            std::printf("\nУкажите --set N или --substep N для чтения данных.\n");
            reader.close();
            return 0;
        }
    }
    
    // Разрешение индекса набора
    if (set_index < 0 && find_substep > 0) {
        set_index = reader.find_set_by_substep(find_substep);
        if (set_index < 0) {
            std::fprintf(stderr, "Набор с подшагом %d не найден.\n", find_substep);
            reader.close();
            return 1;
        }
    }
    
    // Чтение узлов
    if (do_nodes) {
        std::vector<Node> nodes;
        if (reader.read_nodes(nodes, err)) {
            print_nodes(nodes);
        } else {
            std::fprintf(stderr, "Ошибка чтения узлов: %s\n", err.c_str());
        }
    }
    
    // Чтение элементов
    if (do_elements) {
        std::vector<Element> elems;
        if (reader.read_elements(elems, err)) {
            print_elements(elems);
        } else {
            std::fprintf(stderr, "Ошибка чтения элементов: %s\n", err.c_str());
        }
    }
    
    // Чтение перемещений
    if (do_displacements && set_index >= 0) {
        std::vector<Displacement> disps;
        if (reader.read_displacements(set_index, disps, err)) {
            print_displacements(disps);
        } else {
            std::fprintf(stderr, "Ошибка чтения перемещений: %s\n", err.c_str());
        }
    }
    
    // Чтение напряжений
    if (do_stresses && set_index >= 0) {
        std::vector<NodalStress> stresses;
        if (reader.read_nodal_stresses(set_index, stresses, err)) {
            // Фильтрация по компоненту
            if (!component_name.empty()) {
                const Component* found = nullptr;
                for (const auto& c : reader.components()) {
                    if (c.name == component_name) { found = &c; break; }
                }
                // Попробовать в верхнем регистре
                if (!found) {
                    std::string upper = component_name;
                    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                    for (const auto& c : reader.components()) {
                        if (c.name == upper) { found = &c; break; }
                    }
                }
                if (found && found->is_node) {
                    stresses = filter_by_component(stresses, *found);
                    std::printf("Отфильтровано по компоненту '%s': %zu узлов\n",
                                found->name.c_str(), stresses.size());
                } else if (found) {
                    std::fprintf(stderr, "Компонент '%s' — элементный, фильтрация по узлам не поддерживается\n",
                                component_name.c_str());
                } else {
                    std::fprintf(stderr, "Компонент '%s' не найден\n", component_name.c_str());
                }
            }
            
            print_stresses(stresses, do_mises);
            
            // Запись в CSV
            if (!csv_path.empty()) {
                if (write_csv(csv_path, stresses, do_mises))
                    std::printf("\nСохранено в: %s\n", csv_path.c_str());
                else
                    std::fprintf(stderr, "Ошибка записи CSV: %s\n", csv_path.c_str());
            }
        } else {
            std::fprintf(stderr, "Ошибка чтения напряжений: %s\n", err.c_str());
        }
    }
    
    reader.close();
    return 0;
}
