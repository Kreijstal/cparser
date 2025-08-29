#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H

#include "../../parser.h"
#include "../../combinators.h"

// --- Public parser constructors ---
combinator_t* p_program();
combinator_t* p_asm_block();
combinator_t* p_identifier_list();
combinator_t* p_expression();
combinator_t* p_declarations();

#endif // PASCAL_PARSER_H
