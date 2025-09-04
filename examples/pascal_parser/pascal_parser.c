#include "pascal_parser.h"
#include "pascal_keywords.h"
#include "pascal_type.h"
#include "pascal_expression.h"
#include "pascal_statement.h"
#include "pascal_declaration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Forward Declarations ---
typedef struct { char* str; } match_args;  // For keyword matching
typedef struct { tag_t tag; combinator_t** expr_parser; } set_args;  // For set constructor
extern ast_t* ast_nil;  // From parser.c

// --- Helper Functions ---
static bool is_whitespace_char(char c) {
    return isspace((unsigned char)c);
}

// Helper function to check if a character can be part of an identifier
static bool is_identifier_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

// Word-boundary aware case-insensitive keyword matching
static ParseResult keyword_ci_fn(input_t* in, void* args) {
    char* str = ((match_args*)args)->str;
    InputState state;
    save_input_state(in, &state);
    
    int len = strlen(str);
    
    // Match the keyword case-insensitively
    for (int i = 0; i < len; i++) {
        char c = read1(in);
        if (tolower(c) != tolower(str[i])) {
            restore_input_state(in, &state);
            char* err_msg;
            asprintf(&err_msg, "Expected keyword '%s' (case-insensitive)", str);
            return make_failure(in, err_msg);
        }
    }
    
    // Check for word boundary: next character should not be alphanumeric or underscore
    if (in->start < in->length) {
        char next_char = in->buffer[in->start];
        if (is_identifier_char(next_char)) {
            restore_input_state(in, &state);
            char* err_msg;
            asprintf(&err_msg, "Expected keyword '%s', not part of identifier", str);
            return make_failure(in, err_msg);
        }
    }
    
    return make_success(ast_nil);
}

// Create word-boundary aware case-insensitive keyword parser
combinator_t* keyword_ci(char* str) {
    match_args* args = (match_args*)safe_malloc(sizeof(match_args));
    args->str = str;
    combinator_t* comb = new_combinator();
    comb->type = P_CI_KEYWORD;
    comb->fn = keyword_ci_fn;
    comb->args = args;
    return comb;
}

// Pascal-style comment parser using proper combinators: { comment content }
combinator_t* pascal_comment() {
    return seq(new_combinator(), PASCAL_T_NONE,
        match("{"),
        until(match("}"), PASCAL_T_NONE),
        match("}"),
        NULL);
}

// C++-style comment parser: // comment content until end of line
combinator_t* cpp_comment() {
    return seq(new_combinator(), PASCAL_T_NONE,
        match("//"),
        until(match("\n"), PASCAL_T_NONE),
        optional(match("\n")),  // consume the newline if present
        NULL);
}

// Compiler directive parser: {$directive parameter}
combinator_t* compiler_directive(tag_t tag) {
    return right(
        match("{$"),
        left(
            until(match("}"), tag),
            match("}")
        )
    );
}

// Enhanced whitespace parser that handles whitespace, Pascal comments, C++ comments, and compiler directives
combinator_t* pascal_whitespace() {
    combinator_t* ws_char = satisfy(is_whitespace_char, PASCAL_T_NONE);
    combinator_t* pascal_comment_parser = pascal_comment();
    combinator_t* cpp_comment_parser = cpp_comment();
    combinator_t* directive = compiler_directive(PASCAL_T_NONE);  // Treat directives as ignorable whitespace
    combinator_t* ws_or_comment = multi(new_combinator(), PASCAL_T_NONE,
        ws_char,
        pascal_comment_parser,
        cpp_comment_parser,
        directive,  // Include compiler directives in whitespace
        NULL
    );
    return many(ws_or_comment);
}

// Renamed token parser with better Pascal-aware whitespace handling
combinator_t* pascal_token(combinator_t* p) {
    combinator_t* ws = pascal_whitespace();
    return right(ws, left(p, pascal_whitespace()));
}

// Backward compatibility: keep old token() function name for existing code
combinator_t* token(combinator_t* p) {
    return pascal_token(p);
}


