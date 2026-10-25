@echo off
REM Build without make (Windows + MinGW GCC). Run from the project root.
if not exist build mkdir build
gcc -std=c99 -Wall -Wextra -pedantic -O2 -D__USE_MINGW_ANSI_STDIO=1 src\main.c src\lexer.c src\parser.c src\ast.c src\semantic.c src\eval.c src\util.c -o build\math_compiler.exe -lm
if errorlevel 1 (echo BUILD FAILED & exit /b 1) else echo Build OK: build\math_compiler.exe
