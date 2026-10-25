# Architecture

```
 Source formula (stdin)
        |
        v
 +--------------+  Token stream   +--------------+   AST    +------------------+
 |  lexer.c     | --------------> |  parser.c    | -------> |  semantic.c      |
 | (scanner)    |  type/lexeme/   | (recursive   |  ast.c   | (AST walk,       |
 +--------------+  line:col       |  descent)    |          |  symbol table,   |
        |                         +--------------+          |  constant checks)|
        | lexical errors                | syntax errors     +------------------+
        v                               v                          | semantic errors
 +-------------------------------------------------------------------------------+
 | main.c: runs stages in order, STOPS at the first failed stage, marks later     |
 | stages "skipped", and prints a text or JSON report                             |
 +-------------------------------------------------------------------------------+
        |  JSON (stdout)                           optional: eval.c (AST interpreter)
        v
 ui/compiler_bridge.py  (subprocess, no shell)  -->  ui/app.py (Tkinter display only)
```

## Stage contracts

| Stage | Input | Output | Fails when |
|---|---|---|---|
| Source Input | bytes from stdin | NUL-terminated string | longer than 2000 bytes |
| Lexical Analysis / Token Generation | string | `TokenList` ending in END_OF_INPUT | invalid character, malformed number, over-long lexeme. All lexical errors are collected. |
| Syntax Analysis / AST Generation | `TokenList` | `Node *` AST | first violation of the grammar (stops at first error; no error recovery) |
| Semantic Analysis | AST | symbol table, check counters, diagnostics | unsupported function, wrong argument count, constant division by zero, constant sqrt of negative, `0^negative` |
| Evaluation (optional) | AST + variable values | number or runtime error | undefined variable, run-time division by zero, domain error, overflow |

Because every stage consumes only the previous stage's output, the stages can be explained and tested independently.
If a stage fails, the following stages are reported as **skipped**, never as passed.

## Error categories

* **Lexical** – the text cannot be split into valid tokens.
* **Syntax** – the tokens do not match the grammar.
* **Semantic** – the program is grammatical but violates a meaning rule that is decidable without variable values.
* **Runtime** – only produced by the optional evaluator; the formula still counts as *compiled*.

## Semantic policy for variables
Variables are implicitly declared real numbers. A formula with variables compiles without values. Checks that depend
on values (division by `x`) are not guessed; `x/(y-y)` is **not** flagged because `y` is unknown to the analyzer
(documented limitation of constant folding). Only provably-constant divisors (`3/(2-2)`) are rejected.

## Safety
* Formula travels over stdin; variables are separate argv items – no shell, no `eval`.
* Fixed size limits (2000 bytes, nesting limit ~200 parentheses) prevent stack/buffer problems.
* The C code was exercised under AddressSanitizer/UBSan with edge cases and random fuzzing without findings.
