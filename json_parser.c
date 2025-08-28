#include "json_parser.h"
#include "combinators.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Predicate for whitespace
static bool is_whitespace(char c) {
    return isspace((unsigned char)c);
}

//=============================================================================
// Custom JSON-Specific Parser Implementations
//=============================================================================

// Forward declarations
static ParseResult number_fn(input_t* in, void* args);

combinator_t* number();
combinator_t* json_null();
combinator_t* json_bool();


static ParseResult number_fn(input_t* in, void* args) {
    InputState state;
    save_input_state(in, &state);
    int start_pos = in->start;
    char c = read1(in);
    if (!isdigit(c) && c != '-') { restore_input_state(in, &state); return make_failure(in, strdup("Expected a number.")); }
    if (c == '-') {
        c = read1(in);
        if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected a digit after minus.")); }
    }
    // consume digits
    while (in->start < in->length && isdigit(in->buffer[in->start])) in->start++;
    // check for .
    if (in->start < in->length && in->buffer[in->start] == '.') {
        in->start++; // consume .
        if (in->start >= in->length || !isdigit(in->buffer[in->start])) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid fractional part.")); }
        while (in->start < in->length && isdigit(in->buffer[in->start])) in->start++;
    }
    // check for e or E
    if (in->start < in->length && (in->buffer[in->start] == 'e' || in->buffer[in->start] == 'E')) {
        in->start++; // consume e/E
        if (in->start < in->length && (in->buffer[in->start] == '+' || in->buffer[in->start] == '-')) in->start++; // consume +/-
        if (in->start >= in->length || !isdigit(in->buffer[in->start])) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid exponent part.")); }
        while (in->start < in->length && isdigit(in->buffer[in->start])) in->start++;
    }
    int len = in->start - start_pos;
    if (len == 0 || (len == 1 && in->buffer[start_pos] == '-')) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid number.")); }
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_pos, len);
    text[len] = '\0';
    ast_t* ast = new_ast();
    ast->typ = T_INT; // Re-using T_INT tag
    ast->sym = sym_lookup(text);
    free(text);
    return make_success(ast);
}

combinator_t* number() {
    combinator_t* ws = many(satisfy(is_whitespace));
    combinator_t* num = new_combinator();
    num->type = P_INTEGER;
    num->fn = number_fn;
    num->args = NULL;
    return right(ws, num);
}

static ast_t* map_null(ast_t* ast) {
    free_ast(ast);
    ast_t* new = new_ast();
    new->typ = T_NONE;
    return new;
}

combinator_t* json_null() {
    combinator_t* ws = many(satisfy(is_whitespace));
    combinator_t* null_core = map(match("null"), map_null);
    return right(ws, null_core);
}

static ast_t* map_true(ast_t* ast) {
    free_ast(ast);
    ast_t* new = new_ast();
    new->typ = T_INT;
    new->sym = sym_lookup("1");
    return new;
}

static ast_t* map_false(ast_t* ast) {
    free_ast(ast);
    ast_t* new = new_ast();
    new->typ = T_INT;
    new->sym = sym_lookup("0");
    return new;
}

combinator_t* json_bool() {
    combinator_t* ws = many(satisfy(is_whitespace));

    combinator_t* p_true = map(match("true"), map_true);
    combinator_t* p_false = map(match("false"), map_false);

    combinator_t* bool_core = new_combinator();
    multi(bool_core, T_NONE, p_true, p_false, NULL);

    return right(ws, bool_core);
}

//=============================================================================
// The Complete JSON Grammar
//=============================================================================

combinator_t* json_parser() {
    static combinator_t* json_value_singleton = NULL;
    if (json_value_singleton != NULL) {
        return json_value_singleton;
    }

    // The core recursive parser for any single JSON value.
    combinator_t* json_value = new_combinator();

    // A single lazy proxy is created for json_value and shared.
    // This avoids creating multiple lazy combinators pointing to the same
    // target, which may have confused the cycle detection in free_combinator.
    combinator_t* lazy_json_value = lazy(&json_value);

    // The various types of JSON values
    combinator_t* j_string = string();
    combinator_t* j_number = number();
    combinator_t* j_null = json_null();
    combinator_t* j_bool = json_bool();

    // Recursive definitions for array and object, using the *same* lazy proxy
    combinator_t* kv_pair = seq(new_combinator(), T_ASSIGN, j_string, expect(match(":"), "Expected ':'"), lazy_json_value, NULL);
    combinator_t* j_array = seq(new_combinator(), T_SEQ, match("["), sep_by(lazy_json_value, match(",")), expect(match("]"), "Expected ']'"), NULL);
    combinator_t* j_object = seq(new_combinator(), T_SEQ, match("{"), sep_by(kv_pair, match(",")), expect(match("}"), "Expected '}'"), NULL);

    // The main json_value parser is a `multi` choice between all possible types.
    multi(json_value, T_NONE, j_string, j_number, j_null, j_bool, j_array, j_object, NULL);

    json_value_singleton = json_value;
    return json_value_singleton;
}
