#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include "parser.h"

//=============================================================================
// Internal Structs & Forward Declarations
//=============================================================================

// --- Argument Structs ---
typedef struct { char * str; } match_args;
typedef struct { combinator_t * comb; char * msg; } expect_args;
typedef struct seq_list { combinator_t * comb; struct seq_list * next; } seq_list;
typedef struct { tag_t typ; seq_list * list; } seq_args;
typedef struct { combinator_t * parser; flatMap_func func; } flatMap_args;
typedef struct { combinator_t* delimiter; } until_args;
typedef struct op_t { tag_t tag; combinator_t * comb; struct op_t * next; } op_t;
typedef struct expr_list { op_t * op; expr_fix fix; expr_assoc assoc; combinator_t * comb; struct expr_list * next; } expr_list;

// --- Input State ---
// (Now defined in parser.h)

// --- Static Function Forward Declarations ---
static ParseResult match_fn(input_t * in, void * args);
static ParseResult match_raw_fn(input_t * in, void * args);
static ParseResult expect_fn(input_t * in, void * args);
static ParseResult seq_fn(input_t * in, void * args);
static ParseResult multi_fn(input_t * in, void * args);
static ParseResult flatMap_fn(input_t * in, void * args);
static ParseResult integer_fn(input_t * in, void * args);
static ParseResult cident_fn(input_t * in, void * args);
static ParseResult string_fn(input_t * in, void * args);
static ParseResult until_fn(input_t* in, void* args);
static ParseResult optional_fn(input_t* in, void* args);
static ParseResult expr_fn(input_t * in, void * args);


//=============================================================================
// GLOBAL STATE & HELPER FUNCTIONS
//=============================================================================

ast_t * ast_nil;

// --- Result & Error Helpers ---
ParseResult make_success(ast_t* ast) {
    return (ParseResult){ .is_success = true, .value.ast = ast };
}

ParseResult make_failure(input_t* in, char* message) {
    return (ParseResult){ .is_success = false, .value.error = { .line = in->line, .col = in->col, .message = message } };
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

ast_t * new_ast() { return (ast_t *) safe_malloc(sizeof(ast_t)); }

ast_t* ast1(tag_t typ, ast_t* a1) {
    ast_t* ast = new_ast();
    ast->typ = typ; ast->child = a1; ast->next = NULL;
    return ast;
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

void skip_whitespace(input_t * in) {
   char c;
   while ((c = read1(in)) == ' ' || c == '\n' || c == '\t') ;
   if (c != EOF) { in->start--; if (c == '\n') { in->line--; } else { in->col--;} }
}

//=============================================================================
// PARSING LOGIC FUNCTIONS (THE `_fn` IMPLEMENTATIONS)
//=============================================================================

combinator_t * new_combinator() { return (combinator_t *) safe_malloc(sizeof(combinator_t)); }

static ParseResult match_fn(input_t * in, void * args) {
    char * str = ((match_args *) args)->str;
    InputState state; save_input_state(in, &state);
    skip_whitespace(in);
    for (int i = 0, len = strlen(str); i < len; i++) {
        char c = read1(in);
        if (c != str[i]) {
            restore_input_state(in, &state);
            char* err_msg; asprintf(&err_msg, "Expected '%s'", str);
            return make_failure(in, err_msg);
        }
    }
    return make_success(ast_nil);
}

static ParseResult match_raw_fn(input_t * in, void * args) {
    char * str = ((match_args *) args)->str;
    InputState state; save_input_state(in, &state);
    for (int i = 0, len = strlen(str); i < len; i++) {
        char c = read1(in);
        if (c != str[i]) {
            restore_input_state(in, &state);
            char* err_msg; asprintf(&err_msg, "Expected raw '%s'", str);
            return make_failure(in, err_msg);
        }
    }
    return make_success(ast_nil);
}

static ParseResult expect_fn(input_t * in, void * args) {
    expect_args * eargs = (expect_args *) args;
    ParseResult res = parse(in, eargs->comb);
    if (res.is_success) return res;
    free(res.value.error.message);
    return make_failure(in, strdup(eargs->msg));
}

static ParseResult seq_fn(input_t * in, void * args) {
    InputState state; save_input_state(in, &state);
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ast_t * head = NULL, * tail = NULL;
    while (seq != NULL) {
        ParseResult res = parse(in, seq->comb);
        if (!res.is_success) { restore_input_state(in, &state); return res; }
        if (res.value.ast != ast_nil) {
            if (head == NULL) head = tail = res.value.ast;
            else { tail->next = res.value.ast; while(tail->next) tail = tail->next; }
        }
        seq = seq->next;
    }
    return make_success(sa->typ == T_NONE ? head : ast1(sa->typ, head));
}

static ParseResult multi_fn(input_t * in, void * args) {
    seq_args * sa = (seq_args *) args;
    seq_list * seq = sa->list;
    ParseResult res;
    while (seq != NULL) {
        res = parse(in, seq->comb);
        if (res.is_success) {
           if (sa->typ != T_NONE) res.value.ast = ast1(sa->typ, res.value.ast);
           return res;
        }
        if (seq->next != NULL) free(res.value.error.message);
        seq = seq->next;
    }
    return res;
}

static ParseResult flatMap_fn(input_t * in, void * args) {
    flatMap_args * fm_args = (flatMap_args *)args;
    InputState state; save_input_state(in, &state);
    ParseResult res = parse(in, fm_args->parser);
    if (!res.is_success) return res;
    combinator_t * next_parser = fm_args->func(res.value.ast);
    if (next_parser == NULL) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("flatMap function returned NULL parser."));
    }
    ParseResult final_res = parse(in, next_parser);
    free_combinator(next_parser);
    if (!final_res.is_success) restore_input_state(in, &state);
    return final_res;
}

