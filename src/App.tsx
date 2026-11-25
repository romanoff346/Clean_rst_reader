import { useState } from 'react'
import './index.css'

type Tab = 'overview' | 'format' | 'api' | 'usage' | 'code'

function App() {
  const [activeTab, setActiveTab] = useState<Tab>('overview')

  return (
    <div className="app">
      <header className="header">
        <div className="header-content">
          <h1>🔧 ANSYS RST Reader</h1>
          <p className="subtitle">
            Прямой бинарный парсинг файлов результатов ANSYS MAPDL без использования ANSYS
          </p>
        </div>
      </header>

      <nav className="tabs">
        {(['overview', 'format', 'api', 'usage', 'code'] as Tab[]).map(tab => (
          <button
            key={tab}
            className={`tab ${activeTab === tab ? 'active' : ''}`}
            onClick={() => setActiveTab(tab)}
          >
            {tab === 'overview' && '📋 Обзор'}
            {tab === 'format' && '📐 Формат'}
            {tab === 'api' && '🔌 API'}
            {tab === 'usage' && '💻 Использование'}
            {tab === 'code' && '📁 Структура'}
          </button>
        ))}
      </nav>

      <main className="content">
        {activeTab === 'overview' && <OverviewTab />}
        {activeTab === 'format' && <FormatTab />}
        {activeTab === 'api' && <ApiTab />}
        {activeTab === 'usage' && <UsageTab />}
        {activeTab === 'code' && <CodeTab />}
      </main>

      <footer className="footer">
        <p>
          Подход: прямой бинарный парсинг FORTRAN unformatted sequential формата.
          Аналогично ResRdDemo.F (binlib.dll) и MultiModelReader/cpp.
        </p>
        <p>
          <a href="https://github.com/romanoff346/MultiModelReader/tree/ansys2022r2-sparse/tools" target="_blank" rel="noreferrer">
            Источник: MultiModelReader/tools
          </a>
        </p>
      </footer>
    </div>
  )
}

function OverviewTab() {
  return (
    <div className="tab-content">
      <section>
        <h2>🎯 Что это?</h2>
        <p>
          Программа для чтения файлов результатов ANSYS MAPDL (<code>.rst</code> / <code>.rnn</code>)
          <strong> без использования ANSYS</strong>. Реализован прямой бинарный парсинг формата
          FORTRAN unformatted sequential.
        </p>
      </section>

      <section>
        <h2>📊 Анализ исходного кода</h2>
        <p>В репозитории <code>tools/</code> представлены:</p>
        <div className="card-grid">
          <div className="card">
            <h3>ResRdDemo.F</h3>
            <p>Fortran-демо, использующее <code>binlib.dll</code> — проприетарную ANSYS-библиотеку.
            Вызывает функции <code>ResRd*</code> для чтения геометрии, элементов, перемещений, напряжений.</p>
            <div className="tag-list">
              <span className="tag">Fortran</span>
              <span className="tag">binlib.dll</span>
              <span className="tag">ResRd API</span>
            </div>
          </div>
          <div className="card">
            <h3>result_read.f90</h3>
            <p>Альтернативная Fortran-реализация того же подхода. Использует те же функции
            <code> ResRdBegin, ResRdNode, ResRdElem, ResRdDisp</code> и т.д.</p>
            <div className="tag-list">
              <span className="tag">Fortran 90</span>
              <span className="tag">Результаты</span>
            </div>
          </div>
          <div className="card">
            <h3>binlib.dll + библиотеки</h3>
            <p>Проприетарные ANSYS-библиотеки: <code>binlib.dll</code> (основная),
            <code> ServicingCommon.dll</code>, Fortran runtime (<code>libifcoremd.lib</code>,
            <code> libifportmd.lib</code>), системные (<code>kernel32.Lib</code>, <code>msvcrt.lib</code>).</p>
            <div className="tag-list">
              <span className="tag">Windows DLL</span>
              <span className="tag">Intel Fortran</span>
            </div>
          </div>
        </div>
      </section>

      <section>
        <h2>✨ Возможности</h2>
        <div className="features">
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Сжатые записи ANSCOMP</strong>
              <p>Поддержка DEFLATE-сжатия (OUTRES,...,,ANSYS, по умолчанию в MAPDL ≥ 2023 R1)</p>
            </div>
          </div>
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Несжатые записи</strong>
              <p>Классический формат FORTRAN unformatted (OUTRES,...,NOCOMPRESS)</p>
            </div>
          </div>
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Геометрия</strong>
              <p>Координаты узлов, связность элементов, типы элементов</p>
            </div>
          </div>
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Результаты</strong>
              <p>Перемещения, узловые напряжения, реакции, Name Selections</p>
            </div>
          </div>
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Без зависимостей</strong>
              <p>Только стандартная библиотека C++20. Встроенный DEFLATE декодер.</p>
            </div>
          </div>
          <div className="feature">
            <span className="feature-icon">✅</span>
            <div>
              <strong>Кроссплатформенность</strong>
              <p>Linux (pread) и Windows (ReadFile/OVERLAPPED)</p>
            </div>
          </div>
        </div>
      </section>

      <section>
        <h2>🔄 Сравнение подходов</h2>
        <table className="comparison-table">
          <thead>
            <tr>
              <th>Характеристика</th>
              <th>ResRdDemo.F<br/>(binlib.dll)</th>
              <th>ansys-mapdl-reader<br/>(Python)</th>
              <th>Данный ридер<br/>(C++)</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Язык</td>
              <td>Fortran</td>
              <td>Python / Cython</td>
              <td>C++20</td>
            </tr>
            <tr>
              <td>Зависимости</td>
              <td>ANSYS DLL</td>
              <td>pip пакет</td>
              <td>Нет (только C++ std)</td>
            </tr>
            <tr>
              <td>Сжатые файлы</td>
              <td>✅</td>
              <td>✅</td>
              <td>✅</td>
            </tr>
            <tr>
              <td>Скорость</td>
              <td>Быстро</td>
              <td>Средне</td>
              <td>Быстро</td>
            </tr>
            <tr>
              <td>Параллелизм</td>
              <td>Нет</td>
              <td>Нет</td>
              <td>Да (pread)</td>
            </tr>
            <tr>
              <td>Полный API</td>
              <td>✅ (ResRd*)</td>
              <td>✅</td>
              <td>Частичный</td>
            </tr>
          </tbody>
        </table>
      </section>
    </div>
  )
}

