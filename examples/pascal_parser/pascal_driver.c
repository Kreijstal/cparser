#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "pascal_parser.h"

// Function to print AST with indentation
static void print_ast_indented(ast_t* ast, int depth) {
    if (ast == NULL || ast->typ == 0) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    
    switch (ast->typ) {
        case PASCAL_T_PROGRAM:
            printf("PROGRAM");
            break;
        case PASCAL_T_IDENT:
            printf("IDENT: %s", ast->sym ? ast->sym->name : "NULL");
            break;
        case PASCAL_T_IF:
            printf("IF");
            break;
        case PASCAL_T_FOR:
            printf("FOR");
            break;
        case PASCAL_T_ASSIGN:
            printf("ASSIGN");
            break;
        case PASCAL_T_PROC_CALL:
            printf("PROC_CALL: %s", ast->sym ? ast->sym->name : "NULL");
            break;
        case PASCAL_T_INT_NUM:
            printf("INT: %s", ast->sym ? ast->sym->name : "NULL");
            break;
        case PASCAL_T_STRING:
            printf("STRING: %s", ast->sym ? ast->sym->name : "NULL");
            break;
        default:
            printf("UNKNOWN(%d)", ast->typ);
            break;
    }
    printf("\n");
    
    // Print children
    ast_t* child = ast->child;
    while (child != NULL) {
        print_ast_indented(child, depth + 1);
        child = child->next;
    }
    
    // Print siblings
    ast_t* sibling = ast->next;
    while (sibling != NULL) {
        print_ast_indented(sibling, depth);
        sibling = sibling->next;
    }
}

// Helper function to print ParseError with partial AST
static void print_error_with_partial_ast(ParseError* error, int depth) {
    if (error == NULL) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    printf("Error at line %d, col %d: %s\n", error->line, error->col, error->message);
    
    if (error->partial_ast != NULL) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("Partial AST:\n");
        print_ast_indented(error->partial_ast, depth + 1);
    }
    
    if (error->cause != NULL) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("Caused by:\n");
        print_error_with_partial_ast(error->cause, depth + 1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate memory for file content\n");
        fclose(file);
        return 1;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    // --- Parser Definition ---
    combinator_t *parser = p_program();

    // --- Parsing ---
    input_t *in = new_input();
    in->buffer = buffer;
    in->length = length;

    ast_nil = new_ast();
    ast_nil->typ = 0; // PASCAL_T_NONE is not defined here, but 0 should be fine.

    ParseResult result = parse(in, parser);

    // --- Output ---
    if (result.is_success && in->start == in->length) {
        printf("Successfully parsed %s\n", argv[1]);
        free_ast(result.value.ast);
    } else {
        fprintf(stderr, "Failed to parse %s\n", argv[1]);
        if (!result.is_success) {
            fprintf(stderr, "  Error at line %d, col %d: %s\n",
                    result.value.error->line,
                    result.value.error->col,
                    result.value.error->message);
            
            // Debug: Check if partial AST is set
            fprintf(stderr, "  Partial AST pointer: %p\n", (void*)result.value.error->partial_ast);
            if (result.value.error->partial_ast != NULL) {
                fprintf(stderr, "  Partial AST type: %d\n", result.value.error->partial_ast->typ);
            }
            
            // Print partial AST if available
            if (result.value.error->partial_ast != NULL) {
                fprintf(stderr, "  Partial AST was successfully parsed:\n");
                print_ast_indented(result.value.error->partial_ast, 2);
            }
            
            // Print error chain with partial ASTs
            if (result.value.error->cause != NULL) {
                fprintf(stderr, "  Error chain:\n");
                print_error_with_partial_ast(result.value.error->cause, 2);
            }
            
            free_error(result.value.error);
        } else {
            fprintf(stderr, "  Parser did not consume entire input. Trailing characters: '%s'\n", in->buffer + in->start);
            free_ast(result.value.ast);
        }
    }

    // --- Cleanup ---
    free_combinator(parser);
    free(in);
    free(ast_nil);
    free(buffer);

    return result.is_success && in->start == in->length ? 0 : 1;
}
