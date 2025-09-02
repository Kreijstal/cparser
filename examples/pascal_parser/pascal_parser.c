#include "pascal_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, PASCAL_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, PASCAL_T_NONE))));
}

// --- AST Printing ---
const char* pascal_tag_to_string(tag_t tag) {
    switch (tag) {
        case PASCAL_T_NONE: return "NONE";
        case PASCAL_T_INTEGER: return "INTEGER";
        case PASCAL_T_REAL: return "REAL";
        case PASCAL_T_IDENTIFIER: return "IDENTIFIER";
        case PASCAL_T_STRING: return "STRING";
        case PASCAL_T_BOOLEAN: return "BOOLEAN";
        case PASCAL_T_ADD: return "ADD";
        case PASCAL_T_SUB: return "SUB";
        case PASCAL_T_MUL: return "MUL";
        case PASCAL_T_DIV: return "DIV";
        case PASCAL_T_INTDIV: return "INTDIV";
        case PASCAL_T_MOD: return "MOD";
        case PASCAL_T_NEG: return "NEG";
        case PASCAL_T_EQ: return "EQ";
        case PASCAL_T_NE: return "NE";
        case PASCAL_T_LT: return "LT";
        case PASCAL_T_GT: return "GT";
        case PASCAL_T_LE: return "LE";
        case PASCAL_T_GE: return "GE";
        case PASCAL_T_FUNC_CALL: return "FUNC_CALL";
        case PASCAL_T_ARG_LIST: return "ARG_LIST";
        default: return "UNKNOWN";
    }
}

static void print_ast_recursive(ast_t* ast, int depth) {
    if (ast == NULL || ast == ast_nil) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("(%s", pascal_tag_to_string(ast->typ));
    if (ast->sym) printf(" %s", ast->sym->name);

    ast_t* child = ast->child;
    if (child) {
        printf("\n");
        print_ast_recursive(child, depth + 1);
    }
    printf(")");

    if (ast->next) {
        printf("\n");
        print_ast_recursive(ast->next, depth);
    }
}

void print_pascal_ast(ast_t* ast) {
    print_ast_recursive(ast, 0);
    printf("\n");
}

// --- Parser Definition ---
void init_pascal_expression_parser(combinator_t** p) {
    // Function call parser: identifier followed by optional argument list
    combinator_t* arg_list = between(
        token(match("(")),
        token(match(")")),
        optional(sep_by(lazy(p), token(match(","))))
    );
    
    combinator_t* func_call = seq(new_combinator(), PASCAL_T_FUNC_CALL,
        token(cident(PASCAL_T_IDENTIFIER)),
        arg_list,
        NULL
    );
    
    combinator_t *factor = multi(new_combinator(), PASCAL_T_NONE,
        token(integer(PASCAL_T_INTEGER)),
        token(string(PASCAL_T_STRING)),
        func_call,
        token(cident(PASCAL_T_IDENTIFIER)),
        between(token(match("(")), token(match(")")), lazy(p)),
        NULL
    );

    expr(*p, factor);
    
    // Precedence 0: Addition and Subtraction (includes string concatenation)
    expr_insert(*p, 0, PASCAL_T_ADD, EXPR_INFIX, ASSOC_LEFT, token(match("+")));
    expr_altern(*p, 0, PASCAL_T_SUB, token(match("-")));
    
    // Precedence 1: Multiplication, Division, and Modulo
    expr_insert(*p, 1, PASCAL_T_MUL, EXPR_INFIX, ASSOC_LEFT, token(match("*")));
    expr_altern(*p, 1, PASCAL_T_DIV, token(match("/")));
    expr_altern(*p, 1, PASCAL_T_INTDIV, token(match("div")));
    expr_altern(*p, 1, PASCAL_T_MOD, token(match("mod")));
    expr_altern(*p, 1, PASCAL_T_MOD, token(match("%")));
    
    // Precedence 2: Unary minus (highest precedence)
    expr_insert(*p, 2, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
}
