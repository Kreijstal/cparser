#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H

#include "../../parser.h"
#include "../../combinators.h"

// --- Custom Tags for Pascal ---
typedef enum {
    PASCAL_T_NONE, PASCAL_T_IDENT, PASCAL_T_PROGRAM, PASCAL_T_ASM_BLOCK, PASCAL_T_IDENT_LIST,
    PASCAL_T_INT_NUM, PASCAL_T_REAL_NUM,
    PASCAL_T_ADD, PASCAL_T_SUB, PASCAL_T_MUL, PASCAL_T_DIV, PASCAL_T_MOD,
    PASCAL_T_ASSIGN, PASCAL_T_PROC_CALL, PASCAL_T_STRING,
    PASCAL_T_IF, PASCAL_T_GT, PASCAL_T_LT, PASCAL_T_EQ,
    PASCAL_T_VAR_DECL, PASCAL_T_TYPE_INTEGER, PASCAL_T_TYPE_REAL,
    PASCAL_T_FOR, PASCAL_T_FOR_TO, PASCAL_T_FOR_DOWNTO
} pascal_tag_t;

// --- Public parser constructors ---
combinator_t* p_program();
combinator_t* p_asm_block();
combinator_t* p_identifier_list();
combinator_t* p_expression();
combinator_t* p_declarations();
combinator_t* p_statement();
combinator_t* p_if_statement();
combinator_t* p_for_statement();

#endif // PASCAL_PARSER_H
