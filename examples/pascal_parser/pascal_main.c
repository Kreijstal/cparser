#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pascal_parser.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"<pascal_code>\"\n", argv[0]);
        return 1;
    }

    char *code = argv[1];
    input_t *in = new_input();
    in->buffer = code;
    in->length = strlen(code);

    ast_nil = new_ast();

    combinator_t* parser = p_program();
    ParseResult result = parse(in, parser);

    if (result.is_success) {
        // Since we removed the generic printer, we can't print the AST here.
        // A local visitor-based printer would be needed.
        printf("Successfully parsed the input.\n");
        free_ast(result.value.ast);
    } else {
        fprintf(stderr, "Parsing Error at line %d, col %d: %s\n",
                result.value.error->line,
                result.value.error->col,
                result.value.error->message);
        free_error(result.value.error);
    }

    free_combinator(parser);
    free(in);
    free(ast_nil);

    return 0;
}
