# Mathematical Formula Compiler — Project Report

*Compiler Design Mini Project — Front-End Analysis*

## 1. Abstract
This project implements the front end of a compiler for mathematical formulas. A formula is processed by a hand-written
lexical analyzer, a recursive-descent parser that builds an Abstract Syntax Tree (AST), and a separate semantic analyzer
that builds a symbol table and performs safe constant checks. The core is written in C; a dark-themed Tkinter desktop
application displays the real output of each stage. An optional evaluator interprets the AST with user-supplied variable values.

## 2. Introduction
A compiler front end analyses source text and produces an intermediate representation plus diagnostics. Mathematical
formulas are an ideal small language: they have lexical structure (numbers, names, operators), a non-trivial grammar
(precedence, associativity, unary operators, function calls) and meaningful semantic rules (argument counts, division by zero).

## 3. Problem Statement
Build a compiler front end — not a calculator — that accepts formulas, rejects invalid ones with informative, positioned
diagnostics and exposes every intermediate stage (tokens, AST, symbol table) for demonstration and examination.

## 4. Objectives
1. Implement a real lexer, parser, AST and semantic analyzer in C. 2. Document the CFG, precedence and associativity.
3. Report lexical, syntax and semantic errors separately, with line and column. 4. Provide a GUI wired to the real compiler.
5. Provide at least 10 valid and 10 invalid automated tests. 6. (Optional) evaluate the AST safely.

## 5. Language Specification
Integers and decimals (`10`, `3.14`), identifiers (`radius`), operators `+ - * / ^`, unary `+ -`, parentheses, comma, and
the functions `sqrt(e)` and `pow(e, e)`. Identifiers and function names are case-sensitive. Not supported: scientific notation,
implicit multiplication, assignments, other functions, non-ASCII text. Limits: 2000 characters, about 200 nested parentheses.

## 6. System Architecture
`Source → Lexer → Token stream → Parser → AST → Semantic analyzer → Result`, driven by `main.c`, which stops at the first
failed stage and marks later stages as skipped. The compiler prints a JSON report; `ui/compiler_bridge.py` runs the executable
through `subprocess` (formula on stdin, no shell) and `ui/app.py` only displays that report. See `docs/architecture.md`.

## 7. CFG and Grammar Transformations
```
expression     -> additive
additive       -> multiplicative { ("+" | "-") multiplicative }
multiplicative -> unary { ("*" | "/") unary }
unary          -> ("+" | "-") unary | power
power          -> primary [ "^" unary ]
primary        -> NUMBER | IDENTIFIER | call | "(" expression ")"
call           -> (FUNCTION | IDENTIFIER) "(" [ expression { "," expression } ] ")"
```
The textbook left-recursive rule `E -> E + T | T` is transformed to `E -> T E'`, `E' -> + T E' | ε` (written as a repetition).
Right associativity of `^` comes from the right operand being a `unary` that can reach `power` again; placing `unary` above
`power` makes `-2^2 = -(2^2)`. The assignment's grammar was reviewed and kept, with one deliberate change: function calls use a
general `call` rule so argument-count and unknown-function errors are reported by the semantic stage with precise messages.
Details: `docs/grammar.md`.

## 8. Lexical Analysis
A deterministic scanner (`lexer.c`) recognises NUMBER, IDENTIFIER, FUNCTION (`sqrt`, `pow`), PLUS, MINUS, MULTIPLY, DIVIDE,
POWER, LEFT_PAREN, RIGHT_PAREN, COMMA and END_OF_INPUT, recording type, lexeme and line:col. Invalid characters and malformed
numbers (`3.`, `.5`, `1.2.3`, `12abc`) become INVALID_TOKEN and a lexical diagnostic; scanning continues so all lexical errors are listed.
The parser never receives a stream containing invalid tokens.

## 9. Parsing Technique
Recursive descent with one-token lookahead and no backtracking (LL(1)). One C function per grammar rule; loops implement the
repetition and create left-leaning nodes. A recursion limit prevents stack overflow on pathological input. The parser stops at the first
syntax error (no error recovery), reporting: missing operands, unexpected operators, unmatched `(` / `)`, misplaced commas,
missing argument lists, and trailing tokens.

## 10. AST Construction
`Node` structures (Number, Variable, Unary, Binary, Call) are created while parsing. Operator nodes remember the operator's
position. Parentheses create no node; tree shape alone encodes precedence, e.g. `2 + 3 * 4` gives `(+ 2 (* 3 4))`, `2^3^2` gives
`(^ 2 (^ 3 2))`, `-2^2` gives `(neg (^ 2 2))`. The tree is printed in a readable box-drawing format and in prefix form.

