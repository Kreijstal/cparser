#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "parser.h"
#include "combinator_internals.h"

//=============================================================================
// Internal Structs & Forward Declarations
//=============================================================================

// --- Argument Structs ---
typedef struct { char * str; } match_args;
typedef struct { combinator_t* delimiter; tag_t tag; } until_args;
typedef struct op_t { tag_t tag; combinator_t * comb; struct op_t * next; } op_t;
typedef struct expr_list { op_t * op; expr_fix fix; expr_assoc assoc; combinator_t * comb; struct expr_list * next; } expr_list;

// --- Static Function Forward Declarations ---
static ParseResult lazy_fn(input_t * in, void * args);
static ParseResult match_fn(input_t * in, void * args);
static ParseResult integer_fn(input_t * in, void * args);
static ParseResult cident_fn(input_t * in, void * args);
static ParseResult string_fn(input_t * in, void * args);
static ParseResult until_fn(input_t * in, void * args);
static ParseResult any_char_fn(input_t * in, void * args);
static ParseResult satisfy_fn(input_t * in, void * args);
static ParseResult expr_fn(input_t * in, void * args);
static ast_t* ensure_ast_nil_initialized();


//=============================================================================
// GLOBAL STATE & HELPER FUNCTIONS
//=============================================================================

ast_t * ast_nil = NULL;

// --- Result & Error Helpers ---
ParseResult make_success(ast_t* ast) {
    return (ParseResult){ .is_success = true, .value.ast = ast };
}

ParseResult make_failure(input_t* in, char* message) {
    ParseError* err = (ParseError*)safe_malloc(sizeof(ParseError));
    err->line = in->line;
    err->col = in->col;
    err->message = message;
    err->cause = NULL;
    return (ParseResult){ .is_success = false, .value.error = err };
}

ParseResult wrap_failure(input_t* in, char* message, ParseResult cause) {
    ParseError* err = (ParseError*)safe_malloc(sizeof(ParseError));
    err->line = in->line;
    err->col = in->col;
    err->message = message;
    err->cause = cause.value.error;
    return (ParseResult){ .is_success = false, .value.error = err };
}

// --- Input State Management ---
void save_input_state(input_t* in, InputState* state) {
    state->start = in->start; state->line = in->line; state->col = in->col;
}

void restore_input_state(input_t* in, InputState* state) {
    in->start = state->start; in->line = state->line; in->col = state->col;
}

// --- Public Helpers ---
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) { fprintf(stderr, "Fatal: malloc failed.\n"); exit(1); }
    return ptr;
}

void exception(const char * err) {
   fprintf(stderr, "Fatal Error: %s\n", err);
   exit(1);
}

ast_t * new_ast() {
    ast_t* ast = (ast_t *) safe_malloc(sizeof(ast_t));
    ast->typ = 0; // Default tag
    ast->child = NULL;
    ast->next = NULL;
    ast->sym = NULL;
    return ast;
}

ast_t* ast1(tag_t typ, ast_t* a1) {
    ast_t* ast = new_ast();
    ast->typ = typ; ast->child = a1; ast->next = NULL;
    return ast;
}

ast_t* copy_ast(ast_t* orig) {
    if (orig == NULL) return NULL;
    if (orig == ensure_ast_nil_initialized()) return ensure_ast_nil_initialized();
    ast_t* new = new_ast();
    new->typ = orig->typ;
    new->sym = orig->sym ? sym_lookup(orig->sym->name) : NULL;
    new->child = copy_ast(orig->child);
    new->next = copy_ast(orig->next);
    return new;
}

ast_t* ast2(tag_t typ, ast_t* a1, ast_t* a2) {
    ast_t* ast = new_ast();
    ast->typ = typ; ast->child = a1; a1->next = a2; ast->next = NULL;
    return ast;
}

sym_t * sym_lookup(const char * name) {
   sym_t * sym = (sym_t *) safe_malloc(sizeof(sym_t));
   sym->name = (char *) safe_malloc(strlen(name) + 1);
   strcpy(sym->name, name);
   return sym;
}

input_t * new_input() {
    input_t * in = (input_t *) safe_malloc(sizeof(input_t));
    in->buffer = NULL; in->alloc = 0; in->length = 0; in->start = 0; in->line = 1; in->col = 1;
    return in;
}

