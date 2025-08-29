#include "fpc_parser.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

// --- Custom Tags for FPC ---
typedef enum {
    FPC_T_NONE, FPC_T_IDENT, FPC_T_PROGRAM
} fpc_tag_t;


// Wrapper for isspace to match the required function signature for satisfy.
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

// Helper to wrap a parser with whitespace consumption on both sides.
combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, FPC_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, FPC_T_NONE))));
}

// Custom parse function for case-insensitive string matching.
static ParseResult p_match_ci(input_t* in, void* args) {
    const char* to_match = (const char*)args;
    int len = strlen(to_match);

    if (in->start + len > in->length) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Expected '%s', but reached end of input.", to_match);
        return make_failure(in, strdup(msg));
    }

    for (int i = 0; i < len; i++) {
        if (tolower((unsigned char)in->buffer[in->start + i]) != tolower((unsigned char)to_match[i])) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Expected '%s', but got '%.*s'", to_match, len, in->buffer + in->start);
            return make_failure(in, strdup(msg));
        }
    }

    in->start += len;
    return make_success(ast_nil); // Success, no AST node produced.
}

// Public constructor for the case-insensitive keyword parser.
combinator_t* keyword(const char* s) {
    combinator_t *c = new_combinator();
    c->type = P_MATCH; // For debugging, identify as a match-like parser
    c->fn = p_match_ci;
    c->args = (void*)s;
    c->extra_to_free = NULL;
    return c;
}

// --- Primitive Parsers ---

combinator_t* p_ident() {
    return token(cident(FPC_T_IDENT));
}

combinator_t* p_program_kw() {
    return token(keyword("program"));
}

combinator_t* p_begin_kw() {
    return token(keyword("begin"));
}

combinator_t* p_end_kw() {
    return token(keyword("end"));
}

combinator_t* p_semicolon() {
    return token(match(";"));
}

combinator_t* p_dot() {
    return token(match("."));
}

// --- Program Structure Parser ---

combinator_t* p_program() {
    return seq(new_combinator(), FPC_T_PROGRAM,
        p_program_kw(),
        p_ident(),
        p_semicolon(),
        p_begin_kw(),
        p_end_kw(),
        p_dot(),
        NULL
    );
}
