# Presentation Outline (7–10 minutes)

| # | Slide | Time | Talking points / what to show |
|---|---|---|---|
| 1 | Project overview | 0:45 | Goal: a compiler *front end* for formulas, not a calculator. Pipeline in one line: lex → parse/AST → semantic. C core + Tkinter GUI. |
| 2 | Language specification | 0:45 | Numbers, identifiers, `+ - * / ^`, unary, `sqrt`, `pow`; case-sensitive; limitations (no `1e5`, no `2x`). |
| 3 | Compiler architecture | 1:00 | Diagram from `docs/architecture.md`; each stage consumes the previous output; first failure stops the pipeline; GUI only displays C's JSON. |
| 4 | Lexer and token generation | 1:00 | Token table; line:col; malformed numbers (`3.`, `1.2.3`, `12abc`); `sqrt`/`pow` as FUNCTION tokens. Show `lexer.c`. |
| 5 | CFG and grammar transformations | 1:15 | EBNF grammar; left-recursion removal (`E → T E'`); why `2^3^2 = 2^(3^2)` and `-2^2 = -(2^2)`; precedence table. |
| 6 | Parsing and AST | 1:15 | Recursive descent (function per rule); AST node types; tree for `2 + 3 * 4`; parentheses leave no node. |
| 7 | Semantic analysis | 1:00 | Separate pass; symbol table; arg counts; constant folding catches `3/(2-2)`; limitation `x/(y-y)`; syntax vs semantic vs runtime. |
| 8 | Valid and invalid test cases | 0:45 | `python tests/test_runner.py`; number of cases; table in `docs/test_results.md`. |
| 9 | **Live demonstration** | 2:00 | Run `2 + 3 * 4` (tokens, AST) → `x^2 + 2*x + 1` (symbol table) → `2 @ 3` (lexical fail, later stages skipped) → `(2 + 3` (syntax) → `3 / (2 - 2)` (semantic) → optional evaluation `sqrt(x^2+y^2)` with `x=3, y=4`. |
| 10 | GitHub repository and code structure | 0:30 | Folder tour: `src/`, `ui/`, `tests/`, `docs/`; build with `build.bat`. *(Show the real repo URL once you have created it.)* |
| 11 | Individual contributions | 0:30 | One line per member (fill in the template in the README). |

## Likely viva questions (short answers)
* **Why recursive descent?** Grammar is LL(1) after left-recursion removal; one function per rule is easy to explain and to attach AST construction to.
* **How is right associativity of `^` obtained?** `power → primary [ "^" unary ]`: the right operand can contain another `^`.
* **Why is `pow(2)` not a syntax error?** Call syntax is valid; the *number of arguments* is a semantic rule, which gives a precise message.
* **Why is `x/(y-y)` accepted?** Variable values are unknown at compile time; constant folding only proves constant subtrees. A runtime check catches it in the evaluator.
* **What happens after a lexical error?** All lexical errors are collected; the parser is not run on an invalid token stream.
* **Difference between a parse tree and an AST?** The AST drops grammar-only nodes (parentheses, `additive`, `primary`…) and keeps operators and operands.
