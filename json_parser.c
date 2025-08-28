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
static ParseResult null_core_fn(input_t* in, void* args);
static ParseResult bool_core_fn(input_t* in, void* args);

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

static ParseResult null_core_fn(input_t* in, void* args) {
    ParseResult result = parse(in, match("null"));
    if (result.is_success) {
        free_ast(result.value.ast);
        ast_t* ast = new_ast();
        ast->typ = T_NONE;
        return make_success(ast);
    }
    return result;
}

combinator_t* json_null() {
    combinator_t* ws = many(satisfy(is_whitespace));
    combinator_t* null_core = new_combinator();
    null_core->type = P_MATCH;
    null_core->fn = null_core_fn;
    null_core->args = NULL;
    return right(ws, null_core);
}

static ParseResult bool_core_fn(input_t* in, void* args) {
    InputState state;
    save_input_state(in, &state);
    ParseResult res_true = parse(in, match("true"));
    if (res_true.is_success) {
        free_ast(res_true.value.ast);
        ast_t* ast = new_ast();
        ast->typ = T_INT;
        ast->sym = sym_lookup("1");
        return make_success(ast);
    }
    free_error(res_true.value.error);
    restore_input_state(in, &state);
    ParseResult res_false = parse(in, match("false"));
    if (res_false.is_success) {
        free_ast(res_false.value.ast);
        ast_t* ast = new_ast();
        ast->typ = T_INT;
        ast->sym = sym_lookup("0");
        return make_success(ast);
    }
    return res_false;
}

combinator_t* json_bool() {
    combinator_t* ws = many(satisfy(is_whitespace));
    combinator_t* bool_core = new_combinator();
    bool_core->type = P_MATCH;
    bool_core->fn = bool_core_fn;
    bool_core->args = NULL;
    return right(ws, bool_core);
}

//=============================================================================
// The Complete JSON Grammar
//=============================================================================

combinator_t* json_parser() {
    // The core recursive parser for any single JSON value.
    combinator_t** p_json_value = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *p_json_value = new_combinator();
    (*p_json_value)->extra_to_free = p_json_value;

    // The various types of JSON values
    combinator_t* j_string = string();
    combinator_t* j_number = number();
    combinator_t* j_null = json_null();
    combinator_t* j_bool = json_bool();

    // Recursive definitions for array and object, using new lazy proxies each time
    // A separate string() parser is needed for the key in a kv_pair to avoid double-freeing j_string
    combinator_t* kv_pair = seq(new_combinator(), T_ASSIGN, string(), expect(match(":"), "Expected ':'"), lazy(p_json_value), NULL);
    combinator_t* j_array = seq(new_combinator(), T_SEQ, match("["), sep_by(lazy(p_json_value), match(",")), expect(match("]"), "Expected ']'"), NULL);
    combinator_t* j_object = seq(new_combinator(), T_SEQ, match("{"), sep_by(kv_pair, match(",")), expect(match("}"), "Expected '}'"), NULL);

    // The main json_value parser is a `multi` choice between all possible types.
    multi(*p_json_value, T_NONE, j_string, j_number, j_null, j_bool, j_array, j_object, NULL);

    return *p_json_value;
}