## 11. Semantic Analysis
A separate pass over the finished AST: (1) only `sqrt` and `pow` are callable, (2) argument counts (1 and 2), (3) static division
by zero via constant folding (`3/(2-2)`), (4) `sqrt` of a constant negative value and `0^negative`, (5) symbol table of variables
(implicitly declared real numbers, occurrence count, first position). Unknown variables are never guessed, so `x/(y-y)` is accepted.
Semantic errors are distinct from syntax errors and from runtime errors of the optional evaluator.

## 12. Error Detection and Recovery
Every diagnostic has a category (Lexical, Syntax, Semantic, Runtime), a message and, where applicable, line:col; the GUI also
highlights the offending character in the editor. Recovery policy: lexical errors are all collected; the parser stops at the first
syntax error; the semantic analyzer reports every violation it finds. Invalid input never crashes the program: the C code
was run under AddressSanitizer/UBSan on edge cases (empty input, 2000+ characters, deep nesting, binary bytes) and 4,500 random inputs without findings.

## 13. Test Cases and Results
The tables below are produced by `python tests/test_runner.py` (copied from `docs/test_results.md`; re-run the tests to refresh them).



Generated by `tests/test_runner.py` from the real compiler executable.

#### Valid formulas

| # | Input formula | Expected result | Actual result | Stage involved | Status |
|---|---|---|---|---|---|
| 1 | `2 + 3` | Compiles; AST (+ 2 3) | (+ 2 3) | All stages passed | PASS |
| 2 | `2 + 3 * 4` | Compiles; AST (+ 2 (* 3 4)) | (+ 2 (* 3 4)) | All stages passed | PASS |
| 3 | `(a + b) * c` | Compiles; AST (* (+ a b) c) | (* (+ a b) c) | All stages passed | PASS |
| 4 | `x^2 + 2*x + 1` | Compiles; AST (+ (+ (^ x 2) (* 2 x)) 1) | (+ (+ (^ x 2) (* 2 x)) 1) | All stages passed | PASS |
| 5 | `sqrt(25)` | Compiles; AST (call sqrt 25) | (call sqrt 25) | All stages passed | PASS |
| 6 | `sqrt(x^2 + y^2)` | Compiles; AST (call sqrt (+ (^ x 2) (^ y 2))) | (call sqrt (+ (^ x 2) (^ y 2))) | All stages passed | PASS |
| 7 | `pow(2, 3)` | Compiles; AST (call pow 2 3) | (call pow 2 3) | All stages passed | PASS |
| 8 | `pow(a + b, 2)` | Compiles; AST (call pow (+ a b) 2) | (call pow (+ a b) 2) | All stages passed | PASS |
| 9 | `-x + 4.5` | Compiles; AST (+ (neg x) 4.5) | (+ (neg x) 4.5) | All stages passed | PASS |
| 10 | `2^3^2` | Compiles; AST (^ 2 (^ 3 2)) | (^ 2 (^ 3 2)) | All stages passed | PASS |
| 11 | `-2^2` | Compiles; AST (neg (^ 2 2)) | (neg (^ 2 2)) | All stages passed | PASS |
| 12 | `2^-3` | Compiles; AST (^ 2 (neg 3)) | (^ 2 (neg 3)) | All stages passed | PASS |
| 13 | `10 - 4 - 3` | Compiles; AST (- (- 10 4) 3) | (- (- 10 4) 3) | All stages passed | PASS |
| 14 | `8 / 4 / 2` | Compiles; AST (/ (/ 8 4) 2) | (/ (/ 8 4) 2) | All stages passed | PASS |
| 15 | `pow(2, 3) + 5` | Compiles; AST (+ (call pow 2 3) 5) | (+ (call pow 2 3) 5) | All stages passed | PASS |
| 16 | `+x` | Compiles; AST (pos x) | (pos x) | All stages passed | PASS |
| 17 | `((1))` | Compiles; AST 1 | 1 | All stages passed | PASS |
| 18 | `2 * (3 + 4)^2` | Compiles; AST (* 2 (^ (+ 3 4) 2)) | (* 2 (^ (+ 3 4) 2)) | All stages passed | PASS |
| 19 | `sqrt(pow(3,2) + pow(4,2))` | Compiles; AST (call sqrt (+ (call pow 3 2) (call pow 4 2))) | (call sqrt (+ (call pow 3 2) (call pow 4 2))) | All stages passed | PASS |
| 20 | `2 +\n3 * radius` | Compiles; AST (+ 2 (* 3 radius)) | (+ 2 (* 3 radius)) | All stages passed | PASS |