// Pascal identifier parser that excludes reserved keywords
static ParseResult pascal_identifier_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    int start_pos = in->start;
    char c = read1(in);
    
    // Must start with letter or underscore
    if (c != '_' && !isalpha(c)) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected identifier"));
    }
    
    // Continue with alphanumeric or underscore
    while (isalnum(c = read1(in)) || c == '_');
    if (c != EOF) in->start--;
    
    // Extract the identifier text
    int len = in->start - start_pos;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_pos, len);
    text[len] = '\0';
    
    // Check if it's a reserved keyword
    if (is_pascal_keyword(text)) {
        free(text);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Identifier cannot be a reserved keyword"));
    }
    
    // Create AST node for valid identifier (following original cident_fn pattern)
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(text);
    free(text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    return make_success(ast);
}

// Create Pascal identifier combinator that excludes keywords
combinator_t* pascal_identifier(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_CIDENT; // Reuse the same type for compatibility
    comb->fn = pascal_identifier_fn;
    comb->args = args;
    return comb;
}

// Keywords that can be used as function names in expressions
static const char* expression_allowed_keywords[] = {
    "procedure", "function", "program", "unit",
    "record", "array", "set", "packed",  // type keywords that can be variable names
    "object", "class",                   // OOP keywords that can be variable names
    NULL
};