char read1(input_t * in) {
    if (in->buffer == NULL) {
        char linebuf[2048];
        if (fgets(linebuf, sizeof(linebuf), stdin) == NULL) {
            in->length = 0; in->start = 0; return EOF;
        }
        in->length = strlen(linebuf);
        in->alloc = in->length + 1;
        in->buffer = (char*)safe_malloc(in->alloc);
        strcpy(in->buffer, linebuf);
        in->start = 0; in->line = 1; in->col = 1;
    }
    if (in->start < in->length) {
        char c = in->buffer[in->start++];
        if (c == '\n') { in->line++; in->col = 1; } else { in->col++; }
        return c;
    }
    return EOF;
}

/*void skip_whitespace(input_t * in) {
   char c;
   while ((c = read1(in)) == ' ' || c == '\n' || c == '\t') ;
   if (c != EOF) { in->start--; if (c == '\n') { in->line--; } else { in->col--;} }
}
*/
//=============================================================================
// PRIMITIVE PARSING FUNCTIONS (THE `_fn` IMPLEMENTATIONS)
//=============================================================================

combinator_t * new_combinator() {
    combinator_t *comb = (combinator_t *) safe_malloc(sizeof(combinator_t));
    // Explicitly zero out the entire struct to avoid uninitialised value warnings
    memset(comb, 0, sizeof(combinator_t));
    comb->type = P_MATCH; // Default value, will be overridden
    comb->extra_to_free = NULL;
    return comb;
}

static ParseResult match_fn(input_t * in, void * args) {
    char * str = ((match_args *) args)->str;
    InputState state; save_input_state(in, &state);
    for (int i = 0, len = strlen(str); i < len; i++) {
        char c = read1(in);
        if (c != str[i]) {
            restore_input_state(in, &state);
            char* err_msg; asprintf(&err_msg, "Expected '%s'", str);
            return make_failure(in, err_msg);
        }
    }
    return make_success(ensure_ast_nil_initialized());
}

static ParseResult integer_fn(input_t * in, void * args) {
   prim_args* pargs = (prim_args*)args;
   InputState state; save_input_state(in, &state);
   int start_pos_ws = in->start;
   char c = read1(in);
   if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected a digit.")); }
   while (isdigit(c = read1(in))) ;
   if (c != EOF) in->start--;
   int len = in->start - start_pos_ws;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos_ws, len);
   text[len] = '\0';
   ast_t * ast = new_ast();
   ast->typ = pargs->tag; ast->sym = sym_lookup(text); free(text);
   ast->child = NULL; ast->next = NULL;
   return make_success(ast);
}

static ParseResult cident_fn(input_t * in, void * args) {
   prim_args* pargs = (prim_args*)args;
   InputState state; save_input_state(in, &state);
   int start_pos_ws = in->start;
   char c = read1(in);
   if (c != '_' && !isalpha(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected identifier."));}
   while (isalnum(c = read1(in)) || c == '_') ;
   if (c != EOF) in->start--;
   int len = in->start - start_pos_ws;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos_ws, len);
   text[len] = '\0';
   ast_t * ast = new_ast();
   ast->typ = pargs->tag; ast->sym = sym_lookup(text); free(text);
   ast->child = NULL; ast->next = NULL;
   return make_success(ast);
}

static ParseResult string_fn(input_t * in, void * args) {
   prim_args* pargs = (prim_args*)args;
   InputState state; save_input_state(in, &state);
   if (read1(in) != '"') { restore_input_state(in, &state); return make_failure(in, strdup("Expected '\"'.")); }
   int capacity = 64;
   char * str_val = (char *) safe_malloc(capacity);
   int len = 0; char c;
   while ((c = read1(in)) != '"') {
      if (c == EOF) { free(str_val); return make_failure(in, strdup("Unterminated string.")); }
      if (c == '\\') {
         c = read1(in);
         if (c == EOF) { free(str_val); return make_failure(in, strdup("Unterminated string.")); }
         switch (c) {
            case 'n': c = '\n'; break; case 't': c = '\t'; break;
            case '"': c = '"'; break; case '\\': c = '\\'; break;
         }
      }
      if (len + 1 >= capacity) {
         capacity *= 2;
         char* new_str_val = realloc(str_val, capacity);
         if (!new_str_val) { free(str_val); exception("realloc failed"); }
         str_val = new_str_val;
      }
      str_val[len++] = c;
   }
   str_val[len] = '\0';
   ast_t * ast = new_ast();
   ast->typ = pargs->tag; ast->sym = sym_lookup(str_val); free(str_val);
   ast->child = NULL; ast->next = NULL;
   return make_success(ast);
}

static ParseResult any_char_fn(input_t * in, void * args) {
    prim_args* pargs = (prim_args*)args;
    InputState state; save_input_state(in, &state);
    char c = read1(in);
    if (c == EOF) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected any character, but found EOF."));
    }
    char str[2] = {c, '\0'};
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(str);
    ast->child = NULL;
    ast->next = NULL;
    return make_success(ast);
}

