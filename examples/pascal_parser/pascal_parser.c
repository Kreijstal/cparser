#include "pascal_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

combinator_t* token(combinator_t* p) {
    combinator_t* ws = many(satisfy(is_whitespace_char, PASCAL_T_NONE));
    return right(ws, left(p, many(satisfy(is_whitespace_char, PASCAL_T_NONE))));
}

// Custom parser for real numbers (e.g., 3.14, 2.0)
static ParseResult real_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    int start_pos = in->start;
    
    // Parse integer part
    char c = read1(in);
    if (!isdigit(c)) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected digit"));
    }
    
    while (isdigit(c = read1(in)));
    if (c != EOF) in->start--; // Back up one if not EOF
    
    // Must have decimal point
    if (read1(in) != '.') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected decimal point"));
    }
    
    // Parse fractional part (at least one digit required)
    c = read1(in);
    if (!isdigit(c)) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected digit after decimal point"));
    }
    
    while (isdigit(c = read1(in)));
    if (c != EOF) in->start--; // Back up one if not EOF
    
    // Create AST node with the real number value
    int len = in->start - start_pos;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_pos, len);
    text[len] = '\0';
    
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(text);
    free(text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    return make_success(ast);
}

static combinator_t* real_number(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = real_fn;
    comb->args = args;
    return comb;
}

// Custom parser for character literals (e.g., 'A', 'x')
static ParseResult char_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Must start with single quote
    if (read1(in) != '\'') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected single quote"));
    }
    
    // Must have at least one character
    char char_value = read1(in);
    if (char_value == EOF) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Unterminated character literal"));
    }
    
    // Must end with single quote  
    if (read1(in) != '\'') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected closing single quote"));
    }
    
    // Create AST node with the character value
    char text[2];
    text[0] = char_value;
    text[1] = '\0';
    
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    return make_success(ast);
}

static combinator_t* char_literal(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = char_fn;
    comb->args = args;
    return comb;
}

// Custom parser for range expressions (e.g., 'a'..'z', 1..10)
static ParseResult range_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // This will be called as part of an infix expression parser
    // We just need to consume the ".." token
    if (read1(in) != '.' || read1(in) != '.') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '..'"));
    }
    
    // Create a placeholder AST node - the actual range will be built by the expression parser
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup("..");
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    return make_success(ast);
}

static combinator_t* range_operator(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = range_fn;
    comb->args = args;
    return comb;
}

// Custom parser for set constructors (e.g., [1, 2, 3], ['a'..'z'])
static ParseResult set_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Must start with '['
    if (read1(in) != '[') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '['"));
    }
    
    ast_t* set_node = new_ast();
    set_node->typ = pargs->tag;
    set_node->sym = NULL;
    set_node->child = NULL;
    set_node->next = NULL;
    set_ast_position(set_node, in);
    
    // Skip whitespace
    char c;
    while (isspace(c = read1(in)));
    if (c != EOF) in->start--;
    
    // Handle empty set
    c = read1(in);
    if (c == ']') {
        return make_success(set_node);
    }
    if (c != EOF) in->start--; // Back up
    
    // Parse set elements (temporarily create a sub-parser for this)
    // For now, we'll handle simple cases - this is a complex recursive parse
    // We'll parse comma-separated expressions until we hit ']'
    ast_t* first_element = NULL;
    ast_t* current_element = NULL;
    
    while (true) {
        // Skip whitespace
        while (isspace(c = read1(in)));
        if (c != EOF) in->start--;
        
        // Try to parse a simple element (number, char, or identifier)
        InputState elem_state;
        save_input_state(in, &elem_state);
        
        ast_t* element = NULL;
        
        // Try character literal first
        if (in->buffer[in->start] == '\'') {
            ParseResult char_result = char_fn(in, &(prim_args){PASCAL_T_CHAR});
            if (char_result.is_success) {
                element = char_result.value.ast;
            }
        }
        
        // Try integer
        if (!element && isdigit(in->buffer[in->start])) {
            int start_pos = in->start;
            while (isdigit(c = read1(in)));
            if (c != EOF) in->start--;
            
            int len = in->start - start_pos;
            char* text = (char*)safe_malloc(len + 1);
            strncpy(text, in->buffer + start_pos, len);
            text[len] = '\0';
            
            element = new_ast();
            element->typ = PASCAL_T_INTEGER;
            element->sym = sym_lookup(text);
            free(text);
            element->child = NULL;
            element->next = NULL;
            set_ast_position(element, in);
        }
        
        // Try identifier
        if (!element && (isalpha(in->buffer[in->start]) || in->buffer[in->start] == '_')) {
            int start_pos = in->start;
            while (isalnum(c = read1(in)) || c == '_');
            if (c != EOF) in->start--;
            
            int len = in->start - start_pos;
            char* text = (char*)safe_malloc(len + 1);
            strncpy(text, in->buffer + start_pos, len);
            text[len] = '\0';
            
            element = new_ast();
            element->typ = PASCAL_T_IDENTIFIER;
            element->sym = sym_lookup(text);
            free(text);
            element->child = NULL;
            element->next = NULL;
            set_ast_position(element, in);
        }
        
        if (!element) {
            free_ast(set_node);
            restore_input_state(in, &state);
            return make_failure(in, strdup("Expected set element"));
        }
        
        // Add element to set
        if (!first_element) {
            first_element = element;
            current_element = element;
            set_node->child = element;
        } else {
            current_element->next = element;
            current_element = element;
        }
        
        // Skip whitespace
        while (isspace(c = read1(in)));
        if (c != EOF) in->start--;
        
        // Check for comma or closing bracket
        c = read1(in);
        if (c == ']') {
            break;
        } else if (c == ',') {
            continue; // Parse next element
        } else {
            free_ast(set_node);
            restore_input_state(in, &state);
            return make_failure(in, strdup("Expected ',' or ']'"));
        }
    }
    
    return make_success(set_node);
}

