#include "parser.h"
#include <stdio.h>

int main() {
    printf("Testing 'until' combinator.\n");
    printf("Reading from stdin...\n");

    // Initialize the library
    ast_nil = new_ast();
    ast_nil->typ = T_NONE;

    // Create the parser: until(match_raw(" "))
    // We must use match_raw because the standard match() skips whitespace,
    // which would cause it to skip over the delimiter we are looking for.
    combinator_t* p = until(match_raw(" "));

    // Create the input stream. read1() will populate it from stdin.
    input_t* in = new_input();

    // Run the parser
    ast_t* result_ast = parse(in, p);

    // Check the result
    if (result_ast) {
        if (result_ast->typ == T_STRING && result_ast->sym) {
            printf("Success! Parsed string: '%s'\n", result_ast->sym->name);
        } else {
            printf("Failure: Parsed something, but it was not the expected string node.\n");
            printf("Result type: %d, value: %s\n", result_ast->typ, result_ast->sym ? result_ast->sym->name : "N/A");
        }
        printf("Remaining input starts at index: %d\n", in->start);
        free_ast(result_ast);
    } else {
        printf("Failure: Parser did not return a result.\n");
    }

    // Cleanup
    free_combinator(p);
    if (in->buffer) free(in->buffer);
    free(in);
    free(ast_nil);

    return 0;
}
