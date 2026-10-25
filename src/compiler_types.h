/* compiler_types.h - shared types and limits for every compiler stage. */
#ifndef COMPILER_TYPES_H
#define COMPILER_TYPES_H

/* Make MinGW use a C99-conforming printf/snprintf family. */
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stddef.h>

#define MAX_SOURCE   2000          /* maximum formula length in bytes        */
#define MAX_LEXEME   64            /* maximum stored lexeme length           */
#define MAX_TOKENS   (MAX_SOURCE + 2)
#define MAX_DIAGS    128
#define MAX_MSG      256
#define MAX_DEPTH    400           /* parser recursion limit = about 200 nested parentheses */
#define MAX_SYMBOLS  256

/* ---- Tokens (output of the lexical analyzer) ---- */
typedef enum {
    TOK_NUMBER, TOK_IDENT, TOK_FUNC,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_CARET,
    TOK_LPAREN, TOK_RPAREN, TOK_COMMA,
    TOK_EOF, TOK_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_LEXEME];
    int line, col;                 /* 1-based source position */
} Token;

typedef struct {
    Token tokens[MAX_TOKENS];
    int count;
} TokenList;

/* ---- Diagnostics shared by all stages ---- */
typedef enum { DIAG_INPUT, DIAG_LEXICAL, DIAG_SYNTAX, DIAG_SEMANTIC } DiagCategory;

typedef struct {
    DiagCategory category;
    int line, col;                 /* 0 when no position applies */
    char message[MAX_MSG];
} Diagnostic;

typedef struct {
    Diagnostic items[MAX_DIAGS];
    int count;
} DiagList;

#endif
