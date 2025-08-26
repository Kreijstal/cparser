#include "parser.h"
#include "combinators.h"
#include "combinator_internals.h"
#include <stdlib.h>
#include <stdarg.h>

// --- Static Function Forward Declarations ---
static ParseResult expect_fn(input_t * in, void * args);
static ParseResult seq_fn(input_t * in, void * args);
static ParseResult multi_fn(input_t * in, void * args);
static ParseResult flatMap_fn(input_t * in, void * args);
static ParseResult left_fn(input_t * in, void * args);
static ParseResult right_fn(input_t * in, void * args);

// --- _fn Implementations ---

static ParseResult expect_fn(input_t * in, void * args) {
    expect_args * eargs = (expect_args *) args;
    ParseResult res = parse(in, eargs->comb);
    if (res.is_success) return res;
    free(res.value.error.message);
    return make_failure(in, strdup(eargs->msg));
}

static ParseResult seq_fn(input_t * in, void * args) {
    InputState state; save_input_state(in, &state);
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ast_t * head = NULL, * tail = NULL;
    while (seq != NULL) {
        ParseResult res = parse(in, seq->comb);
        if (!res.is_success) { restore_input_state(in, &state); return res; }
        if (res.value.ast != ast_nil) {
            if (head == NULL) head = tail = res.value.ast;
            else { tail->next = res.value.ast; while(tail->next) tail = tail->next; }
        }
        seq = seq->next;
    }
    return make_success(sa->typ == T_NONE ? head : ast1(sa->typ, head));
}

static ParseResult multi_fn(input_t * in, void * args) {
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ParseResult res;
    while (seq != NULL) {
        res = parse(in, seq->comb);
        if (res.is_success) {
           if (sa->typ != T_NONE) res.value.ast = ast1(sa->typ, res.value.ast);
           return res;
        }
        if (seq->next != NULL) free(res.value.error.message);
        seq = seq->next;
    }
    return res;
}

static ParseResult flatMap_fn(input_t * in, void * args) {
    flatMap_args * fm_args = (flatMap_args *)args;
    InputState state; save_input_state(in, &state);
    ParseResult res = parse(in, fm_args->parser);
    if (!res.is_success) return res;
    combinator_t * next_parser = fm_args->func(res.value.ast);
    if (next_parser == NULL) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("flatMap function returned NULL parser."));
    }
    ParseResult final_res = parse(in, next_parser);
    free_combinator(next_parser);
    if (!final_res.is_success) restore_input_state(in, &state);
    return final_res;
}

static ParseResult left_fn(input_t * in, void * args) {
    pair_args* pargs = (pair_args*)args;
    InputState state; save_input_state(in, &state);
    ParseResult r1 = parse(in, pargs->p1);
    if (!r1.is_success) return r1;
    ParseResult r2 = parse(in, pargs->p2);
    if (!r2.is_success) {
        restore_input_state(in, &state);
        free_ast(r1.value.ast);
        return r2;
    }
    free_ast(r2.value.ast);
    return r1;
}

static ParseResult right_fn(input_t * in, void * args) {
    pair_args* pargs = (pair_args*)args;
    InputState state; save_input_state(in, &state);
    ParseResult r1 = parse(in, pargs->p1);
    if (!r1.is_success) return r1;
    ParseResult r2 = parse(in, pargs->p2);
    if (!r2.is_success) {
        restore_input_state(in, &state);
        free_ast(r1.value.ast);
        return r2;
    }
    free_ast(r1.value.ast);
    return r2;
}


// --- Constructor Implementations ---

combinator_t * expect(combinator_t * c, char * msg) {
    expect_args * args = (expect_args*)safe_malloc(sizeof(expect_args));
    args->msg = msg; args->comb = c;
    combinator_t * comb = new_combinator();
    comb->type = COMB_EXPECT; comb->fn = expect_fn; comb->args = (void *) args; return comb;
}

combinator_t * seq(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap; va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next; current->comb = c;
    }
    current->next = NULL; va_end(ap);
    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ; args->list = head;
    ret->type = COMB_SEQ; ret->args = (void *) args; ret->fn = seq_fn; return ret;
}

combinator_t * multi(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap; va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next; current->comb = c;
    }
    current->next = NULL; va_end(ap);
    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ; args->list = head;
    ret->type = COMB_MULTI; ret->args = (void *) args; ret->fn = multi_fn; return ret;
}

combinator_t * flatMap(combinator_t * p, flatMap_func func) {
    flatMap_args * args = (flatMap_args*)safe_malloc(sizeof(flatMap_args));
    args->parser = p; args->func = func;
    combinator_t * comb = new_combinator();
    comb->type = COMB_FLATMAP; comb->fn = flatMap_fn; comb->args = args; return comb;
}

combinator_t * left(combinator_t* p1, combinator_t* p2) {
    pair_args* args = (pair_args*)safe_malloc(sizeof(pair_args));
    args->p1 = p1; args->p2 = p2;
    combinator_t * comb = new_combinator();
    comb->type = COMB_LEFT; comb->fn = left_fn; comb->args = (void *) args; return comb;
}

combinator_t * right(combinator_t* p1, combinator_t* p2) {
    pair_args* args = (pair_args*)safe_malloc(sizeof(pair_args));
    args->p1 = p1; args->p2 = p2;
    combinator_t * comb = new_combinator();
    comb->type = COMB_RIGHT; comb->fn = right_fn; comb->args = (void *) args; return comb;
}
