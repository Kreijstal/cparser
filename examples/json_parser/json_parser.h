#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "parser.h"

// Returns a combinator that can parse a complete JSON value.
combinator_t* json_parser();

#endif // JSON_PARSER_H