function FormatTab() {
  return (
    <div className="tab-content">
      <section>
        <h2>📐 Формат ANSYS RST</h2>
        <p>
          Файл результатов ANSYS MAPDL (<code>.rst</code>) использует формат
          <strong> FORTRAN unformatted sequential</strong>: little-endian, double precision.
          Все смещения в заголовках указаны в 4-байтных словах (word = 4 байта).
        </p>
      </section>

      <section>
        <h2>Структура FORTRAN-записи</h2>
        <div className="code-block">
          <pre>{`┌──────────────────────────────────────────────┐
│  FORTRAN Record (несжатая)                    │
│                                               │
│  [int32 reclen] [body ...] [int32 reclen]     │
│       4 bytes    reclen*4 bytes    4 bytes     │
│                                               │
│  reclen — длина тела в 4-байтных словах       │
│  (или в байтах, определяется автоматически)   │
└──────────────────────────────────────────────┘`}</pre>
        </div>
      </section>

      <section>
        <h2>Структура сжатой записи (ANSCOMP)</h2>
        <div className="code-block">
          <pre>{`┌──────────────────────────────────────────────────────────┐
│  Сжатая FORTRAN Record (ANSCOMP)                          │
│                                                           │
│  [int32 reclen] ['ANS-'][ver][orig_len][nchunks]          │
│       4 bytes      4+1+3    4 bytes    4 bytes            │
│                                                           │
│  [chunk_len_1][chunk_len_2]...[chunk_len_n]               │
│      4 bytes      4 bytes        4 bytes                  │
│                                                           │
│  [compressed data chunks ...]                             │
│                                                           │
│  [int32 reclen]                                           │
│      4 bytes                                              │
│                                                           │
│  chunk_len > 0: zlib deflate поток                        │
│  chunk_len < 0: literal (несжатые данные, |len| байт)     │
└──────────────────────────────────────────────────────────┘`}</pre>
        </div>
      </section>

      <section>
        <h2>Общая структура файла</h2>
        <div className="code-block">
          <pre>{`Запись 1 (word 0): Указатели на таблицы
  [PTRPTR] [PTRDAT] [PRSOL] [PTRGEO] [PTRELM] [PTRCMS]
  
PTRDAT → Запись: Таблица данных
  [Glbnnod] [???] [Maxcomp] [...]
  
PRSOL → Запись: Таблица решений
  [Nsets] [RPNTR] [TPRNT3D]
  
RPNTR → Запись: Указатели на заголовки решений
  [rpointer_1] [rpointer_2] ... [rpointer_Nsets]
  
TPRNT3D → Записи: Step/Substep/Time
  [step(int32)][substep(int32)][time(int32)] × N
  
PTRGEO → Запись: Геометрия (узлы, типы)
PTRELM → Запись: Элементы (ELMAP → ELCONN)
PTRCMS → Запись: Name Selections (компоненты)

Для каждого набора результатов:
  RSPEC → [ptr_dof_table] [...]
  DOF Table → [type][node][data_word] × N
  Data → значения по указателям из DOF Table`}</pre>
        </div>
      </section>

      <section>
        <h2>Типы DOF (степеней свободы)</h2>
        <table className="data-table">
          <thead>
            <tr><th>Тип</th><th>Описание</th></tr>
          </thead>
          <tbody>
            <tr><td>1</td><td>UX — перемещение по X</td></tr>
            <tr><td>2</td><td>UY — перемещение по Y</td></tr>
            <tr><td>3</td><td>UZ — перемещение по Z</td></tr>
            <tr><td>4</td><td>ROTX — поворот вокруг X</td></tr>
            <tr><td>5</td><td>ROTY — поворот вокруг Y</td></tr>
            <tr><td>6</td><td>ROTZ — поворот вокруг Z</td></tr>
            <tr><td>7</td><td>AX — потенциал (магнитный)</td></tr>
            <tr><td>8</td><td>TEMP — температура</td></tr>
            <tr><td>9</td><td>SXY — касательное напряжение XY</td></tr>
            <tr><td>10</td><td>SYZ — касательное напряжение YZ</td></tr>
            <tr><td>11</td><td>SXZ — касательное напряжение XZ</td></tr>
          </tbody>
        </table>
      </section>

      <section>
        <h2>Элементные данные (26 типов)</h2>
        <div className="code-block">
          <pre>{`EMS — Miscellaneous stresses
ENF — Nodal forces
ENS — Nodal stresses
ENG — Volume and energy sums
EGR — Nodal gradients
EEL — Elastic strains
EPL — Plastic strains
ECR — Creep strains
ETH — Thermal strains
EUL — User data
EFX — Nodal fluxes
ELF — Local forces
EMN — Miscellaneous non-summable
ECD — Element currents
ENL — Nodal nonlinear data
EHC — Heat generation
EPT — Element temperatures
ESF — Element state flags
ETB — Table data
ECT — Contact data
EXY — Integration point locations
EBA — Backward Euler data
ESV — State variables
SRS — Source result data`}</pre>
        </div>
      </section>
    </div>
  )
}