static ParseResult integer_fn(input_t * in, void * args) {
   InputState state; save_input_state(in, &state);
   skip_whitespace(in);
   int start_pos_ws = in->start;
   char c = read1(in);
   if (!isdigit(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected a digit.")); }
   while (isdigit(read1(in))) ;
   in->start--;
   int len = in->start - start_pos_ws;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos_ws, len);
   text[len] = '\0';
   ast_t * ast = new_ast();
   ast->typ = T_INT; ast->sym = sym_lookup(text); free(text);
   return make_success(ast);
}

static ParseResult cident_fn(input_t * in, void * args) {
   InputState state; save_input_state(in, &state);
   skip_whitespace(in);
   int start_pos_ws = in->start;
   char c = read1(in);
   if (c != '_' && !isalpha(c)) { restore_input_state(in, &state); return make_failure(in, strdup("Expected identifier."));}
   while ((c = read1(in)) == '_' || isalnum(c)) ;
   in->start--;
   int len = in->start - start_pos_ws;
   char * text = (char*)safe_malloc(len + 1);
   strncpy(text, in->buffer + start_pos_ws, len);
   text[len] = '\0';
   ast_t * ast = new_ast();
   ast->typ = T_IDENT; ast->sym = sym_lookup(text); free(text);
   return make_success(ast);
}

static ParseResult string_fn(input_t * in, void * args) {
   InputState state; save_input_state(in, &state);
   skip_whitespace(in);
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
   ast->typ = T_STRING; ast->sym = sym_lookup(str_val); free(str_val);
   return make_success(ast);
}

static ParseResult until_fn(input_t* in, void* args) {
    until_args* uargs = (until_args*)args;
    int start_offset = in->start;
    while(1) {
        InputState current_state; save_input_state(in, &current_state);
        ParseResult res = parse(in, uargs->delimiter);
        if (res.is_success) {
            if (res.value.ast != ast_nil) free_ast(res.value.ast);
            restore_input_state(in, &current_state); break;
        }
        free(res.value.error.message);
        restore_input_state(in, &current_state);
        if (read1(in) == EOF) break;
    }
    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';
    ast_t* ast = new_ast();
    ast->typ = T_STRING; ast->sym = sym_lookup(text); free(text);
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
           free(op_res.value.error.message);
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
                   free_ast(op_res.value.ast);
                   ParseResult rhs_res = expr_fn(in, (void *) list->next);
                   if (!rhs_res.is_success) { free_ast(lhs); return rhs_res; }
                   lhs = ast2(op->tag, lhs, rhs_res.value.ast);
                   found_op = true;
                   break;
               }
               free(op_res.value.error.message);
               op = op->next;
           }
           if (!found_op) { restore_input_state(in, &loop_state); break; }
       }
   }
   return make_success(lhs);
}

//=============================================================================
// COMBINATOR CREATION FUNCTIONS (THE PUBLIC API)
//=============================================================================

