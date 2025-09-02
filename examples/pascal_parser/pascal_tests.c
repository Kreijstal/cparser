#include "acutest.h"
#include "parser.h"
#include "combinators.h"
#include "pascal_parser.h"
#include <stdio.h>

void test_pascal_integer_parsing(void) {
    input_t* input = new_input();
    input->buffer = strdup("123");
    input->length = 3;

    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);
    combinator_t* full_parser = left(p, eoi());

    ParseResult res = parse(input, full_parser);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "123") == 0);

    free_ast(res.value.ast);
    free_combinator(full_parser);
    free(input->buffer);
    free(input);
}

void test_pascal_invalid_input(void) {
    input_t* input = new_input();
    input->buffer = strdup("1 +");
    input->length = 3;

    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);
    combinator_t* full_parser = left(p, eoi());

    ParseResult res = parse(input, full_parser);

    TEST_ASSERT(!res.is_success);
    TEST_ASSERT(res.value.error->partial_ast != NULL);
    TEST_ASSERT(res.value.error->partial_ast->typ == PASCAL_T_ADD);
    ast_t* lhs = res.value.error->partial_ast->child;
    TEST_ASSERT(lhs->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(lhs->sym->name, "1") == 0);

    free_error(res.value.error);
    free_combinator(full_parser);
    free(input->buffer);
    free(input);
}

void test_pascal_function_call(void) {
    input_t* input = new_input();
    input->buffer = strdup("my_func(1, 2)");
    input->length = strlen("my_func(1, 2)");

    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);
    combinator_t* full_parser = left(p, eoi());

    ParseResult res = parse(input, full_parser);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNC_CALL);

    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(func_name->sym->name, "my_func") == 0);

    ast_t* arg1 = func_name->next;
    TEST_ASSERT(arg1->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(arg1->sym->name, "1") == 0);

    ast_t* arg2 = arg1->next;
    TEST_ASSERT(arg2->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(arg2->sym->name, "2") == 0);

    free_ast(res.value.ast);
    free_combinator(full_parser);
    free(input->buffer);
    free(input);
}

TEST_LIST = {
    { "test_pascal_integer_parsing", test_pascal_integer_parsing },
    { "test_pascal_invalid_input", test_pascal_invalid_input },
    { "test_pascal_function_call", test_pascal_function_call },
    { NULL, NULL }
};
