#include "parser.h"
#include "combinators.h"

//=============================================================================
// Calculator-Specific Logic
//=============================================================================

long eval(ast_t * ast) {
    if (!ast) return 0;
    switch (ast->typ) {
    case T_INT:    return atol(ast->sym->name);
    case T_ADD:    return eval(ast->child) + eval(ast->child->next);
    case T_SUB:    return eval(ast->child) - eval(ast->child->next);
    case T_MUL:    return eval(ast->child) * eval(ast->child->next);
    case T_DIV:    return eval(ast->child) / eval(ast->child->next);
    case T_NEG:    return -eval(ast->child);
    case T_SEQ:    // Not really evaluatable, just for demo
        printf("Parsed a sequence via flatMap.\n");
        return 0;
    default:
        fprintf(stderr, "Unknown AST tag in eval: %d\n", ast->typ);
        return 0;
    }
}

//=============================================================================
// Whitespace-Aware Expression Parser
//=============================================================================

// Helper function to check if character is whitespace
static bool is_whitespace_calc(char c) {
    return isspace((unsigned char)c);
}

// Helper function to convert sequence AST to binary AST
ast_t* seq_to_binary(ast_t* seq_ast) {
    if (!seq_ast || !seq_ast->child || !seq_ast->child->next || !seq_ast->child->next->next) {
        return seq_ast; // Return as-is if not the expected structure
    }

    // Extract operands from sequence: left, op, right
    ast_t* left = seq_ast->child;
    ast_t* op = seq_ast->child->next;
    ast_t* right = seq_ast->child->next->next;

    // Determine operator type based on the operator symbol
    tag_t op_type = T_NONE;
    if (op && op->sym && op->sym->name) {
        if (strcmp(op->sym->name, "+") == 0) {
            op_type = T_ADD;
        } else if (strcmp(op->sym->name, "-") == 0) {
            op_type = T_SUB;
        }
    }

    if (op_type == T_NONE) {
        return seq_ast; // Return as-is if unknown operator
    }

    // Remove the operands from the original sequence
    seq_ast->child = NULL;

    // Create proper binary AST
    ast_t* binary_ast = ast2(op_type, left, right);

    // Clean up the original sequence structure
    free_ast(seq_ast);

    return binary_ast;
}

// Simple expression parser - handle addition and subtraction
combinator_t* calc_expr() {
    combinator_t* ws = many(satisfy(is_whitespace_calc));

    // Addition parser: integer + integer
    combinator_t* add_expr_seq = new_combinator();
    seq(add_expr_seq, T_ADD,
        right(ws, integer()),
        right(ws, match("+")),
        right(ws, integer()),
        NULL);

    // Subtraction parser: integer - integer
    combinator_t* sub_expr_seq = new_combinator();
    seq(sub_expr_seq, T_SUB,
        right(ws, integer()),
        right(ws, match("-")),
        right(ws, integer()),
        NULL);

    // Try subtraction first, then addition
    combinator_t* expr_multi = new_combinator();
    multi(expr_multi, T_NONE, sub_expr_seq, add_expr_seq, NULL);

    // Convert sequence AST to binary AST
    return map(expr_multi, seq_to_binary);
}

int main(void) {
    ast_nil = new_ast();
    ast_nil->typ = T_NONE;

    printf("Simple Parser Combinator Calculator (Explicit Whitespace)\n");
    printf("Now supports whitespace in expressions like '1 + 2 * (3 - 1);'\n");
    printf("Enter an empty line to exit.\n\n");

    // --- Define Grammar ---
    combinator_t * root;
    combinator_t * exp = calc_expr();

    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* calc_parser = new_combinator();
    seq(calc_parser, T_NONE, right(ws, exp), expect(right(ws, match(";")), "\";\" expected"), NULL);

    root = calc_parser;

    // --- Main Parse Loop ---
    while (1) {
       printf("> ");
       input_t * in = new_input();

       // Read the first character to see if we should exit.
       char first_char = read1(in);
       if (first_char == '\n' || first_char == EOF) {
           if (in->buffer) free(in->buffer);
           free(in);
           break;
       }
       in->start--; // Rewind to the beginning
       in->line = 1;
       in->col = 1;

       ParseResult result = parse(in, root);
       if (result.is_success) {
          ast_t* result_ast = result.value.ast;
          if (result_ast && result_ast->typ != T_NONE) {
             printf("Result: %ld\n", eval(result_ast));
             free_ast(result_ast);
          } else {
             printf("Parsed successfully, but no result to show.\n");
          }
       } else {
          fprintf(stderr, "Error at line %d, col %d: %s\n",
             result.value.error->line,
             result.value.error->col,
             result.value.error->message);
          free_error(result.value.error);

          // Clear the rest of the line from stdin in case of error
          int c;
          while((c = getchar()) != '\n' && c != EOF);
       }

       if (in->buffer) free(in->buffer);
       free(in);
    }

    // --- Final Cleanup ---
    free_combinator(root);
    free(ast_nil);

    printf("Goodbye.\n");
    return 0;
}