/* main.c - Compiler driver: runs Lexer -> Parser -> Semantic analysis (-> optional
 * evaluation), then prints a report as human-readable text or JSON.
 * The GUI never re-implements any compiler logic; it only displays this JSON.
 *
 * Usage: math_compiler [--json] [--formula "text"] [--eval] [--var name=value]...
 * With no --formula the formula is read from standard input.
 * Exit codes: 0 compiled, 1 compile error, 2 usage/internal error, 3 compiled but evaluation failed. */
#include "compiler_types.h"
#include "ast.h"
#include "eval.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define MAX_VARS 64

enum { ST_SOURCE, ST_LEX, ST_TOKENS, ST_SYNTAX, ST_AST, ST_SEM, ST_RESULT, ST_COUNT };
enum { S_PASSED, S_FAILED, S_SKIPPED };
static const char *STAGE_NAMES[ST_COUNT] = {
    "Source Input", "Lexical Analysis", "Token Generation", "Syntax Analysis",
    "AST Generation", "Semantic Analysis", "Final Result"
};
static const char *STATUS_NAMES[] = { "passed", "failed", "skipped" };

typedef struct {
    char source[MAX_SOURCE + 2];
    int too_long;
    TokenList tokens;
    DiagList diags;
    Node *ast;
    SemanticResult sem;
    int status[ST_COUNT];
    int eval_requested, eval_performed;
    EvalResult ev;
    VarBinding vars[MAX_VARS];
    int nvars;
} Report;

static Report R;   /* static: large arrays are zero-initialised and kept off the stack */

static void run_pipeline(Report *r)
{
    for (int i = 0; i < ST_COUNT; i++) r->status[i] = S_SKIPPED;

    if (r->too_long) {
        diag_add(&r->diags, DIAG_INPUT, 0, 0, "Formula is too long (limit %d characters)", MAX_SOURCE);
        r->status[ST_SOURCE] = r->status[ST_RESULT] = S_FAILED;
        return;
    }
    r->status[ST_SOURCE] = S_PASSED;

    /* Stage 1-2: lexical analysis produces the token stream. */
    int lex_errors = lex(r->source, &r->tokens, &r->diags);
    r->status[ST_LEX] = r->status[ST_TOKENS] = lex_errors ? S_FAILED : S_PASSED;
    if (lex_errors) { r->status[ST_RESULT] = S_FAILED; return; }   /* the parser never sees invalid tokens */

    /* Stage 3-4: syntax analysis builds the AST. */
    r->ast = parse(&r->tokens, &r->diags);
    r->status[ST_SYNTAX] = r->ast ? S_PASSED : S_FAILED;
    if (!r->ast) { r->status[ST_RESULT] = S_FAILED; return; }
    r->status[ST_AST] = S_PASSED;

    /* Stage 5: semantic analysis walks the finished AST. */
    int sem_errors = semantic_analyze(r->ast, &r->diags, &r->sem);
    r->status[ST_SEM] = r->status[ST_RESULT] = sem_errors ? S_FAILED : S_PASSED;
}

static int first_failed_stage(const Report *r)
{
    for (int i = 0; i < ST_RESULT; i++) if (r->status[i] == S_FAILED) return i;
    return -1;
}

static const char *diag_stage(const Diagnostic *d) { return diag_stage_name(d->category); }

