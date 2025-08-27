#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "combinators.h"


// Wrapper for isspace to match the required function signature for satisfy.
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

// Helper to wrap a parser with whitespace consumption on both sides.
static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char));
    return right(ws, left(p, many(satisfy(is_whitespace_char))));
}

// --- Evaluation ---
// Recursively walks the AST to compute the final numerical value.
long eval(ast_t *ast) {
    if (!ast) {
        fprintf(stderr, "Error: trying to evaluate a NULL AST node.\n");
        return 0;
    }

    switch (ast->typ) {
        case T_INT:
            return atol(ast->sym->name);
        case T_ADD:
            return eval(ast->child) + eval(ast->child->next);
        case T_SUB:
            return eval(ast->child) - eval(ast->child->next);
        case T_MUL:
            return eval(ast->child) * eval(ast->child->next);
        case T_DIV: {
            long divisor = eval(ast->child->next);
            if (divisor == 0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                exit(1);
            }
            return eval(ast->child) / divisor;
        }
        case T_NEG:
            return -eval(ast->child);
        default:
            fprintf(stderr, "Runtime Error: Unknown AST node type: %d\n", ast->typ);
            return 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"<expression>\"\n", argv[0]);
        return 1;
    }

    // --- Parser Definition ---
    combinator_t *expr_parser = new_combinator();

    // The base of the expression grammar. It can be a raw integer
    // or a full expression recursively wrapped in parentheses.
    // We use the new `lazy` combinator to break the infinite recursion.
    combinator_t *factor = multi(new_combinator(), T_NONE,
        token(integer()),
        between(token(match("(")), token(match(")")), lazy(&expr_parser)),
        NULL
    );

    // Initialize the expression parser, with `factor` as the base.
    expr(expr_parser, factor);

    // Operator precedence levels (lower number = lower precedence)
    expr_insert(expr_parser, 0, T_ADD, EXPR_INFIX, ASSOC_LEFT, token(match("+")));
    expr_altern(expr_parser, 0, T_SUB, token(match("-")));
    expr_insert(expr_parser, 1, T_MUL, EXPR_INFIX, ASSOC_LEFT, token(match("*")));
    expr_altern(expr_parser, 1, T_DIV, token(match("/")));
    expr_insert(expr_parser, 2, T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));

    // --- Parsing ---
    input_t *in = new_input();
    in->buffer = argv[1];
    in->length = strlen(argv[1]);

    ast_nil = new_ast();
    ast_nil->typ = T_NONE;

    ParseResult result = parse(in, expr_parser);

    // --- Output ---
    if (result.is_success) {
        if (in->start < in->length) {
            fprintf(stderr, "Error: Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
        } else {
            long final_result = eval(result.value.ast);
            printf("%ld\n", final_result);
            free_ast(result.value.ast);
        }
    } else {
        fprintf(stderr, "Parsing Error at line %d, col %d: %s\n",
                result.value.error->line,
                result.value.error->col,
                result.value.error->message);
        free_error(result.value.error);
    }

    // --- Cleanup ---
    free_combinator(expr_parser);
    free(in);
    free(ast_nil); // free the global nil object

    return 0;
}