static ParseResult satisfy_fn(input_t * in, void * args) {
    satisfy_args* sargs = (satisfy_args*)args;
    InputState state; save_input_state(in, &state);
    char c = read1(in);
    if (c == EOF || !sargs->pred(c)) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Predicate not satisfied."));
    }
    char str[2] = {c, '\0'};
    ast_t* ast = new_ast();
    ast->typ = sargs->tag;
    ast->sym = sym_lookup(str);
    ast->child = NULL;
    ast->next = NULL;
    return make_success(ast);
}

static ParseResult until_fn(input_t* in, void* args) {
    until_args* uargs = (until_args*)args;
    int start_offset = in->start;
    while(1) {
        InputState current_state; save_input_state(in, &current_state);
        ParseResult res = parse(in, uargs->delimiter);
        if (res.is_success) {
            if (res.value.ast != ensure_ast_nil_initialized()) free_ast(res.value.ast);
            restore_input_state(in, &current_state); break;
        }
        free_error(res.value.error);
        restore_input_state(in, &current_state);
        if (read1(in) == EOF) break;
    }
    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';
    ast_t* ast = new_ast();
    ast->typ = uargs->tag; ast->sym = sym_lookup(text); free(text);
    return make_success(ast);
}

static ParseResult expr_fn(input_t * in, void * args) {
   expr_list * list = (expr_list *) args;
   if (list == NULL) return make_failure(in, strdup("Invalid expression grammar."));
   if (list->fix == EXPR_BASE) return parse(in, list->comb);
   if (list->fix == EXPR_PREFIX) {
       op_t* op = list->op;
       if (op) {
           InputState state; save_input_state(in, &state);
           ParseResult op_res = parse(in, op->comb);
           if (op_res.is_success) {
               free_ast(op_res.value.ast);
               ParseResult rhs_res = expr_fn(in, args);
               if (!rhs_res.is_success) return rhs_res;
               return make_success(ast1(op->tag, rhs_res.value.ast));
           }
               free_error(op_res.value.error);
           restore_input_state(in, &state);
       }
   }
   ParseResult res = expr_fn(in, (void *) list->next);
   if (!res.is_success) return res;
   ast_t* lhs = res.value.ast;
   if (list->fix == EXPR_INFIX) {
       while (1) {
           InputState loop_state; save_input_state(in, &loop_state);
           op_t *op = list->op;
           bool found_op = false;
           while (op) {
               ParseResult op_res = parse(in, op->comb);
               if (op_res.is_success) {
                   tag_t op_tag = op->tag;
                   free_ast(op_res.value.ast);
                   ParseResult rhs_res = expr_fn(in, (void *) list->next);
                   if (!rhs_res.is_success) { free_ast(lhs); return rhs_res; }
                   lhs = ast2(op_tag, lhs, rhs_res.value.ast);
                   found_op = true;
                   break;
               }
               free_error(op_res.value.error);
               op = op->next;
           }
           if (!found_op) { restore_input_state(in, &loop_state); break; }
       }
   }
   return make_success(lhs);
}

static ParseResult lazy_fn(input_t * in, void * args) {
    lazy_args* largs = (lazy_args*)args;
    if (largs == NULL || largs->parser_ptr == NULL || *largs->parser_ptr == NULL) {
        exception("Lazy parser not initialized.");
    }
    return parse(in, *largs->parser_ptr);
}

//=============================================================================
// PRIMITIVE PARSER CREATION FUNCTIONS (THE PUBLIC API)
//=============================================================================

combinator_t * match(char * str) {
    match_args * args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t * comb = new_combinator();
    comb->type = P_MATCH; comb->fn = match_fn; comb->args = args; return comb;
}
combinator_t * integer(tag_t tag) {
    prim_args * args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t * comb = new_combinator();
    comb->type = P_INTEGER; comb->fn = integer_fn; comb->args = args; return comb;
}
combinator_t * cident(tag_t tag) {
    prim_args * args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t * comb = new_combinator();
    comb->type = P_CIDENT; comb->fn = cident_fn; comb->args = args; return comb;
}
combinator_t * string(tag_t tag) {
    prim_args * args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t * comb = new_combinator();
    comb->type = P_STRING;
    comb->fn = string_fn;
    comb->args = args;
    return comb;
}

