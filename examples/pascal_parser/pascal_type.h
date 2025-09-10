#ifndef PASCAL_TYPE_H
#define PASCAL_TYPE_H

#include "parser.h"
#include "combinators.h"

combinator_t* range_type(tag_t tag);
combinator_t* array_type(tag_t tag);
combinator_t* class_type(tag_t tag);
combinator_t* type_name(tag_t tag);
combinator_t* pointer_type(tag_t tag);
combinator_t* record_type(tag_t tag);

#endif // PASCAL_TYPE_H
