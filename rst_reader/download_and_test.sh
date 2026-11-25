#!/bin/bash
# Скрипт для скачивания и анализа тестовых RST файлов

echo "Скачивание тестовых файлов из GitHub..."
echo ""

# Скачиваем файлы
curl -L -o file_no_compression.rst "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_no_compression.rst"
curl -L -o file_compressed.rst "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_compressed.rst"

echo ""
echo "Файлы скачаны. Запуск анализа..."
echo ""

# Запускаем анализ
python3 analyze_rst.py file_no_compression.rst
echo ""
python3 analyze_rst.py file_compressed.rst

echo ""
echo "Теперь попробуйте открыть файлы с помощью rst_reader_demo:"
echo "  ./rst_reader_demo file_no_compression.rst --list"
echo "  ./rst_reader_demo file_compressed.rst --list"
