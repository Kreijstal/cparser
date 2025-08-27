#include "acutest.h"
#include "parser.h"
#include "combinators.h"
#include <stdio.h>

void test_pnot_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("hello");
    input->length = 5;
    combinator_t* p1 = pnot(match("world"));
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    free_combinator(p1);
    combinator_t* p2 = pnot(match("hello"));
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    free(res2.value.error.message);
    free_combinator(p2);
    free(input->buffer);
    free(input);
}

void test_peek_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("hello");
    input->length = 5;
    combinator_t* p1 = peek(match("hello"));
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    free_ast(res1.value.ast);
    free_combinator(p1);
    combinator_t* p2 = peek(match("world"));
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    free(res2.value.error.message);
    free_combinator(p2);
    free(input->buffer);
    free(input);
}

void test_gseq_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("helloworld");
    input->length = 10;
    combinator_t* p1 = gseq(new_combinator(), T_NONE, match("hello"), match("world"), NULL);
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    free_ast(res1.value.ast);
    free_combinator(p1);
    input->start = 0;
    combinator_t* p2 = gseq(new_combinator(), T_NONE, match("hello"), match("goodbye"), NULL);
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    free(res2.value.error.message);
    free_combinator(p2);
    free(input->buffer);
    free(input);
}

void test_between_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("(hello)");
    input->length = 7;
    combinator_t* p = between(match("("), match(")"), cident());
    ParseResult res = parse(input, p);
    TEST_ASSERT(res.is_success);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "hello") == 0);
    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_sep_by_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("a,b,c");
    input->length = 5;
    combinator_t* p = sep_by(cident(), match(","));
    ParseResult res = parse(input, p);
    TEST_ASSERT(res.is_success);
    ast_t* ast = res.value.ast;
    TEST_ASSERT(strcmp(ast->sym->name, "a") == 0);
    ast = ast->next;
    TEST_ASSERT(strcmp(ast->sym->name, "b") == 0);
    ast = ast->next;
    TEST_ASSERT(strcmp(ast->sym->name, "c") == 0);
    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_sep_end_by_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("a,b,c,");
    input->length = 6;
    combinator_t* p = sep_end_by(cident(), match(","));
    ParseResult res = parse(input, p);
    TEST_ASSERT(res.is_success);
    ast_t* ast = res.value.ast;
    TEST_ASSERT(strcmp(ast->sym->name, "a") == 0);
    ast = ast->next;
    TEST_ASSERT(strcmp(ast->sym->name, "b") == 0);
    ast = ast->next;
    TEST_ASSERT(strcmp(ast->sym->name, "c") == 0);
    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

static combinator_t* add_op() {
    return right(match("+"), succeed(ast1(T_ADD, ast_nil)));
}

void test_chainl1_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("1+2+3");
    input->length = 5;

    combinator_t* p = chainl1(integer(), add_op());
    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    ast_t* ast = res.value.ast;
    TEST_ASSERT(ast->typ == T_ADD);
    TEST_ASSERT(ast->child->typ == T_ADD);
    TEST_ASSERT(strcmp(ast->child->next->sym->name, "3") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

TEST_LIST = {
    { "pnot_combinator", test_pnot_combinator },
    { "peek_combinator", test_peek_combinator },
    { "gseq_combinator", test_gseq_combinator },
    { "between_combinator", test_between_combinator },
    { "sep_by_combinator", test_sep_by_combinator },
    { "sep_end_by_combinator", test_sep_end_by_combinator },
    { "chainl1_combinator", test_chainl1_combinator },
    { NULL, NULL }
};