/* ------------------------------------------------------------------ JSON */
static void print_json(const Report *r)
{
    StrBuf sb; sb_init(&sb);
    int ff = first_failed_stage(r);

    sb_append(&sb, "{\n  \"source\": "); sb_json_string(&sb, r->source);
    sb_appendf(&sb, ",\n  \"status\": \"%s\",\n  \"failed_stage\": ", r->status[ST_RESULT] == S_PASSED ? "success" : "failed");
    if (ff >= 0) sb_json_string(&sb, STAGE_NAMES[ff]); else sb_append(&sb, "null");

    sb_append(&sb, ",\n  \"stages\": [");
    for (int i = 0; i < ST_COUNT; i++) {
        sb_appendf(&sb, "%s{\"name\": \"%s\", \"status\": \"%s\"}", i ? ", " : "", STAGE_NAMES[i], STATUS_NAMES[r->status[i]]);
    }
    sb_append(&sb, "],\n  \"tokens\": [");
    for (int i = 0; i < r->tokens.count; i++) {
        const Token *t = &r->tokens.tokens[i];
        sb_appendf(&sb, "%s\n    {\"type\": \"%s\", \"lexeme\": ", i ? "," : "", token_type_name(t->type));
        sb_json_string(&sb, t->lexeme);
        sb_appendf(&sb, ", \"line\": %d, \"col\": %d}", t->line, t->col);
    }
    sb_append(&sb, "\n  ],\n  \"ast_tree\": ");
    if (r->ast) { StrBuf t; sb_init(&t); ast_print_tree(r->ast, &t); sb_json_string(&sb, t.data); sb_free(&t); }
    else sb_append(&sb, "null");
    sb_append(&sb, ",\n  \"ast_sexpr\": ");
    if (r->ast) { StrBuf t; sb_init(&t); ast_print_sexpr(r->ast, &t); sb_json_string(&sb, t.data); sb_free(&t); }
    else sb_append(&sb, "null");

    sb_append(&sb, ",\n  \"symbols\": [");
    for (int i = 0; i < r->sem.symbols.count; i++) {
        const Symbol *s = &r->sem.symbols.items[i];
        sb_appendf(&sb, "%s\n    {\"name\": \"%s\", \"kind\": \"variable\", \"type\": \"numeric (real)\", \"occurrences\": %d, \"line\": %d, \"col\": %d}",
                   i ? "," : "", s->name, s->occurrences, s->line, s->col);
    }
    sb_append(&sb, "\n  ],\n  \"semantic_checks\": [");
    for (int i = 0; i < SEM_NCHECKS && r->sem.ran; i++) {
        const SemCheck *c = &r->sem.checks[i];
        sb_appendf(&sb, "%s\n    {\"name\": \"%s\", \"description\": ", i ? "," : "", c->name);
        sb_json_string(&sb, c->description);
        sb_appendf(&sb, ", \"examined\": %d, \"violations\": %d, \"status\": \"%s\"}", c->examined, c->violations, c->violations ? "failed" : "passed");
    }
    sb_append(&sb, "\n  ],\n  \"errors\": [");
    for (int i = 0; i < r->diags.count; i++) {
        const Diagnostic *d = &r->diags.items[i];
        sb_appendf(&sb, "%s\n    {\"category\": \"%s\", \"stage\": \"%s\", \"line\": %d, \"col\": %d, \"message\": ",
                   i ? "," : "", diag_category_name(d->category), diag_stage(d), d->line, d->col);
        sb_json_string(&sb, d->message);
        sb_append(&sb, "}");
    }
    sb_appendf(&sb, "\n  ],\n  \"evaluation\": {\"requested\": %s, \"performed\": %s, \"ok\": %s",
               r->eval_requested ? "true" : "false", r->eval_performed ? "true" : "false",
               (r->eval_performed && r->ev.ok) ? "true" : "false");
    if (r->eval_performed && r->ev.ok) sb_appendf(&sb, ", \"value\": %.15g", r->ev.value);
    else sb_append(&sb, ", \"value\": null");
    sb_append(&sb, ", \"message\": ");
    if (r->eval_performed && !r->ev.ok) sb_json_string(&sb, r->ev.message);
    else if (r->eval_requested && !r->eval_performed) sb_json_string(&sb, "Evaluation skipped: the formula did not compile");
    else sb_json_string(&sb, "");
    sb_appendf(&sb, ", \"line\": %d, \"col\": %d}\n}\n", r->eval_performed ? r->ev.line : 0, r->eval_performed ? r->ev.col : 0);

    fputs(sb.data, stdout);
    sb_free(&sb);
}

