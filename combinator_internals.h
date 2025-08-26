#ifndef COMBINATOR_INTERNALS_H
#define COMBINATOR_INTERNALS_H

#include "parser.h"

// --- Argument Structs for Combinators ---

typedef struct {
    combinator_t * comb;
    char * msg;
} expect_args;

typedef struct seq_list {
    combinator_t * comb;
    struct seq_list * next;
} seq_list;

typedef struct {
    tag_t typ;
    seq_list * list;
} seq_args;

typedef struct {
    combinator_t * parser;
    flatMap_func func;
} flatMap_args;

typedef struct {
    combinator_t* p1;
    combinator_t* p2;
} pair_args;

typedef struct {
    combinator_t* p;
    combinator_t* sep;
} sep_by_args;

#endif // COMBINATOR_INTERNALS_H
