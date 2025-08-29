#include "pascal_parser.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

// --- Custom Tags for Pascal ---
typedef enum {
    PASCAL_T_NONE, PASCAL_T_IDENT, PASCAL_T_PROGRAM, PASCAL_T_ASM_BLOCK, PASCAL_T_IDENT_LIST
} pascal_tag_t;

// --- Forward Declarations ---
static combinator_t* token(combinator_t* p);
static combinator_t* keyword(const char* s);
static ParseResult p_match_ci(input_t* in, void* args);

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, PASCAL_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, PASCAL_T_NONE))));
}

static ParseResult p_match_ci(input_t* in, void* args) {
    const char* to_match = (const char*)args;
    int len = strlen(to_match);
    if (in->start + len > in->length) {
        return make_failure(in, NULL); // No message needed, will be wrapped.
    }
    for (int i = 0; i < len; i++) {
        if (tolower((unsigned char)in->buffer[in->start + i]) != tolower((unsigned char)to_match[i])) {
            return make_failure(in, NULL); // No message needed, will be wrapped.
        }
    }
    in->start += len;
    return make_success(ast_nil);
}

static combinator_t* keyword(const char* s) {
    combinator_t *c = new_combinator();
    c->type = P_CI_KEYWORD;
    c->fn = p_match_ci;
    c->args = (void*)strdup(s); // The args field now holds the string to be freed
    c->extra_to_free = NULL;
    return c;
}

// --- Primitive Parsers ---
static combinator_t* p_ident() {
    return token(cident(PASCAL_T_IDENT));
}

static combinator_t* p_program_kw() {
    return token(keyword("program"));
}

static combinator_t* p_begin_kw() {
    return token(keyword("begin"));
}

static combinator_t* p_end_kw() {
    return token(keyword("end"));
}

static combinator_t* p_semicolon() {
    return token(match(";"));
}

static combinator_t* p_dot() {
    return token(match("."));
}

static combinator_t* p_lparen() {
    return token(match("("));
}

static combinator_t* p_rparen() {
    return token(match(")"));
}

static combinator_t* p_comma() {
    return token(match(","));
}

combinator_t* p_identifier_list() {
    return sep_by(p_ident(), p_comma());
}

// --- ASM Block Parser ---

// Custom parser function to consume the body of an ASM block.
// It consumes input until it finds the keyword "end".
static ParseResult p_asm_body_fn(input_t* in, void* args) {
    int start_offset = in->start;
    while (in->start + 3 <= in->length) {
        if (strncasecmp(in->buffer + in->start, "end", 3) == 0) {
            break;
        }
        in->start++;
    }

    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';

    ast_t* ast = new_ast();
    ast->typ = PASCAL_T_ASM_BLOCK;
    ast->sym = sym_lookup(text);
    free(text);

    return make_success(ast);
}

static combinator_t* p_asm_body() {
    combinator_t* c = new_combinator();
    c->fn = p_asm_body_fn;
    return c;
}

combinator_t* p_asm_block() {
    return seq(new_combinator(), PASCAL_T_ASM_BLOCK,
        keyword("asm"),
        p_asm_body(),
        keyword("end"),
        p_semicolon(),
        NULL
    );
}


// --- Program Structure Parser ---

// For now, a statement is just an asm block.
static combinator_t* p_statement() {
    return p_asm_block();
}

static combinator_t* p_statement_list() {
    return many(p_statement());
}

combinator_t* p_program() {
    return seq(new_combinator(), PASCAL_T_PROGRAM,
        p_program_kw(),
        p_ident(),
        p_lparen(),
        p_identifier_list(),
        p_rparen(),
        p_semicolon(),
        // Declarations would go here
        // Subprogram declarations would go here
        p_begin_kw(),
        p_statement_list(),
        p_end_kw(),
        p_dot(),
        NULL
    );
}
