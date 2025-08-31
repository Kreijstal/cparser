#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "pascal_parser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate memory for file content\n");
        fclose(file);
        return 1;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    // --- Parser Definition ---
    combinator_t *parser = p_program();

    // --- Parsing ---
    input_t *in = new_input();
    in->buffer = buffer;
    in->length = length;

    ast_nil = new_ast();
    ast_nil->typ = 0; // PASCAL_T_NONE is not defined here, but 0 should be fine.

    ParseResult result = parse(in, parser);

    // --- Output ---
    if (result.is_success && in->start == in->length) {
        printf("Successfully parsed %s\n", argv[1]);
        free_ast(result.value.ast);
    } else {
        fprintf(stderr, "Failed to parse %s\n", argv[1]);
        if (!result.is_success) {
            fprintf(stderr, "  Error at line %d, col %d: %s\n",
                    result.value.error->line,
                    result.value.error->col,
                    result.value.error->message);
            free_error(result.value.error);
        } else {
            fprintf(stderr, "  Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
        }
    }

    // --- Cleanup ---
    free_combinator(parser);
    free(in);
    free(ast_nil);
    free(buffer);

    return result.is_success && in->start == in->length ? 0 : 1;
}
