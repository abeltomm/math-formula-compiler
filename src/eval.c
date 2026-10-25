/* eval.c - Optional AST interpreter. Runtime errors (undefined variable,
 * division by zero, domain error, overflow) are reported separately from
 * compile-time diagnostics: a formula can compile and still fail to evaluate. */
#include "eval.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct { const VarBinding *vars; int nvars; EvalResult *res; } Ctx;

/* Records a runtime error at node `n`; always returns 0 so callers can `return fail(...)`. */
static int fail(Ctx *c, const Node *n, const char *fmt, ...)
{
    va_list ap;
    c->res->ok = 0; c->res->line = n->line; c->res->col = n->col;
    va_start(ap, fmt);
    vsnprintf(c->res->message, MAX_MSG, fmt, ap);
    va_end(ap);
    return 0;
}

static int check_finite(Ctx *c, const Node *n, double v)
{
    if (isnan(v)) return fail(c, n, "Domain error: the operation has no real-number result");
    if (isinf(v)) return fail(c, n, "Numerical overflow: the result is too large to represent");
    return 1;
}

static int ev(Ctx *c, const Node *n, double *out)
{
    double a, b, r;
    switch (n->type) {
    case NODE_NUMBER: *out = n->value; return 1;
    case NODE_VARIABLE:
        for (int i = 0; i < c->nvars; i++)
            if (strcmp(c->vars[i].name, n->name) == 0) { *out = c->vars[i].value; return 1; }
        return fail(c, n, "Undefined variable '%s': no value was supplied", n->name);
    case NODE_UNARY:
        if (!ev(c, n->left, &a)) return 0;
        *out = (n->op == OP_NEG) ? -a : a;
        return 1;
    case NODE_BINARY:
        if (!ev(c, n->left, &a) || !ev(c, n->right, &b)) return 0;
        switch (n->op) {
        case OP_ADD: r = a + b; break;
        case OP_SUB: r = a - b; break;
        case OP_MUL: r = a * b; break;
        case OP_DIV:
            if (b == 0.0) return fail(c, n, "Division by zero at run time");
            r = a / b; break;
        case OP_POW:
            if (a == 0.0 && b < 0.0) return fail(c, n, "Division by zero at run time: 0 raised to a negative power");
            r = pow(a, b); break;
        default: return 0;
        }
        if (!check_finite(c, n, r)) return 0;
        *out = r;
        return 1;
    case NODE_CALL:
        if (strcmp(n->name, "sqrt") == 0 && n->argc == 1) {
            if (!ev(c, n->args[0], &a)) return 0;
            if (a < 0.0) return fail(c, n, "Domain error: sqrt() of the negative value %g", a);
            *out = sqrt(a);
            return 1;
        }
        if (strcmp(n->name, "pow") == 0 && n->argc == 2) {
            if (!ev(c, n->args[0], &a) || !ev(c, n->args[1], &b)) return 0;
            if (a == 0.0 && b < 0.0) return fail(c, n, "Division by zero at run time: pow(0, negative)");
            r = pow(a, b);
            if (!check_finite(c, n, r)) return 0;
            *out = r;
            return 1;
        }
        return fail(c, n, "Cannot evaluate unsupported call '%s'", n->name);
    }
    return 0;
}

void evaluate(const Node *root, const VarBinding *vars, int nvars, EvalResult *out)
{
    memset(out, 0, sizeof *out);
    Ctx c = { vars, nvars, out };
    out->ok = ev(&c, root, &out->value);
}