static combinator_t* set_constructor(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = set_fn;
    comb->args = args;
    return comb;
}

// Multi combinator approach for relational operators to handle prefix conflicts
static combinator_t* relational_ops() {
    return multi(new_combinator(), PASCAL_T_NONE,
        token(match("<=")), // try longer matches first
        token(match(">=")),
        token(match("<>")),
        token(match("=")),
        token(match("<")),
        token(match(">")),
        token(match("in")),
        token(match("is")),
        token(match("as")),
        NULL
    );
}

// --- AST Printing ---
const char* pascal_tag_to_string(tag_t tag) {
    switch (tag) {
        case PASCAL_T_NONE: return "NONE";
        case PASCAL_T_INTEGER: return "INTEGER";
        case PASCAL_T_REAL: return "REAL";
        case PASCAL_T_IDENTIFIER: return "IDENTIFIER";
        case PASCAL_T_STRING: return "STRING";
        case PASCAL_T_CHAR: return "CHAR";
        case PASCAL_T_BOOLEAN: return "BOOLEAN";
        case PASCAL_T_ADD: return "ADD";
        case PASCAL_T_SUB: return "SUB";
        case PASCAL_T_MUL: return "MUL";
        case PASCAL_T_DIV: return "DIV";
        case PASCAL_T_INTDIV: return "INTDIV";
        case PASCAL_T_MOD: return "MOD";
        case PASCAL_T_NEG: return "NEG";
        case PASCAL_T_POS: return "POS";
        case PASCAL_T_EQ: return "EQ";
        case PASCAL_T_NE: return "NE";
        case PASCAL_T_LT: return "LT";
        case PASCAL_T_GT: return "GT";
        case PASCAL_T_LE: return "LE";
        case PASCAL_T_GE: return "GE";
        case PASCAL_T_AND: return "AND";
        case PASCAL_T_OR: return "OR";
        case PASCAL_T_NOT: return "NOT";
        case PASCAL_T_XOR: return "XOR";
        case PASCAL_T_SHL: return "SHL";
        case PASCAL_T_SHR: return "SHR";
        case PASCAL_T_ADDR: return "ADDR";
        case PASCAL_T_RANGE: return "RANGE";
        case PASCAL_T_SET: return "SET";
        case PASCAL_T_IN: return "IN";
        case PASCAL_T_SET_UNION: return "SET_UNION";
        case PASCAL_T_SET_INTERSECT: return "SET_INTERSECT";
        case PASCAL_T_SET_DIFF: return "SET_DIFF";
        case PASCAL_T_SET_SYM_DIFF: return "SET_SYM_DIFF";
        case PASCAL_T_IS: return "IS";
        case PASCAL_T_AS: return "AS";
        case PASCAL_T_TYPECAST: return "TYPECAST";
        case PASCAL_T_FUNC_CALL: return "FUNC_CALL";
        case PASCAL_T_ARG_LIST: return "ARG_LIST";
        default: return "UNKNOWN";
    }
}

static void print_ast_recursive(ast_t* ast, int depth) {
    if (ast == NULL || ast == ast_nil) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("(%s", pascal_tag_to_string(ast->typ));
    if (ast->sym) printf(" %s", ast->sym->name);

    ast_t* child = ast->child;
    if (child) {
        printf("\n");
        print_ast_recursive(child, depth + 1);
    }
    printf(")");

    if (ast->next) {
        printf("\n");
        print_ast_recursive(ast->next, depth);
    }
}