/* ------------------------------------------------------------------ text */
static void print_text(const Report *r)
{
    int ff = first_failed_stage(r);
    printf("Source: %s\n\n-- Compiler pipeline --\n", r->source);
    for (int i = 0; i < ST_COUNT; i++) printf("  [%-7s] %s\n", STATUS_NAMES[r->status[i]], STAGE_NAMES[i]);

    printf("\n-- Tokens (type, lexeme, line:col) --\n");
    for (int i = 0; i < r->tokens.count; i++) {
        const Token *t = &r->tokens.tokens[i];
        printf("  %-14s %-10s %d:%d\n", token_type_name(t->type), t->lexeme, t->line, t->col);
    }
    if (r->ast) {
        StrBuf t; sb_init(&t); ast_print_tree(r->ast, &t);
        printf("\n-- AST --\n%s", t.data);
        t.len = 0; t.data[0] = '\0'; ast_print_sexpr(r->ast, &t);
        printf("S-expression: %s\n", t.data);
        sb_free(&t);
    }
    if (r->sem.ran) {
        printf("\n-- Symbol table --\n");
        if (!r->sem.symbols.count) printf("  (no variables)\n");
        for (int i = 0; i < r->sem.symbols.count; i++) {
            const Symbol *s = &r->sem.symbols.items[i];
            printf("  %-12s variable, numeric  occurrences=%d  first at %d:%d\n", s->name, s->occurrences, s->line, s->col);
        }
        printf("\n-- Semantic checks --\n");
        for (int i = 0; i < SEM_NCHECKS; i++) {
            const SemCheck *c = &r->sem.checks[i];
            printf("  [%s] %-24s examined=%d violations=%d\n", c->violations ? "FAIL" : "PASS", c->name, c->examined, c->violations);
        }
    }
    printf("\n-- Diagnostics --\n");
    if (!r->diags.count) printf("  none\n");
    for (int i = 0; i < r->diags.count; i++) {
        const Diagnostic *d = &r->diags.items[i];
        if (d->line) printf("  %s error at %d:%d: %s\n", diag_category_name(d->category), d->line, d->col, d->message);
        else         printf("  %s error: %s\n", diag_category_name(d->category), d->message);
    }
    printf("\n-- Result --\n");
    if (ff < 0) printf("  Compilation Successful -- No Lexical, Syntax, or Semantic Errors.\n");
    else        printf("  Compilation FAILED at stage: %s\n", STAGE_NAMES[ff]);
    if (r->eval_requested) {
        if (!r->eval_performed) printf("  Evaluation skipped: the formula did not compile.\n");
        else if (r->ev.ok)      printf("  Evaluation result: %.15g\n", r->ev.value);
        else                    printf("  Runtime error at %d:%d: %s\n", r->ev.line, r->ev.col, r->ev.message);
    }
}

static int add_var(Report *r, const char *spec)
{
    const char *eq = strchr(spec, '=');
    char *end;
    if (!eq || eq == spec || (size_t)(eq - spec) >= MAX_LEXEME || r->nvars >= MAX_VARS) return 0;
    VarBinding *v = &r->vars[r->nvars];
    memcpy(v->name, spec, (size_t)(eq - spec));
    v->name[eq - spec] = '\0';
    v->value = strtod(eq + 1, &end);
    if (end == eq + 1 || *end != '\0') return 0;
    r->nvars++;
    return 1;
}

int main(int argc, char **argv)
{
    int json = 0;
    const char *formula = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--json")) json = 1;
        else if (!strcmp(argv[i], "--eval")) R.eval_requested = 1;
        else if (!strcmp(argv[i], "--formula") && i + 1 < argc) formula = argv[++i];
        else if (!strcmp(argv[i], "--var") && i + 1 < argc) {
            if (!add_var(&R, argv[++i])) { fprintf(stderr, "error: bad --var '%s' (expected name=number)\n", argv[i]); return 2; }
        } else {
            fprintf(stderr, "usage: %s [--json] [--formula TEXT] [--eval] [--var name=value]...\n", argv[0]);
            return 2;
        }
    }

    size_t len;
    if (formula) {
        len = strlen(formula);
        memcpy(R.source, formula, len > MAX_SOURCE ? MAX_SOURCE + 1 : len);
        if (len > MAX_SOURCE) { len = MAX_SOURCE; R.too_long = 1; }
    } else {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        len = fread(R.source, 1, MAX_SOURCE + 1, stdin);
        if (len > MAX_SOURCE) { len = MAX_SOURCE; R.too_long = 1; }
    }
    R.source[len] = '\0';
    for (size_t i = 0; i < len; i++) if (R.source[i] == '\0') R.source[i] = '\x01';  /* NUL would end the C string silently */

    run_pipeline(&R);

    if (R.eval_requested && R.status[ST_RESULT] == S_PASSED) {
        evaluate(R.ast, R.vars, R.nvars, &R.ev);
        R.eval_performed = 1;
    }

    if (json) print_json(&R); else print_text(&R);

    int code = R.status[ST_RESULT] == S_PASSED ? 0 : 1;
    if (code == 0 && R.eval_performed && !R.ev.ok) code = 3;
    ast_free(R.ast);
    return code;
}
