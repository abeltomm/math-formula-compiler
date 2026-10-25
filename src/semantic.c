/* semantic.c - Semantic analysis (separate pass over the finished AST).
 *
 * Policy: variables are free and implicitly declared; they need no value to
 * pass semantic analysis (values are only needed by the optional evaluator).
 * Checks that depend on a value are done only when constant folding proves the
 * value statically - unknown variables are never guessed. */
#include "semantic.h"
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { C_FUNC_NAME, C_ARG_COUNT, C_DIV_ZERO, C_DOMAIN, C_SYMBOLS };

int const_eval(const Node *n, double *out)
{
    double a, b, r;
    switch (n->type) {
    case NODE_NUMBER:   *out = n->value; return 1;
    case NODE_VARIABLE: return 0;
    case NODE_UNARY:
        if (!const_eval(n->left, &a)) return 0;
        *out = (n->op == OP_NEG) ? -a : a;
        return 1;
    case NODE_BINARY:
        if (!const_eval(n->left, &a) || !const_eval(n->right, &b)) return 0;
        switch (n->op) {
        case OP_ADD: r = a + b; break;
        case OP_SUB: r = a - b; break;
        case OP_MUL: r = a * b; break;
        case OP_DIV: if (b == 0.0) return 0; r = a / b; break;
        case OP_POW: r = pow(a, b); break;
        default: return 0;
        }
        if (!isfinite(r)) return 0;
        *out = r;
        return 1;
    case NODE_CALL:
        if (strcmp(n->name, "sqrt") == 0 && n->argc == 1) {
            if (!const_eval(n->args[0], &a) || a < 0.0) return 0;
            *out = sqrt(a);
            return 1;
        }
        if (strcmp(n->name, "pow") == 0 && n->argc == 2) {
            if (!const_eval(n->args[0], &a) || !const_eval(n->args[1], &b)) return 0;
            r = pow(a, b);
            if (!isfinite(r)) return 0;
            *out = r;
            return 1;
        }
        return 0;
    }
    return 0;
}

static void note_symbol(SymbolTable *st, const Node *n)
{
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->items[i].name, n->name) == 0) { st->items[i].occurrences++; return; }
    if (st->count >= MAX_SYMBOLS) return;
    Symbol *s = &st->items[st->count++];
    snprintf(s->name, MAX_LEXEME, "%s", n->name);
    s->occurrences = 1; s->line = n->line; s->col = n->col;
}

static void walk(const Node *n, DiagList *d, SemanticResult *r, int *errors)
{
    double a, b;
    switch (n->type) {
    case NODE_NUMBER: return;
    case NODE_VARIABLE: note_symbol(&r->symbols, n); return;
    case NODE_UNARY: walk(n->left, d, r, errors); return;

    case NODE_BINARY:
        walk(n->left, d, r, errors);
        walk(n->right, d, r, errors);
        if (n->op == OP_DIV) {
            r->checks[C_DIV_ZERO].examined++;
            if (const_eval(n->right, &a) && a == 0.0) {
                r->checks[C_DIV_ZERO].violations++; (*errors)++;
                diag_add(d, DIAG_SEMANTIC, n->line, n->col, "Division by zero: the divisor is a constant expression equal to 0");
            }
        } else if (n->op == OP_POW) {
            r->checks[C_DOMAIN].examined++;
            if (const_eval(n->left, &a) && const_eval(n->right, &b) && a == 0.0 && b < 0.0) {
                r->checks[C_DOMAIN].violations++; (*errors)++;
                diag_add(d, DIAG_SEMANTIC, n->line, n->col, "Invalid power: 0 raised to the negative constant %g is a division by zero", b);
            }
        }
        return;

    case NODE_CALL: {
        for (int i = 0; i < n->argc; i++) walk(n->args[i], d, r, errors);
        int is_sqrt = strcmp(n->name, "sqrt") == 0, is_pow = strcmp(n->name, "pow") == 0;

        r->checks[C_FUNC_NAME].examined++;
        if (!is_sqrt && !is_pow) {
            r->checks[C_FUNC_NAME].violations++; (*errors)++;
            diag_add(d, DIAG_SEMANTIC, n->line, n->col, "Unsupported function '%s'; supported functions are sqrt and pow (names are case-sensitive)", n->name);
            return;
        }
        int want = is_sqrt ? 1 : 2;
        r->checks[C_ARG_COUNT].examined++;
        if (n->argc != want) {
            r->checks[C_ARG_COUNT].violations++; (*errors)++;
            diag_add(d, DIAG_SEMANTIC, n->line, n->col, "Function '%s' expects exactly %d argument%s but %d %s given",
                     n->name, want, want == 1 ? "" : "s", n->argc, n->argc == 1 ? "was" : "were");
            return;
        }
        if (is_sqrt) {
            r->checks[C_DOMAIN].examined++;
            if (const_eval(n->args[0], &a) && a < 0.0) {
                r->checks[C_DOMAIN].violations++; (*errors)++;
                diag_add(d, DIAG_SEMANTIC, n->line, n->col, "Invalid argument to sqrt(): the argument is the constant %g, but the square root of a negative number is undefined", a);
            }
        }
        return; }
    }
}

int semantic_analyze(const Node *root, DiagList *diags, SemanticResult *out)
{
    int errors = 0;
    memset(out, 0, sizeof *out);
    out->ran = 1;
    out->checks[C_FUNC_NAME] = (SemCheck){ "Supported functions", "Only sqrt and pow may be called", 0, 0 };
    out->checks[C_ARG_COUNT] = (SemCheck){ "Argument counts", "sqrt takes 1 argument, pow takes 2", 0, 0 };
    out->checks[C_DIV_ZERO]  = (SemCheck){ "Static division by zero", "Constant-folds each divisor; flags a divisor that is provably 0", 0, 0 };
    out->checks[C_DOMAIN]    = (SemCheck){ "Static domain checks", "Constant sqrt argument must be >= 0; 0 ^ negative constant is invalid", 0, 0 };
    out->checks[C_SYMBOLS]   = (SemCheck){ "Symbol table", "Collects every distinct variable (implicitly declared, numeric)", 0, 0 };

    walk(root, diags, out, &errors);
    out->checks[C_SYMBOLS].examined = out->symbols.count;
    return errors;
}
