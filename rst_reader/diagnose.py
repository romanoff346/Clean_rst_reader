#!/usr/bin/env python3
"""
Скрипт для скачивания тестовых файлов и анализа их структуры.
Запустите этот скрипт для диагностики проблем с открытием RST файлов.
"""

import subprocess
import sys
import os
from pathlib import Path

def download_files():
    """Скачивает тестовые файлы из GitHub"""
    print("Скачивание тестовых файлов из GitHub...")
    print()
    
    files = [
        ("file_no_compression.rst", "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_no_compression.rst"),
        ("file_compressed.rst", "https://github.com/romanoff346/Clean_rst_reader/raw/main/file_compressed.rst"),
    ]
    
    for filename, url in files:
        print(f"Скачивание {filename}...")
        result = subprocess.run(
            ["curl", "-L", "-o", filename, url],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            size = Path(filename).stat().st_size
            print(f"  ✓ Скачан: {size} байт ({size/1024:.1f} КБ)")
        else:
            print(f"  ✗ Ошибка скачивания: {result.stderr}")
    print()

def analyze_files():
    """Анализирует структуру файлов"""
    print("="*70)
    print("Анализ структуры файлов")
    print("="*70)
    print()
    
    # Запускаем analyze_rst.py для каждого файла
    for filename in ["file_no_compression.rst", "file_compressed.rst"]:
        if not Path(filename).exists():
            print(f"Файл {filename} не найден, пропускаем анализ")
            continue
        
        print(f"\n{'='*70}")
        print(f"Анализ: {filename}")
        print(f"{'='*70}\n")
        
        result = subprocess.run(
            [sys.executable, "analyze_rst.py", filename],
            capture_output=False
        )

def main():
    print("ANSYS RST Reader — Диагностика формата файлов")
    print("="*70)
    print()
    
    # Скачиваем файлы
    download_files()
    
    # Анализируем файлы
    analyze_files()
    
    print("\n" + "="*70)
    print("Следующие шаги:")
    print("="*70)
    print()
    print("1. Скомпилируйте программу:")
    print("   cd rst_reader")
    print("   make")
    print()
    print("2. Попробуйте открыть файлы:")
    print("   ./rst_reader_demo file_no_compression.rst --list")
    print("   ./rst_reader_demo file_compressed.rst --list")
    print()
    print("3. Если возникнет ошибка, программа выведет диагностическую информацию.")
    print("   Скопируйте её и отправьте для анализа.")
    print()

if __name__ == '__main__':
    main()
