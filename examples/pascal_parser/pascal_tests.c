#include "../../acutest.h"
#include "pascal_parser.h"
#include <string.h>

// --- Custom Tags for Pascal (mirrored from pascal_parser.c) ---
typedef enum {
    PASCAL_T_NONE, PASCAL_T_IDENT, PASCAL_T_PROGRAM, PASCAL_T_ASM_BLOCK, PASCAL_T_IDENT_LIST,
    PASCAL_T_INT_NUM, PASCAL_T_REAL_NUM,
    PASCAL_T_ADD, PASCAL_T_SUB, PASCAL_T_MUL, PASCAL_T_DIV
} pascal_tag_t;

void test_pascal_minimal_program(void) {
    const char* input_str = "program MyTestProgram(input, output); begin end.";

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

        ast_t* child = ast->child;
        TEST_CHECK(child != NULL);

        // Check program name
        TEST_CHECK(child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(child->sym->name, "MyTestProgram") == 0);

        // Check parameter list
        child = child->next;
        TEST_CHECK(child != NULL);
        if (child) {
            TEST_CHECK(child->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(child->sym->name, "input") == 0);
        }

        child = child->next;
        TEST_CHECK(child != NULL);
        if (child) {
            TEST_CHECK(child->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(child->sym->name, "output") == 0);
        }

        // The next child should be the statement list, which is nil for an empty program, so the list should end here.
        child = child->next;
        TEST_CHECK(child == NULL);

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

void test_pascal_identifier_list(void) {
    const char* input_str = "one, two, three";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_identifier_list();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s", result.value.error->message);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast != NULL && ast != ast_nil);

        // Check first identifier
        TEST_CHECK(ast->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->sym->name, "one") == 0);

        // Check second identifier
        ast = ast->next;
        TEST_CHECK(ast != NULL);
        if (ast) {
            TEST_CHECK(ast->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(ast->sym->name, "two") == 0);
        }

        // Check third identifier
        ast = ast->next;
        TEST_CHECK(ast != NULL);
        if (ast) {
            TEST_CHECK(ast->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(ast->sym->name, "three") == 0);
        }

        // Should be the end of the list
        ast = ast->next;
        TEST_CHECK(ast == NULL);

        free_ast(result.value.ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_expressions(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    // Test case 1: Simple addition
    const char* input1 = "1 + 2";
    input_t *in1 = new_input();
    in1->buffer = strdup(input1);
    in1->length = strlen(input1);
    combinator_t* parser1 = p_expression();
    ParseResult res1 = parse(in1, parser1);
    TEST_CHECK(res1.is_success);
    if(res1.is_success) {
        ast_t* ast = res1.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ADD);
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->sym->name, "1") == 0);
        TEST_CHECK(ast->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->next->sym->name, "2") == 0);
        free_ast(ast);
    }
    free_combinator(parser1);
    free(in1->buffer);
    free(in1);

    // Test case 2: Operator precedence
    const char* input2 = "1 + 2 * 3";
    input_t *in2 = new_input();
    in2->buffer = strdup(input2);
    in2->length = strlen(input2);
    combinator_t* parser2 = p_expression();
    ParseResult res2 = parse(in2, parser2);
    TEST_CHECK(res2.is_success);
    if(res2.is_success) {
        ast_t* ast = res2.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ADD); // Root is +
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM); // Left child is 1
        ast_t* mul_node = ast->child->next;
        TEST_CHECK(mul_node->typ == PASCAL_T_MUL); // Right child is a * expression
        TEST_CHECK(mul_node->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(mul_node->child->sym->name, "2") == 0);
        TEST_CHECK(mul_node->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(mul_node->child->next->sym->name, "3") == 0);
        free_ast(ast);
    }
    free_combinator(parser2);
    free(in2->buffer);
    free(in2);

    free(ast_nil);
}


TEST_LIST = {
    { "test_pascal_minimal_program", test_pascal_minimal_program },
    { "test_pascal_asm_block", test_pascal_asm_block },
    { "test_pascal_identifier_list", test_pascal_identifier_list },
    { "test_pascal_expressions", test_pascal_expressions },
    { NULL, NULL }
};
