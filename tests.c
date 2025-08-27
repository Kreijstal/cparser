#include "acutest.h"
#include "parser.h"
#include "combinators.h"
#include <stdio.h>

// --- Test `not` combinator ---
void test_pnot_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("hello");
    input->length = 5;

    // `pnot` should succeed when the parser fails
    combinator_t* p1 = pnot(match("world"));
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    TEST_ASSERT(input->start == 0); // `pnot` should not consume input
    free_combinator(p1);

    // `pnot` should fail when the parser succeeds
    combinator_t* p2 = pnot(match("hello"));
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    TEST_ASSERT(input->start == 0); // `not` should not consume input
    free(res2.value.error.message);
    free_combinator(p2);

    free(input->buffer);
    free(input);
}

// --- Test `peek` combinator ---
void test_peek_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("hello");
    input->length = 5;

    // `peek` should succeed when the parser succeeds
    combinator_t* p1 = peek(match("hello"));
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    TEST_ASSERT(input->start == 0); // `peek` should not consume input
    free_ast(res1.value.ast);
    free_combinator(p1);

    // `peek` should fail when the parser fails
    combinator_t* p2 = peek(match("world"));
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    TEST_ASSERT(input->start == 0); // `peek` should not consume input
    free(res2.value.error.message);
    free_combinator(p2);

    free(input->buffer);
    free(input);
}

// --- Test `gseq` combinator ---
void test_gseq_combinator(void) {
    input_t* input = new_input();
    input->buffer = strdup("helloworld");
    input->length = 10;

    // `gseq` should succeed when all parsers succeed
    combinator_t* p1 = gseq(new_combinator(), T_NONE, match("hello"), match("world"), NULL);
    ParseResult res1 = parse(input, p1);
    TEST_ASSERT(res1.is_success);
    TEST_ASSERT(input->start == 10);
    free_ast(res1.value.ast);
    free_combinator(p1);

    // `gseq` should fail and not backtrack
    input->start = 0;
    combinator_t* p2 = gseq(new_combinator(), T_NONE, match("hello"), match("goodbye"), NULL);
    ParseResult res2 = parse(input, p2);
    TEST_ASSERT(!res2.is_success);
    TEST_ASSERT(input->start == 5); // Should not backtrack
    free(res2.value.error.message);
    free_combinator(p2);

    free(input->buffer);
    free(input);
}

TEST_LIST = {
    { "pnot_combinator", test_pnot_combinator },
    { "peek_combinator", test_peek_combinator },
    { "gseq_combinator", test_gseq_combinator },
    { NULL, NULL }
};
