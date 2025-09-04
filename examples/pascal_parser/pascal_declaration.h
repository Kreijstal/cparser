#ifndef PASCAL_DECLARATION_H
#define PASCAL_DECLARATION_H

#include "parser.h"
#include "combinators.h"

void init_pascal_procedure_parser(combinator_t** p);
void init_pascal_method_implementation_parser(combinator_t** p);
void init_pascal_complete_program_parser(combinator_t** p);

#endif // PASCAL_DECLARATION_H
