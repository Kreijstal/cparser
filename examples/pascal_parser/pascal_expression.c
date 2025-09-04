#include "pascal_expression.h"
#include "pascal_parser.h"
#include "pascal_keywords.h"
#include "pascal_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Post-process AST to handle semantic operations like set union
static void post_process_set_operations(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) return;

    // Process children first (depth-first)
    post_process_set_operations(ast->child);
    post_process_set_operations(ast->next);

    // Check if this is an ADD operation with two SET operands
    if (ast->typ == PASCAL_T_ADD && ast->child && ast->child->next) {
        ast_t* left = ast->child;
        ast_t* right = ast->child->next;

        if (left->typ == PASCAL_T_SET && right->typ == PASCAL_T_SET) {
            // Convert ADD to SET_UNION
            ast->typ = PASCAL_T_SET_UNION;
        }
    }
}

// --- Parser Definition ---
void init_pascal_expression_parser(combinator_t** p) {
    // Pascal identifier parser - use expression identifier that allows some keywords in expression contexts
    combinator_t* identifier = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));

    // Function name: use expression identifier parser that allows certain keywords as function names
    combinator_t* func_name = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));

    // Function call parser: function name followed by optional argument list
    combinator_t* arg_list = between(
        token(match("(")),
        token(match(")")),
        optional(sep_by(lazy(p), token(match(","))))
    );

    combinator_t* func_call = seq(new_combinator(), PASCAL_T_FUNC_CALL,
        func_name,                        // function name (built-in or custom)
        arg_list,
        NULL
    );

    // Array access parser: identifier[index1, index2, ...]
    combinator_t* index_list = between(
        token(match("[")),
        token(match("]")),
        sep_by(lazy(p), token(match(",")))
    );

    combinator_t* array_access = seq(new_combinator(), PASCAL_T_ARRAY_ACCESS,
        func_name,                        // array name (built-in or custom identifier)
        index_list,
        NULL
    );

    // Type cast parser: TypeName(expression) - only for built-in types
    combinator_t* typecast = seq(new_combinator(), PASCAL_T_TYPECAST,
        token(type_name(PASCAL_T_IDENTIFIER)), // type name - only built-in types
        between(token(match("(")), token(match(")")), lazy(p)), // expression
        NULL
    );

    // Boolean literal parsers
    combinator_t* boolean_true = seq(new_combinator(), PASCAL_T_BOOLEAN,
        match_ci("true"),
        NULL
    );
    combinator_t* boolean_false = seq(new_combinator(), PASCAL_T_BOOLEAN,
        match_ci("false"),
        NULL
    );

    // Tuple constructor: (expr, expr, ...) - for nested array constants like ((1,2),(3,4))
    combinator_t* tuple = seq(new_combinator(), PASCAL_T_TUPLE,
        token(match("(")),
        sep_by(lazy(p), token(match(","))),  // comma-separated list of expressions
        token(match(")")),
        NULL
    );

    combinator_t *factor = multi(new_combinator(), PASCAL_T_NONE,
        token(real_number(PASCAL_T_REAL)),        // Real numbers (3.14) - try first
        token(integer(PASCAL_T_INTEGER)),         // Integers (123)
        token(char_literal(PASCAL_T_CHAR)),       // Characters ('A')
        token(pascal_string(PASCAL_T_STRING)),    // Strings ("hello" or 'hello')
        token(set_constructor(PASCAL_T_SET, p)),  // Set constructors [1, 2, 3]
        token(boolean_true),                      // Boolean true
        token(boolean_false),                     // Boolean false
        typecast,                                 // Type casts Integer(x) - try before func_call
        array_access,                             // Array access table[i,j] - try before func_call
        func_call,                                // Function calls func(x)
        between(token(match("(")), token(match(")")), lazy(p)), // Parenthesized expressions - try before tuple
        tuple,                                    // Tuple constants (a,b,c) - try after parenthesized expressions
        identifier,                               // Identifiers (variables, built-ins)
        NULL
    );

    expr(*p, factor);

    // Precedence levels (lower number = lower precedence, must be consecutive starting from 0)
    // Precedence 0: Boolean OR (lowest precedence)
    expr_insert(*p, 0, PASCAL_T_OR, EXPR_INFIX, ASSOC_LEFT, token(match("or")));

    // Precedence 1: Boolean XOR
    expr_insert(*p, 1, PASCAL_T_XOR, EXPR_INFIX, ASSOC_LEFT, token(match("xor")));

    // Precedence 2: Boolean AND
    expr_insert(*p, 2, PASCAL_T_AND, EXPR_INFIX, ASSOC_LEFT, token(match("and")));

    // Precedence 3: All relational operators - multi-char operators added last (tried first)
    // Single character operators
    expr_insert(*p, 3, PASCAL_T_EQ, EXPR_INFIX, ASSOC_LEFT, token(match("=")));
    expr_altern(*p, 3, PASCAL_T_LT, token(match("<")));
    expr_altern(*p, 3, PASCAL_T_GT, token(match(">")));
    expr_altern(*p, 3, PASCAL_T_IN, token(keyword_ci("in")));
    expr_altern(*p, 3, PASCAL_T_IS, token(keyword_ci("is")));
    expr_altern(*p, 3, PASCAL_T_AS, token(keyword_ci("as")));
    // Multi-character operators (added last = tried first in expr parser)
    expr_altern(*p, 3, PASCAL_T_NE, token(match("<>")));
    expr_altern(*p, 3, PASCAL_T_GE, token(match(">=")));
    expr_altern(*p, 3, PASCAL_T_LE, token(match("<=")));

    // Multi-character operators (added last = tried first in expr parser)
    expr_altern(*p, 3, PASCAL_T_NE, token(match("<>")));
    expr_altern(*p, 3, PASCAL_T_GE, token(match(">=")));
    expr_altern(*p, 3, PASCAL_T_LE, token(match("<=")));

    // Precedence 4: Range operator (..)
    expr_insert(*p, 4, PASCAL_T_RANGE, EXPR_INFIX, ASSOC_LEFT, token(match("..")));

    // Precedence 5: Addition and Subtraction (includes string concatenation and set operations)
    expr_insert(*p, 5, PASCAL_T_ADD, EXPR_INFIX, ASSOC_LEFT, token(match("+")));
    expr_altern(*p, 5, PASCAL_T_SUB, token(match("-")));

    // Precedence 6: Multiplication, Division, Modulo, and Bitwise shifts
    expr_insert(*p, 6, PASCAL_T_MUL, EXPR_INFIX, ASSOC_LEFT, token(match("*")));
    expr_altern(*p, 6, PASCAL_T_DIV, token(match("/")));
    expr_altern(*p, 6, PASCAL_T_INTDIV, token(keyword_ci("div")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(keyword_ci("mod")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(match("%")));
    expr_altern(*p, 6, PASCAL_T_SHL, token(keyword_ci("shl")));
    expr_altern(*p, 6, PASCAL_T_SHR, token(keyword_ci("shr")));

    // Precedence 7: Unary operators
    expr_insert(*p, 7, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
    expr_insert(*p, 7, PASCAL_T_POS, EXPR_PREFIX, ASSOC_NONE, token(match("+")));
    expr_insert(*p, 7, PASCAL_T_NOT, EXPR_PREFIX, ASSOC_NONE, token(match("not")));
    expr_insert(*p, 7, PASCAL_T_ADDR, EXPR_PREFIX, ASSOC_NONE, token(match("@")));

    // Field width operator for formatted output: expression:width (same precedence as unary)
    expr_insert(*p, 7, PASCAL_T_FIELD_WIDTH, EXPR_INFIX, ASSOC_LEFT, token(match(":")));

    // Precedence 8: Member access (highest precedence) - but not if followed by another dot
    combinator_t* member_access_op = seq(new_combinator(), PASCAL_T_NONE,
        match("."),
        pnot(match(".")),  // not followed by another dot
        NULL
    );
    expr_insert(*p, 8, PASCAL_T_MEMBER_ACCESS, EXPR_INFIX, ASSOC_LEFT, token(member_access_op));

    // Range operator (..) - SECOND insertion to override member access priority
    // (Multi-character operators added last = tried first in expr parser)
    expr_altern(*p, 4, PASCAL_T_RANGE, token(match("..")));
}

// --- Utility Functions ---
ParseResult parse_pascal_expression(input_t* input, combinator_t* parser) {
    ParseResult result = parse(input, parser);
    if (result.is_success) {
        post_process_set_operations(result.value.ast);
    }
    return result;
}