combinator_t * satisfy(char_predicate pred, tag_t tag) {
    satisfy_args* args = (satisfy_args*)safe_malloc(sizeof(satisfy_args));
    args->pred = pred;
    args->tag = tag;
    combinator_t * comb = new_combinator();
    comb->type = P_SATISFY;
    comb->fn = satisfy_fn;
    comb->args = (void*)args;
    return comb;
}
combinator_t* until(combinator_t* p, tag_t tag) {
    until_args* args = (until_args*)safe_malloc(sizeof(until_args));
    args->delimiter = p;
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_UNTIL; comb->fn = until_fn; comb->args = args; return comb;
}

combinator_t * any_char(tag_t tag) {
    prim_args * args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t * comb = new_combinator();
    comb->type = P_ANY_CHAR;
    comb->fn = any_char_fn;
    comb->args = args;
    return comb;
}
combinator_t * expr(combinator_t * exp, combinator_t * base) {
   expr_list * args = (expr_list*)safe_malloc(sizeof(expr_list));
   args->next = NULL; args->fix = EXPR_BASE; args->comb = base; args->op = NULL;
   exp->type = COMB_EXPR; exp->fn = expr_fn; exp->args = args; return exp;
}
void expr_insert(combinator_t * exp, int prec, tag_t tag, expr_fix fix, expr_assoc assoc, combinator_t * comb) {
    expr_list *node = (expr_list*)safe_malloc(sizeof(expr_list));
    op_t *op = (op_t*)safe_malloc(sizeof(op_t));
    op->tag = tag; op->comb = comb; op->next = NULL;
    node->op = op; node->fix = fix; node->assoc = assoc; node->comb = NULL;
    expr_list **p_list = (expr_list**)&exp->args;
    for (int i = 0; i < prec; i++) {
        if (*p_list == NULL || (*p_list)->fix == EXPR_BASE) exception("Invalid precedence for expression");
        p_list = &(*p_list)->next;
    }
    node->next = *p_list; *p_list = node;
}
void expr_altern(combinator_t * exp, int prec, tag_t tag, combinator_t * comb) {
    expr_list* list = (expr_list*)exp->args;
    for (int i = 0; i < prec; i++) {
        if (list == NULL) exception("Invalid precedence for expression alternative");
        list = list->next;
    }
    if (list->fix == EXPR_BASE || list == NULL) exception("Invalid precedence");
    op_t* op = (op_t*)safe_malloc(sizeof(op_t));
    op->tag = tag; op->comb = comb; op->next = list->op;
    list->op = op;
}

//=============================================================================
// THE UNIVERSAL PARSE FUNCTION
//=============================================================================
ParseResult parse(input_t * in, combinator_t * comb) {
    if (!comb || !comb->fn) exception("Attempted to parse with a NULL or uninitialized combinator.");
    return comb->fn(in, (void *)comb->args);
}

combinator_t * lazy(combinator_t** parser_ptr) {
    lazy_args* args = (lazy_args*)safe_malloc(sizeof(lazy_args));
    args->parser_ptr = parser_ptr;
    combinator_t * comb = new_combinator();
    comb->type = COMB_LAZY;
    comb->fn = lazy_fn;
    comb->args = args;
    return comb;
}

//=============================================================================
// MEMORY MANAGEMENT
//=============================================================================

void free_error(ParseError* err) {
    if (err == NULL) return;
    free(err->message);
    free_error(err->cause);
    free(err);
}

void free_ast(ast_t* ast) {
    if (ast == NULL || ast == ensure_ast_nil_initialized()) return;
    free_ast(ast->child);
    free_ast(ast->next);
    if (ast->sym) { free(ast->sym->name); free(ast->sym); }
    free(ast);
}

// Initialize ast_nil if not already initialized
static ast_t* ensure_ast_nil_initialized() {
    if (ast_nil == NULL) {
        ast_nil = new_ast();
        ast_nil->typ = 0;
    }
    return ast_nil;
}


void parser_walk_ast(ast_t* ast, ast_visitor_fn visitor, void* context) {
    if (ast == NULL || ast == ensure_ast_nil_initialized()) {
        return;
    }

    visitor(ast, context);

    if (ast->child) {
        parser_walk_ast(ast->child, visitor, context);
    }

    if (ast->next) {
        parser_walk_ast(ast->next, visitor, context);
    }
}

typedef struct visited_node { const void* ptr; struct visited_node* next; } visited_node;

void free_combinator_recursive(combinator_t* comb, visited_node** visited);

bool is_visited(const void* ptr, visited_node* list) {
    for (visited_node* current = list; current != NULL; current = current->next) {
        if (current->ptr == ptr) return true;
    }
    return false;
}

