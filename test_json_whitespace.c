#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    input_t* input = new_input();

    // Test cases for different JSON values with whitespace
    const char* test_cases[] = {
        "  42  ",           // number with whitespace
        "42",               // number without whitespace
        "  null  ",         // null with whitespace
        "null",             // null without whitespace
        "  true  ",         // boolean with whitespace
        "false",            // boolean without whitespace
        "  \"hello\"  ",    // string with whitespace
        "\"world\"",        // string without whitespace
    };

    combinator_t* parser = json_parser();

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        printf("Testing: '%s'\n", test_cases[i]);

        input->buffer = strdup(test_cases[i]);
        input->length = strlen(input->buffer);
        input->start = 0;
        input->line = 1;
        input->col = 1;

        ParseResult res = parse(input, parser);
        if (res.is_success) {
            printf("SUCCESS: Parsed as %s\n", res.value.ast->sym ? res.value.ast->sym->name : "null");
            free_ast(res.value.ast);
        } else {
            printf("FAILED: %s\n", res.value.error->message);
            free_error(res.value.error);
        }

        free(input->buffer);
        printf("\n");
    }

    free_combinator(parser);
    free(input);
    return 0;
}