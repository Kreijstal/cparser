#include "pascal_parser.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

// --- Forward Declarations ---
static combinator_t* token(combinator_t* p);
static combinator_t* keyword(const char* s);
static ParseResult p_match_ci(input_t* in, void* args);
static combinator_t* p_pascal_string();
static ParseResult p_pascal_string_fn(input_t* in, void* args);

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

static combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, PASCAL_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, PASCAL_T_NONE))));
}

static ParseResult p_match_ci(input_t* in, void* args) {
    const char* to_match = (const char*)args;
    int len = strlen(to_match);
    if (in->start + len > in->length) {
        char* err_msg;
        asprintf(&err_msg, "Expected keyword '%s'", to_match);
        return make_failure(in, err_msg);
    }
    for (int i = 0; i < len; i++) {
        if (tolower((unsigned char)in->buffer[in->start + i]) != tolower((unsigned char)to_match[i])) {
            char* err_msg;
            asprintf(&err_msg, "Expected keyword '%s'", to_match);
            return make_failure(in, err_msg);
        }
    }
    in->start += len;
    return make_success(ast_nil);
}

static combinator_t* keyword(const char* s) {
    combinator_t *c = new_combinator();
    c->type = P_CI_KEYWORD;
    c->fn = p_match_ci;
    c->args = (void*)strdup(s); // The args field now holds the string to be freed
    c->extra_to_free = NULL;
    return c;
}

// --- Primitive Parsers ---
static combinator_t* p_ident() {
    return token(cident(PASCAL_T_IDENT));
}

static combinator_t* p_program_kw() {
    return token(keyword("program"));
}

static combinator_t* p_begin_kw() {
    return token(keyword("begin"));
}

static combinator_t* p_end_kw() {
    return token(keyword("end"));
}

static combinator_t* p_semicolon() {
    return token(match(";"));
}

static combinator_t* p_dot() {
    return token(match("."));
}

static combinator_t* p_int_num() {
    return token(integer(PASCAL_T_INT_NUM));
}

static combinator_t* p_plus() {
    return token(match("+"));
}

static combinator_t* p_minus() {
    return token(match("-"));
}

static combinator_t* p_star() {
    return token(match("*"));
}

static combinator_t* p_slash() {
    return token(match("/"));
}

static combinator_t* p_gt() {
    return token(match(">"));
}

static combinator_t* p_lt() {
    return token(match("<"));
}

static combinator_t* p_eq() {
    return token(match("="));
}

static combinator_t* p_if_kw() {
    return token(keyword("if"));
}

static combinator_t* p_then_kw() {
    return token(keyword("then"));
}

static combinator_t* p_else_kw() {
    return token(keyword("else"));
}

static combinator_t* p_for_kw() {
    return token(keyword("for"));
}

static combinator_t* p_to_kw() {
    return token(keyword("to"));
}

static combinator_t* p_downto_kw() {
    return token(keyword("downto"));
}

static combinator_t* p_do_kw() {
    return token(keyword("do"));
}

static combinator_t* p_mod_kw() {
    return token(keyword("mod"));
}

static combinator_t* p_assign() {
    return token(match(":="));
}

