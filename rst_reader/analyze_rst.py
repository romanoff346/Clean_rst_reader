#!/usr/bin/env python3
"""
Диагностический скрипт для анализа структуры ANSYS RST файлов.
Помогает определить формат записей (маркеры длины, сжатие и т.д.)
"""

import struct
import sys
from pathlib import Path

def analyze_rst_file(filepath):
    """Анализирует структуру RST файла"""
    print(f"\n{'='*70}")
    print(f"Анализ файла: {filepath}")
    print(f"{'='*70}\n")
    
    with open(filepath, 'rb') as f:
        # Размер файла
        f.seek(0, 2)
        file_size = f.tell()
        f.seek(0)
        print(f"Размер файла: {file_size} байт ({file_size/1024/1024:.2f} МБ)\n")
        
        # Читаем первые 100 байт для анализа
        header = f.read(100)
        
        print("Первые 100 байт (hex):")
        for i in range(0, min(100, len(header)), 16):
            hex_str = ' '.join(f'{b:02x}' for b in header[i:i+16])
            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in header[i:i+16])
            print(f"{i:04x}: {hex_str:<48} {ascii_str}")
        
        print("\n" + "-"*70)
        print("Анализ первой FORTRAN-записи:")
        print("-"*70)
        
        # Пробуем разные интерпретации
        f.seek(0)
        
        # Гипотеза 1: reclen в словах (4 байта)
        reclen_words = struct.unpack('<i', f.read(4))[0]
        print(f"\nГипотеза 1: reclen в СЛОВАХ (4 байта)")
        print(f"  reclen = {reclen_words} слов = {reclen_words * 4} байт")
        
        if reclen_words > 0 and reclen_words * 4 + 8 <= file_size:
            # Читаем тело
            body_words = f.read(reclen_words * 4)
            reclen_post_words = struct.unpack('<i', f.read(4))[0]
            print(f"  post-reclen = {reclen_post_words} слов")
            print(f"  Совпадение маркеров: {reclen_words == reclen_post_words}")
            
            # Проверяем на ANSCOMP
            if len(body_words) >= 8:
                magic = body_words[:4]
                print(f"  Магическое слово: {magic} ({magic.hex()})")
                if magic == b'ANS-':
                    print(f"  ✓ СЖАТАЯ запись ANSCOMP!")
                    version = body_words[4:5]
                    orig_len = struct.unpack('<i', body_words[12:16])[0]
                    nchunks = struct.unpack('<i', body_words[16:20])[0]
                    print(f"    Версия: {version}")
                    print(f"    Исходный размер: {orig_len} байт")
                    print(f"    Число чанков: {nchunks}")
        
        # Гипотеза 2: reclen в байтах (4 байта)
        f.seek(0)
        reclen_bytes = struct.unpack('<i', f.read(4))[0]
        print(f"\nГипотеза 2: reclen в БАЙТАХ (4 байта)")
        print(f"  reclen = {reclen_bytes} байт")
        
        if reclen_bytes > 0 and reclen_bytes + 8 <= file_size:
            f.seek(4)  # пропускаем reclen
            body_bytes = f.read(reclen_bytes)
            reclen_post_bytes = struct.unpack('<i', f.read(4))[0]
            print(f"  post-reclen = {reclen_post_bytes} байт")
            print(f"  Совпадение маркеров: {reclen_bytes == reclen_post_bytes}")
            
            # Проверяем на ANSCOMP
            if len(body_bytes) >= 8:
                magic = body_bytes[:4]
                print(f"  Магическое слово: {magic} ({magic.hex()})")
                if magic == b'ANS-':
                    print(f"  ✓ СЖАТАЯ запись ANSCOMP!")
        
        # Гипотеза 3: reclen в словах (8 байт, int64)
        f.seek(0)
        reclen_words_64 = struct.unpack('<q', f.read(8))[0]
        print(f"\nГипотеза 3: reclen в СЛОВАХ (8 байт, int64)")
        print(f"  reclen = {reclen_words_64} слов = {reclen_words_64 * 4} байт")
        
        if reclen_words_64 > 0 and reclen_words_64 * 4 + 16 <= file_size:
            body_words_64 = f.read(reclen_words_64 * 4)
            reclen_post_words_64 = struct.unpack('<q', f.read(8))[0]
            print(f"  post-reclen = {reclen_post_words_64} слов")
            print(f"  Совпадение маркеров: {reclen_words_64 == reclen_post_words_64}")
        
        # Гипотеза 4: reclen в байтах (8 байт, int64)
        f.seek(0)
        reclen_bytes_64 = struct.unpack('<q', f.read(8))[0]
        print(f"\nГипотеза 4: reclen в БАЙТАХ (8 байт, int64)")
        print(f"  reclen = {reclen_bytes_64} байт")
        
        if reclen_bytes_64 > 0 and reclen_bytes_64 + 16 <= file_size:
            f.seek(8)
            body_bytes_64 = f.read(reclen_bytes_64)
            reclen_post_bytes_64 = struct.unpack('<q', f.read(8))[0]
            print(f"  post-reclen = {reclen_post_bytes_64} байт")
            print(f"  Совпадение маркеров: {reclen_bytes_64 == reclen_post_bytes_64}")
        
        # Гипотеза 5: Stream I/O (без маркеров)
        print(f"\nГипотеза 5: Stream I/O (без маркеров)")
        print(f"  Данные идут подряд без заголовков записей")
        
        print("\n" + "="*70)
        print("Рекомендации:")
        print("="*70)
        
        # Определяем наиболее вероятный формат
        f.seek(0)
        test_val = struct.unpack('<i', f.read(4))[0]
        
        if test_val > 0 and test_val < 1000000:
            if test_val * 4 + 8 <= file_size:
                print("✓ Наиболее вероятно: reclen в СЛОВАХ (4 байта)")
                print("  Используйте: reclen_bytes_ = -1")
            elif test_val + 8 <= file_size:
                print("✓ Наиболее вероятно: reclen в БАЙТАХ (4 байта)")
                print("  Используйте: reclen_bytes_ = 1")
        else:
            print("? Не удалось определить формат автоматически")
            print("  Проверьте гипотезы выше и выберите подходящую")

def main():
    if len(sys.argv) < 2:
        print("Использование: python analyze_rst.py <file.rst>")
        print("\nПример:")
        print("  python analyze_rst.py file_no_compression.rst")
        print("  python analyze_rst.py file_compressed.rst")
        sys.exit(1)
    
    for filepath in sys.argv[1:]:
        if not Path(filepath).exists():
            print(f"Файл не найден: {filepath}")
            continue
        analyze_rst_file(filepath)

if __name__ == '__main__':
    main()