// Check if a keyword is allowed as an identifier in expressions
static bool is_expression_allowed_keyword(const char* str) {
    for (int i = 0; expression_allowed_keywords[i] != NULL; i++) {
        if (strcasecmp(str, expression_allowed_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Pascal identifier parser for expressions - allows certain keywords as function names
static ParseResult pascal_expression_identifier_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    int start_pos = in->start;
    char c = read1(in);
    
    // Must start with letter or underscore
    if (c != '_' && !isalpha(c)) {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected identifier"));
    }
    
    // Continue with alphanumeric or underscore
    while (isalnum(c = read1(in)) || c == '_');
    if (c != EOF) in->start--;
    
    // Extract the identifier text
    int len = in->start - start_pos;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_pos, len);
    text[len] = '\0';
    
    // Check if it's a reserved keyword that's NOT allowed in expressions
    if (is_pascal_keyword(text) && !is_expression_allowed_keyword(text)) {
        free(text);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Identifier cannot be a reserved keyword"));
    }
    
    // Create AST node for valid identifier
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(text);
    free(text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    return make_success(ast);
}

// Create Pascal expression identifier combinator that allows certain keywords
combinator_t* pascal_expression_identifier(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_CIDENT;
    comb->fn = pascal_expression_identifier_fn;
    comb->args = args;
    return comb;
}

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
    
    // Optional exponent part (e.g., E+10, e-5, E3)
    c = read1(in);
    if (c == 'e' || c == 'E') {
        // Parse optional sign
        c = read1(in);
        if (c == '+' || c == '-') {
            c = read1(in);
        }
        
        // Must have at least one digit after E/e
        if (!isdigit(c)) {
            restore_input_state(in, &state);
            return make_failure(in, strdup("Expected digit after exponent"));
        }
        
        // Parse remaining exponent digits
        while (isdigit(c = read1(in)));
        if (c != EOF) in->start--; // Back up one if not EOF
    } else if (c != EOF) {
        in->start--; // Back up if we didn't find exponent
    }
    
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

// Simplified set constructor parser using existing parse utilities
static ParseResult set_fn(input_t* in, void* args) {
    set_args* sargs = (set_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Must start with '['
    if (read1(in) != '[') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '['"));
    }
    
    ast_t* set_node = new_ast();
    set_node->typ = sargs->tag;
    set_node->sym = NULL;
    set_node->child = NULL;
    set_node->next = NULL;
    set_ast_position(set_node, in);
    
    // Skip whitespace manually
    char c;
    while (isspace(c = read1(in)));
    if (c != EOF) in->start--;
    
    // Check for empty set
    c = read1(in);
    if (c == ']') {
        return make_success(set_node);
    }
    if (c != EOF) in->start--; // Back up
    
    // Parse set elements using the provided expression parser
    combinator_t* expr_parser = lazy(sargs->expr_parser);
    
    // Parse comma-separated expressions
    ast_t* first_element = NULL;
    ast_t* current_element = NULL;
    
    while (true) {
        // Skip whitespace
        while (isspace(c = read1(in)));
        if (c != EOF) in->start--;
        
        ParseResult elem_result = parse(in, expr_parser);
        if (!elem_result.is_success) {
            free_ast(set_node);
            free_combinator(expr_parser);
            restore_input_state(in, &state);
            return make_failure(in, strdup("Expected set element"));
        }
        
        // Add element to set
        if (!first_element) {
            first_element = elem_result.value.ast;
            current_element = elem_result.value.ast;
            set_node->child = elem_result.value.ast;
        } else {
            current_element->next = elem_result.value.ast;
            current_element = elem_result.value.ast;
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
            free_combinator(expr_parser);
            restore_input_state(in, &state);
            return make_failure(in, strdup("Expected ',' or ']'"));
        }
    }
    
    free_combinator(expr_parser);
    return make_success(set_node);
}

static combinator_t* set_constructor(tag_t tag, combinator_t** expr_parser) {
    set_args* args = (set_args*)safe_malloc(sizeof(set_args));
    args->tag = tag;
    args->expr_parser = expr_parser;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY;
    comb->fn = set_fn;
    comb->args = args;
    return comb;
}

// Removed unused relational_ops() function that had non-boundary-aware match("in")

// Pascal single-quoted string content parser using combinators - handles '' escaping
static ParseResult pascal_single_quoted_content_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    int start_offset = in->start;
    
    // Build content by parsing until we hit the closing quote (not doubled)
    while(1) {
        InputState current_state; 
        save_input_state(in, &current_state);
        
        char c = read1(in);
        if (c == EOF) {
            break; // End of content
        }
        
        if (c == '\'') {
            // Check if this is an escaped quote (doubled quote)
            char next_c = read1(in);
            if (next_c == '\'') {
                // This is an escaped single quote, continue parsing
                continue;
            } else {
                // This is the end of the string, put back the character and break
                if (next_c != EOF) in->start--;
                restore_input_state(in, &current_state);
                break;
            }
        }
    }
    
    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';
    
    // Process Pascal-style escape sequences (doubled quotes)
    int processed_len = 0;
    char* processed_text = (char*)safe_malloc(len + 1);
    
    for (int i = 0; i < len; i++) {
        if (text[i] == '\'' && i + 1 < len && text[i + 1] == '\'') {
            // This is an escaped single quote - add one quote and skip the next
            processed_text[processed_len++] = '\'';
            i++; // Skip the next quote
        } else {
            processed_text[processed_len++] = text[i];
        }
    }
    processed_text[processed_len] = '\0';
    
    ast_t* ast = new_ast();
    ast->typ = pargs->tag; 
    ast->sym = sym_lookup(processed_text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    free(text);
    free(processed_text);
    return make_success(ast);
}

combinator_t* pascal_single_quoted_content(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = pascal_single_quoted_content_fn;
    comb->args = args;
    return comb;
}

// Pascal double-quoted string content parser - handles \ escaping  
static ParseResult pascal_double_quoted_content_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    int start_offset = in->start;
    
    while(1) {
        char c = read1(in);
        if (c == EOF) {
            break; // End of content
        }
        
        if (c == '"') {
            // End of string - put back the quote and break
            in->start--;
            break;
        }
        
        if (c == '\\') {
            // Skip the escaped character
            read1(in);
        }
    }
    
    int len = in->start - start_offset;
    char* text = (char*)safe_malloc(len + 1);
    strncpy(text, in->buffer + start_offset, len);
    text[len] = '\0';
    
    // Process C-style escape sequences
    int processed_len = 0;
    char* processed_text = (char*)safe_malloc(len + 1);
    
    for (int i = 0; i < len; i++) {
        if (text[i] == '\\' && i + 1 < len) {
            // Process escape sequence
            char next = text[i + 1];
            switch (next) {
                case 'n': processed_text[processed_len++] = '\n'; break;
                case 't': processed_text[processed_len++] = '\t'; break;
                case '"': processed_text[processed_len++] = '"'; break;
                case '\\': processed_text[processed_len++] = '\\'; break;
                default: 
                    // Unknown escape - keep both characters
                    processed_text[processed_len++] = text[i];
                    processed_text[processed_len++] = text[i + 1];
                    break;
            }
            i++; // Skip the next character
        } else {
            processed_text[processed_len++] = text[i];
        }
    }
    processed_text[processed_len] = '\0';
    
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(processed_text);
    ast->child = NULL;
    ast->next = NULL;
    set_ast_position(ast, in);
    
    free(text);
    free(processed_text);
    return make_success(ast);
}

// Create combinator for Pascal double-quoted string content
combinator_t* pascal_double_quoted_content(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_SATISFY; // Reuse existing type for custom parser
    comb->fn = pascal_double_quoted_content_fn;
    comb->args = args;
    return comb;
}

// Map function to extract string content from sequence AST
static ast_t* extract_string_from_seq(ast_t* seq_ast) {
    if (!seq_ast || !seq_ast->child) {
        return seq_ast; // Return as-is if malformed
    }
    
    // The sequence should be: quote, content, quote
    // We want to return just the content (middle element)
    ast_t* content = seq_ast->child->next; // Skip first element (opening quote)
    if (!content) {
        return seq_ast; // Return as-is if malformed
    }
    
    // Create a copy of the content node
    ast_t* result = new_ast();
    result->typ = content->typ;
    result->sym = content->sym;
    result->child = copy_ast(content->child);
    result->next = NULL;
    result->line = content->line;
    result->col = content->col;
    
    return result;
}

combinator_t* pascal_string(tag_t tag) {
    // Create a combinator that extracts just the content from the sequence
    combinator_t* single_quoted = seq(new_combinator(), PASCAL_T_NONE,
        match("'"),                               // opening single quote
        pascal_single_quoted_content(tag),        // content with Pascal escaping  
        match("'"),                               // closing single quote
        NULL
    );
    
    combinator_t* double_quoted = seq(new_combinator(), PASCAL_T_NONE,
        match("\""),                              // opening double quote
        pascal_double_quoted_content(tag),        // content with C-style escaping
        match("\""),                              // closing double quote
        NULL
    );
    
    // Map function to extract just the string content from the sequence
    combinator_t* single_quoted_mapped = map(single_quoted, extract_string_from_seq);
    combinator_t* double_quoted_mapped = map(double_quoted, extract_string_from_seq);
    
    // Return a multi combinator that tries both alternatives
    return multi(new_combinator(), PASCAL_T_NONE,
        single_quoted_mapped,
        double_quoted_mapped,
        NULL
    );
}
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
        case PASCAL_T_ARRAY_ACCESS: return "ARRAY_ACCESS";
        case PASCAL_T_MEMBER_ACCESS: return "MEMBER_ACCESS";
        case PASCAL_T_ARG_LIST: return "ARG_LIST";
        case PASCAL_T_TUPLE: return "TUPLE";
        case PASCAL_T_PROCEDURE_DECL: return "PROCEDURE_DECL";
        case PASCAL_T_FUNCTION_DECL: return "FUNCTION_DECL";
        case PASCAL_T_PARAM_LIST: return "PARAM_LIST";
        case PASCAL_T_PARAM: return "PARAM";
        case PASCAL_T_RETURN_TYPE: return "RETURN_TYPE";
        case PASCAL_T_ASSIGNMENT: return "ASSIGNMENT";
        case PASCAL_T_STATEMENT: return "STATEMENT";
        case PASCAL_T_STATEMENT_LIST: return "STATEMENT_LIST";
        case PASCAL_T_IF_STMT: return "IF_STMT";
        case PASCAL_T_THEN: return "THEN";
        case PASCAL_T_ELSE: return "ELSE";
        case PASCAL_T_BEGIN_BLOCK: return "BEGIN_BLOCK";
        case PASCAL_T_END_BLOCK: return "END_BLOCK";
        case PASCAL_T_FOR_STMT: return "FOR_STMT";
        case PASCAL_T_WHILE_STMT: return "WHILE_STMT";
        case PASCAL_T_DO: return "DO";
        case PASCAL_T_TO: return "TO";
        case PASCAL_T_DOWNTO: return "DOWNTO";
        case PASCAL_T_ASM_BLOCK: return "ASM_BLOCK";
        // Exception handling types
        case PASCAL_T_TRY_BLOCK: return "TRY_BLOCK";
        case PASCAL_T_FINALLY_BLOCK: return "FINALLY_BLOCK";
        case PASCAL_T_EXCEPT_BLOCK: return "EXCEPT_BLOCK";
        case PASCAL_T_RAISE_STMT: return "RAISE_STMT";
        case PASCAL_T_INHERITED_STMT: return "INHERITED_STMT";
        case PASCAL_T_ON_CLAUSE: return "ON_CLAUSE";
        case PASCAL_T_PROGRAM_DECL: return "PROGRAM_DECL";
        case PASCAL_T_PROGRAM_HEADER: return "PROGRAM_HEADER";
        case PASCAL_T_PROGRAM_PARAMS: return "PROGRAM_PARAMS";
        case PASCAL_T_VAR_SECTION: return "VAR_SECTION";
        case PASCAL_T_VAR_DECL: return "VAR_DECL";
        case PASCAL_T_TYPE_SPEC: return "TYPE_SPEC";
        case PASCAL_T_MAIN_BLOCK: return "MAIN_BLOCK";
        // New enhanced parser types
        case PASCAL_T_COMPILER_DIRECTIVE: return "COMPILER_DIRECTIVE";
        case PASCAL_T_COMMENT: return "COMMENT";
        case PASCAL_T_TYPE_SECTION: return "TYPE_SECTION";
        case PASCAL_T_TYPE_DECL: return "TYPE_DECL";
        case PASCAL_T_RANGE_TYPE: return "RANGE_TYPE";
        case PASCAL_T_ARRAY_TYPE: return "ARRAY_TYPE";
        case PASCAL_T_CLASS_TYPE: return "CLASS_TYPE";
        case PASCAL_T_CLASS_MEMBER: return "CLASS_MEMBER";
        case PASCAL_T_ACCESS_MODIFIER: return "ACCESS_MODIFIER";
        case PASCAL_T_CLASS_BODY: return "CLASS_BODY";
        case PASCAL_T_PRIVATE_SECTION: return "PRIVATE_SECTION";
        case PASCAL_T_PUBLIC_SECTION: return "PUBLIC_SECTION";
        case PASCAL_T_PROTECTED_SECTION: return "PROTECTED_SECTION";
        case PASCAL_T_PUBLISHED_SECTION: return "PUBLISHED_SECTION";
        case PASCAL_T_FIELD_DECL: return "FIELD_DECL";
        case PASCAL_T_METHOD_DECL: return "METHOD_DECL";
        case PASCAL_T_PROPERTY_DECL: return "PROPERTY_DECL";
        case PASCAL_T_CONSTRUCTOR_DECL: return "CONSTRUCTOR_DECL";
        case PASCAL_T_DESTRUCTOR_DECL: return "DESTRUCTOR_DECL";
        // Uses clause types
        case PASCAL_T_USES_SECTION: return "USES_SECTION";
        case PASCAL_T_USES_UNIT: return "USES_UNIT";
        // Const section types
        case PASCAL_T_CONST_SECTION: return "CONST_SECTION";
        case PASCAL_T_CONST_DECL: return "CONST_DECL";
        // Field width specifier
        case PASCAL_T_FIELD_WIDTH: return "FIELD_WIDTH";
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


