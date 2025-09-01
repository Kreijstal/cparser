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
    free_combinator(p); // full_parser is freed as part of p
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
    TEST_ASSERT(res.value.error->partial_ast->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(res.value.error->partial_ast->sym->name, "1") == 0);

    free_error(res.value.error);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

TEST_LIST = {
    { "test_pascal_integer_parsing", test_pascal_integer_parsing },
    { "test_pascal_invalid_input", test_pascal_invalid_input },
    { NULL, NULL }
};