void free_combinator(combinator_t* comb) {
    visited_node* visited = NULL;
    free_combinator_recursive(comb, &visited);
    visited_node* current = visited;
    while (current != NULL) {
        visited_node* temp = current;
        current = current->next;
        free(temp);
    }
}

void free_combinator_recursive(combinator_t* comb, visited_node** visited) {
    if (comb == NULL || is_visited(comb, *visited)) return;
    visited_node* new_visited = (visited_node*)safe_malloc(sizeof(visited_node));
    new_visited->ptr = comb;
    new_visited->next = *visited;
    *visited = new_visited;

    // Ensure type is valid to avoid uninitialised value warnings
    if (comb->type >= P_MATCH && comb->type <= COMB_ERRMAP) {
        // Type is valid, proceed with normal logic
    } else {
        // Type is invalid/uninitialised, set to default and free args if present
        comb->type = P_MATCH;
        if (comb->args != NULL) {
            free(comb->args);
            comb->args = NULL;
        }
        free(comb);
        return;
    }

    if (comb->extra_to_free) {
        free(comb->extra_to_free);
        comb->extra_to_free = NULL;
    }

    if (comb->args != NULL) {
        switch (comb->type) {
            case P_CI_KEYWORD:
                free(comb->args);
                break;
            case P_MATCH:
                free((match_args*)comb->args);
                break;
            case COMB_EXPECT: {
                expect_args* args = (expect_args*)comb->args;
                free_combinator_recursive(args->comb, visited);
                free(args);
                break;
            }
            case COMB_OPTIONAL: {
                optional_args* args = (optional_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free(args);
                break;
            }
            case COMB_ERRMAP: {
                errmap_args* args = (errmap_args*)comb->args;
                free_combinator_recursive(args->parser, visited);
                free(args);
                break;
            }
            case COMB_MAP: {
                map_args* args = (map_args*)comb->args;
                free_combinator_recursive(args->parser, visited);
                free(args);
                break;
            }
            case P_SUCCEED: {
                succeed_args* args = (succeed_args*)comb->args;
                free_ast(args->ast);
                free(args);
                break;
            }
            case COMB_CHAINL1: {
                chainl1_args* args = (chainl1_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free_combinator_recursive(args->op, visited);
                free(args);
                break;
            }
            case COMB_SEP_END_BY: {
                sep_end_by_args* args = (sep_end_by_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free_combinator_recursive(args->sep, visited);
                free(args);
                break;
            }
            case COMB_SEP_BY: {
                sep_by_args* args = (sep_by_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free_combinator_recursive(args->sep, visited);
                free(args);
                break;
            }
            case COMB_NOT: {
                not_args* args = (not_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free(args);
                break;
            }
            case COMB_PEEK: {
                peek_args* args = (peek_args*)comb->args;
                free_combinator_recursive(args->p, visited);
                free(args);
                break;
            }
            case COMB_BETWEEN: {
                between_args* args = (between_args*)comb->args;
                free_combinator_recursive(args->open, visited);
                free_combinator_recursive(args->close, visited);
                free_combinator_recursive(args->p, visited);
                free(args);
                break;
            }
            case COMB_GSEQ:
            case COMB_SEQ:
            case COMB_MULTI: {
                seq_args* args = (seq_args*)comb->args;
                seq_list* current = args->list;
                while (current != NULL) {
                    free_combinator_recursive(current->comb, visited);
                    seq_list* temp = current;
                    current = current->next;
                    free(temp);
                }
                free(args);
                break;
            }
            case COMB_FLATMAP: {
                flatMap_args* args = (flatMap_args*)comb->args;
                free_combinator_recursive(args->parser, visited);
                free(args);
                break;
            }
            case P_UNTIL: {
                until_args* args = (until_args*)comb->args;
                free_combinator_recursive(args->delimiter, visited);
                free(args);
                break;
            }
            case COMB_LAZY: {
                free((lazy_args*)comb->args);
                break;
            }
            case COMB_EXPR: {
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
                break;
            }
            case P_SATISFY: {
                free((satisfy_args*)comb->args);
                break;
            }
            case COMB_LEFT:
            case COMB_RIGHT: {
                pair_args* args = (pair_args*)comb->args;
                free_combinator_recursive(args->p1, visited);
                free_combinator_recursive(args->p2, visited);
                free(args);
                break;
            }
            case COMB_MANY: {
                free_combinator_recursive((combinator_t*)comb->args, visited);
                break;
            }
            // P_INTEGER, P_CIDENT, P_STRING, P_ANY_CHAR have NULL args
            default:
                break;
        }
    }
    free(comb);
}
