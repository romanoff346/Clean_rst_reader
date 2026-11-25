# Инструкция по тестированию RST Reader

## Проблема

Программа `rst_reader_demo.exe` выдаёт ошибку:
```
Ошибка открытия RST: record markers mismatch
```

Эта ошибка означает, что pre-маркер и post-маркер FORTRAN-записи не совпадают. Причина — неправильное определение формата файла (слова/байты, 32-bit/64-bit).

## Решение

Код был обновлён для поддержки всех возможных форматов:
1. ✅ reclen в 4-байтных словах (int32) — классический FORTRAN
2. ✅ reclen в 4-байтных байтах (int32)
3. ✅ reclen в 8-байтных словах (int64)
4. ✅ reclen в 8-байтных байтах (int64)
5. ✅ Сжатые записи ANSCOMP (DEFLATE)

Также добавлена проверка второй записи для подтверждения формата.

## Шаги по тестированию

### 1. Скачайте тестовые файлы

```bash
cd rst_reader

# Вариант A: Автоматическое скачивание и анализ
python3 diagnose.py

# Вариант B: Ручное скачивание
curl -L -o file_no_compression.rst "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_no_compression.rst"
curl -L -o file_compressed.rst "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_compressed.rst"
```

### 2. Проанализируйте формат файлов

```bash
python3 analyze_rst.py file_no_compression.rst
python3 analyze_rst.py file_compressed.rst
```

Скрипт выведет:
- Размер файла
- Первые байты (hex)
- Результаты тестов для разных форматов
- Рекомендации по формату

### 3. Скомпилируйте программу

```bash
# Linux
make

# Windows (MinGW)
make WIN=1

# Или вручную
g++ -std=c++20 -O2 -Wall -I. -c rst_reader.cpp -o rst_reader.o
g++ -std=c++20 -O2 -Wall -I. -c deflate.cpp -o deflate.o
g++ -std=c++20 -O2 -Wall -I. -c main.cpp -o main.o
g++ -std=c++20 -o rst_reader_demo rst_reader.o deflate.o main.o -pthread
```

### 4. Протестируйте чтение файлов

```bash
# Несжатый файл
./rst_reader_demo file_no_compression.rst --list

# Сжатый файл
./rst_reader_demo file_compressed.rst --list
```

### 5. Если ошибка сохраняется

Программа выведет подробную диагностическую информацию:
- Размер файла
- Первые 32 байта (hex)
- Значения маркеров для разных форматов
- Совпадение pre/post маркеров

Скопируйте эту информацию и отправьте для анализа.

## Ожидаемый результат

```
=== Информация о файле ===
Файл: file_no_compression.rst
Сжатие: нет
Наборов результатов: 1
Узлов: 8
Элементов: 1
Компонентов (Name Selections): 0

=== Наборы результатов ===
Index  Step     Substep    Time          
----------------------------------------------------------------------
0      1        1          1
```

## Дополнительные возможности

```bash
# Координаты узлов
./rst_reader_demo file_no_compression.rst --nodes

# Элементы
./rst_reader_demo file_no_compression.rst --elements

# Напряжения (если есть)
./rst_reader_demo file_no_compression.rst --set 0 --stresses --mises

# Экспорт в CSV
./rst_reader_demo file_no_compression.rst --set 0 --stresses --csv output.csv
```

## Технические детали

### Формат FORTRAN unformatted sequential

Каждая запись на диске:
```
[int32 reclen][body ...][int32 reclen]
```

где:
- `reclen` — длина тела (в словах или байтах)
- `body` — данные записи

### Определение формата

Программа автоматически определяет формат по первой записи:
1. Читает первые 4 байта как int32 (reclen)
2. Пробует 4 гипотезы:
   - reclen в словах (int32): тело = reclen * 4 байт
   - reclen в байтах (int32): тело = reclen байт
   - reclen в словах (int64): тело = reclen * 4 байт
   - reclen в байтах (int64): тело = reclen байт
3. Для каждой гипотезы проверяет совпадение pre/post маркеров
4. Если несколько гипотез проходят — проверяет вторую запись
5. Для сжатых записей (ANSCOMP) использует байты

### Сжатые записи (ANSCOMP)

Сжатая запись:
```
[int32 reclen]['ANS-' version][orig_len][nchunks][chunk_lens...][data...][int32 reclen]
```

где:
- `reclen` — размер сжатого кадра
- `'ANS-'` — магическое слово
- `orig_len` — исходный размер
- `nchunks` — число чанков
- `chunk_lens` — длины чанков (положительные = zlib, отрицательные = literal)

## Поддержка

Если проблема не решена, предоставьте:
1. Вывод `analyze_rst.py` для проблемного файла
2. Диагностическую информацию из `rst_reader_demo`
3. Размер файла и версию ANSYS, в которой он был создан
