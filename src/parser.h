#ifndef PARSER_H
#define PARSER_H
#include "compiler_types.h"
#include "ast.h"

/* Recursive-descent parser. Returns the AST root, or NULL after appending a
 * syntax diagnostic to `diags` (the parser stops at the first syntax error). */
Node *parse(const TokenList *tokens, DiagList *diags);

#endif
