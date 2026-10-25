#ifndef AST_H
#define AST_H
#include "compiler_types.h"
#include "util.h"

typedef enum { NODE_NUMBER, NODE_VARIABLE, NODE_UNARY, NODE_BINARY, NODE_CALL } NodeType;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_NEG, OP_POS } OpKind;

typedef struct Node {
    NodeType type;
    OpKind op;                 /* NODE_UNARY / NODE_BINARY */
    double value;              /* NODE_NUMBER */
    char name[MAX_LEXEME];     /* number lexeme, variable name or function name */
    struct Node *left, *right; /* unary uses left; binary uses both */
    struct Node **args;        /* NODE_CALL arguments */
    int argc;
    int line, col;             /* source position (operators: the operator token) */
} Node;

Node *ast_number(const char *lexeme, int line, int col);
Node *ast_variable(const char *name, int line, int col);
Node *ast_unary(OpKind op, Node *operand, int line, int col);
Node *ast_binary(OpKind op, Node *l, Node *r, int line, int col);
Node *ast_call(const char *name, int line, int col);
void  ast_add_arg(Node *call, Node *arg);
void  ast_free(Node *n);

void  ast_print_tree(const Node *root, StrBuf *out);   /* box-drawing tree */
void  ast_print_sexpr(const Node *root, StrBuf *out);  /* compact prefix form, e.g. (+ 2 (* 3 4)) */

#endif
