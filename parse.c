/**
 * A single-file implementation of a parser combinator library in C.
 *
 * This file consolidates the core concepts from the wbhart-cesium3 repository,
 * including all major struct definitions, into one place. It demonstrates
 * how closures are simulated using function pointers and void* contexts.
 *
 * This version includes a `flatMap` combinator and proper memory management
 * to prevent leaks.
 *
 * To compile: gcc -o calculator single_file_parser_combinator.c
 * To run:     ./calculator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdbool.h> // For bool type in free tracking

//=============================================================================
// 1. FORWARD DECLARATIONS & CORE STRUCT DEFINITIONS
//=============================================================================

// Forward declarations are necessary because many structs refer to each other.
typedef struct ast_t ast_t;
typedef struct combinator_t combinator_t;
typedef struct input_t input_t;

// --- Abstract Syntax Tree (AST) ---
// Enum for different types of AST nodes.
typedef enum {
    T_NONE, T_INT, T_IDENT, T_ASSIGN, T_SEQ,
    T_ADD, T_SUB, T_MUL, T_DIV, T_NEG
} tag_t;

// Represents a symbol (e.g., an identifier or a number literal).
typedef struct sym_t {
   char * name;
} sym_t;

// The core AST node structure.
struct ast_t {
   tag_t typ;
   ast_t * child;
   ast_t * next;
   sym_t * sym;
};

// --- Input Stream ---
struct input_t {
   char * buffer;
   int alloc;
   int length;
   int start;
};

// --- Parser Combinator Core ---
typedef ast_t * (*comb_fn)(input_t *in, void *args);

struct combinator_t {
    comb_fn fn;
    void * args;
};

// --- Argument Structs (The "Closure Environments") ---
typedef struct {
    char * str;
} match_args;

typedef struct {
    combinator_t * comb;
    char * msg;
} expect_args;

typedef struct seq_list {
    combinator_t * comb;
    struct seq_list * next;
} seq_list;

typedef struct {
    tag_t typ;
    seq_list * list;
} seq_args;

typedef combinator_t * (*flatMap_func)(ast_t *result);

typedef struct {
    combinator_t * parser;
    flatMap_func func;
} flatMap_args;

// --- Expression Parser Structs ---
typedef enum { EXPR_BASE, EXPR_INFIX, EXPR_PREFIX, EXPR_POSTFIX } expr_fix;
typedef enum { ASSOC_LEFT, ASSOC_RIGHT, ASSOC_NONE } expr_assoc;

typedef struct op_t {
   tag_t tag;
   combinator_t * comb;
   struct op_t * next;
} op_t;

typedef struct expr_list {
   op_t * op;
   expr_fix fix;
   expr_assoc assoc;
   combinator_t * comb;
   struct expr_list * next;
} expr_list;


//=============================================================================
// 2. GLOBAL STATE & HELPER FUNCTIONS
//=============================================================================

ast_t * ast_nil;
jmp_buf exc;

void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Fatal: malloc failed.\n");
        exit(1);
    }
    return ptr;
}

void exception(const char * err) {
   fprintf(stderr, "Error: %s\n", err);
   longjmp(exc, 1);
}

// --- AST Helpers ---
ast_t * new_ast() {
   return (ast_t *) safe_malloc(sizeof(ast_t));
}

ast_t* ast1(tag_t typ, ast_t* a1) {
    ast_t* ast = new_ast();
    ast->typ = typ;
    ast->child = a1;
    ast->next = NULL;
    return ast;
}

ast_t* ast2(tag_t typ, ast_t* a1, ast_t* a2) {
    ast_t* ast = new_ast();
    ast->typ = typ;
    ast->child = a1;
    a1->next = a2;
    ast->next = NULL;
    return ast;
}

// --- Symbol Table Helpers ---
sym_t * sym_lookup(const char * name) {
   sym_t * sym = (sym_t *) safe_malloc(sizeof(sym_t));
   sym->name = (char *) safe_malloc(strlen(name) + 1);
   strcpy(sym->name, name);
   return sym;
}

// --- Input Stream Helpers ---
input_t * new_input() {
    input_t * in = (input_t *) safe_malloc(sizeof(input_t));
    in->buffer = NULL;
    in->alloc = 0;
    in->length = 0;
    in->start = 0;
    return in;
}

char read1(input_t * in) {
   if (in->start < in->length) return in->buffer[in->start++];
   if (in->alloc == in->length) {
      in->alloc += 50;
      in->buffer = (char *) realloc(in->buffer, in->alloc);
      if (!in->buffer) exception("realloc failed");
   }
   int c = getchar();
   if (c == EOF) return EOF;
   in->start++;
   return in->buffer[in->length++] = (char)c;
}

void skip_whitespace(input_t * in) {
   char c;
   while ((c = read1(in)) == ' ' || c == '\n' || c == '\t') ;
   if (c != EOF) in->start--;
}

ast_t * parse(input_t * in, combinator_t * comb);


//=============================================================================
// 3. PARSING LOGIC FUNCTIONS (THE `_fn` IMPLEMENTATIONS)
//=============================================================================

// Forward declaration for memory management
void free_combinator(combinator_t* comb);

combinator_t * new_combinator() {
    return (combinator_t *) safe_malloc(sizeof(combinator_t));
}

ast_t * match_fn(input_t * in, void * args) {
    char * str = ((match_args *) args)->str;
    int start_pos = in->start;
    skip_whitespace(in);
    int i = 0, len = strlen(str);
    while (i < len && str[i] == read1(in)) i++;
    if (i == len) return ast_nil;
    in->start = start_pos;
    return NULL;
}

ast_t * expect_fn(input_t * in, void * args) {
    expect_args * eargs = (expect_args *) args;
    ast_t * ast = parse(in, eargs->comb);
    if (ast) return ast;
    exception(eargs->msg);
    return NULL;
}

ast_t * seq_fn(input_t * in, void * args) {
    int start_pos = in->start;
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ast_t * head = NULL, * tail = NULL;

    while (seq != NULL) {
        ast_t * a = parse(in, seq->comb);
        if (a == NULL) {
           in->start = start_pos;
           return NULL;
        }
        if (a != ast_nil) {
            if (head == NULL) head = tail = a;
            else tail->next = a;
            while(tail->next != NULL) tail = tail->next;
        }
        seq = seq->next;
    }
    if (sa->typ == T_NONE) return head;
    return ast1(sa->typ, head);
}

ast_t * multi_fn(input_t * in, void * args) {
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    while (seq != NULL) {
        ast_t * a = parse(in, seq->comb);
        if (a != NULL) {
           if (sa->typ == T_NONE) return a;
           return ast1(sa->typ, a);
        }
        seq = seq->next;
    }
    return NULL;
}

ast_t * flatMap_fn(input_t * in, void * args) {
    flatMap_args * fm_args = (flatMap_args *)args;
    int start_pos = in->start;
    ast_t * initial_result = parse(in, fm_args->parser);
    if (initial_result == NULL) return NULL;

    combinator_t * next_parser = fm_args->func(initial_result);
    if (next_parser == NULL) {
        in->start = start_pos;
        return NULL;
    }
    
    ast_t * final_result = parse(in, next_parser);
    
    // IMPORTANT: The dynamically generated parser must be freed here.
    free_combinator(next_parser);

    if (final_result == NULL) {
        in->start = start_pos;
        return NULL;
    }
    return final_result;
}

ast_t * integer_fn(input_t * in, void * args) {
   skip_whitespace(in);
   int start_pos = in->start;
   char c = read1(in);
   if (!isdigit(c)) {
      in->start = start_pos; return NULL;
   }
   while (isdigit(c = read1(in))) ;
   in->start--;

   int len = in->start - start_pos;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos, len);
   text[len] = '\0';

   ast_t * ast = new_ast();
   ast->typ = T_INT;
   ast->sym = sym_lookup(text);
   free(text);
   return ast;
}

ast_t * cident_fn(input_t * in, void * args) {
   skip_whitespace(in);
   int start_pos = in->start;
   char c = read1(in);
   if (c != '_' && !isalpha(c)) {
      in->start = start_pos; return NULL;
   }
   while ((c = read1(in)) == '_' || isalnum(c)) ;
   in->start--;

   int len = in->start - start_pos;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos, len);
   text[len] = '\0';

   ast_t * ast = new_ast();
   ast->typ = T_IDENT;
   ast->sym = sym_lookup(text);
   free(text);
   return ast;
}

ast_t * expr_fn(input_t * in, void * args) {
   expr_list * list = (expr_list *) args;

   if (list->fix == EXPR_BASE) {
      return parse(in, list->comb);
   }

   ast_t * lhs = expr_fn(in, (void *) list->next);
   if (!lhs) return NULL;

   if (list->fix == EXPR_INFIX) {
       while (1) {
           op_t *op = list->op;
           while (op) {
               int start_pos = in->start;
               if (parse(in, op->comb)) {
                   ast_t *rhs = expr_fn(in, (void *) list->next);
                   if (!rhs) exception("Expression expected after operator!");
                   lhs = ast2(op->tag, lhs, rhs);
                   goto next_token;
               }
               in->start = start_pos;
               op = op->next;
           }
           break;
           next_token:;
       }
   } else if (list->fix == EXPR_PREFIX) {
        op_t * op = list->op;
        while(op) {
            int start_pos = in->start;
            if (parse(in, op->comb)) {
                ast_t* rhs = expr_fn(in, args);
                if(!rhs) exception("Expression expected after prefix operator");
                return ast1(op->tag, rhs);
            }
            in->start = start_pos;
            op = op->next;
        }
   }
   return lhs;
}

//=============================================================================
// 4. COMBINATOR CREATION FUNCTIONS (THE PUBLIC API)
//=============================================================================

combinator_t * match(char * str) {
    match_args * args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t * comb = new_combinator();
    comb->fn = match_fn;
    comb->args = args;
    return comb;
}

combinator_t * expect(combinator_t * c, char * msg) {
    expect_args * args = (expect_args*)safe_malloc(sizeof(expect_args));
    args->msg = msg;
    args->comb = c;
    combinator_t * comb = new_combinator();
    comb->fn = expect_fn;
    comb->args = (void *) args;
    return comb;
}

combinator_t * seq(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap;
    va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next;
        current->comb = c;
    }
    current->next = NULL;
    va_end(ap);

    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ;
    args->list = head;
    ret->args = (void *) args;
    ret->fn = seq_fn;
    return ret;
}

combinator_t * multi(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap;
    va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next;
        current->comb = c;
    }
    current->next = NULL;
    va_end(ap);

    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ;
    args->list = head;
    ret->args = (void *) args;
    ret->fn = multi_fn;
    return ret;
}

combinator_t * flatMap(combinator_t * p, flatMap_func func) {
    flatMap_args * args = (flatMap_args*)safe_malloc(sizeof(flatMap_args));
    args->parser = p;
    args->func = func;
    combinator_t * comb = new_combinator();
    comb->fn = flatMap_fn;
    comb->args = args;
    return comb;
}

combinator_t * integer() {
    combinator_t * comb = new_combinator();
    comb->fn = integer_fn;
    comb->args = NULL;
    return comb;
}

combinator_t * cident() {
    combinator_t * comb = new_combinator();
    comb->fn = cident_fn;
    comb->args = NULL;
    return comb;
}

combinator_t * expr(combinator_t * exp, combinator_t * base) {
   expr_list * args = (expr_list*)safe_malloc(sizeof(expr_list));
   args->next = NULL;
   args->fix = EXPR_BASE;
   args->comb = base;
   args->op = NULL;
   exp->fn = expr_fn;
   exp->args = args;
   return exp;
}

void expr_insert(combinator_t * exp, int prec, tag_t tag, expr_fix fix, expr_assoc assoc, combinator_t * comb) {
   expr_list * list = (expr_list *) exp->args;
   expr_list * node = (expr_list*)safe_malloc(sizeof(expr_list));
   op_t * op = (op_t*)safe_malloc(sizeof(op_t));
   op->tag = tag;
   op->comb = comb;
   op->next = NULL;
   node->op = op;
   node->fix = fix;
   node->assoc = assoc;
   node->comb = NULL;

   if (prec == 0) {
      node->next = list;
      exp->args = (void *) node;
      return;
   }
   for (int i = 0; i < prec - 1; i++) {
      if (list->next == NULL) exception("Invalid precedence for expression");
      list = list->next;
   }
   node->next = list->next;
   list->next = node;
}

void expr_altern(combinator_t * exp, int prec, tag_t tag, combinator_t * comb) {
    expr_list* list = (expr_list*)exp->args;
    for (int i = 0; i < prec; i++) {
        if (list == NULL) exception("Invalid precedence for expression alternative");
        list = list->next;
    }
    if (list->fix == EXPR_BASE || list == NULL) exception("Invalid precedence");
    
    op_t* op = (op_t*)safe_malloc(sizeof(op_t));
    op->tag = tag;
    op->comb = comb;
    op->next = list->op;
    list->op = op;
}

//=============================================================================
// 5. THE UNIVERSAL PARSE FUNCTION
//=============================================================================
ast_t * parse(input_t * in, combinator_t * comb) {
    if (!comb || !comb->fn) {
        exception("Attempted to parse with a NULL or uninitialized combinator.");
    }
    return comb->fn(in, (void *)comb->args);
}

//=============================================================================
// 6. MEMORY MANAGEMENT
//=============================================================================

/**
 * @brief Recursively frees an entire AST tree.
 */
