#include "json_parser.h"
#include <stdbool.h>
#include <string.h>

//=============================================================================
// Custom JSON-Specific Parser Implementations
//=============================================================================

// Forward declarations
static ParseResult number_fn(input_t* in, void* args);
static ParseResult null_fn(input_t* in, void* args);
static ParseResult bool_fn(input_t* in, void* args);

combinator_t* number();
combinator_t* json_null();
combinator_t* json_bool();


static ParseResult number_fn(input_t* in, void* args) {
    InputState state;
    save_input_state(in, &state);
    skip_whitespace(in);
    int start_pos = in->start;
    if (read1(in) != '-') { in->start--; }
    char c = read1(in);
    if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected a number.")); }
    while (isdigit(read1(in))) {}
    in->start--;
    if (read1(in) == '.') {
        c = read1(in);
        if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid fractional part.")); }
        while (isdigit(read1(in))) {}
        in->start--;
    } else { in->start--; }
    c = read1(in);
    if (c == 'e' || c == 'E') {
        c = read1(in);
        if (c == '+' || c == '-') {} else { in->start--; }
        c = read1(in);
        if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid exponent part.")); }
        while (isdigit(read1(in))) {}
        in->start--;
    } else { in->start--; }
    int len = in->start - start_pos;
    if (len == 0 || (len == 1 && (in->buffer[start_pos] == '-'))) { restore_input_state(in, &state); return make_failure(in, strdup("Invalid number.")); }
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
    combinator_t* comb = new_combinator();
    comb->fn = number_fn;
    comb->args = NULL;
    return comb;
}

static ParseResult null_fn(input_t* in, void* args) {
    ParseResult result = parse(in, (combinator_t*)args);
    if (result.is_success) {
        free_ast(result.value.ast);
        ast_t* ast = new_ast();
        ast->typ = T_NONE;
        return make_success(ast);
    }
    return result;
}

combinator_t* json_null() {
    combinator_t* comb = new_combinator();
    comb->fn = null_fn;
    comb->args = (void*)match("null");
    return comb;
}

static ParseResult bool_fn(input_t* in, void* args) {
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
    free(res_true.value.error.message);
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
    combinator_t* comb = new_combinator();
    comb->fn = bool_fn;
    comb->args = NULL;
    return comb;
}

//=============================================================================
// The Complete JSON Grammar
//=============================================================================

combinator_t* json_parser() {
    combinator_t* json_value = new_combinator();
    combinator_t* j_string = string();
    combinator_t* j_number = number();
    combinator_t* j_null = json_null();
    combinator_t* j_bool = json_bool();
    combinator_t* j_array = seq(new_combinator(), T_SEQ, match("["), sep_by(json_value, match(",")), expect(match("]"), "Expected ']'"), NULL);
    combinator_t* kv_pair = seq(new_combinator(), T_ASSIGN, j_string, expect(match(":"), "Expected ':'"), json_value, NULL);
    combinator_t* j_object = seq(new_combinator(), T_SEQ, match("{"), sep_by(kv_pair, match(",")), expect(match("}"), "Expected '}'"), NULL);
    multi(json_value, T_NONE, j_string, j_number, j_null, j_bool, j_array, j_object, NULL);
    return json_value;
}
