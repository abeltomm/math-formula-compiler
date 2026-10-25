/* ast.c - Abstract Syntax Tree nodes, construction, destruction and printing.
 * Parentheses never appear in the tree: grouping is expressed by tree shape. */
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Node *alloc_node(NodeType t, int line, int col)
{
    Node *n = calloc(1, sizeof *n);
    if (!n) { fprintf(stderr, "fatal: out of memory\n"); exit(2); }
    n->type = t; n->line = line; n->col = col;
    return n;
}

Node *ast_number(const char *lexeme, int line, int col)
{
    Node *n = alloc_node(NODE_NUMBER, line, col);
    snprintf(n->name, MAX_LEXEME, "%s", lexeme);
    n->value = strtod(lexeme, NULL);
    return n;
}
Node *ast_variable(const char *name, int line, int col)
{
    Node *n = alloc_node(NODE_VARIABLE, line, col);
    snprintf(n->name, MAX_LEXEME, "%s", name);
    return n;
}
Node *ast_unary(OpKind op, Node *operand, int line, int col)
{
    Node *n = alloc_node(NODE_UNARY, line, col);
    n->op = op; n->left = operand;
    return n;
}
Node *ast_binary(OpKind op, Node *l, Node *r, int line, int col)
{
    Node *n = alloc_node(NODE_BINARY, line, col);
    n->op = op; n->left = l; n->right = r;
    return n;
}
Node *ast_call(const char *name, int line, int col)
{
    Node *n = alloc_node(NODE_CALL, line, col);
    snprintf(n->name, MAX_LEXEME, "%s", name);
    return n;
}
void ast_add_arg(Node *call, Node *arg)
{
    Node **p = realloc(call->args, sizeof(Node *) * (size_t)(call->argc + 1));
    if (!p) { fprintf(stderr, "fatal: out of memory\n"); exit(2); }
    call->args = p;
    call->args[call->argc++] = arg;
}
void ast_free(Node *n)
{
    if (!n) return;
    ast_free(n->left);
    ast_free(n->right);
    for (int i = 0; i < n->argc; i++) ast_free(n->args[i]);
    free(n->args);
    free(n);
}

/* ---- printing ---- */
static void label(const Node *n, char *buf, size_t sz)
{
    switch (n->type) {
    case NODE_NUMBER:   snprintf(buf, sz, "Number: %s", n->name); break;
    case NODE_VARIABLE: snprintf(buf, sz, "Variable: %s", n->name); break;
    case NODE_CALL:     snprintf(buf, sz, "Function Call: %s (%d argument%s)", n->name, n->argc, n->argc == 1 ? "" : "s"); break;
    case NODE_UNARY:    snprintf(buf, sz, "%s", n->op == OP_NEG ? "Unary Minus (-)" : "Unary Plus (+)"); break;
    case NODE_BINARY: {
        static const char *names[] = { "Addition (+)", "Subtraction (-)", "Multiplication (*)", "Division (/)", "Exponentiation (^)" };
        snprintf(buf, sz, "%s", names[n->op]);
        break; }
    }
}

static int child_count(const Node *n)
{
    switch (n->type) {
    case NODE_UNARY:  return 1;
    case NODE_BINARY: return 2;
    case NODE_CALL:   return n->argc;
    default:          return 0;
    }
}
static const Node *child_at(const Node *n, int i)
{
    if (n->type == NODE_CALL) return n->args[i];
    return i == 0 ? n->left : n->right;
}

/* `prefix` is one shared buffer that is extended on the way down and truncated on the way up. */
static void walk(const Node *n, StrBuf *out, char *prefix, size_t plen, int is_root, int is_last)
{
    char lab[MAX_LEXEME + 64];
    label(n, lab, sizeof lab);
    if (is_root) sb_appendf(out, "%s\n", lab);
    else         sb_appendf(out, "%s%s%s\n", prefix, is_last ? "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ", lab);

    size_t np = plen;
    if (!is_root) {
        const char *ext = is_last ? "    " : "\xE2\x94\x82   ";
        size_t el = strlen(ext);
        memcpy(prefix + plen, ext, el + 1);
        np = plen + el;
    }
    int cc = child_count(n);
    for (int i = 0; i < cc; i++) walk(child_at(n, i), out, prefix, np, 0, i == cc - 1);
    prefix[plen] = '\0';
}

void ast_print_tree(const Node *root, StrBuf *out)
{
    static char prefix[MAX_SOURCE * 6 + 16];   /* deepest tree has < MAX_SOURCE levels */
    prefix[0] = '\0';
    walk(root, out, prefix, 0, 1, 1);
}

void ast_print_sexpr(const Node *n, StrBuf *out)
{
    static const char *ops[] = { "+", "-", "*", "/", "^", "neg", "pos" };
    switch (n->type) {
    case NODE_NUMBER:
    case NODE_VARIABLE: sb_append(out, n->name); break;
    case NODE_UNARY:
        sb_appendf(out, "(%s ", ops[n->op]); ast_print_sexpr(n->left, out); sb_append(out, ")"); break;
    case NODE_BINARY:
        sb_appendf(out, "(%s ", ops[n->op]);
        ast_print_sexpr(n->left, out); sb_append(out, " "); ast_print_sexpr(n->right, out);
        sb_append(out, ")"); break;
    case NODE_CALL:
        sb_appendf(out, "(call %s", n->name);
        for (int i = 0; i < n->argc; i++) { sb_append(out, " "); ast_print_sexpr(n->args[i], out); }
        sb_append(out, ")"); break;
    }
}
