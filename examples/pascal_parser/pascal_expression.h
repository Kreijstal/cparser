#ifndef PASCAL_EXPRESSION_H
#define PASCAL_EXPRESSION_H

#include "parser.h"
#include "combinators.h"

void init_pascal_expression_parser(combinator_t** p);
ParseResult parse_pascal_expression(input_t* input, combinator_t* parser);

#endif // PASCAL_EXPRESSION_H
