#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdbool.h>

//=============================================================================
// Public-Facing Structs and Enums
//=============================================================================

// Forward declarations
typedef struct ast_t ast_t;
typedef struct combinator_t combinator_t;
typedef struct input_t input_t;

// AST node types
typedef enum {
    T_NONE, T_INT, T_IDENT, T_ASSIGN, T_SEQ,
    T_ADD, T_SUB, T_MUL, T_DIV, T_NEG, T_STRING
} tag_t;

// Symbol
typedef struct sym_t {
   char * name;
} sym_t;

// AST node
struct ast_t {
   tag_t typ;
   ast_t * child;
   ast_t * next;
   sym_t * sym;
};

// Input stream
struct input_t {
   char * buffer;
   int alloc;
   int length;
   int start;
};

// Main combinator struct
typedef ast_t * (*comb_fn)(input_t *in, void *args);

struct combinator_t {
    comb_fn fn;
    void * args;
};

// For flatMap
typedef combinator_t * (*flatMap_func)(ast_t *result);


//=============================================================================
// Global Variables
//=============================================================================

extern ast_t * ast_nil;
extern jmp_buf exc;


//=============================================================================
// Public Function Prototypes
//=============================================================================

// --- Core Parser Function ---
ast_t * parse(input_t * in, combinator_t * comb);

// --- Combinator Constructors ---
combinator_t * match(char * str);
combinator_t * match_raw(char * str);
combinator_t * expect(combinator_t * c, char * msg);
combinator_t * seq(combinator_t * ret, tag_t typ, combinator_t * c1, ...);
combinator_t * multi(combinator_t * ret, tag_t typ, combinator_t * c1, ...);
combinator_t * flatMap(combinator_t * p, flatMap_func func);
combinator_t * integer();
combinator_t * cident();
combinator_t * until(combinator_t* p);

// --- Expression Parser Constructors ---
typedef enum { EXPR_BASE, EXPR_INFIX, EXPR_PREFIX, EXPR_POSTFIX } expr_fix;
typedef enum { ASSOC_LEFT, ASSOC_RIGHT, ASSOC_NONE } expr_assoc;

combinator_t * expr(combinator_t * exp, combinator_t * base);
void expr_insert(combinator_t * exp, int prec, tag_t tag, expr_fix fix, expr_assoc assoc, combinator_t * comb);
void expr_altern(combinator_t * exp, int prec, tag_t tag, combinator_t * comb);

// --- Input Stream Helpers ---
input_t * new_input();
char read1(input_t * in);

// --- AST Helpers ---
ast_t* new_ast();
void free_ast(ast_t* ast);

// --- Combinator Helpers ---
combinator_t* new_combinator();

// --- Memory Management ---
void free_combinator(combinator_t* comb);
void exception(const char * err);


#endif // PARSER_H
