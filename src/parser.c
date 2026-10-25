/* parser.c - Syntax analysis by recursive descent (LL(1) with one-token lookahead).
 *
 * One C function per grammar non-terminal (see docs/grammar.md):
 *   expression     -> additive
 *   additive       -> multiplicative { ("+"|"-") multiplicative }   left-assoc
 *   multiplicative -> unary { ("*"|"/") unary }                     left-assoc
 *   unary          -> ("+"|"-") unary | power
 *   power          -> primary [ "^" unary ]                         right-assoc
 *   primary        -> NUMBER | IDENTIFIER | call | "(" expression ")"
 *   call           -> (FUNCTION|IDENTIFIER) "(" [ expression { "," expression } ] ")"
 *
 * Because the exponent of "^" is a `unary`, which itself reaches `power`,
 * 2^3^2 parses as 2^(3^2) (right-assoc) and 2^-3 is legal. Because unary sits
 * ABOVE power, -2^2 parses as -(2^2). Argument-count and function-name checks
 * are deliberately left to semantic analysis. */
#include "parser.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

typedef struct { const TokenList *tl; int pos; DiagList *diags; int depth; } Parser;

static const Token *peek(Parser *p) { return &p->tl->tokens[p->pos]; }
static int check(Parser *p, TokenType t) { return peek(p)->type == t; }
static const Token *advance(Parser *p)
{
    const Token *t = &p->tl->tokens[p->pos];
    if (t->type != TOK_EOF) p->pos++;
    return t;
}

static void describe(const Token *t, char *buf, size_t sz)
{
    switch (t->type) {
    case TOK_EOF:    snprintf(buf, sz, "end of input"); break;
    case TOK_NUMBER: snprintf(buf, sz, "number '%s'", t->lexeme); break;
    case TOK_IDENT:  snprintf(buf, sz, "identifier '%s'", t->lexeme); break;
    case TOK_FUNC:   snprintf(buf, sz, "function name '%s'", t->lexeme); break;
    default:         snprintf(buf, sz, "'%s'", t->lexeme); break;
    }
}

static int enter(Parser *p, const Token *at)
{
    if (p->depth >= MAX_DEPTH) {
        diag_add(p->diags, DIAG_SYNTAX, at->line, at->col, "Expression is nested too deeply (limit %d levels)", MAX_DEPTH);
        return 0;
    }
    p->depth++;
    return 1;
}

static Node *parse_expression(Parser *p);
static Node *parse_unary(Parser *p);

static Node *parse_call(Parser *p, const Token *name)
{
    const Token *lp = advance(p);                  /* '(' */
    Node *call = ast_call(name->lexeme, name->line, name->col);

    if (check(p, TOK_RPAREN)) { advance(p); return call; }   /* zero arguments: semantic stage decides */
    for (;;) {
        Node *arg = parse_expression(p);
        if (!arg) { ast_free(call); return NULL; }
        ast_add_arg(call, arg);
        if (check(p, TOK_COMMA)) { advance(p); continue; }
        break;
    }
    if (check(p, TOK_RPAREN)) { advance(p); return call; }

    const Token *t = peek(p);
    char d[MAX_LEXEME + 32];
    describe(t, d, sizeof d);
    if (t->type == TOK_EOF)
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Unmatched '(' opened at %d:%d in call to '%s': missing closing ')'", lp->line, lp->col, name->lexeme);
    else
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Expected ',' or ')' in the argument list of '%s' but found %s", name->lexeme, d);
    ast_free(call);
    return NULL;
}