void print_pascal_ast(ast_t* ast) {
    print_ast_recursive(ast, 0);
    printf("\n");
}

// --- Parser Definition ---
void init_pascal_expression_parser(combinator_t** p) {
    // Function call parser: identifier followed by optional argument list
    combinator_t* arg_list = between(
        token(match("(")),
        token(match(")")),
        optional(sep_by(lazy(p), token(match(","))))
    );
    
    combinator_t* func_call = seq(new_combinator(), PASCAL_T_FUNC_CALL,
        token(cident(PASCAL_T_IDENTIFIER)),
        arg_list,
        NULL
    );
    
    // Type cast parser: TypeName(expression)
    combinator_t* typecast = seq(new_combinator(), PASCAL_T_TYPECAST,
        token(cident(PASCAL_T_IDENTIFIER)), // type name
        between(token(match("(")), token(match(")")), lazy(p)), // expression
        NULL
    );
    
    combinator_t *factor = multi(new_combinator(), PASCAL_T_NONE,
        token(real_number(PASCAL_T_REAL)),        // Real numbers (3.14) - try first
        token(integer(PASCAL_T_INTEGER)),         // Integers (123)
        token(char_literal(PASCAL_T_CHAR)),       // Characters ('A')
        token(string(PASCAL_T_STRING)),           // Strings ("hello")
        token(set_constructor(PASCAL_T_SET)),     // Set constructors [1, 2, 3]
        typecast,                                 // Type casts Integer(x)
        func_call,                                // Function calls func(x)
        token(cident(PASCAL_T_IDENTIFIER)),       // Identifiers (variables)
        between(token(match("(")), token(match(")")), lazy(p)), // Parenthesized expressions
        NULL
    );

    expr(*p, factor);
    
    // Precedence levels (lower number = lower precedence, must be consecutive starting from 0)
    // Precedence 0: Boolean OR (lowest precedence)
    expr_insert(*p, 0, PASCAL_T_OR, EXPR_INFIX, ASSOC_LEFT, token(match("or")));
    
    // Precedence 1: Boolean XOR  
    expr_insert(*p, 1, PASCAL_T_XOR, EXPR_INFIX, ASSOC_LEFT, token(match("xor")));
    
    // Precedence 2: Boolean AND
    expr_insert(*p, 2, PASCAL_T_AND, EXPR_INFIX, ASSOC_LEFT, token(match("and")));
    
    // Precedence 3: All relational operators - multi-char operators added last (tried first)
    // Single character operators
    expr_insert(*p, 3, PASCAL_T_EQ, EXPR_INFIX, ASSOC_LEFT, token(match("=")));
    expr_altern(*p, 3, PASCAL_T_LT, token(match("<")));
    expr_altern(*p, 3, PASCAL_T_GT, token(match(">")));
    expr_altern(*p, 3, PASCAL_T_IN, token(match("in")));
    expr_altern(*p, 3, PASCAL_T_IS, token(match("is")));
    expr_altern(*p, 3, PASCAL_T_AS, token(match("as")));
    // Multi-character operators (added last = tried first in expr parser)
    expr_altern(*p, 3, PASCAL_T_NE, token(match("<>")));
    expr_altern(*p, 3, PASCAL_T_GE, token(match(">=")));
    expr_altern(*p, 3, PASCAL_T_LE, token(match("<=")));
    
    // Precedence 4: Range operator (..)  
    expr_insert(*p, 4, PASCAL_T_RANGE, EXPR_INFIX, ASSOC_LEFT, token(match("..")));
    
    // Precedence 5: Addition and Subtraction (includes string concatenation and set operations)
    expr_insert(*p, 5, PASCAL_T_ADD, EXPR_INFIX, ASSOC_LEFT, token(match("+")));
    expr_altern(*p, 5, PASCAL_T_SUB, token(match("-")));
    
    // Precedence 6: Multiplication, Division, Modulo, and Bitwise shifts
    expr_insert(*p, 6, PASCAL_T_MUL, EXPR_INFIX, ASSOC_LEFT, token(match("*")));
    expr_altern(*p, 6, PASCAL_T_DIV, token(match("/")));
    expr_altern(*p, 6, PASCAL_T_INTDIV, token(match("div")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(match("mod")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(match("%")));
    expr_altern(*p, 6, PASCAL_T_SHL, token(match("shl")));
    expr_altern(*p, 6, PASCAL_T_SHR, token(match("shr")));
    
    // Precedence 7: Unary operators (highest precedence)
    expr_insert(*p, 7, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
    expr_altern(*p, 7, PASCAL_T_POS, token(match("+")));
    expr_altern(*p, 7, PASCAL_T_NOT, token(match("not")));
    expr_altern(*p, 7, PASCAL_T_ADDR, token(match("@")));
}
