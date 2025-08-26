#ifndef COMBINATORS_H
#define COMBINATORS_H

#include "parser.h"

//=============================================================================
// Combinator Constructors
//=============================================================================

combinator_t * expect(combinator_t * c, char * msg);
combinator_t * seq(combinator_t * ret, tag_t typ, combinator_t * c1, ...);
combinator_t * multi(combinator_t * ret, tag_t typ, combinator_t * c1, ...);
combinator_t * flatMap(combinator_t * p, flatMap_func func);
combinator_t * left(combinator_t* p1, combinator_t* p2);
combinator_t * right(combinator_t* p1, combinator_t* p2);

#endif // COMBINATORS_H