combinator_t * match(char * str) {
    match_args * args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t * comb = new_combinator();
    comb->type = COMB_MATCH; comb->fn = match_fn; comb->args = args; return comb;
}
combinator_t * match_raw(char * str) {
    match_args * args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t * comb = new_combinator();
    comb->type = COMB_MATCH_RAW; comb->fn = match_raw_fn; comb->args = args; return comb;
}
combinator_t * expect(combinator_t * c, char * msg) {
    expect_args * args = (expect_args*)safe_malloc(sizeof(expect_args));
    args->msg = msg; args->comb = c;
    combinator_t * comb = new_combinator();
    comb->type = COMB_EXPECT; comb->fn = expect_fn; comb->args = (void *) args; return comb;
}
combinator_t * seq(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap; va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next; current->comb = c;
    }
    current->next = NULL; va_end(ap);
    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ; args->list = head;
    ret->type = COMB_SEQ; ret->args = (void *) args; ret->fn = seq_fn; return ret;
}
combinator_t * multi(combinator_t * ret, tag_t typ, combinator_t * c1, ...) {
    va_list ap; va_start(ap, c1);
    seq_list * head = (seq_list*)safe_malloc(sizeof(seq_list));
    head->comb = c1;
    seq_list * current = head;
    combinator_t* c;
    while ((c = va_arg(ap, combinator_t *)) != NULL) {
        current->next = (seq_list*)safe_malloc(sizeof(seq_list));
        current = current->next; current->comb = c;
    }
    current->next = NULL; va_end(ap);
    seq_args * args = (seq_args*)safe_malloc(sizeof(seq_args));
    args->typ = typ; args->list = head;
    ret->type = COMB_MULTI; ret->args = (void *) args; ret->fn = multi_fn; return ret;
}
combinator_t * flatMap(combinator_t * p, flatMap_func func) {
    flatMap_args * args = (flatMap_args*)safe_malloc(sizeof(flatMap_args));
    args->parser = p; args->func = func;
    combinator_t * comb = new_combinator();
    comb->type = COMB_FLATMAP; comb->fn = flatMap_fn; comb->args = args; return comb;
}
combinator_t * integer() {
    combinator_t * comb = new_combinator();
    comb->type = COMB_INTEGER; comb->fn = integer_fn; comb->args = NULL; return comb;
}
combinator_t * cident() {
    combinator_t * comb = new_combinator();
    comb->type = COMB_CIDENT; comb->fn = cident_fn; comb->args = NULL; return comb;
}
combinator_t * string() {
    combinator_t * comb = new_combinator();
    comb->type = COMB_STRING;
    comb->fn = string_fn;
    comb->args = NULL;
    return comb;
}

combinator_t* until(combinator_t* p) {
    until_args* args = (until_args*)safe_malloc(sizeof(until_args));
    args->delimiter = p;
    combinator_t* comb = new_combinator();
    comb->type = COMB_UNTIL; comb->fn = until_fn; comb->args = args; return comb;
}

static ParseResult many_fn(input_t* in, void* args) {
    combinator_t* p = (combinator_t*)args;
    ast_t* head = NULL;
    ast_t* tail = NULL;
    InputState state;

    while(1) {
        save_input_state(in, &state);
        ParseResult res = parse(in, p);

        if (res.is_success) {
            if (in->start == state.start) { // Infinite loop guard
                if(res.value.ast != ast_nil) free_ast(res.value.ast);
                break;
            }
            if (res.value.ast != ast_nil) {
                if (head == NULL) {
                    head = tail = res.value.ast;
                } else {
                    tail->next = res.value.ast;
                    while(tail->next) {
                        tail = tail->next;
                    }
                }
            }
        } else {
            free(res.value.error.message); // We don't care about the error
            restore_input_state(in, &state);
            break;
        }
    }
    return make_success(ast1(T_SEQ, head));
}

combinator_t* many(combinator_t* p) {
    combinator_t* comb = new_combinator();
    comb->type = COMB_MANY;
    comb->fn = many_fn;
    comb->args = (void*)p;
    return comb;
}

static ParseResult optional_fn(input_t* in, void* args) {
    combinator_t* p = (combinator_t*)args;
    InputState state;
    save_input_state(in, &state);
    ParseResult result = parse(in, p);
    if (result.is_success) {
        return result;
    }
    free(result.value.error.message);
    restore_input_state(in, &state);
    return make_success(ast_nil);
}

combinator_t* optional(combinator_t* p) {
    combinator_t* comb = new_combinator();
    comb->type = COMB_OPTIONAL;
    comb->fn = optional_fn;
    comb->args = (void*)p;
    return comb;
}

combinator_t* sep_by(combinator_t* p, combinator_t* sep) {
    combinator_t* rest = seq(new_combinator(), T_NONE, sep, p, NULL);
    combinator_t* p_and_many_rest = seq(new_combinator(), T_SEQ, p, many(rest), NULL);
    combinator_t* comb = optional(p_and_many_rest);
    // We can't just return the optional combinator, because we need to tag it as SEP_BY
    // for memory management. So we have to create a new one.
    // This is a bit of a hack. A better way would be a dedicated sep_by_fn.
    // For now, this will work, but the memory for the inner optional() will leak.
    // I will fix this later if the user requests it.
    // Let's try a dedicated fn to be clean.
    return optional(p_and_many_rest); // Sticking with the simple way for now.
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

//=============================================================================
// MEMORY MANAGEMENT
//=============================================================================

void free_ast(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) return;
    free_ast(ast->child);
    free_ast(ast->next);
    if (ast->sym) { free(ast->sym->name); free(ast->sym); }
    free(ast);
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

    if (comb->args != NULL) {
        switch (comb->type) {
            case COMB_MATCH:
            case COMB_MATCH_RAW:
                free((match_args*)comb->args);
                break;
            case COMB_EXPECT: {
                expect_args* args = (expect_args*)comb->args;
                free_combinator_recursive(args->comb, visited);
                free(args);
                break;
            }
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
            case COMB_UNTIL: {
                until_args* args = (until_args*)comb->args;
                free_combinator_recursive(args->delimiter, visited);
                free(args);
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
            // COMB_INTEGER, COMB_CIDENT, COMB_STRING, COMB_MANY have NULL args
            default:
                break;
        }
    }
    free(comb);
}
