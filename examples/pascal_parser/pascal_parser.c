#include "pascal_parser.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

// --- Custom Tags for Pascal ---
typedef enum {
    PASCAL_T_NONE, PASCAL_T_IDENT, PASCAL_T_PROGRAM, PASCAL_T_ASM_BLOCK, PASCAL_T_IDENT_LIST,
    PASCAL_T_INT_NUM, PASCAL_T_REAL_NUM,
    PASCAL_T_ADD, PASCAL_T_SUB, PASCAL_T_MUL, PASCAL_T_DIV,
    PASCAL_T_VAR_DECL, PASCAL_T_TYPE_INTEGER, PASCAL_T_TYPE_REAL
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

static combinator_t* p_int_num() {
    return token(integer(PASCAL_T_INT_NUM));
}

static combinator_t* p_plus() {
    return token(match("+"));
}

static combinator_t* p_minus() {
    return token(match("-"));
}

static combinator_t* p_star() {
    return token(match("*"));
}

static combinator_t* p_slash() {
    return token(match("/"));
}

static combinator_t* p_colon() {
    return token(match(":"));
}

static combinator_t* p_var_kw() {
    return token(keyword("var"));
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


// --- Expression Parser ---
combinator_t* p_expression() {
    combinator_t* expr_parser = new_combinator();

    // The base of the expression grammar (a "factor")
    combinator_t* factor = multi(new_combinator(), PASCAL_T_NONE,
        p_int_num(),
        between(p_lparen(), p_rparen(), lazy(&expr_parser)),
        NULL
    );

    // Initialize the expression parser with the factor as the base
    expr(expr_parser, factor);

    // Add operators with precedence (higher number = higher precedence)
    // Level 0: +, -
    expr_insert(expr_parser, 0, PASCAL_T_ADD, EXPR_INFIX, ASSOC_LEFT, p_plus());
    expr_altern(expr_parser, 0, PASCAL_T_SUB, p_minus());
    // Level 1: *, /
    expr_insert(expr_parser, 1, PASCAL_T_MUL, EXPR_INFIX, ASSOC_LEFT, p_star());
    expr_altern(expr_parser, 1, PASCAL_T_DIV, p_slash());

    return expr_parser;
}


// --- Declaration Parser ---

// Helper map functions to convert keyword ASTs to type ASTs
static ast_t* map_to_integer_type(ast_t* ast) {
    free_ast(ast); // free the keyword ast
    return ast1(PASCAL_T_TYPE_INTEGER, ast_nil);
}

static ast_t* map_to_real_type(ast_t* ast) {
    free_ast(ast);
    return ast1(PASCAL_T_TYPE_REAL, ast_nil);
}

static combinator_t* p_standard_type() {
    return multi(new_combinator(), PASCAL_T_NONE,
        map(keyword("integer"), map_to_integer_type),
        map(keyword("real"), map_to_real_type),
        NULL
    );
}

static combinator_t* p_var_declaration_line() {
    return seq(new_combinator(), PASCAL_T_VAR_DECL,
        p_identifier_list(),
        p_colon(),
        p_standard_type(),
        p_semicolon(),
        NULL
    );
}

combinator_t* p_declarations() {
    return seq(new_combinator(), PASCAL_T_NONE, // Don't create a wrapping node for the whole var block
        p_var_kw(),
        many(p_var_declaration_line()),
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
        optional(p_declarations()),
        // Subprogram declarations would go here
        p_begin_kw(),
        p_statement_list(),
        p_end_kw(),
        p_dot(),
        NULL
    );
}
