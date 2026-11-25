# Makefile для ANSYS RST Reader
#
# Сборка: make            (g++ >= 9 / clang >= 12, Linux)
#         make WIN=1      (MSVC / MinGW, Windows)
#
# Требования: C++20 (для std::span, structured bindings)
#
# Подход: прямой бинарный парсинг ANSYS RST файлов без внешних зависимостей.
# Аналогично MultiModelReader/cpp (romanoff346/MultiModelReader).

CXX      ?= g++
CXXFLAGS += -std=c++20 -O2 -Wall -Wextra -I.
LDFLAGS  += -pthread

SRC := rst_reader.cpp deflate.cpp main.cpp
OBJ := $(SRC:.cpp=.o)
BIN := rst_reader_demo

# Windows-сборка
ifdef WIN
  CXX = g++
  LDFLAGS += -static
  BIN := rst_reader_demo.exe
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp rst_reader.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) rst_reader_demo.exe

# Быстрый тест (если есть test.rst)
test: $(BIN)
	@if [ -f test.rst ]; then \
		echo "=== Тест: список наборов ==="; \
		./$(BIN) test.rst --list; \
	else \
		echo "test.rst не найден. Поместите RST файл в текущую директорию."; \
	fi

.PHONY: all clean test
