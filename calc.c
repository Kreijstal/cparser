#include "parser.h"
#include "combinators.h"

//=============================================================================
// Calculator-Specific Logic
//=============================================================================

// Forward declarations for whitespace-aware parsers
combinator_t* calc_integer();
combinator_t* calc_lparen();
combinator_t* calc_rparen();
combinator_t* calc_semicolon();
combinator_t* calc_operator(const char* op);

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
// Whitespace-Aware Parser Functions
//=============================================================================

// Helper function to check if character is whitespace
static bool is_whitespace_calc(char c) {
    return isspace((unsigned char)c);
}

// Whitespace-aware integer parser
combinator_t* calc_integer() {
    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* int_parser = integer();
    return right(ws, int_parser);
}

// Whitespace-aware parenthesis parsers
combinator_t* calc_lparen() {
    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* lparen = match("(");
    return right(ws, lparen);
}

combinator_t* calc_rparen() {
    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* rparen = match(")");
    return right(ws, rparen);
}

// Whitespace-aware semicolon parser
combinator_t* calc_semicolon() {
    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* semicolon = match(";");
    return right(ws, semicolon);
}

// Whitespace-aware operator parser
combinator_t* calc_operator(const char* op) {
    combinator_t* ws = many(satisfy(is_whitespace_calc));
    combinator_t* operator = match((char*)op);
    return right(ws, operator);
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

   multi(base, T_NONE, calc_integer(), paren, NULL);
   seq(paren, T_NONE, calc_lparen(), exp, expect(calc_rparen(), "\")\" expected"), NULL);
   expr(exp, base);
   // Precedence: ADD(0) < MUL(1) < NEG(2)
   expr_insert(exp, 0, T_ADD, EXPR_INFIX, ASSOC_LEFT, calc_operator("+"));
   expr_altern(exp, 0, T_SUB, calc_operator("-"));
   expr_insert(exp, 1, T_MUL, EXPR_INFIX, ASSOC_LEFT, calc_operator("*"));
   expr_altern(exp, 1, T_DIV, calc_operator("/"));
   expr_insert(exp, 2, T_NEG, EXPR_PREFIX, ASSOC_NONE, calc_operator("-"));

   combinator_t* calc_parser = new_combinator();
   seq(calc_parser, T_NONE, exp, expect(calc_semicolon(), "\";\" expected"), NULL);

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
         free_error(result.value.error); // Free the allocated error message

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
