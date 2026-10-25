# Grammar, Precedence and Associativity

## 1. Tokens (lexical grammar)

| Token | Pattern | Notes |
|---|---|---|
| NUMBER | `digit+ ( "." digit+ )?` | `3.`, `.5`, `1.2.3`, `12abc`, `1e5` are *malformed* (lexical error) |
| IDENTIFIER | `[A-Za-z_][A-Za-z0-9_]*` | case-sensitive |
| FUNCTION | `sqrt` or `pow` | reserved built-ins, case-sensitive (`Sqrt` is an IDENTIFIER) |
| PLUS MINUS MULTIPLY DIVIDE POWER | `+ - * / ^` | |
| LEFT_PAREN RIGHT_PAREN COMMA | `( ) ,` | |
| END_OF_INPUT | end of source | always the last token |
| INVALID_TOKEN | anything else | reported as a lexical error |

Whitespace (space, tab, CR, LF) separates tokens and is discarded; line/column are tracked.

## 2. Context-free grammar (EBNF)

```
expression     -> additive
additive       -> multiplicative { ("+" | "-") multiplicative }
multiplicative -> unary { ("*" | "/") unary }
unary          -> ("+" | "-") unary | power
power          -> primary [ "^" unary ]
primary        -> NUMBER
                | IDENTIFIER
                | call
                | "(" expression ")"
call           -> (FUNCTION | IDENTIFIER) "(" [ expression { "," expression } ] ")"
```

### Review of the suggested grammar
The grammar in the assignment was checked against `2^3^2` and `-2^2`:

* `power -> primary [ "^" unary ]` makes `^` **right-associative**: the right operand is a `unary`,
  which can reach `power` again, so `2^3^2` = `2^(3^2)` = 512.
* `unary -> ("+"|"-") unary | power` places unary operators **above** `power`, so `-2^2` = `-(2^2)` = -4
  (the usual mathematical convention). `2^-3` is also accepted because the exponent is a `unary`.
* **One deliberate change:** the assignment lists `"sqrt" "(" expression ")"` and
  `"pow" "(" expression "," expression ")"` directly in `primary`. We use one general `call` rule instead,
  so that `sqrt()`, `pow(2)`, `pow(2,3,4)` and `unknown(5)` are *syntactically* well formed and are rejected by
  the **semantic** analyzer with precise messages ("expects exactly 2 arguments", "unsupported function").
  This is exactly the separation the assignment asks for (argument counts and function names are semantic checks),
  and it gives better diagnostics than a generic "unexpected token". A bare `sqrt` with no `(` is still a *syntax* error.

## 3. Precedence and associativity (highest first)

| Level | Operators | Associativity | Rule |
|---|---|---|---|
| 1 | `( )`, function call | – | primary |
| 2 | `^` | right | power |
| 3 | unary `+` `-` | right (prefix) | unary |
| 4 | `*` `/` | left | multiplicative |
| 5 | `+` `-` | left | additive |

## 4. Grammar transformations

A textbook grammar for the same language is **left-recursive**:

```
E -> E + T | E - T | T          T -> T * F | T / F | F          F -> P ^ F | P      (and so on)
```

A top-down (recursive-descent) parser would loop forever on `E -> E + T`, so left recursion is removed:

```
E  -> T E'
E' -> + T E' | - T E' | ε
```

The EBNF repetition `{ ("+"|"-") multiplicative }` is the compact form of `E'`. In code it is a `while` loop that
folds each new operand into a **left-leaning** tree (`10 - 4 - 3` -> `(10 - 4) - 3`), keeping left associativity.
The grammar is **LL(1)**: one token of lookahead always decides the production (`IDENTIFIER` followed by `(` vs. not
is the only two-token look, handled by peeking after consuming the identifier). No backtracking is used.

## 5. Parse of `2 + 3 * 4`

```
expression -> additive
additive -> multiplicative(2) "+" multiplicative(3 * 4)
```
`3 * 4` is grouped inside the right `multiplicative`, so multiplication binds tighter:
`(+ 2 (* 3 4))`.