function ApiTab() {
  return (
    <div className="tab-content">
      <section>
        <h2>🔌 API ридера</h2>
        <p>Основные классы и методы для работы с RST файлами.</p>
      </section>

      <section>
        <h2>Класс RstReader</h2>
        <div className="code-block">
          <pre>{`class RstReader {
public:
    // Открытие/закрытие файла
    bool open(const std::string& path, std::string& err);
    void close();
    bool is_open() const;
    
    // Метаданные
    const FileHeader& header() const;
    int num_sets() const;
    int64_t num_nodes() const;
    int64_t num_elements() const;
    const std::vector<ResultSet>& result_sets() const;
    const std::vector<Component>& components() const;
    
    // Геометрия
    bool read_nodes(std::vector<Node>& nodes, std::string& err);
    bool read_elements(std::vector<Element>& elems, std::string& err);
    bool read_element_types(std::vector<ElementType>& types, std::string& err);
    
    // Результаты
    bool read_displacements(int set_index, 
                           std::vector<Displacement>& disps, 
                           std::string& err);
    bool read_nodal_stresses(int set_index, 
                            std::vector<NodalStress>& stresses, 
                            std::string& err);
    bool read_reaction_forces(int set_index, 
                             std::vector<ReactionForce>& forces, 
                             std::string& err);
    
    // Поиск
    int find_set_by_substep(int substep) const;
    int find_set_by_time(double time, double tol = 1e-9) const;
    
    // Информация
    bool is_compressed() const;
    const std::string& path() const;
};`}</pre>
        </div>
      </section>

      <section>
        <h2>Структуры данных</h2>
        
        <h3>ResultSet</h3>
        <div className="code-block">
          <pre>{`struct ResultSet {
    int32_t step;           // номер шага нагружения
    int32_t substep;        // номер подшага
    double  time;           // время
    int64_t rpointer_words; // смещение заголовка решения (в словах)
};`}</pre>
        </div>

        <h3>Node</h3>
        <div className="code-block">
          <pre>{`struct Node {
    int32_t id;
    double x, y, z;        // координаты
    double rx, ry, rz;     // вращения (если есть)
};`}</pre>
        </div>

        <h3>Element</h3>
        <div className="code-block">
          <pre>{`struct Element {
    int32_t id;
    int32_t type_id;
    int32_t real_id;
    int32_t material_id;
    std::vector<int32_t> nodes;  // узлы (1-based)
};`}</pre>
        </div>

        <h3>NodalStress</h3>
        <div className="code-block">
          <pre>{`struct NodalStress {
    int32_t node_id;
    double sx, sy, sz;     // нормальные напряжения
    double sxy, syz, sxz;  // касательные напряжения
    
    double von_mises() const;  // эквивалентное напряжение
};`}</pre>
        </div>

        <h3>Component (Name Selection)</h3>
        <div className="code-block">
          <pre>{`struct Component {
    std::string name;
    std::vector<int32_t> nodes;  // для узлового компонента
    std::vector<int32_t> elems;  // для элементного компонента
    bool is_node;
    bool is_elem;
};`}</pre>
        </div>
      </section>

      <section>
        <h2>Аналог в Fortran (ResRdDemo.F)</h2>
        <p>Для сравнения — как выглядит чтение тех же данных через ANSYS API:</p>
        <div className="code-block">
          <pre>{`c  Открытие файла
      kerr = ResRdBegin(Nunit, Lunit, Fname, 8, Title, JobName,
     x     Units, NumDof, DOF, UserCode,
     x     MaxNode, NumNode, MaxElem, NumElem,
     x     MaxResultSet, NumResultSet)

c  Чтение узлов
      call ResRdNodeBegin
      do i = 1, nXYZnode
         n = ResRdNode(i, xyz(1))
         if (n .gt. 0) then
            write (*,*) 'Node=', n, 'xyz=', xyz
         endif
      enddo
      call ResRdNodeEnd

c  Чтение перемещений (для набора iSet)
      call ResRdDispBegin
      do i = 1, MaxNode
         k = ResRdDisp(i, Disp(1))
         if (k .gt. 0) then
            write (*,*) 'Node=', i, 'Disp=', Disp
         endif
      enddo
      call ResRdDispEnd

c  Закрытие
      call ResRdEnd`}</pre>
        </div>
      </section>
    </div>
  )
}

