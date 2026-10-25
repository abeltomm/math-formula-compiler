CC      ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2 -D__USE_MINGW_ANSI_STDIO=1
SRC     = src/main.c src/lexer.c src/parser.c src/ast.c src/semantic.c src/eval.c src/util.c
ifeq ($(OS),Windows_NT)
  TARGET = build/math_compiler.exe
else
  TARGET = build/math_compiler
endif

all: $(TARGET)
$(TARGET): $(SRC) $(wildcard src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) -lm
test: all
	python tests/test_runner.py
clean:
	rm -f build/math_compiler build/math_compiler.exe