static Node *parse_primary(Parser *p)
{
    const Token *t = peek(p);
    char d[MAX_LEXEME + 32];

    switch (t->type) {
    case TOK_NUMBER:
        advance(p);
        return ast_number(t->lexeme, t->line, t->col);

    case TOK_IDENT:
        advance(p);
        if (check(p, TOK_LPAREN)) return parse_call(p, t);
        return ast_variable(t->lexeme, t->line, t->col);

    case TOK_FUNC:
        advance(p);
        if (!check(p, TOK_LPAREN)) {
            const Token *n = peek(p);
            describe(n, d, sizeof d);
            diag_add(p->diags, DIAG_SYNTAX, n->line, n->col, "Function '%s' must be followed by '(' but found %s", t->lexeme, d);
            return NULL;
        }
        return parse_call(p, t);

    case TOK_LPAREN: {
        advance(p);
        Node *inner = parse_expression(p);
        if (!inner) return NULL;
        if (check(p, TOK_RPAREN)) { advance(p); return inner; }   /* grouping leaves no node */
        const Token *n = peek(p);
        describe(n, d, sizeof d);
        if (n->type == TOK_EOF)
            diag_add(p->diags, DIAG_SYNTAX, n->line, n->col, "Unmatched '(' opened at %d:%d: missing closing ')'", t->line, t->col);
        else
            diag_add(p->diags, DIAG_SYNTAX, n->line, n->col, "Expected ')' to close '(' opened at %d:%d but found %s", t->line, t->col, d);
        ast_free(inner);
        return NULL; }

    case TOK_COMMA:
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Misplaced ',': commas may only separate function arguments");
        return NULL;
    case TOK_EOF:
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Missing operand: unexpected end of input (expected a number, variable, function call or '(')");
        return NULL;
    case TOK_RPAREN:
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Missing operand: unexpected ')' (expected a number, variable, function call or '(')");
        return NULL;
    default:
        describe(t, d, sizeof d);
        diag_add(p->diags, DIAG_SYNTAX, t->line, t->col, "Missing operand: unexpected operator %s (expected a number, variable, function call or '(')", d);
        return NULL;
    }
}

static Node *parse_power(Parser *p)
{
    Node *base = parse_primary(p);
    if (!base) return NULL;
    if (check(p, TOK_CARET)) {
        const Token *op = advance(p);
        Node *exp = parse_unary(p);                /* parse_unary -> power gives right associativity */
        if (!exp) { ast_free(base); return NULL; }
        return ast_binary(OP_POW, base, exp, op->line, op->col);
    }
    return base;
}

static Node *parse_unary(Parser *p)
{
    if (!enter(p, peek(p))) return NULL;
    Node *result;
    if (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        const Token *op = advance(p);
        Node *operand = parse_unary(p);
        result = operand ? ast_unary(op->type == TOK_MINUS ? OP_NEG : OP_POS, operand, op->line, op->col) : NULL;
    } else {
        result = parse_power(p);
    }
    p->depth--;
    return result;
}

static Node *parse_multiplicative(Parser *p)
{
    Node *left = parse_unary(p);
    if (!left) return NULL;
    while (check(p, TOK_STAR) || check(p, TOK_SLASH)) {
        const Token *op = advance(p);
        Node *right = parse_unary(p);
        if (!right) { ast_free(left); return NULL; }
        left = ast_binary(op->type == TOK_STAR ? OP_MUL : OP_DIV, left, right, op->line, op->col);
    }
    return left;
}

static Node *parse_additive(Parser *p)
{
    Node *left = parse_multiplicative(p);
    if (!left) return NULL;
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        const Token *op = advance(p);
        Node *right = parse_multiplicative(p);
        if (!right) { ast_free(left); return NULL; }
        left = ast_binary(op->type == TOK_PLUS ? OP_ADD : OP_SUB, left, right, op->line, op->col);
    }
    return left;
}

static Node *parse_expression(Parser *p)
{
    if (!enter(p, peek(p))) return NULL;
    Node *n = parse_additive(p);
    p->depth--;
    return n;
}

Node *parse(const TokenList *tl, DiagList *diags)
{
    Parser p = { tl, 0, diags, 0 };
    const Token *first = peek(&p);
    if (first->type == TOK_EOF) {
        diag_add(diags, DIAG_SYNTAX, first->line, first->col, "Empty formula: expected an expression");
        return NULL;
    }
    Node *root = parse_expression(&p);
    if (!root) return NULL;

    const Token *t = peek(&p);
    if (t->type != TOK_EOF) {                       /* the whole input must be one expression */
        char d[MAX_LEXEME + 32];
        describe(t, d, sizeof d);
        if (t->type == TOK_RPAREN)
            diag_add(diags, DIAG_SYNTAX, t->line, t->col, "Unmatched ')': there is no matching '(' before it");
        else
            diag_add(diags, DIAG_SYNTAX, t->line, t->col, "Unexpected %s after a complete expression (missing operator?)", d);
        ast_free(root);
        return NULL;
    }
    return root;
}
