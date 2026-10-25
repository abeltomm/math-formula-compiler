#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sb_reserve(StrBuf *sb, size_t extra)
{
    if (sb->len + extra + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap : 256;
        while (nc < sb->len + extra + 1) nc *= 2;
        char *p = realloc(sb->data, nc);
        if (!p) { fprintf(stderr, "fatal: out of memory\n"); exit(2); }
        sb->data = p; sb->cap = nc;
    }
}

void sb_init(StrBuf *sb) { sb->data = NULL; sb->len = sb->cap = 0; sb_reserve(sb, 0); sb->data[0] = '\0'; }
void sb_free(StrBuf *sb) { free(sb->data); sb->data = NULL; sb->len = sb->cap = 0; }

void sb_append(StrBuf *sb, const char *s)
{
    size_t n = strlen(s);
    sb_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

void sb_appendf(StrBuf *sb, const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n >= 0) {
        sb_reserve(sb, (size_t)n);
        vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap2);
        sb->len += (size_t)n;
    }
    va_end(ap2);
}

void sb_json_string(StrBuf *sb, const char *s)
{
    sb_append(sb, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  sb_append(sb, "\\\""); break;
        case '\\': sb_append(sb, "\\\\"); break;
        case '\n': sb_append(sb, "\\n");  break;
        case '\r': sb_append(sb, "\\r");  break;
        case '\t': sb_append(sb, "\\t");  break;
        default:
            if (*p < 0x20 || *p == 0x7f) sb_appendf(sb, "\\u%04x", *p);
            else { char c[2] = { (char)*p, 0 }; sb_append(sb, c); }  /* UTF-8 passes through */
        }
    }
    sb_append(sb, "\"");
}

void diag_add(DiagList *d, DiagCategory cat, int line, int col, const char *fmt, ...)
{
    if (d->count >= MAX_DIAGS) return;
    Diagnostic *x = &d->items[d->count++];
    x->category = cat; x->line = line; x->col = col;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(x->message, MAX_MSG, fmt, ap);
    va_end(ap);
}

const char *diag_category_name(DiagCategory c)
{
    switch (c) {
    case DIAG_INPUT:    return "Input";
    case DIAG_LEXICAL:  return "Lexical";
    case DIAG_SYNTAX:   return "Syntax";
    default:            return "Semantic";
    }
}

const char *diag_stage_name(DiagCategory c)
{
    switch (c) {
    case DIAG_INPUT:    return "Source Input";
    case DIAG_LEXICAL:  return "Lexical Analysis";
    case DIAG_SYNTAX:   return "Syntax Analysis";
    default:            return "Semantic Analysis";
    }
}