void free_ast(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) {
        return;
    }
    // Recursively free children and siblings
    free_ast(ast->child);
    free_ast(ast->next);

    // Free the symbol if it exists
    if (ast->sym) {
        free(ast->sym->name);
        free(ast->sym);
    }
    // Free the node itself
    free(ast);
}

// --- Combinator Freeing with Cycle Detection ---

// A linked list to track visited combinators during the free operation.
typedef struct visited_node {
    const void* ptr;
    struct visited_node* next;
} visited_node;

void free_combinator_recursive(combinator_t* comb, visited_node** visited);

// Helper to check if a combinator pointer is already in our visited list.
bool is_visited(const void* ptr, visited_node* list) {
    for (visited_node* current = list; current != NULL; current = current->next) {
        if (current->ptr == ptr) {
            return true;
        }
    }
    return false;
}

/**
 * @brief The main cleanup function. Frees the entire grammar structure.
 * Uses a recursive helper with a "visited" list to handle cycles.
 */
void free_combinator(combinator_t* comb) {
    visited_node* visited = NULL;
    free_combinator_recursive(comb, &visited);

    // Free the visited list itself
    visited_node* current = visited;
    while (current != NULL) {
        visited_node* temp = current;
        current = current->next;
        free(temp);
    }
}

// The recursive worker function for freeing combinators.
void free_combinator_recursive(combinator_t* comb, visited_node** visited) {
    if (comb == NULL || is_visited(comb, *visited)) {
        return;
    }

    // Add current combinator to the visited list to prevent infinite recursion.
    visited_node* new_visited = (visited_node*)safe_malloc(sizeof(visited_node));
    new_visited->ptr = comb;
    new_visited->next = *visited;
    *visited = new_visited;

    // Free the `args` struct based on which function it belongs to.
    if (comb->args != NULL) {
        if (comb->fn == match_fn) {
            free((match_args*)comb->args);
        } else if (comb->fn == expect_fn) {
            expect_args* args = (expect_args*)comb->args;
            free_combinator_recursive(args->comb, visited);
            free(args);
        } else if (comb->fn == seq_fn || comb->fn == multi_fn) {
            seq_args* args = (seq_args*)comb->args;
            seq_list* current = args->list;
            while (current != NULL) {
                free_combinator_recursive(current->comb, visited);
                seq_list* temp = current;
                current = current->next;
                free(temp);
            }
            free(args);
        } else if (comb->fn == flatMap_fn) {
            flatMap_args* args = (flatMap_args*)comb->args;
            free_combinator_recursive(args->parser, visited);
            free(args);
        } else if (comb->fn == expr_fn) {
            expr_list* list = (expr_list*)comb->args;
            while (list != NULL) {
                if (list->fix == EXPR_BASE) {
                    free_combinator_recursive(list->comb, visited);
                }
                op_t* op = list->op;
                while (op != NULL) {
                    free_combinator_recursive(op->comb, visited);
                    op_t* temp_op = op;
                    op = op->next;
                    free(temp_op);
                }
                expr_list* temp_list = list;
                list = list->next;
                free(temp_list);
            }
        }
    }
    
    // Finally, free the combinator struct itself.
    free(comb);
}