#### Invalid formulas

| # | Input formula | Expected result | Actual result | Stage involved | Status |
|---|---|---|---|---|---|
| 1 | `2 + * 3` | Syntax error containing "Missing operand: unexpected operator '*'" | Syntax error 1:5: Missing operand: unexpected operator '*' (expected a number, variable, function call or '(') | Syntax Analysis | PASS |
| 2 | `(2 + 3` | Syntax error containing "Unmatched '('" | Syntax error 1:7: Unmatched '(' opened at 1:1: missing closing ')' | Syntax Analysis | PASS |
| 3 | `2 + 3)` | Syntax error containing "Unmatched ')'" | Syntax error 1:6: Unmatched ')': there is no matching '(' before it | Syntax Analysis | PASS |
| 4 | `sqrt()` | Semantic error containing "expects exactly 1 argument" | Semantic error 1:1: Function 'sqrt' expects exactly 1 argument but 0 were given | Semantic Analysis | PASS |
| 5 | `pow(2)` | Semantic error containing "expects exactly 2 arguments" | Semantic error 1:1: Function 'pow' expects exactly 2 arguments but 1 was given | Semantic Analysis | PASS |
| 6 | `pow(2, 3, 4)` | Semantic error containing "expects exactly 2 arguments" | Semantic error 1:1: Function 'pow' expects exactly 2 arguments but 3 were given | Semantic Analysis | PASS |
| 7 | `unknown(5)` | Semantic error containing "Unsupported function 'unknown'" | Semantic error 1:1: Unsupported function 'unknown'; supported functions are sqrt and pow (names are case-sensitive) | Semantic Analysis | PASS |
| 8 | `2 @ 3` | Lexical error containing "Invalid character '@'" | Lexical error 1:3: Invalid character '@' | Lexical Analysis | PASS |
| 9 | `5 +` | Syntax error containing "Missing operand: unexpected end of input" | Syntax error 1:4: Missing operand: unexpected end of input (expected a number, variable, function call or '(') | Syntax Analysis | PASS |
| 10 | `3 / (2 - 2)` | Semantic error containing "Division by zero" | Semantic error 1:3: Division by zero: the divisor is a constant expression equal to 0 | Semantic Analysis | PASS |
| 11 | `3. + 1` | Lexical error containing "Malformed numeric literal '3.'" | Lexical error 1:1: Malformed numeric literal '3.': a digit is required after the decimal point | Lexical Analysis | PASS |
| 12 | `1.2.3` | Lexical error containing "more than one decimal point" | Lexical error 1:1: Malformed numeric literal '1.2.3': more than one decimal point | Lexical Analysis | PASS |
| 13 | `12abc` | Lexical error containing "Malformed numeric literal '12abc'" | Lexical error 1:1: Malformed numeric literal '12abc': unexpected letters or symbols attached to the number (scientific notation is not supported) | Lexical Analysis | PASS |
| 14 | `.5 + 1` | Lexical error containing "Malformed numeric literal '.5'" | Lexical error 1:1: Malformed numeric literal '.5': a digit is required before the decimal point | Lexical Analysis | PASS |
| 15 | `2 3` | Syntax error containing "Unexpected number '3'" | Syntax error 1:3: Unexpected number '3' after a complete expression (missing operator?) | Syntax Analysis | PASS |
| 16 | `sqrt 4` | Syntax error containing "must be followed by '('" | Syntax error 1:6: Function 'sqrt' must be followed by '(' but found number '4' | Syntax Analysis | PASS |
| 17 | `pow(2,)` | Syntax error containing "Missing operand: unexpected ')'" | Syntax error 1:7: Missing operand: unexpected ')' (expected a number, variable, function call or '(') | Syntax Analysis | PASS |
| 18 | `(,)` | Syntax error containing "Misplaced ','" | Syntax error 1:2: Misplaced ',': commas may only separate function arguments | Syntax Analysis | PASS |
| 19 | `()` | Syntax error containing "Missing operand: unexpected ')'" | Syntax error 1:2: Missing operand: unexpected ')' (expected a number, variable, function call or '(') | Syntax Analysis | PASS |
| 20 | `pow(2 3)` | Syntax error containing "Expected ',' or ')'" | Syntax error 1:7: Expected ',' or ')' in the argument list of 'pow' but found number '3' | Syntax Analysis | PASS |
| 21 | `sqrt(4` | Syntax error containing "Unmatched '('" | Syntax error 1:7: Unmatched '(' opened at 1:5 in call to 'sqrt': missing closing ')' | Syntax Analysis | PASS |
| 22 | `* 2` | Syntax error containing "Missing operand: unexpected operator '*'" | Syntax error 1:1: Missing operand: unexpected operator '*' (expected a number, variable, function call or '(') | Syntax Analysis | PASS |
| 23 | `sqrt(-4)` | Semantic error containing "Invalid argument to sqrt()" | Semantic error 1:1: Invalid argument to sqrt(): the argument is the constant -4, but the square root of a negative number is undefined | Semantic Analysis | PASS |
| 24 | `Sqrt(4)` | Semantic error containing "Unsupported function 'Sqrt'" | Semantic error 1:1: Unsupported function 'Sqrt'; supported functions are sqrt and pow (names are case-sensitive) | Semantic Analysis | PASS |
| 25 | `1 / 0` | Semantic error containing "Division by zero" | Semantic error 1:3: Division by zero: the divisor is a constant expression equal to 0 | Semantic Analysis | PASS |
| 26 | `0 ^ -1` | Semantic error containing "0 raised to the negative constant" | Semantic error 1:3: Invalid power: 0 raised to the negative constant -1 is a division by zero | Semantic Analysis | PASS |
| 27 | `x # y` | Lexical error containing "Invalid character '#'" | Lexical error 1:3: Invalid character '#' | Lexical Analysis | PASS |

