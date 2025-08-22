#include "parser.h"

//=============================================================================
// Internal-Only Struct Definitions
//=============================================================================

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

typedef struct {
    combinator_t * parser;
    flatMap_func func;
} flatMap_args;

// --- Expression Parser Structs ---
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
// GLOBAL STATE & HELPER FUNCTIONS
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


//=============================================================================
// PARSING LOGIC FUNCTIONS (THE `_fn` IMPLEMENTATIONS)
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
   ast_t* lhs;

   if (list == NULL) return NULL;

   if (list->fix == EXPR_BASE) {
       return parse(in, list->comb);
   }

   if (list->fix == EXPR_PREFIX) {
       op_t* op = list->op;
       if (op) {
           int start_pos = in->start;
           if (parse(in, op->comb)) {
               ast_t* rhs = expr_fn(in, args);
               if (!rhs) exception("Expression expected after prefix operator");
               return ast1(op->tag, rhs);
           }
           in->start = start_pos;
       }
   }

   lhs = expr_fn(in, (void *) list->next);
   if (!lhs) return NULL;

   if (list->fix == EXPR_INFIX) {
       while (1) {
           op_t *op = list->op;
           bool found_op = false;
           while (op) {
               int start_pos = in->start;
               if (parse(in, op->comb)) {
                   ast_t *rhs = expr_fn(in, (void *) list->next);
                   if (!rhs) exception("Expression expected after operator!");
                   lhs = ast2(op->tag, lhs, rhs);
                   found_op = true;
                   break;
               }
               in->start = start_pos;
               op = op->next;
           }
           if (!found_op) break;
       }
   }

   return lhs;
}


//=============================================================================
// COMBINATOR CREATION FUNCTIONS (THE PUBLIC API)
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
    expr_list *node = (expr_list*)safe_malloc(sizeof(expr_list));
    op_t *op = (op_t*)safe_malloc(sizeof(op_t));
    op->tag = tag;
    op->comb = comb;
    op->next = NULL;
    node->op = op;
    node->fix = fix;
    node->assoc = assoc;
    node->comb = NULL;

    expr_list **p_list = (expr_list**)&exp->args;
    for (int i = 0; i < prec; i++) {
        if (*p_list == NULL || (*p_list)->fix == EXPR_BASE) {
            exception("Invalid precedence for expression");
        }
        p_list = &(*p_list)->next;
    }
    node->next = *p_list;
    *p_list = node;
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
// THE UNIVERSAL PARSE FUNCTION
//=============================================================================
ast_t * parse(input_t * in, combinator_t * comb) {
    if (!comb || !comb->fn) {
        exception("Attempted to parse with a NULL or uninitialized combinator.");
    }
    return comb->fn(in, (void *)comb->args);
}


//=============================================================================
// MEMORY MANAGEMENT
//=============================================================================

void free_ast(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) {
        return;
    }
    free_ast(ast->child);
    free_ast(ast->next);
    if (ast->sym) {
        free(ast->sym->name);
        free(ast->sym);
    }
    free(ast);
}

typedef struct visited_node {
    const void* ptr;
    struct visited_node* next;
} visited_node;

void free_combinator_recursive(combinator_t* comb, visited_node** visited);

bool is_visited(const void* ptr, visited_node* list) {
    for (visited_node* current = list; current != NULL; current = current->next) {
        if (current->ptr == ptr) {
            return true;
        }
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
    if (comb == NULL || is_visited(comb, *visited)) {
        return;
    }

    visited_node* new_visited = (visited_node*)safe_malloc(sizeof(visited_node));
    new_visited->ptr = comb;
    new_visited->next = *visited;
    *visited = new_visited;

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
    
    free(comb);
}
