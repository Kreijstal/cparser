#include "pascal_keywords.h"
#include <string.h>
#include <ctype.h>

const char* pascal_reserved_keywords[] = {
    "begin", "end", "if", "then", "else", "while", "do", "for", "to", "downto",
    "repeat", "until", "case", "of", "var", "const", "type",
    "and", "or", "not", "xor", "div", "mod", "in", "nil", "true", "false",
    "array", "record", "set", "packed",
    // Exception handling keywords
    "try", "finally", "except", "raise", "on",
    // Class and object-oriented keywords
    "class", "object", "private", "public", "protected", "published",
    "property", "inherited", "self", "constructor", "destructor",
    // Additional Pascal keywords
    "function", "procedure", "program", "unit", "uses", "interface", "implementation",
    NULL
};

bool is_pascal_keyword(const char* str) {
    for (int i = 0; pascal_reserved_keywords[i] != NULL; i++) {
        if (strcasecmp(str, pascal_reserved_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Word-boundary aware case-insensitive keyword matching
static ParseResult keyword_ci_fn(input_t* in, void* args) {
    char* str = ((match_args*)args)->str;
    InputState state;
    save_input_state(in, &state);

    int len = strlen(str);

    // Match the keyword case-insensitively
    for (int i = 0; i < len; i++) {
        char c = read1(in);
        if (tolower(c) != tolower(str[i])) {
            restore_input_state(in, &state);
            char* err_msg;
            asprintf(&err_msg, "Expected keyword '%s' (case-insensitive)", str);
            return make_failure(in, err_msg);
        }
    }

    // Check for word boundary: next character should not be alphanumeric or underscore
    if (in->start < in->length) {
        char next_char = in->buffer[in->start];
        if (isalnum((unsigned char)next_char) || next_char == '_') {
            restore_input_state(in, &state);
            char* err_msg;
            asprintf(&err_msg, "Expected keyword '%s', not part of identifier", str);
            return make_failure(in, err_msg);
        }
    }

    return make_success(ast_nil);
}

// Create word-boundary aware case-insensitive keyword parser
combinator_t* keyword_ci(char* str) {
    match_args* args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t* comb = new_combinator();
    comb->type = P_CI_KEYWORD;
    comb->fn = keyword_ci_fn;
    comb->args = args;
    return comb;
}
