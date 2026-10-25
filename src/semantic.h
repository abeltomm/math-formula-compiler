#ifndef SEMANTIC_H
#define SEMANTIC_H
#include "compiler_types.h"
#include "ast.h"

typedef struct { char name[MAX_LEXEME]; int occurrences; int line, col; } Symbol;
typedef struct { Symbol items[MAX_SYMBOLS]; int count; } SymbolTable;

#define SEM_NCHECKS 5
typedef struct { const char *name; const char *description; int examined; int violations; } SemCheck;

typedef struct {
    int ran;                          /* 0 when the stage was skipped */
    SymbolTable symbols;
    SemCheck checks[SEM_NCHECKS];
} SemanticResult;

/* Walks the AST, fills the symbol table and check counters, and appends
 * semantic diagnostics. Returns the number of semantic errors. */
int semantic_analyze(const Node *root, DiagList *diags, SemanticResult *out);

/* Safe constant folding: returns 1 and sets *out only if the whole subtree is
 * a compile-time constant that evaluates without error. */
int const_eval(const Node *n, double *out);

#endif