function UsageTab() {
  return (
    <div className="tab-content">
      <section>
        <h2>💻 Использование</h2>
      </section>

      <section>
        <h2>Сборка</h2>
        <div className="code-block">
          <pre>{`# Linux (g++ / clang)
cd rst_reader
make

# Windows (MinGW)
make WIN=1

# Ручная сборка
g++ -std=c++20 -O2 -Wall -I. -c rst_reader.cpp -o rst_reader.o
g++ -std=c++20 -O2 -Wall -I. -c deflate.cpp -o deflate.o
g++ -std=c++20 -O2 -Wall -I. -c main.cpp -o main.o
g++ -std=c++20 -o rst_reader_demo rst_reader.o deflate.o main.o -pthread`}</pre>
        </div>
      </section>

      <section>
        <h2>Командная строка</h2>
        <div className="code-block">
          <pre>{`./rst_reader_demo <file.rst> [опции]

Опции:
  --list              показать информацию о файле
  --set N             прочитать набор с индексом N (0-based)
  --substep N         найти набор по номеру подшага
  --nodes             вывести координаты узлов
  --elements          вывести элементы
  --displacements     вывести перемещения
  --stresses          вывести узловые напряжения
  --mises             добавить напряжения Мизеса
  --component NAME    фильтр по Name Selection
  --csv FILE          сохранить результат в CSV`}</pre>
        </div>
      </section>

      <section>
        <h2>Примеры</h2>
        
        <h3>Информация о файле</h3>
        <div className="code-block">
          <pre>{`$ ./rst_reader_demo model.rst --list

=== Информация о файле ===
Файл: model.rst
Сжатие: ANSCOMP (DEFLATE)
Наборов результатов: 10
Узлов: 15420
Элементов: 12800
Компонентов (Name Selections): 5

=== Наборы результатов ===
Index  Step     Substep    Time          
----------------------------------------------------------------------
0      1        1          1            
1      1        2          2            
2      1        3          3            
...

=== Name Selections ===
  FIX_NODES                        [NODE] 42 узлов
  LOAD_FACE                        [ELEM] 128 элементов
  SYMMETRY                         [NODE] 356 узлов`}</pre>
        </div>

        <h3>Напряжения с фильтром</h3>
        <div className="code-block">
          <pre>{`$ ./rst_reader_demo model.rst --set 0 --stresses --mises --component FIX_NODES

=== Узловые напряжения (отфильтровано по компоненту 'FIX_NODES': 42 узлов) ===
Node       SX           SY           SZ           SXY          SYZ          SXZ          MISES      
----------------------------------------------------------------------
1          1.234e+08    -5.678e+06   2.345e+07    1.111e+06    2.222e+06    3.333e+06    1.312e+08
2          1.156e+08    -4.890e+06   2.100e+07    9.876e+05    1.987e+06    2.876e+06    1.225e+08
...`}</pre>
        </div>

        <h3>Экспорт в CSV</h3>
        <div className="code-block">
          <pre>{`$ ./rst_reader_demo model.rst --set 0 --stresses --mises --csv output.csv

Сохранено в: output.csv

# Содержимое output.csv:
Node,SX,SY,SZ,SXY,SYZ,SXZ,MISES
1,1.234e+08,-5.678e+06,2.345e+07,1.111e+06,2.222e+06,3.333e+06,1.312e+08
2,1.156e+08,-4.890e+06,2.100e+07,9.876e+05,1.987e+06,2.876e+06,1.225e+08
...`}</pre>
        </div>
      </section>

      <section>
        <h2>Использование как библиотеки</h2>
        <div className="code-block">
          <pre>{`#include "rst_reader.h"
#include &lt;cstdio&gt;

int main() {
    rst_reader::RstReader reader;
    std::string err;
    
    // Открытие файла
    if (!reader.open("model.rst", err)) {
        fprintf(stderr, "Ошибка: %s\\n", err.c_str());
        return 1;
    }
    
    // Информация
    printf("Наборов: %d\\n", reader.num_sets());
    printf("Узлов: %lld\\n", (long long)reader.num_nodes());
    
    // Чтение узлов
    std::vector&lt;rst_reader::Node&gt; nodes;
    reader.read_nodes(nodes, err);
    
    // Чтение напряжений для набора 0
    std::vector&lt;rst_reader::NodalStress&gt; stresses;
    reader.read_nodal_stresses(0, stresses, err);
    
    for (const auto& s : stresses) {
        printf("Node %d: von Mises = %.3e\\n", 
               s.node_id, s.von_mises());
    }
    
    reader.close();
    return 0;
}`}</pre>
        </div>
      </section>

      <section>
        <h2>Требования</h2>
        <ul>
          <li>C++ компилятор с поддержкой C++20 (g++ ≥ 9, clang ≥ 12, MSVC 2019+)</li>
          <li>Make (опционально)</li>
          <li>Не требует ANSYS или других внешних библиотек</li>
        </ul>
      </section>
    </div>
  )
}

