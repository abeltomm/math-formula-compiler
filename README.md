# Mathematical Formula Compiler — Compiler Design Mini Project

A **compiler front end** for mathematical formulas, written in C, with a dark-themed Tkinter desktop interface.
A formula passes through real compiler stages — **lexical analysis → syntax analysis (AST) → semantic analysis** —
and every token, tree, symbol and error shown in the GUI is produced by the C compiler at run time (nothing is hardcoded).
An optional back end evaluates the AST with user-supplied variable values.

## Features
* Hand-written lexer: token type, lexeme, line:col; rejects invalid characters and malformed numbers (`3.`, `.5`, `1.2.3`, `12abc`).
* Recursive-descent parser for an LL(1) grammar with correct precedence and associativity (`2^3^2`, `-2^2`).
* Real AST in C (`Number`, `Variable`, `Unary`, `Binary`, `Call` nodes) printed as a tree.
* Separate semantic pass: function names, argument counts, symbol table, static division-by-zero and `sqrt(negative)` using safe constant folding.
* Categorised diagnostics (Lexical / Syntax / Semantic / Runtime) with source positions; the program never crashes on bad input.
* Pipeline view that marks a failed stage and shows later stages as *skipped*.
* Automated test runner: 20 valid + 27 invalid + 10 evaluation cases through the real executable.

## Language specification
| Element | Supported |
|---|---|
| Numbers | integers `10`, decimals `3.14`, `0.5` (digit required on both sides of `.`) |
| Identifiers | `[A-Za-z_][A-Za-z0-9_]*`, case-sensitive |
| Operators | `+ - * / ^`, unary `+ -`, parentheses, comma |
| Functions | `sqrt(e)`, `pow(e, e)` — case-sensitive; anything else is a semantic error |

**Limitations:** no scientific notation (`1e5`), no assignment/declarations, no implicit multiplication (`2x`), only `sqrt` and `pow`,
ASCII only, max 2000 characters, ~200 nested parentheses, the parser stops at the first syntax error (no error recovery),
and constant folding only sees literals (`x/(y-y)` is not flagged).

## Compiler architecture
`Source → Lexer → Token stream → Parser → AST → Semantic analyzer → Result` (details: [docs/architecture.md](docs/architecture.md)).
Each stage consumes only the previous stage's output. `main.c` stops at the first failed stage.

## CFG and grammar
```
expression     -> additive
additive       -> multiplicative { ("+" | "-") multiplicative }
multiplicative -> unary { ("*" | "/") unary }
unary          -> ("+" | "-") unary | power
power          -> primary [ "^" unary ]
primary        -> NUMBER | IDENTIFIER | call | "(" expression ")"
call           -> (FUNCTION | IDENTIFIER) "(" [ expression { "," expression } ] ")"
```
Precedence (high→low): `( )`/call, `^` (right), unary (right), `* /` (left), `+ -` (left).
`call` is general on purpose so that wrong argument counts and unknown functions are *semantic* errors with clear messages.
Full discussion, left-recursion removal and grammar review: [docs/grammar.md](docs/grammar.md).

## Parsing technique
Recursive descent, one function per non-terminal, one-token lookahead, no backtracking. The `{ ... }` repetitions are `while`
loops building left-leaning nodes; right associativity of `^` comes from `power -> primary [ "^" unary ]`.

## AST
Nodes carry type, operator, value/name, children and the source position. Parentheses produce no nodes — precedence is
expressed by tree shape. Example for `2 + 3 * 4`:
```
Addition (+)
├── Number: 2
└── Multiplication (*)
    ├── Number: 3
    └── Number: 4
```

## Semantic rules
1. Only `sqrt` and `pow` may be called. 2. `sqrt` takes 1 argument, `pow` takes 2.
3. A divisor that constant-folds to 0 is an error (`3/(2-2)`). 4. `sqrt` of a constant < 0 is an error; `0^negative constant` is an error.
5. Variables are implicitly declared real numbers and go into the symbol table; they need no value to compile.
Syntax, semantic and runtime errors are kept distinct; runtime errors only occur in the optional evaluator.

## Project structure
```
math_formula_compiler/
├── src/            C compiler: compiler_types.h util.[ch] lexer.[ch] parser.[ch] ast.[ch] semantic.[ch] eval.[ch] main.c
├── ui/             app.py (Tkinter GUI)   compiler_bridge.py (subprocess interface)
├── tests/          valid_tests.txt  invalid_tests.txt  eval_tests.txt  test_runner.py
├── docs/           grammar.md  architecture.md  report.md  presentation_outline.md  test_results.md (generated)
├── screenshots/    (add your own real screenshots here)
├── build/          compiled executable (git-ignored)
├── Makefile  build.bat  README.md  requirements.txt  .gitignore  .vscode/tasks.json
```

## Dependencies
* GCC (MinGW-w64 on Windows) — no other C libraries besides the standard library and `libm`.
* Python 3.8+ with Tkinter (included in the python.org Windows installer). No `pip` packages.

## Windows + VS Code setup
1. Install **MSYS2/MinGW-w64** (or any GCC for Windows) and add its `bin` folder to `PATH`; check with `gcc --version`.
2. Install **Python 3** from python.org (keep "tcl/tk and IDLE" ticked); check with `python --version`.
3. Open the project folder in VS Code (`File → Open Folder`). Optional extensions: *C/C++* and *Python*.
4. Terminal → Run Build Task (`Ctrl+Shift+B`) builds the compiler using `build.bat`.

## Build and run
```
build.bat                       :: Windows (or: mingw32-make)
make                            # Linux/macOS

python ui\app.py                :: start the GUI (Ctrl+Enter = Analyze)
build\math_compiler.exe --formula "x^2 + 2*x + 1"            :: command-line text report
build\math_compiler.exe --json --formula "sqrt(x^2+y^2)"       :: JSON report used by the GUI
build\math_compiler.exe --eval --var x=3 --var y=4 --formula "sqrt(x^2+y^2)"   :: optional evaluation => 5
```
Tip: if the box-drawing characters of the AST look wrong in a console, run `chcp 65001` first. The GUI is unaffected.

## Run the tests
```
python tests/test_runner.py
```
Prints PASS/FAIL per case, writes `docs/test_results.md` (input, expected, actual, stage, status), and exits non-zero on any failure.
A test passes only if the outcome, the failing stage *and* the pipeline integrity (no stage after a failure reports "passed") match.

## Sample
Input `3 / (2 - 2)` → Lexical ✓, Syntax ✓, AST ✓, **Semantic ✗**:
`Semantic error at 1:3: Division by zero: the divisor is a constant expression equal to 0`.
Input `2 @ 3` → **Lexical ✗** at 1:3 (`Invalid character '@'`); syntax, AST and semantic stages are *skipped*.

## Future improvements
Syntax-error recovery (panic mode), more functions (`sin`, `log`), scientific notation, user-declared variables with types,
three-address-code generation, constant folding/optimisation pass, an LR(1) table-driven parser for comparison.

## Publishing to GitHub (nothing has been pushed yet)
```
git init
git add .
git commit -m "Mathematical Formula Compiler: lexer, parser, AST, semantic analysis, GUI, tests"
git branch -M main
git remote add origin <YOUR-REPOSITORY-URL>      # create an empty repo on github.com first
git push -u origin main
```

## Team contributions (fill in)
| Member | Roll no. | Contribution |
|---|---|---|
| | | Lexer / tokens |
| | | Parser / grammar |
| | | AST / semantic analysis |
| | | GUI / bridge |
| | | Tests / documentation / report |
"# math-formula-compiler" 