#### Optional evaluation

| # | Input formula | Expected result | Actual result | Stage involved | Status |
|---|---|---|---|---|---|
| 1 | `x^2 + 2*x + 1   [x=3]` | 16 | 16 | Evaluation (runtime) | PASS |
| 2 | `sqrt(x^2 + y^2)   [x=3, y=4]` | 5 | 5 | Evaluation (runtime) | PASS |
| 3 | `2^3^2` | 512 | 512 | Evaluation (runtime) | PASS |
| 4 | `-2^2` | -4 | -4 | Evaluation (runtime) | PASS |
| 5 | `pow(2, 3) + 5` | 13 | 13 | Evaluation (runtime) | PASS |
| 6 | `10 / y   [y=0]` | error: Division by zero | error: Division by zero at run time | Evaluation (runtime) | PASS |
| 7 | `sqrt(x)   [x=-4]` | error: sqrt() | error: Domain error: sqrt() of the negative value -4 | Evaluation (runtime) | PASS |
| 8 | `a + 1` | error: Undefined variable 'a' | error: Undefined variable 'a': no value was supplied | Evaluation (runtime) | PASS |
| 9 | `pow(10, 400)` | error: overflow | error: Numerical overflow: the result is too large to represent | Evaluation (runtime) | PASS |
| 10 | `pow(-8, 0.5)` | error: Domain error | error: Domain error: the operation has no real-number result | Evaluation (runtime) | PASS |

**57 tests, 57 passed, 0 failed**

## 14. Screenshots
*To be added by the team.* Run `python ui\app.py`, analyse the formulas below and save real screenshots in `screenshots/`:
`2 + 3 * 4` (tokens + AST), `x^2 + 2*x + 1` (symbol table), `2 @ 3` (lexical failure), `(2 + 3` (syntax failure),
`3 / (2 - 2)` (semantic failure), and a successful evaluation. No screenshots are included in this repository until you capture them.

## 15. Limitations
No error recovery after the first syntax error; only two built-in functions; no scientific notation or implicit multiplication; constant folding
works only on literals; ASCII only; the source is limited to 2000 characters; there is no code-generation phase.

## 16. Conclusion
The project demonstrates the real stages of a compiler front end on a small but non-trivial language, with explainable code, a documented grammar,
informative diagnostics, automated tests and a GUI that shows genuine compiler output.

## 17. References
1. Aho, Lam, Sethi, Ullman — *Compilers: Principles, Techniques, and Tools*, 2nd ed., Pearson, 2006.
2. Nystrom, R. — *Crafting Interpreters*, 2021 (recursive descent and AST design).
3. Cooper, K., Torczon, L. — *Engineering a Compiler*, 2nd ed., Morgan Kaufmann, 2011.
4. GCC / MinGW-w64 and Python `tkinter` documentation.

## 18. Individual Contributions
| Member | Roll no. | Contribution |
|---|---|---|
| | | |
