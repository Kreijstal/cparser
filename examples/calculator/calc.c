#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "combinators.h"

// --- Custom Tags for Calculator ---
typedef enum {
    CALC_T_NONE, CALC_T_INT, CALC_T_ADD, CALC_T_SUB, CALC_T_MUL, CALC_T_DIV, CALC_T_NEG
} calc_tag_t;

// --- Forward declarations for local functions ---
void print_calculator_ast(ast_t* ast);
void count_nodes_visitor(ast_t* node, void* context);
static void print_error_with_partial_ast(ParseError* error);
static void print_ast_indented(ast_t* ast, int depth);
void init_calculator_parser(combinator_t** p);

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, CALC_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, CALC_T_NONE))));
}

// --- Evaluation ---
long eval(ast_t *ast) {
    if (!ast) {
        fprintf(stderr, "Error: trying to evaluate a NULL AST node.\n");
        return 0;
    }
    switch (ast->typ) {
        case CALC_T_INT: return atol(ast->sym->name);
        case CALC_T_ADD: return eval(ast->child) + eval(ast->child->next);
        case CALC_T_SUB: return eval(ast->child) - eval(ast->child->next);
        case CALC_T_MUL: return eval(ast->child) * eval(ast->child->next);
        case CALC_T_DIV: {
            long divisor = eval(ast->child->next);
            if (divisor == 0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                exit(1);
            }
            return eval(ast->child) / divisor;
        }
        case CALC_T_NEG: return -eval(ast->child);
        default:
            fprintf(stderr, "Runtime Error: Unknown AST node type: %d\n", ast->typ);
            return 0;
    }
}

// --- AST Printing ---
static const char* calc_tag_to_string(tag_t tag) {
    switch (tag) {
        case CALC_T_NONE: return "NONE";
        case CALC_T_INT: return "INT";
        case CALC_T_ADD: return "ADD";
        case CALC_T_SUB: return "SUB";
        case CALC_T_MUL: return "MUL";
        case CALC_T_DIV: return "DIV";
        case CALC_T_NEG: return "NEG";
        default: return "UNKNOWN";
    }
}

void print_calculator_ast(ast_t* ast) {
    print_ast_indented(ast, 0);
    printf("\n");
}

// --- Error Printing with Partial AST ---
static void print_error_with_partial_ast(ParseError* error) {
    if (error == NULL) return;

    printf("Error at line %d, col %d: %s\n", error->line, error->col, error->message);

    if (error->partial_ast != NULL) {
        printf("Partial AST:\n");
        print_ast_indented(error->partial_ast, 1);
    }
}

static void print_ast_indented(ast_t* ast, int depth) {
    if (ast == NULL || ast == ast_nil) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("(%s", calc_tag_to_string(ast->typ));
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


// --- Visitor for counting nodes ---
void count_nodes_visitor(ast_t* node, void* context) {
    int* counter = (int*)context;
    (*counter)++;
}

// --- Parser Definition ---
void init_calculator_parser(combinator_t** p) {
    combinator_t *factor = multi(new_combinator(), CALC_T_NONE,
        token(integer(CALC_T_INT)),
        between(token(match("(")), token(match(")")), lazy(p)),
        NULL
    );
    expr(*p, factor);
    expr_insert(*p, 0, CALC_T_ADD, EXPR_INFIX, ASSOC_LEFT, token(match("+")));
    expr_altern(*p, 0, CALC_T_SUB, token(match("-")));
    expr_insert(*p, 1, CALC_T_MUL, EXPR_INFIX, ASSOC_LEFT, token(match("*")));
    expr_altern(*p, 1, CALC_T_DIV, token(match("/")));
    expr_insert(*p, 2, CALC_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
}

// --- Main ---
int main(int argc, char *argv[]) {
    bool print_ast = false;
    bool count_nodes = false;
    char *expr_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-ast") == 0) {
            print_ast = true;
        } else if (strcmp(argv[i], "--count-nodes") == 0) {
            count_nodes = true;
        } else {
            expr_str = argv[i];
        }
    }

    if (expr_str == NULL) {
        fprintf(stderr, "Usage: %s [--print-ast] [--count-nodes] \"<expression>\"\n", argv[0]);
        return 1;
    }

    combinator_t *expr_parser = new_combinator();
    init_calculator_parser(&expr_parser);

    // Parsing
    input_t *in = new_input();
    in->buffer = expr_str;
    in->length = strlen(expr_str);
    ast_nil = new_ast();
    ast_nil->typ = CALC_T_NONE;
    ParseResult result = parse(in, expr_parser);

    // Output
    if (result.is_success) {
        if (in->start < in->length) {
            fprintf(stderr, "Error: Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
            return 1;
        }
        if (print_ast) {
            print_calculator_ast(result.value.ast);
        }
        if (count_nodes) {
            int node_count = 0;
            parser_walk_ast(result.value.ast, count_nodes_visitor, &node_count);
            printf("AST contains %d nodes.\n", node_count);
        }
        long final_result = eval(result.value.ast);
        printf("%ld\n", final_result);
        free_ast(result.value.ast);
    } else {
        print_error_with_partial_ast(result.value.error);
        free_error(result.value.error);
        return 1;
    }

    // Cleanup
    free_combinator(expr_parser);
    free(in);
    free(ast_nil);
    return 0;
}