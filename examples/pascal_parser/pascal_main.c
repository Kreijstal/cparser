#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pascal_parser.h"

// Forward declaration
static void print_ast_indented(ast_t* ast, int depth);
static void print_error_with_partial_ast(ParseError* error);

// Helper function to print ParseError with partial AST
static void print_error_with_partial_ast(ParseError* error) {
    if (error == NULL) return;

    printf("Error at line %d, col %d: %s\n", error->line, error->col, error->message);

    if (error->partial_ast != NULL) {
        printf("Partial AST:\n");
        print_ast_indented(error->partial_ast, 1);
    }
}

// Helper function to print AST with indentation
static void print_ast_indented(ast_t* ast, int depth) {
    if (ast == NULL || ast == ast_nil) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("(%s", pascal_tag_to_string(ast->typ));
    if (ast->sym) printf(" %s", ast->sym->name);

    ast_t* child = ast->child;
    if (child) {
        printf("\n");
        print_ast_indented(child, depth + 1);
    }
    printf(")");

    if (ast->next) {
        printf("\n");
        print_ast_indented(ast->next, depth);
    }
}


int main(int argc, char *argv[]) {
    bool print_ast = false;
    char *expr_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-ast") == 0) {
            print_ast = true;
        } else {
            expr_str = argv[i];
        }
    }

    if (expr_str == NULL) {
        fprintf(stderr, "Usage: %s [--print-ast] \"<expression>\"\n", argv[0]);
        return 1;
    }

    combinator_t *parser = new_combinator();
    init_pascal_expression_parser(&parser);

    input_t *in = new_input();
    in->buffer = expr_str;
    in->length = strlen(expr_str);
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    ParseResult result = parse(in, parser);

    if (result.is_success) {
        if (in->start < in->length) {
            fprintf(stderr, "Error: Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
            return 1;
        }
        if (print_ast) {
            print_pascal_ast(result.value.ast);
        }
        free_ast(result.value.ast);
    } else {
        print_error_with_partial_ast(result.value.error);
        free_error(result.value.error);
        return 1;
    }

    free_combinator(parser);
    free(in);
    free(ast_nil);

    return 0;
}