// Custom parser for single-quoted strings
static ParseResult p_pascal_string_fn(input_t * in, void * args) {
   prim_args* pargs = (prim_args*)args;
   InputState state; save_input_state(in, &state);
   if (read1(in) != '\'') { restore_input_state(in, &state); return make_failure(in, strdup("Expected '\\''.")); }
   int capacity = 64;
   char * str_val = (char *) safe_malloc(capacity);
   int len = 0; char c;
   while ((c = read1(in)) != '\'') {
      if (c == EOF) { free(str_val); return make_failure(in, strdup("Unterminated string.")); }
      // Pascal uses two single quotes to represent a single quote inside a string.
      if (c == '\'') {
         if (in->start < in->length && in->buffer[in->start] == '\'') {
            in->start++; // consume the second single quote
         } else {
            free(str_val);
            return make_failure(in, strdup("Unterminated string."));
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

static combinator_t* p_pascal_string() {
    prim_args * args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = PASCAL_T_STRING;
    combinator_t * comb = new_combinator();
    comb->type = P_STRING; // This is a bit of a hack, the type is not really P_STRING
    comb->fn = p_pascal_string_fn;
    comb->args = args;
    return token(comb);
}

static combinator_t* p_colon() {
    return token(match(":"));
}

static combinator_t* p_var_kw() {
    return token(keyword("var"));
}

static combinator_t* p_lparen() {
    return token(match("("));
}

static combinator_t* p_rparen() {
    return token(match(")"));
}

static combinator_t* p_comma() {
    return token(match(","));
}

combinator_t* p_identifier_list() {
    return seq(new_combinator(), PASCAL_T_IDENT_LIST,
        sep_by(p_ident(), p_comma()),
        NULL
    );
}

// --- ASM Block Parser ---

// Custom parser function to consume the body of an ASM block.
// It consumes input until it finds the keyword "end".
static ParseResult p_asm_body_fn(input_t* in, void* args) {
    int start_offset = in->start;
    while (in->start + 3 <= in->length) {
        if (strncasecmp(in->buffer + in->start, "end", 3) == 0) {
            break;
        }
        in->start++;
    }

    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';

    ast_t* ast = new_ast();
    ast->typ = PASCAL_T_ASM_BLOCK;
    ast->sym = sym_lookup(text);
    free(text);

    return make_success(ast);
}

static combinator_t* p_asm_body() {
    combinator_t* c = new_combinator();
    c->fn = p_asm_body_fn;
    return c;
}

combinator_t* p_asm_block() {
    return seq(new_combinator(), PASCAL_T_ASM_BLOCK,
        keyword("asm"),
        p_asm_body(),
        keyword("end"),
        p_semicolon(),
        NULL
    );
}


// --- Expression Parser ---
combinator_t* p_expression() {
    combinator_t* expr_parser = new_combinator();

    // The base of the expression grammar (a "factor")
    // Create a lazy reference for recursion with proper memory management
    combinator_t** expr_parser_ref = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *expr_parser_ref = expr_parser;
    expr_parser->extra_to_free = expr_parser_ref;
    
    combinator_t* factor = multi(new_combinator(), PASCAL_T_NONE,
        p_int_num(),
        p_ident(),
        between(p_lparen(), p_rparen(), lazy(expr_parser_ref)),
        NULL
    );

    // Initialize the expression parser with the factor as the base
    expr(expr_parser, factor);

    // Add operators with precedence (higher number = higher precedence)
    // Level 0: >, <, =
    expr_insert(expr_parser, 0, PASCAL_T_GT, EXPR_INFIX, ASSOC_LEFT, p_gt());
    expr_altern(expr_parser, 0, PASCAL_T_LT, p_lt());
    expr_altern(expr_parser, 0, PASCAL_T_EQ, p_eq());
    // Level 1: +, -
    expr_insert(expr_parser, 1, PASCAL_T_ADD, EXPR_INFIX, ASSOC_LEFT, p_plus());
    expr_altern(expr_parser, 1, PASCAL_T_SUB, p_minus());
    // Level 2: *, /
    expr_insert(expr_parser, 2, PASCAL_T_MUL, EXPR_INFIX, ASSOC_LEFT, p_star());
    expr_altern(expr_parser, 2, PASCAL_T_DIV, p_slash());
    expr_altern(expr_parser, 2, PASCAL_T_MOD, p_mod_kw());
    // Level 3: unary minus (prefix) - higher precedence than multiplication/division
    expr_insert(expr_parser, 3, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, p_minus());

    return expr_parser;
}


// --- Declaration Parser ---

// Helper map functions to convert keyword ASTs to type ASTs
static ast_t* map_to_integer_type(ast_t* ast) {
    free_ast(ast); // free the keyword ast
    return ast1(PASCAL_T_TYPE_INTEGER, ast_nil);
}

static ast_t* map_to_real_type(ast_t* ast) {
    free_ast(ast);
    return ast1(PASCAL_T_TYPE_REAL, ast_nil);
}

static combinator_t* p_standard_type() {
    return multi(new_combinator(), PASCAL_T_NONE,
        map(keyword("integer"), map_to_integer_type),
        map(keyword("real"), map_to_real_type),
        NULL
    );
}

static combinator_t* p_var_declaration_line() {
    return seq(new_combinator(), PASCAL_T_VAR_DECL,
        p_identifier_list(),
        p_colon(),
        p_standard_type(),
        p_semicolon(),
        NULL
    );
}

combinator_t* p_declarations() {
    return right(
        p_var_kw(),
        many(p_var_declaration_line())
    );
}


// --- Program Structure Parser ---

// The arguments to a procedure/function call are expressions.
// We also need to allow strings as arguments.
static combinator_t* p_arg() {
    return multi(new_combinator(), PASCAL_T_NONE,
        p_expression(),
        p_pascal_string(),
        NULL
    );
}

static combinator_t* p_arg_list() {
    return between(p_lparen(), p_rparen(), sep_by(p_arg(), p_comma()));
}

// This function will be used to transform the AST of a procedure call.
// The raw AST from the seq parser will be:
//   PASCAL_T_PROC_CALL
//     |
//     child -> PASCAL_T_IDENT (the procedure name)
//            -> next -> (the argument list)
// We want to move the procedure name to the `sym` field of the root node.
static ast_t* map_proc_call(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) return ast;
    ast_t* ident_node = ast->child;
    if(ident_node == NULL || ident_node == ast_nil) return ast; // Should not happen
    ast_t* arg_list_node = ident_node->next;
    ast->sym = ident_node->sym;
    ast->child = arg_list_node;
    ident_node->sym = NULL; // prevent double free
    ident_node->next = NULL;
    free_ast(ident_node);
    return ast;
}

static combinator_t* p_proc_call() {
    return map(
        seq(new_combinator(), PASCAL_T_PROC_CALL,
            p_ident(),
            optional(p_arg_list()),
            NULL
        ),
        map_proc_call
    );
}

static combinator_t* p_proc_call_statement() {
    return left(p_proc_call(), p_semicolon());
}

static combinator_t* p_assignment_statement() {
    return seq(new_combinator(), PASCAL_T_ASSIGN,
        p_ident(),
        p_assign(),
        p_expression(),
        NULL
    );
}

combinator_t* p_if_statement() {
    // Create a lazy reference to the statement parser for recursion
    combinator_t** p_statement_ref = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *p_statement_ref = new_combinator();
    (*p_statement_ref)->extra_to_free = p_statement_ref;
    
    // Initialize the statement parser with all statement types
    multi(*p_statement_ref, PASCAL_T_NONE,
        p_asm_block(),
        p_proc_call_statement(),
        p_assignment_statement(),
        NULL
    );
    
    return seq(new_combinator(), PASCAL_T_IF,
        p_if_kw(),
        p_expression(),
        p_then_kw(),
        lazy(p_statement_ref),
        optional(
            seq(new_combinator(), PASCAL_T_NONE,
                p_else_kw(),
                lazy(p_statement_ref),
                NULL
            )
        ),
        NULL
    );
}

// For now, a statement is just an asm block.
combinator_t* p_statement() {
    return multi(new_combinator(), PASCAL_T_NONE,
        p_if_statement(),
        p_for_statement(),
        p_asm_block(),
        p_proc_call_statement(),
        p_assignment_statement(),
        NULL
    );
}

static combinator_t* p_statement_list() {
    return sep_end_by(p_statement(), p_semicolon());
}

combinator_t* p_for_statement() {
    // Create a lazy reference to the statement parser for recursion
    combinator_t** p_statement_ref = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *p_statement_ref = new_combinator();
    (*p_statement_ref)->extra_to_free = p_statement_ref;
    
    // Initialize the statement parser with all statement types
    multi(*p_statement_ref, PASCAL_T_NONE,
        p_if_statement(),
        p_asm_block(),
        p_proc_call_statement(),
        p_assignment_statement(),
        NULL
    );
    
    return seq(new_combinator(), PASCAL_T_FOR,
        p_for_kw(),
        p_ident(),
        p_assign(),
        p_expression(),
        multi(new_combinator(), PASCAL_T_NONE,
            seq(new_combinator(), PASCAL_T_FOR_TO,
                p_to_kw(),
                p_expression(),
                NULL
            ),
            seq(new_combinator(), PASCAL_T_FOR_DOWNTO,
                p_downto_kw(),
                p_expression(),
                NULL
            ),
            NULL
        ),
        p_do_kw(),
        lazy(p_statement_ref),
        NULL
    );
}

combinator_t* p_program() {
    return seq(new_combinator(), PASCAL_T_PROGRAM,
        p_program_kw(),
        p_ident(),
        optional(between(p_lparen(), p_rparen(), sep_by(p_ident(), p_comma()))),
        p_semicolon(),
        optional(p_declarations()),
        // Subprogram declarations would go here
        p_begin_kw(),
        p_statement_list(),
        p_end_kw(),
        p_dot(),
        NULL
    );
}
