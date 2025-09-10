#include "pascal_statement.h"
#include "pascal_parser.h"
#include "pascal_expression.h"
#include "pascal_keywords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ASM block body parser - uses proper until() combinator instead of manual scanning
static combinator_t* asm_body(tag_t tag) {
    return until(match("end"), tag);  // Use raw match instead of token to preserve whitespace
}

// --- Pascal Statement Parser Implementation ---
void init_pascal_statement_parser(combinator_t** p) {
    // First create the expression parser to use within statements
    combinator_t** expr_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *expr_parser = new_combinator();
    (*expr_parser)->extra_to_free = expr_parser;
    init_pascal_expression_parser(expr_parser);

    // Create the main statement parser pointer for recursive references
    combinator_t** stmt_parser = p;

    // Left-value parser: accepts identifiers, member access expressions, and array access
    combinator_t* simple_identifier = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));
    combinator_t* member_access_lval = seq(new_combinator(), PASCAL_T_MEMBER_ACCESS,
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // object name
        token(match(".")),                                            // dot
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // field/property name
        NULL
    );
    // Array access for lvalue: identifier[index, ...]
    combinator_t* array_access_lval = seq(new_combinator(), PASCAL_T_ARRAY_ACCESS,
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // array name
        between(token(match("[")), token(match("]")),
            sep_by(lazy(expr_parser), token(match(",")))),             // indices
        NULL
    );
    combinator_t* lvalue = multi(new_combinator(), PASCAL_T_NONE,
        array_access_lval,       // try array access first
        member_access_lval,      // try member access second
        simple_identifier,       // then simple identifier
        NULL
    );

    // Assignment statement: lvalue := expression (no semicolon here)
    combinator_t* assignment = seq(new_combinator(), PASCAL_T_ASSIGNMENT,
        lvalue,                                // left-hand side (identifier or member access)
        token(match(":=")),                    // assignment operator
        lazy(expr_parser),                     // expression
        NULL
    );

    // Simple expression statement: expression (no semicolon here)
    combinator_t* expr_stmt = seq(new_combinator(), PASCAL_T_STATEMENT,
        lazy(expr_parser),                     // expression
        NULL
    );

    // Begin-end block: begin [statement_list] end
    // Handle empty begin-end blocks explicitly to avoid recursive parsing issues
    combinator_t* empty_begin_end = seq(new_combinator(), PASCAL_T_BEGIN_BLOCK,
        token(keyword_ci("begin")),              // begin keyword
        token(keyword_ci("end")),                // end keyword (immediately after begin)
        NULL
    );

    // Standard BEGIN-END block for main statement parser (uses full recursive parsing)
    // Create custom statement list parser to properly handle Pascal semicolon rules
    // Pascal semicolons are separators, but there can be an optional trailing semicolon

    // Simplified statement list parser - just use sep_by with optional trailing semicolon
    combinator_t* stmt_list = seq(new_combinator(), PASCAL_T_NONE,
        sep_by(lazy(stmt_parser), token(match(";"))),     // statements separated by semicolons
        optional(token(match(";"))),                      // optional trailing semicolon
        NULL
    );

    combinator_t* non_empty_begin_end = seq(new_combinator(), PASCAL_T_BEGIN_BLOCK,
        token(keyword_ci("begin")),              // begin keyword
        stmt_list,                             // statements each terminated by semicolon
        token(keyword_ci("end")),                // end keyword
        NULL
    );

    // Try empty block first, then non-empty
    combinator_t* begin_end_block = multi(new_combinator(), PASCAL_T_NONE,
        empty_begin_end,                       // try empty block first
        non_empty_begin_end,                   // then try non-empty block
        NULL
    );

    // If statement: if expression then statement [else statement]
    combinator_t* if_stmt = seq(new_combinator(), PASCAL_T_IF_STMT,
        token(keyword_ci("if")),                     // if keyword (case-insensitive)
        lazy(expr_parser),                         // condition
        token(keyword_ci("then")),                   // then keyword (case-insensitive)
        lazy(stmt_parser),                         // then statement
        optional(seq(new_combinator(), PASCAL_T_ELSE,    // optional else part
            token(keyword_ci("else")),               // else keyword (case-insensitive)
            lazy(stmt_parser),
            NULL
        )),
        NULL
    );

    // For statement: for identifier := expression (to|downto) expression do statement
    combinator_t* for_direction = multi(new_combinator(), PASCAL_T_NONE,
        token(keyword_ci("to")),                 // to keyword (case-insensitive)
        token(keyword_ci("downto")),             // downto keyword (case-insensitive)
        NULL
    );
    combinator_t* for_stmt = seq(new_combinator(), PASCAL_T_FOR_STMT,
        token(keyword_ci("for")),                // for keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),    // loop variable
        token(match(":=")),                    // assignment
        lazy(expr_parser),                     // start expression
        for_direction,                         // to or downto
        lazy(expr_parser),                     // end expression
        token(keyword_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // loop body statement
        NULL
    );

    // While statement: while expression do statement
    combinator_t* while_stmt = seq(new_combinator(), PASCAL_T_WHILE_STMT,
        token(keyword_ci("while")),              // while keyword (case-insensitive)
        lazy(expr_parser),                     // condition
        token(keyword_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // body statement
        NULL
    );

    // With statement: with expression do statement
    combinator_t* with_stmt = seq(new_combinator(), PASCAL_T_WITH_STMT,
        token(keyword_ci("with")),               // with keyword (case-insensitive)
        lazy(expr_parser),                     // expression
        token(keyword_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // body statement
        NULL
    );

    // ASM block: asm ... end
    combinator_t* asm_stmt = seq(new_combinator(), PASCAL_T_ASM_BLOCK,
        token(match("asm")),                   // asm keyword
        asm_body(PASCAL_T_NONE),               // asm body content
        token(match("end")),                   // end keyword
        NULL
    );

    // Try-finally block: try statements finally statements end
    // Use more lenient statement parsing that doesn't require semicolons
    combinator_t* try_finally = seq(new_combinator(), PASCAL_T_TRY_BLOCK,
        token(keyword_ci("try")),              // try keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in try block
        token(keyword_ci("finally")),          // finally keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in finally block
        token(keyword_ci("end")),              // end keyword (case-insensitive)
        NULL
    );

    // Try-except block: try statements except statements end
    combinator_t* try_except = seq(new_combinator(), PASCAL_T_TRY_BLOCK,
        token(keyword_ci("try")),              // try keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in try block
        token(keyword_ci("except")),           // except keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in except block
        token(keyword_ci("end")),              // end keyword (case-insensitive)
        NULL
    );

    // Raise statement: raise [expression]
    combinator_t* raise_stmt = seq(new_combinator(), PASCAL_T_RAISE_STMT,
        token(keyword_ci("raise")),            // raise keyword (case-insensitive)
        optional(lazy(expr_parser)),           // optional exception expression
        NULL
    );

    // Inherited statement: inherited [method_call]
    combinator_t* inherited_stmt = seq(new_combinator(), PASCAL_T_INHERITED_STMT,
        token(keyword_ci("inherited")),        // inherited keyword (case-insensitive)
        optional(lazy(expr_parser)),           // optional method call expression (e.g., inherited Destroy)
        NULL
    );

    // Exit statement: exit
    combinator_t* exit_stmt = token(create_keyword_parser("exit", PASCAL_T_EXIT_STMT));

    // Case statement: case expression of case_item; case_item; ... [else statement] end
    // Simplified version first
    combinator_t* case_stmt = seq(new_combinator(), PASCAL_T_CASE_STMT,
        token(keyword_ci("case")),                 // case keyword
        token(cident(PASCAL_T_IDENTIFIER)),        // selector (just identifier for now)
        token(keyword_ci("of")),                   // of keyword
        token(integer(PASCAL_T_INTEGER)),          // simple case value
        token(match(":")),                         // colon
        token(cident(PASCAL_T_IDENTIFIER)),        // target identifier
        token(match(":=")),                        // assignment
        token(integer(PASCAL_T_INTEGER)),          // value
        token(keyword_ci("end")),                  // end keyword
        NULL
    );

    // Main statement parser: try different types of statements (order matters!)
    // Note: VAR sections are handled by the complete program parser context
    multi(*stmt_parser, PASCAL_T_NONE,
        case_stmt,                            // case statements (before assignment/expr)
        //begin_end_block,                      // compound statements (must come before expr_stmt)
        //try_finally,                          // try-finally blocks
        //try_except,                           // try-except blocks
        //raise_stmt,                           // raise statements
        //inherited_stmt,                       // inherited statements
        //exit_stmt,                            // exit statements
        //asm_stmt,                             // inline assembly blocks
        //if_stmt,                              // if statements
        //for_stmt,                             // for statements
        //while_stmt,                           // while statements
        //with_stmt,                            // with statements
        //assignment,                            // assignment statements
        //expr_stmt,                            // expression statements (must be last)
        NULL
    );
}
