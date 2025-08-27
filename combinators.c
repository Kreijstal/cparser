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
static ParseResult pnot_fn(input_t * in, void * args);
static ParseResult peek_fn(input_t * in, void * args);
static ParseResult gseq_fn(input_t * in, void * args);
static ParseResult between_fn(input_t * in, void * args);
static ParseResult sep_by_fn(input_t * in, void * args);
static ParseResult sep_end_by_fn(input_t * in, void * args);
static ParseResult chainl1_fn(input_t * in, void * args);
static ParseResult succeed_fn(input_t * in, void * args);

// --- _fn Implementations ---

static ParseResult pnot_fn(input_t * in, void * args) {
    not_args* nargs = (not_args*)args;
    InputState state; save_input_state(in, &state);
    ParseResult res = parse(in, nargs->p);
    restore_input_state(in, &state);
    if (res.is_success) {
        free_ast(res.value.ast);
        return make_failure(in, strdup("not combinator failed."));
    }
    // The error from the inner parse is consumed and we return success.
    free(res.value.error.message);
    return make_success(ast_nil);
}

static ParseResult peek_fn(input_t * in, void * args) {
    peek_args* pargs = (peek_args*)args;
    InputState state; save_input_state(in, &state);
    ParseResult res = parse(in, pargs->p);
    restore_input_state(in, &state);
    return res;
}

static ParseResult expect_fn(input_t * in, void * args) {
    expect_args * eargs = (expect_args *) args;
    ParseResult res = parse(in, eargs->comb);
    if (res.is_success) return res;
    free(res.value.error.message);
    return make_failure(in, strdup(eargs->msg));
}

static ParseResult between_fn(input_t * in, void * args) {
    between_args* bargs = (between_args*)args;
    InputState state; save_input_state(in, &state);

    ParseResult r_open = parse(in, bargs->open);
    if (!r_open.is_success) return r_open;
    free_ast(r_open.value.ast);

    ParseResult r_p = parse(in, bargs->p);
    if (!r_p.is_success) {
        restore_input_state(in, &state);
        return r_p;
    }

    ParseResult r_close = parse(in, bargs->close);
    if (!r_close.is_success) {
        restore_input_state(in, &state);
        free_ast(r_p.value.ast);
        return r_close;
    }
    free_ast(r_close.value.ast);

    return r_p;
}

static ParseResult sep_by_fn(input_t * in, void * args) {
    sep_by_args* sargs = (sep_by_args*)args;
    ast_t* head = NULL;
    ast_t* tail = NULL;

    ParseResult res = parse(in, sargs->p);
    if (!res.is_success) {
        return make_success(ast_nil);
    }
    head = tail = res.value.ast;

    while (1) {
        InputState state; save_input_state(in, &state);
        ParseResult sep_res = parse(in, sargs->sep);
        if (!sep_res.is_success) {
            restore_input_state(in, &state);
            break;
        }
        free_ast(sep_res.value.ast);

        ParseResult p_res = parse(in, sargs->p);
        if (!p_res.is_success) {
            restore_input_state(in, &state);
            break;
        }
        tail->next = p_res.value.ast;
        tail = tail->next;
    }

    return make_success(head);
}

static ParseResult sep_end_by_fn(input_t * in, void * args) {
    sep_end_by_args* sargs = (sep_end_by_args*)args;
    ast_t* head = NULL;
    ast_t* tail = NULL;

    ParseResult res = parse(in, sargs->p);
    if (!res.is_success) {
        return make_success(ast_nil);
    }
    head = tail = res.value.ast;

    while (1) {
        InputState state; save_input_state(in, &state);
        ParseResult sep_res = parse(in, sargs->sep);
        if (!sep_res.is_success) {
            restore_input_state(in, &state);
            break;
        }
        free_ast(sep_res.value.ast);

        ParseResult p_res = parse(in, sargs->p);
        if (!p_res.is_success) {
            restore_input_state(in, &state);
            break;
        }
        tail->next = p_res.value.ast;
        tail = tail->next;
    }

    // Try to parse a final separator
    InputState final_sep_state; save_input_state(in, &final_sep_state);
    ParseResult final_sep_res = parse(in, sargs->sep);
    if (!final_sep_res.is_success) {
        restore_input_state(in, &final_sep_state);
    } else {
        free_ast(final_sep_res.value.ast);
    }

    return make_success(head);
}

