#include "../../acutest.h"
#include "pascal_parser.h"
#include <string.h>

// --- Custom Tags for Pascal (mirrored from pascal_parser.c) ---
typedef enum {
    PASCAL_T_NONE, PASCAL_T_IDENT, PASCAL_T_PROGRAM, PASCAL_T_ASM_BLOCK
} pascal_tag_t;

void test_pascal_minimal_program(void) {
    const char* input_str = "program MyTestProgram; begin end.";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_program();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s (line %d, col %d)", result.value.error->message, result.value.error->line, result.value.error->col);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_PROGRAM);
        TEST_MSG("Expected root AST node to be PASCAL_T_PROGRAM, but got %d", ast->typ);

        ast_t* ident_node = ast->child;
        TEST_CHECK(ident_node != NULL);
        if (ident_node) {
            TEST_CHECK(ident_node->typ == PASCAL_T_IDENT);
            TEST_MSG("Expected child node to be PASCAL_T_IDENT, but got %d", ident_node->typ);
            TEST_CHECK(strcmp(ident_node->sym->name, "MyTestProgram") == 0);
            TEST_MSG("Expected identifier to be 'MyTestProgram', but got '%s'", ident_node->sym->name);
            TEST_CHECK(ident_node->next == NULL);
        }

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_asm_block(void) {
    const char* input_str = "asm mov eax, 1; end;";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_asm_block();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s (line %d, col %d)", result.value.error->message, result.value.error->line, result.value.error->col);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ASM_BLOCK);

        ast_t* body_node = ast->child;
        TEST_CHECK(body_node != NULL);
        if (body_node) {
            TEST_CHECK(body_node->typ == PASCAL_T_ASM_BLOCK);
            TEST_CHECK(strcmp(body_node->sym->name, " mov eax, 1; ") == 0);
        }

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

TEST_LIST = {
    { "test_pascal_minimal_program", test_pascal_minimal_program },
    { "test_pascal_asm_block", test_pascal_asm_block },
    { NULL, NULL }
};