function CodeTab() {
  return (
    <div className="tab-content">
      <section>
        <h2>📁 Структура проекта</h2>
        <div className="code-block">
          <pre>{`rst_reader/
├── rst_reader.h      — заголовочный файл (структуры данных, API)
├── rst_reader.cpp    — реализация ридера (парсинг RST, чтение данных)
├── deflate.cpp       — DEFLATE декодер (RFC 1951 + zlib обёртка)
├── main.cpp          — демонстрационная программа (CLI)
├── Makefile          — скрипт сборки
└── README.md         — документация`}</pre>
        </div>
      </section>

      <section>
        <h2>rst_reader.h</h2>
        <p>Заголовочный файл. Содержит:</p>
        <ul>
          <li>Константы (размер слова, макс. DOF, типы данных)</li>
          <li>Платформо-зависимые утилиты (fd_open, fd_pread)</li>
          <li>Утилиты чтения (rd_i32, rd_i64, rd_f64)</li>
          <li>Структуры данных (ResultSet, Node, Element, NodalStress, ...)</li>
          <li>Класс <code>RstReader</code> — основной API</li>
          <li>Namespace <code>deflate</code> — DEFLATE декодер</li>
        </ul>
      </section>

      <section>
        <h2>rst_reader.cpp</h2>
        <p>Основная реализация. Ключевые методы:</p>
        <ul>
          <li><code>open()</code> — открытие файла, чтение указателей и метаданных</li>
          <li><code>read_record()</code> — чтение одной FORTRAN-записи</li>
          <li><code>read_pread()</code> — потокобезопасное позиционное чтение</li>
          <li><code>ansc_uncompress()</code> — распаковка ANSCOMP записей</li>
          <li><code>detect_reclen_unit()</code> — определение единиц маркеров (слова/байты)</li>
          <li><code>parse_components()</code> — парсинг Name Selections</li>
          <li><code>read_nodes()</code> — чтение координат узлов</li>
          <li><code>read_elements()</code> — чтение связности элементов</li>
          <li><code>read_displacements()</code> — чтение перемещений</li>
          <li><code>read_nodal_stresses()</code> — чтение узловых напряжений</li>
        </ul>
      </section>

      <section>
        <h2>deflate.cpp</h2>
        <p>Минимальный DEFLATE-декодер (RFC 1951) + zlib-обёртка (RFC 1950).</p>
        <ul>
          <li><code>BitReader</code> — побитовое чтение (LSB-first)</li>
          <li><code>Huff</code> — канонический Huffman-кодер</li>
          <li><code>inflate_raw()</code> — распаковка raw deflate потока</li>
          <li><code>inflate_zlib()</code> — распаковка zlib-обёрнутого потока</li>
        </ul>
        <p>Поддерживает все три типа блоков DEFLATE: stored, fixed Huffman, dynamic Huffman.</p>
      </section>

      <section>
        <h2>main.cpp</h2>
        <p>Демонстрационная CLI-программа. Парсит аргументы командной строки и вызывает
        соответствующие методы <code>RstReader</code>. Поддерживает:</p>
        <ul>
          <li>Вывод информации о файле (<code>--list</code>)</li>
          <li>Чтение геометрии (<code>--nodes</code>, <code>--elements</code>)</li>
          <li>Чтение результатов (<code>--displacements</code>, <code>--stresses</code>)</li>
          <li>Фильтрацию по компонентам (<code>--component</code>)</li>
          <li>Экспорт в CSV (<code>--csv</code>)</li>
        </ul>
      </section>

      <section>
        <h2>Ключевые алгоритмы</h2>
        
        <h3>Определение единиц маркеров длины</h3>
        <div className="code-block">
          <pre>{`// Классический FORTRAN хранит reclen в 4-байтных СЛОВАХ;
// некоторые сборки пишут его в БАЙТАХ. Определяем по первой записи:
// 1. Читаем первое int32 как reclen
// 2. Проверяем гипотезу "слова": reclen * 4 + 8 <= file_size
// 3. Проверяем гипотезу "байты": reclen + 8 <= file_size
// 4. Сверяем пост-маркер с пре-маркером
// 5. Если слова не совпадают, а байты да — используем байты`}</pre>
        </div>

        <h3>Распаковка ANSCOMP</h3>
        <div className="code-block">
          <pre>{`// 1. Проверяем магическое слово 'ANS-' + версия
// 2. Читаем orig_len (исходный размер) и nchunks (число чанков)
// 3. Для каждого чанка:
//    - clen > 0: zlib inflate (сжатые данные)
//    - clen < 0: literal (несжатые данные, |clen| байт)
//    - clen == 0: пропуск
// 4. Обрезаем результат до orig_len (отбрасываем padding)`}</pre>
        </div>

        <h3>Парсинг DOF таблицы</h3>
        <div className="code-block">
          <pre>{`// DOF таблица: массив записей по 3 int32:
//   [type] [node] [data_word]
// type: 1=UX, 2=UY, 3=UZ, 9=SXY, 10=SYZ, 11=SXZ, ...
// node: номер узла (1-based)
// data_word: словесное смещение значения в файле
//
// Для чтения напряжения узла N типа T:
//   1. Найти запись (T, N, dw) в DOF таблице
//   2. Прочитать FORTRAN-запись по смещению dw
//   3. Первый double в записи = значение`}</pre>
        </div>
      </section>
    </div>
  )
}

export default App
