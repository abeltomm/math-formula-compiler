/* lexer.c - Lexical analysis.
 *
 * The lexer is a hand-written deterministic scanner. Regular definitions:
 *     digit      -> [0-9]
 *     NUMBER     -> digit+ ( "." digit+ )?
 *     IDENTIFIER -> [A-Za-z_] [A-Za-z0-9_]*
 * Keywords "sqrt" and "pow" (case-sensitive) become FUNCTION tokens.
 * A number glued to a trailing "." / letters / another "." is one *malformed*
 * literal (e.g. 3.  1.2.3  12abc  .5), reported once instead of several times. */
#include "lexer.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

static int is_digit(int c) { return c >= '0' && c <= '9'; }
static int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

const char *token_type_name(TokenType t)
{
    static const char *names[] = {
        "NUMBER", "IDENTIFIER", "FUNCTION", "PLUS", "MINUS", "MULTIPLY", "DIVIDE",
        "POWER", "LEFT_PAREN", "RIGHT_PAREN", "COMMA", "END_OF_INPUT", "INVALID_TOKEN"
    };
    return names[t];
}

static void push(TokenList *tl, TokenType t, const char *s, size_t n, int line, int col)
{
    Token *tk = &tl->tokens[tl->count++];   /* capacity: >= 1 char per token + EOF */
    if (n >= MAX_LEXEME) n = MAX_LEXEME - 1;
    tk->type = t;
    memcpy(tk->lexeme, s, n);
    tk->lexeme[n] = '\0';
    tk->line = line; tk->col = col;
}

int lex(const char *src, TokenList *out, DiagList *diags)
{
    int i = 0, line = 1, col = 1, errors = 0;
    out->count = 0;

    while (src[i]) {
        unsigned char c = (unsigned char)src[i];

        if (c == '\n') { line++; col = 1; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { i++; col++; continue; }

        /* ---- numbers (and malformed numbers) ---- */
        if (is_digit(c) || (c == '.' && is_digit((unsigned char)src[i + 1]))) {
            int start = i, malformed = 0, why = 0;
            if (c == '.') { malformed = 1; why = 1; }                 /* ".5" */
            while (is_digit((unsigned char)src[i])) i++;
            if (src[i] == '.') {
                i++;
                if (is_digit((unsigned char)src[i])) { while (is_digit((unsigned char)src[i])) i++; }
                else if (!malformed) { malformed = 1; why = 2; }      /* "3." */
            }
            while (is_alpha((unsigned char)src[i]) || is_digit((unsigned char)src[i]) || src[i] == '.') {
                if (!malformed) { malformed = 1; why = (src[i] == '.') ? 3 : 4; }
                i++;
            }
            int len = i - start;
            if (len >= MAX_LEXEME) {
                errors++;
                diag_add(diags, DIAG_LEXICAL, line, col, "Numeric literal is too long (limit %d characters)", MAX_LEXEME - 1);
                push(out, TOK_INVALID, src + start, (size_t)len, line, col);
            } else if (malformed) {
                static const char *reasons[] = { "", "a digit is required before the decimal point",
                    "a digit is required after the decimal point", "more than one decimal point",
                    "unexpected letters or symbols attached to the number (scientific notation is not supported)" };
                errors++;
                diag_add(diags, DIAG_LEXICAL, line, col, "Malformed numeric literal '%.*s': %s", len, src + start, reasons[why]);
                push(out, TOK_INVALID, src + start, (size_t)len, line, col);
            } else {
                push(out, TOK_NUMBER, src + start, (size_t)len, line, col);
            }
            col += len;
            continue;
        }

        /* ---- identifiers and built-in function names ---- */
        if (is_alpha(c)) {
            int start = i;
            while (is_alpha((unsigned char)src[i]) || is_digit((unsigned char)src[i])) i++;
            int len = i - start;
            if (len >= MAX_LEXEME) {
                errors++;
                diag_add(diags, DIAG_LEXICAL, line, col, "Identifier is too long (limit %d characters)", MAX_LEXEME - 1);
                push(out, TOK_INVALID, src + start, (size_t)len, line, col);
            } else {
                int is_fn = (len == 4 && strncmp(src + start, "sqrt", 4) == 0) ||
                            (len == 3 && strncmp(src + start, "pow", 3) == 0);
                push(out, is_fn ? TOK_FUNC : TOK_IDENT, src + start, (size_t)len, line, col);
            }
            col += len;
            continue;
        }

        /* ---- single-character tokens ---- */
        TokenType t = TOK_INVALID;
        switch (c) {
        case '+': t = TOK_PLUS;   break;   case '-': t = TOK_MINUS;  break;
        case '*': t = TOK_STAR;   break;   case '/': t = TOK_SLASH;  break;
        case '^': t = TOK_CARET;  break;   case '(': t = TOK_LPAREN; break;
        case ')': t = TOK_RPAREN; break;   case ',': t = TOK_COMMA;  break;
        default: break;
        }
        if (t != TOK_INVALID) {
            push(out, t, src + i, 1, line, col);
        } else {
            char lex[8];
            errors++;
            if (c >= 32 && c < 127) {
                lex[0] = (char)c; lex[1] = '\0';
                diag_add(diags, DIAG_LEXICAL, line, col, "Invalid character '%c'", c);
            } else {
                snprintf(lex, sizeof lex, "\\x%02X", c);
                diag_add(diags, DIAG_LEXICAL, line, col, "Invalid character (byte 0x%02X); only ASCII is supported", c);
            }
            push(out, TOK_INVALID, lex, strlen(lex), line, col);
        }
        i++; col++;
    }
    push(out, TOK_EOF, "<EOF>", 5, line, col);
    return errors;
}
