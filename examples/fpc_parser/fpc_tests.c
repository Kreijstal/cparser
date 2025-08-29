#include "../../acutest.h"
#include "fpc_parser.h"
#include <string.h>

// --- Custom Tags for FPC (mirrored from fpc_parser.c) ---
typedef enum {
    FPC_T_NONE, FPC_T_IDENT, FPC_T_PROGRAM
} fpc_tag_t;

void test_minimal_program_parsing(void) {
    const char* input_str = "program MyTestProgram; begin end.";

    input_t *in = new_input();
    in->buffer = strdup(input_str); // Use a heap-allocated copy
    in->length = strlen(input_str);

    ast_nil = new_ast(); // Initialize the global nil AST
    ast_nil->typ = FPC_T_NONE;

    combinator_t* parser = p_program();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s (line %d, col %d)", result.value.error->message, result.value.error->line, result.value.error->col);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == FPC_T_PROGRAM);
        TEST_MSG("Expected root AST node to be FPC_T_PROGRAM, but got %d", ast->typ);

        // Check the program name identifier
        ast_t* ident_node = ast->child;
        TEST_CHECK(ident_node != NULL);
        if (ident_node) {
            TEST_CHECK(ident_node->typ == FPC_T_IDENT);
            TEST_MSG("Expected child node to be FPC_T_IDENT, but got %d", ident_node->typ);
            TEST_CHECK(strcmp(ident_node->sym->name, "MyTestProgram") == 0);
            TEST_MSG("Expected identifier to be 'MyTestProgram', but got '%s'", ident_node->sym->name);
            TEST_CHECK(ident_node->next == NULL); // Should only be one child
        }

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer); // Free the duplicated string
    free(in);
    free(ast_nil);
}

TEST_LIST = {
    { "test_minimal_program_parsing", test_minimal_program_parsing },
    { NULL, NULL }
};
