#ifndef UTIL_H
#define UTIL_H
#include "compiler_types.h"
#include <stddef.h>

/* Growable string buffer used to build the AST text and JSON output. */
typedef struct { char *data; size_t len, cap; } StrBuf;

void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_append(StrBuf *sb, const char *s);
void sb_appendf(StrBuf *sb, const char *fmt, ...);
void sb_json_string(StrBuf *sb, const char *s);   /* appends a quoted, escaped JSON string */

void diag_add(DiagList *d, DiagCategory cat, int line, int col, const char *fmt, ...);
const char *diag_category_name(DiagCategory c);
const char *diag_stage_name(DiagCategory c);

#endif