static ParseResult chainl1_fn(input_t * in, void * args) {
    chainl1_args* cargs = (chainl1_args*)args;

    ParseResult res = parse(in, cargs->p);
    if (!res.is_success) {
        return res;
    }
    ast_t* left = res.value.ast;

    while (1) {
        InputState state; save_input_state(in, &state);
        ParseResult op_res = parse(in, cargs->op);
        if (!op_res.is_success) {
            restore_input_state(in, &state);
            break;
        }
        tag_t op_tag = op_res.value.ast->typ;
        free_ast(op_res.value.ast);

        ParseResult right_res = parse(in, cargs->p);
        if (!right_res.is_success) {
            restore_input_state(in, &state);
            free_ast(left);
            free(op_res.value.error.message);
            return make_failure(in, strdup("Expected operand after operator in chainl1"));
        }
        ast_t* right = right_res.value.ast;
        left = ast2(op_tag, left, right);
    }

    return make_success(left);
}

static ParseResult succeed_fn(input_t * in, void * args) {
    succeed_args* sargs = (succeed_args*)args;
    return make_success(copy_ast(sargs->ast));
}

static ParseResult gseq_fn(input_t * in, void * args) {
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ast_t * head = NULL, * tail = NULL;
    while (seq != NULL) {
        ParseResult res = parse(in, seq->comb);
        if (!res.is_success) return res;
        if (res.value.ast != ast_nil) {
            if (head == NULL) head = tail = res.value.ast;
            else { tail->next = res.value.ast; while(tail->next) tail = tail->next; }
        }
        seq = seq->next;
    }
    return make_success(sa->typ == T_NONE ? head : ast1(sa->typ, head));
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
        InputState state; save_input_state(in, &state);
        res = parse(in, seq->comb);
        if (res.is_success) {
           if (sa->typ != T_NONE) res.value.ast = ast1(sa->typ, res.value.ast);
           return res;
        }
        restore_input_state(in, &state);
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

combinator_t * pnot(combinator_t* p) {
    not_args* args = (not_args*)safe_malloc(sizeof(not_args));
    args->p = p;
    combinator_t * comb = new_combinator();
    comb->type = COMB_NOT; comb->fn = pnot_fn; comb->args = (void *) args; return comb;
}

combinator_t * peek(combinator_t* p) {
    peek_args* args = (peek_args*)safe_malloc(sizeof(peek_args));
    args->p = p;
    combinator_t * comb = new_combinator();
    comb->type = COMB_PEEK; comb->fn = peek_fn; comb->args = (void *) args; return comb;
}

combinator_t * gseq(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
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
    ret->type = COMB_GSEQ; ret->args = (void *) args; ret->fn = gseq_fn; return ret;
}

combinator_t * between(combinator_t* open, combinator_t* close, combinator_t* p) {
    between_args* args = (between_args*)safe_malloc(sizeof(between_args));
    args->open = open;
    args->close = close;
    args->p = p;
    combinator_t * comb = new_combinator();
    comb->type = COMB_BETWEEN;
    comb->fn = between_fn;
    comb->args = (void *) args;
    return comb;
}

combinator_t * sep_by(combinator_t* p, combinator_t* sep) {
    sep_by_args* args = (sep_by_args*)safe_malloc(sizeof(sep_by_args));
    args->p = p;
    args->sep = sep;
    combinator_t * comb = new_combinator();
    comb->type = COMB_SEP_BY;
    comb->fn = sep_by_fn;
    comb->args = (void *) args;
    return comb;
}

combinator_t * sep_end_by(combinator_t* p, combinator_t* sep) {
    sep_end_by_args* args = (sep_end_by_args*)safe_malloc(sizeof(sep_end_by_args));
    args->p = p;
    args->sep = sep;
    combinator_t * comb = new_combinator();
    comb->type = COMB_SEP_END_BY;
    comb->fn = sep_end_by_fn;
    comb->args = (void *) args;
    return comb;
}

combinator_t * chainl1(combinator_t* p, combinator_t* op) {
    chainl1_args* args = (chainl1_args*)safe_malloc(sizeof(chainl1_args));
    args->p = p;
    args->op = op;
    combinator_t * comb = new_combinator();
    comb->type = COMB_CHAINL1;
    comb->fn = chainl1_fn;
    comb->args = (void *) args;
    return comb;
}

combinator_t * succeed(ast_t* ast) {
    succeed_args* args = (succeed_args*)safe_malloc(sizeof(succeed_args));
    args->ast = ast;
    combinator_t * comb = new_combinator();
    comb->type = COMB_SUCCEED;
    comb->fn = succeed_fn;
    comb->args = (void *) args;
    return comb;
}
