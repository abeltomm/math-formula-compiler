#ifndef LEXER_H
#define LEXER_H
#include "compiler_types.h"

/* Scans `src` into `out`. Lexical errors are appended to `diags` and produce
 * TOK_INVALID tokens; scanning continues so every error is reported.
 * Returns the number of lexical errors. The list always ends with TOK_EOF. */
int lex(const char *src, TokenList *out, DiagList *diags);
const char *token_type_name(TokenType t);

#endif
