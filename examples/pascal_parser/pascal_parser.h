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
    PASCAL_T_CHAR,
    PASCAL_T_BOOLEAN,
    PASCAL_T_ADD,
    PASCAL_T_SUB,
    PASCAL_T_MUL,
    PASCAL_T_DIV,
    PASCAL_T_INTDIV,
    PASCAL_T_MOD,
    PASCAL_T_NEG,
    PASCAL_T_POS,
    PASCAL_T_EQ,
    PASCAL_T_NE,
    PASCAL_T_LT,
    PASCAL_T_GT,
    PASCAL_T_LE,
    PASCAL_T_GE,
    PASCAL_T_AND,
    PASCAL_T_OR,
    PASCAL_T_NOT,
    PASCAL_T_XOR,
    PASCAL_T_SHL,
    PASCAL_T_SHR,
    PASCAL_T_ADDR,
    PASCAL_T_RANGE,
    PASCAL_T_SET,
    PASCAL_T_IN,
    PASCAL_T_SET_UNION,
    PASCAL_T_SET_INTERSECT,
    PASCAL_T_SET_DIFF,
    PASCAL_T_SET_SYM_DIFF,
    PASCAL_T_IS,
    PASCAL_T_AS,
    PASCAL_T_TYPECAST,
    PASCAL_T_FUNC_CALL,
    PASCAL_T_ARG_LIST,
    // Statement types
    PASCAL_T_ASSIGNMENT,
    PASCAL_T_STATEMENT,
    PASCAL_T_STATEMENT_LIST,
    PASCAL_T_IF_STMT,
    PASCAL_T_THEN,
    PASCAL_T_ELSE,
    PASCAL_T_BEGIN_BLOCK,
    PASCAL_T_END_BLOCK,
    PASCAL_T_FOR_STMT,
    PASCAL_T_WHILE_STMT,
    PASCAL_T_DO,
    PASCAL_T_TO,
    PASCAL_T_DOWNTO
} pascal_tag_t;

// --- Function Declarations ---
void init_pascal_expression_parser(combinator_t** p);
void init_pascal_statement_parser(combinator_t** p);
void print_pascal_ast(ast_t* ast);
const char* pascal_tag_to_string(tag_t tag);

#endif // PASCAL_PARSER_H
