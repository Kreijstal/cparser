#ifndef FPC_PARSER_H
#define FPC_PARSER_H

#include "../../parser.h"
#include "../../combinators.h"

// Helper to wrap a parser with whitespace consumption.
combinator_t* token(combinator_t* p);

// Case-insensitive keyword parser.
combinator_t* keyword(const char* s);

// --- Primitive Parsers ---
combinator_t* p_ident();
combinator_t* p_program_kw();
combinator_t* p_program();
combinator_t* p_begin_kw();
combinator_t* p_end_kw();
combinator_t* p_semicolon();
combinator_t* p_dot();

#endif // FPC_PARSER_H