//=============================================================================
// 7. EXAMPLE USAGE: A SIMPLE CALCULATOR & flatMap DEMO
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
        return 0;
    default:
        fprintf(stderr, "Unknown AST tag in eval: %d\n", ast->typ);
        return 0;
    }
}

combinator_t* repeat_ident_parser(ast_t* ast) {
    if (ast->typ != T_INT) return NULL;
    long count = atol(ast->sym->name);
    if (count <= 0 || count > 10) {
        printf("Repeat count must be between 1 and 10.\n");
        return NULL;
    }
    
    combinator_t *s = new_combinator();
    seq_list *head = NULL, *current = NULL;

    for (int i = 0; i < count; i++) {
        seq_list *node = (seq_list*)safe_malloc(sizeof(seq_list));
        node->comb = cident();
        node->next = NULL;

        if (head == NULL) {
            head = current = node;
        } else {
            current->next = node;
            current = node;
        }
    }
    
    seq_args* args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = T_SEQ;
    args->list = head;
    
    s->args = (void*)args;
    s->fn = seq_fn;

    return s;
}

int main(void) {
   ast_nil = new_ast();
   ast_nil->typ = T_NONE;

   printf("Simple Parser Combinator Calculator\n");
   printf("Enter expressions like '1 + 2 * (3 - 1);'\n");
   printf("Try the flatMap example: 'repeat 3 foo bar baz;'\n");
   printf("Enter an empty line to exit.\n\n");

   // --- Define Grammar ---
   combinator_t * root = new_combinator();
   combinator_t * exp = new_combinator();
   combinator_t * base = new_combinator();
   combinator_t * paren = new_combinator();

   multi(base, T_NONE, integer(), paren, NULL);
   seq(paren, T_NONE, match("("), exp, expect(match(")"), "\")\" expected"), NULL);
   expr(exp, base);
   expr_insert(exp, 0, T_NEG, EXPR_PREFIX, ASSOC_NONE, match("-"));
   expr_insert(exp, 1, T_MUL, EXPR_INFIX, ASSOC_LEFT, match("*"));
   expr_altern(exp, 1, T_DIV, match("/"));
   expr_insert(exp, 2, T_ADD, EXPR_INFIX, ASSOC_LEFT, match("+"));
   expr_altern(exp, 2, T_SUB, match("-"));
   
   combinator_t* calc_parser = new_combinator();
   seq(calc_parser, T_NONE, exp, expect(match(";"), "\";\" expected"), NULL);
   
   combinator_t* flatMap_parser = new_combinator();
   seq(flatMap_parser, T_NONE,
        match("repeat"),
        flatMap(integer(), repeat_ident_parser),
        expect(match(";"), "\";\" expected"),
        NULL);
   
   multi(root, T_NONE, calc_parser, flatMap_parser, NULL);

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
            free_ast(result_ast); // Free the AST after using it
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
