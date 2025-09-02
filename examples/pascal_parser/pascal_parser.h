#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H

#include "parser.h"
#include "combinators.h"

// --- Custom Tags for Pascal Expressions ---
typedef enum {
    PASCAL_T_NONE,
    PASCAL_T_INTEGER,
    PASCAL_T_REAL,
    PASCAL_T_IDENTIFIER,
    PASCAL_T_STRING,
    PASCAL_T_BOOLEAN,
    PASCAL_T_ADD,
    PASCAL_T_SUB,
    PASCAL_T_MUL,
    PASCAL_T_DIV,
    PASCAL_T_INTDIV,
    PASCAL_T_MOD,
    PASCAL_T_NEG,
    PASCAL_T_EQ,
    PASCAL_T_NE,
    PASCAL_T_LT,
    PASCAL_T_GT,
    PASCAL_T_LE,
    PASCAL_T_GE,
    PASCAL_T_FUNC_CALL,
    PASCAL_T_ARG_LIST
} pascal_tag_t;

// --- Function Declarations ---
void init_pascal_expression_parser(combinator_t** p);
void print_pascal_ast(ast_t* ast);
const char* pascal_tag_to_string(tag_t tag);

#endif // PASCAL_PARSER_H
