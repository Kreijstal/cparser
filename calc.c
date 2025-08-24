#include "parser.h"

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

int main(void) {
   ast_nil = new_ast();
   ast_nil->typ = T_NONE;

   printf("Simple Parser Combinator Calculator\n");
   printf("Enter expressions like '1 + 2 * (3 - 1);'\n");
   printf("Enter an empty line to exit.\n\n");

   // --- Define Grammar ---
   combinator_t * root;
   combinator_t * exp = new_combinator();
   combinator_t * base = new_combinator();
   combinator_t * paren = new_combinator();

   multi(base, T_NONE, integer(), paren, NULL);
   seq(paren, T_NONE, match("("), exp, expect(match(")"), "\")\" expected"), NULL);
   expr(exp, base);
   // Precedence: ADD(0) < MUL(1) < NEG(2)
   expr_insert(exp, 0, T_ADD, EXPR_INFIX, ASSOC_LEFT, match("+"));
   expr_altern(exp, 0, T_SUB, match("-"));
   expr_insert(exp, 1, T_MUL, EXPR_INFIX, ASSOC_LEFT, match("*"));
   expr_altern(exp, 1, T_DIV, match("/"));
   expr_insert(exp, 2, T_NEG, EXPR_PREFIX, ASSOC_NONE, match("-"));

   combinator_t* calc_parser = new_combinator();
   seq(calc_parser, T_NONE, exp, expect(match(";"), "\";\" expected"), NULL);

   root = calc_parser;


   // --- Main Parse Loop ---
   while (1) {
      printf("> ");
      input_t * in = new_input();

      char first_char = read1(in);
      if (first_char == '\n' || first_char == EOF) {
          free(in->buffer);
          free(in);
          break;
      }
      in->start--;

      if (!setjmp(exc)) {
         ast_t * result_ast = parse(in, root);
         if (result_ast) {
            if (result_ast->typ == T_SEQ) {
                 printf("Successfully parsed flatMap example!\n");
            } else {
                 printf("Result: %ld\n", eval(result_ast));
            }
            free_ast(result_ast);
         } else {
            fprintf(stderr, "Error: Invalid expression.\n");
         }
      } else {
         int c;
         while((c = getchar()) != '\n' && c != EOF);
      }
      free(in->buffer);
      free(in);
   }

   // --- Final Cleanup ---
   free_combinator(root);
   free(ast_nil);

   printf("Goodbye.\n");
   return 0;
}
