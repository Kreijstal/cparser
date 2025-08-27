#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "combinators.h"

// Forward declaration for AST printing
static void print_ast_recursive(ast_t *ast, int indent);

// Helper to convert a tag_t to its string representation
const char* tag_to_string(tag_t tag) {
    switch (tag) {
        case T_NONE:   return "NONE";
        case T_INT:    return "INT";
        case T_IDENT:  return "IDENT";
        case T_ASSIGN: return "ASSIGN";
        case T_SEQ:    return "SEQ";
        case T_ADD:    return "ADD";
        case T_SUB:    return "SUB";
        case T_MUL:    return "MUL";
        case T_DIV:    return "DIV";
        case T_NEG:    return "NEG";
        case T_STRING: return "STRING";
        default:       return "UNKNOWN";
    }
}

// Public function to initiate AST printing
void print_ast(ast_t *ast) {
    print_ast_recursive(ast, 0);
}

// Recursive function to print the AST with indentation
static void print_ast_recursive(ast_t *ast, int indent) {
    if (ast == NULL || ast == ast_nil) {
        return;
    }
    for (int i = 0; i < indent; ++i) printf("  ");
    printf("- %s", tag_to_string(ast->typ));
    if (ast->sym != NULL && ast->sym->name != NULL) {
        printf(" (%s)", ast->sym->name);
    }
    printf("\n");
    if (ast->child != NULL) print_ast_recursive(ast->child, indent + 1);
    if (ast->next != NULL) print_ast_recursive(ast->next, indent);
}

// Wrapper for isspace to match the required function signature for satisfy.
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

// Helper to wrap a parser with whitespace consumption on both sides.
static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char));
    return right(ws, left(p, many(satisfy(is_whitespace_char))));
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
            printf("Parsing successful. AST:\n");
            print_ast(result.value.ast);
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
    free_ast(ast_nil);

    return 0;
}