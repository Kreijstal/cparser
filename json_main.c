#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "json_parser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"<json_string>\"\n", argv[0]);
        return 1;
    }

    // --- Parser Definition ---
    combinator_t *parser = json_parser();

    // --- Parsing ---
    input_t *in = new_input();
    in->buffer = argv[1];
    in->length = strlen(argv[1]);

    ast_nil = new_ast();
    ast_nil->typ = T_NONE;

    ParseResult result = parse(in, parser);

    // --- Output ---
    if (result.is_success) {
        if (in->start < in->length) {
            fprintf(stderr, "Error: Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
        } else {
            printf("JSON parsed successfully.\n");
            // AST is not printed because it can be very large.
            free_ast(result.value.ast);
        }
    } else {
        fprintf(stderr, "Parsing Error at line %d, col %d: %s\n",
                result.value.error->line,
                result.value.error->col,
                result.value.error->message);
        free_error(result.value.error);
    }

    // --- Cleanup ---
    free_combinator(parser);
    free(in);
    free(ast_nil);

    return 0;
}
