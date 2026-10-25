#ifndef EVAL_H
#define EVAL_H
#include "compiler_types.h"
#include "ast.h"

typedef struct { char name[MAX_LEXEME]; double value; } VarBinding;

typedef struct {
    int ok;                    /* 1 = evaluated successfully */
    double value;
    char message[MAX_MSG];     /* runtime error text when !ok */
    int line, col;
} EvalResult;

/* Optional back-end: evaluates the AST (never the source string). */
void evaluate(const Node *root, const VarBinding *vars, int nvars, EvalResult *out);

#endif
