#include "pascal_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Forward Declarations ---
typedef struct { char* str; } match_args;  // For keyword matching
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

// Pascal-style comment parser: { comment content }
static ParseResult pascal_comment_fn(input_t* in, void* args) {
    InputState state;
    save_input_state(in, &state);
    
    char c = read1(in);
    if (c != '{') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '{'"));
    }
    
    // Read until '}' 
    while ((c = read1(in)) != EOF) {
        if (c == '}') {
            return make_success(ast_nil);
        }
    }
    
    restore_input_state(in, &state);
    return make_failure(in, strdup("Unterminated comment"));
}

combinator_t* pascal_comment() {
    combinator_t* comb = new_combinator();
    comb->fn = pascal_comment_fn;
    comb->args = NULL;
    return comb;
}

// Enhanced whitespace parser that handles both whitespace and Pascal comments
combinator_t* pascal_whitespace() {
    combinator_t* ws_char = satisfy(is_whitespace_char, PASCAL_T_NONE);
    combinator_t* comment = pascal_comment();
    combinator_t* ws_or_comment = multi(new_combinator(), PASCAL_T_NONE,
        ws_char,
        comment,
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

// Compiler directive parser: {$directive parameter}
static ParseResult compiler_directive_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Must start with {$
    char c1 = read1(in);
    char c2 = read1(in);
    if (c1 != '{' || c2 != '$') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '{$'"));
    }
    
    // Read directive name and parameters until }
    int start_pos = in->start;
    char c;
    while ((c = read1(in)) != EOF) {
        if (c == '}') {
            int len = in->start - start_pos - 1; // -1 for the '}'
            if (len <= 0) {
                restore_input_state(in, &state);
                return make_failure(in, strdup("Empty compiler directive"));
            }
            
            // Extract directive content
            char* directive_text = strndup(in->buffer + start_pos, len);
            
            ast_t* ast = new_ast();
            ast->typ = pargs->tag;
            ast->sym = sym_lookup(directive_text);
            ast->child = NULL;
            ast->next = NULL;
            free(directive_text);
            set_ast_position(ast, in);
            return make_success(ast);
        }
    }
    
    restore_input_state(in, &state);
    return make_failure(in, strdup("Unterminated compiler directive"));
}

combinator_t* compiler_directive(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = compiler_directive_fn;
    return comb;
}

// Range type parser: start..end (e.g., -1..1)
static ParseResult range_type_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Parse start value (can be negative integer or identifier)
    input_t* temp_input = new_input();
    temp_input->buffer = in->buffer + in->start;
    temp_input->length = in->length - in->start;
    temp_input->line = in->line;
    temp_input->col = in->col;
    
    // Try parsing integer (including negative)
    combinator_t* start_parser = multi(new_combinator(), PASCAL_T_NONE,
        integer(PASCAL_T_INTEGER),
        seq(new_combinator(), PASCAL_T_INTEGER,
            match("-"),
            integer(PASCAL_T_INTEGER),
            NULL),
        cident(PASCAL_T_IDENTIFIER),
        NULL);
    
    ParseResult start_result = parse(temp_input, start_parser);
    if (!start_result.is_success) {
        free_combinator(start_parser);
        free(temp_input);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected range start value"));
    }
    
    // Update main input position
    in->start += temp_input->start;
    
    // Skip whitespace
    while (in->start < in->length && isspace(in->buffer[in->start])) {
        in->start++;
    }
    
    // Expect ".."
    if (in->start + 2 > in->length || 
        in->buffer[in->start] != '.' || in->buffer[in->start + 1] != '.') {
        free_ast(start_result.value.ast);
        free_combinator(start_parser);
        free(temp_input);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '..' in range type"));
    }
    in->start += 2;
    
    // Skip whitespace
    while (in->start < in->length && isspace(in->buffer[in->start])) {
        in->start++;
    }
    
    // Parse end value
    temp_input->buffer = in->buffer + in->start;
    temp_input->length = in->length - in->start;
    temp_input->start = 0;
    
    ParseResult end_result = parse(temp_input, start_parser);
    if (!end_result.is_success) {
        free_ast(start_result.value.ast);
        free_combinator(start_parser);
        free(temp_input);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected range end value"));
    }
    
    // Update main input position
    in->start += temp_input->start;
    
    // Create range AST
    ast_t* range_ast = new_ast();
    range_ast->typ = pargs->tag;
    range_ast->child = start_result.value.ast;
    start_result.value.ast->next = end_result.value.ast;
    range_ast->sym = NULL;
    range_ast->next = NULL;
    
    free_combinator(start_parser);
    free(temp_input);
    set_ast_position(range_ast, in);
    return make_success(range_ast);
}

combinator_t* range_type(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = range_type_fn;
    return comb;
}

// Array type parser: ARRAY[range1,range2,...] OF element_type
static ParseResult array_type_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Parse "ARRAY" keyword (case insensitive)
    combinator_t* array_keyword = token(match_ci("array"));
    ParseResult array_res = parse(in, array_keyword);
    if (!array_res.is_success) {
        free_combinator(array_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'array'"));
    }
    free_ast(array_res.value.ast);
    free_combinator(array_keyword);
    
    // Parse [
    combinator_t* open_bracket = token(match("["));
    ParseResult open_res = parse(in, open_bracket);
    if (!open_res.is_success) {
        free_combinator(open_bracket);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '[' after 'array'"));
    }
    free_ast(open_res.value.ast);
    free_combinator(open_bracket);
    
    // Parse ranges/indices (simplified - just accept any identifiers/ranges for now)
    combinator_t* array_index = multi(new_combinator(), PASCAL_T_NONE,
        range_type(PASCAL_T_RANGE_TYPE),
        token(cident(PASCAL_T_IDENTIFIER)),
        NULL
    );
    combinator_t* index_list = sep_by(array_index, token(match(",")));
    ParseResult indices_res = parse(in, index_list);
    ast_t* indices_ast = NULL;
    if (indices_res.is_success) {
        indices_ast = indices_res.value.ast;
    } else {
        free_combinator(index_list);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected array indices"));
    }
    free_combinator(index_list);
    
    // Parse ]
    combinator_t* close_bracket = token(match("]"));
    ParseResult close_res = parse(in, close_bracket);
    if (!close_res.is_success) {
        free_ast(indices_ast);
        free_combinator(close_bracket);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected ']'"));
    }
    free_ast(close_res.value.ast);
    free_combinator(close_bracket);
    
    // Parse OF
    combinator_t* of_keyword = token(match_ci("of"));
    ParseResult of_res = parse(in, of_keyword);
    if (!of_res.is_success) {
        free_ast(indices_ast);
        free_combinator(of_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'OF' after array indices"));
    }
    free_ast(of_res.value.ast);
    free_combinator(of_keyword);
    
    // Parse element type (simplified)
    combinator_t* element_type = token(cident(PASCAL_T_IDENTIFIER));
    ParseResult elem_res = parse(in, element_type);
    ast_t* element_ast = NULL;
    if (elem_res.is_success) {
        element_ast = elem_res.value.ast;
    } else {
        free_ast(indices_ast);
        free_combinator(element_type);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected element type after 'OF'"));
    }
    free_combinator(element_type);
    
    // Build AST
    ast_t* array_ast = new_ast();
    array_ast->typ = pargs->tag;
    array_ast->sym = NULL;
    array_ast->child = indices_ast;
    if (indices_ast) {
        // Link element type as the last child
        ast_t* current = indices_ast;
        while (current->next) current = current->next;
        current->next = element_ast;
    } else {
        array_ast->child = element_ast;
    }
    array_ast->next = NULL;
    
    set_ast_position(array_ast, in);
    return make_success(array_ast);
}

combinator_t* array_type(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = array_type_fn;
    return comb;
}

// Simple class type parser: class ... end
static ParseResult class_type_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Parse "class" keyword (case insensitive)
    combinator_t* class_keyword = token(match_ci("class"));
    ParseResult class_res = parse(in, class_keyword);
    if (!class_res.is_success) {
        free_combinator(class_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'class'"));
    }
    free_ast(class_res.value.ast);
    free_combinator(class_keyword);
    
    // For now, skip class body and just parse "end" directly for testing
    combinator_t* end_keyword = token(match_ci("end"));
    ParseResult end_res = parse(in, end_keyword);
    if (!end_res.is_success) {
        free_combinator(end_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'end' after class"));
    }
    free_ast(end_res.value.ast);
    free_combinator(end_keyword);
    
    // Build AST
    ast_t* class_ast = new_ast();
    class_ast->typ = pargs->tag;
    class_ast->sym = NULL;
    class_ast->child = NULL; // No body for now
    class_ast->next = NULL;
    
    set_ast_position(class_ast, in);
    return make_success(class_ast);
}

combinator_t* class_type(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = class_type_fn;
    return comb;
}

static ParseResult type_name_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    const char* type_names[] = {
        "integer", "real", "boolean", "char", "string", 
        "byte", "word", "longint", NULL
    };
    
    for (int i = 0; type_names[i] != NULL; i++) {
        save_input_state(in, &state);
        int len = strlen(type_names[i]);
        bool matches = true;
        
        for (int j = 0; j < len; j++) {
            char c = read1(in);
            if (tolower(c) != type_names[i][j]) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            // Check that next character is not alphanumeric (word boundary)
            char next_char = read1(in);
            if (next_char != EOF && (isalnum(next_char) || next_char == '_')) {
                matches = false;
            } else {
                if (next_char != EOF) in->start--; // Back up if not EOF
            }
        }
        
        if (matches) {
            // Create symbol with the actual matched text (preserving original case)
            restore_input_state(in, &state);
            char* matched_text = malloc(len + 1);
            for (int j = 0; j < len; j++) {
                matched_text[j] = read1(in);
            }
            matched_text[len] = '\0';
            
            ast_t* ast = new_ast();
            ast->typ = pargs->tag;
            ast->sym = sym_lookup(matched_text);
            ast->child = NULL;
            ast->next = NULL;
            free(matched_text);
            set_ast_position(ast, in);
            return make_success(ast);
        }
    }
    
    restore_input_state(in, &state);
    return make_failure(in, strdup("Expected built-in type name"));
}

combinator_t* type_name(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = type_name_fn;
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

// Removed unused relational_ops() function that had non-boundary-aware match("in")

// Pascal string literal parser - supports both single 'text' and double "text" quotes
static ParseResult pascal_string_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state; 
    save_input_state(in, &state);
    
    char quote_char = read1(in);
    if (quote_char != '"' && quote_char != '\'') {
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '\"' or '\\''"));
    }
    
    int capacity = 64;
    char* str_val = (char*)safe_malloc(capacity);
    int len = 0;
    char c;
    
    while ((c = read1(in)) != quote_char) {
        if (c == EOF) {
            free(str_val);
            return make_failure(in, strdup("Unterminated string"));
        }
        
        // Handle escape sequences for double quotes
        if (quote_char == '"' && c == '\\') {
            c = read1(in);
            if (c == EOF) {
                free(str_val);
                return make_failure(in, strdup("Unterminated string"));
            }
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
            }
        }
        
        // Expand buffer if needed
        if (len + 1 >= capacity) {
            capacity *= 2;
            char* new_str_val = realloc(str_val, capacity);
            if (!new_str_val) {
                free(str_val);
                exception("realloc failed");
            }
            str_val = new_str_val;
        }
        
        str_val[len++] = c;
    }
    
    str_val[len] = '\0';
    
    ast_t* ast = new_ast();
    ast->typ = pargs->tag;
    ast->sym = sym_lookup(str_val);
    ast->child = NULL;
    ast->next = NULL;
    free(str_val);
    set_ast_position(ast, in);
    
    return make_success(ast);
}

combinator_t* pascal_string(tag_t tag) {
    prim_args* args = (prim_args*)safe_malloc(sizeof(prim_args));
    args->tag = tag;
    combinator_t* comb = new_combinator();
    comb->type = P_STRING; // Reuse the same type
    comb->fn = pascal_string_fn;
    comb->args = args;
    return comb;
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
        case PASCAL_T_ARG_LIST: return "ARG_LIST";
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
        // Uses clause types
        case PASCAL_T_USES_SECTION: return "USES_SECTION";
        case PASCAL_T_USES_UNIT: return "USES_UNIT";
        // Const section types
        case PASCAL_T_CONST_SECTION: return "CONST_SECTION";
        case PASCAL_T_CONST_DECL: return "CONST_DECL";
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

// Post-process AST to handle semantic operations like set union
static void post_process_set_operations(ast_t* ast) {
    if (ast == NULL || ast == ast_nil) return;
    
    // Process children first (depth-first)
    post_process_set_operations(ast->child);
    post_process_set_operations(ast->next);
    
    // Check if this is an ADD operation with two SET operands
    if (ast->typ == PASCAL_T_ADD && ast->child && ast->child->next) {
        ast_t* left = ast->child;
        ast_t* right = ast->child->next;
        
        if (left->typ == PASCAL_T_SET && right->typ == PASCAL_T_SET) {
            // Convert ADD to SET_UNION
            ast->typ = PASCAL_T_SET_UNION;
        }
    }
}

// Helper function to create case-insensitive identifier parsers for built-in functions
// Arguments struct for builtin identifier parsing


// --- Parser Definition ---
void init_pascal_expression_parser(combinator_t** p) {
    // Standard Pascal identifier parser - handles all identifiers uniformly
    combinator_t* identifier = token(cident(PASCAL_T_IDENTIFIER));
    
    // Function name: use standard identifier parser  
    combinator_t* func_name = token(cident(PASCAL_T_IDENTIFIER));
    
    // Function call parser: function name followed by optional argument list
    combinator_t* arg_list = between(
        token(match("(")),
        token(match(")")),
        optional(sep_by(lazy(p), token(match(","))))
    );
    
    combinator_t* func_call = seq(new_combinator(), PASCAL_T_FUNC_CALL,
        func_name,                        // function name (built-in or custom)
        arg_list,
        NULL
    );
    
    // Array access parser: identifier[index1, index2, ...]
    combinator_t* index_list = between(
        token(match("[")),
        token(match("]")),
        sep_by(lazy(p), token(match(",")))
    );
    
    combinator_t* array_access = seq(new_combinator(), PASCAL_T_ARRAY_ACCESS,
        func_name,                        // array name (built-in or custom identifier)
        index_list,
        NULL
    );
    
    // Type cast parser: TypeName(expression) - only for built-in types
    combinator_t* typecast = seq(new_combinator(), PASCAL_T_TYPECAST,
        token(type_name(PASCAL_T_IDENTIFIER)), // type name - only built-in types
        between(token(match("(")), token(match(")")), lazy(p)), // expression
        NULL
    );
    
    // Boolean literal parsers
    combinator_t* boolean_true = seq(new_combinator(), PASCAL_T_BOOLEAN,
        match_ci("true"),
        NULL
    );
    combinator_t* boolean_false = seq(new_combinator(), PASCAL_T_BOOLEAN,
        match_ci("false"),  
        NULL
    );

    combinator_t *factor = multi(new_combinator(), PASCAL_T_NONE,
        token(real_number(PASCAL_T_REAL)),        // Real numbers (3.14) - try first
        token(integer(PASCAL_T_INTEGER)),         // Integers (123)
        token(char_literal(PASCAL_T_CHAR)),       // Characters ('A')
        token(pascal_string(PASCAL_T_STRING)),    // Strings ("hello" or 'hello')
        token(set_constructor(PASCAL_T_SET)),     // Set constructors [1, 2, 3]
        token(boolean_true),                      // Boolean true
        token(boolean_false),                     // Boolean false
        typecast,                                 // Type casts Integer(x) - try before func_call
        array_access,                             // Array access table[i,j] - try before func_call
        func_call,                                // Function calls func(x)
        identifier,                               // Identifiers (variables, built-ins)
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
    expr_altern(*p, 3, PASCAL_T_IN, token(keyword_ci("in")));
    expr_altern(*p, 3, PASCAL_T_IS, token(keyword_ci("is")));
    expr_altern(*p, 3, PASCAL_T_AS, token(keyword_ci("as")));
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
    expr_altern(*p, 6, PASCAL_T_INTDIV, token(keyword_ci("div")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(keyword_ci("mod")));
    expr_altern(*p, 6, PASCAL_T_MOD, token(match("%")));
    expr_altern(*p, 6, PASCAL_T_SHL, token(keyword_ci("shl")));
    expr_altern(*p, 6, PASCAL_T_SHR, token(keyword_ci("shr")));
    
    // Precedence 7: Unary operators (highest precedence)
    expr_insert(*p, 7, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
    expr_insert(*p, 7, PASCAL_T_POS, EXPR_PREFIX, ASSOC_NONE, token(match("+")));
    expr_insert(*p, 7, PASCAL_T_NOT, EXPR_PREFIX, ASSOC_NONE, token(match("not")));
    expr_insert(*p, 7, PASCAL_T_ADDR, EXPR_PREFIX, ASSOC_NONE, token(match("@")));
}

// ASM block body parser - uses proper until() combinator instead of manual scanning
combinator_t* asm_body(tag_t tag) {
    return until(match("end"), tag);  // Use raw match instead of token to preserve whitespace
}

// --- Utility Functions ---
ParseResult parse_pascal_expression(input_t* input, combinator_t* parser) {
    ParseResult result = parse(input, parser);
    if (result.is_success) {
        post_process_set_operations(result.value.ast);
    }
    return result;
}

// --- Pascal Statement Parser Implementation ---
void init_pascal_statement_parser(combinator_t** p) {
    // First create the expression parser to use within statements
    combinator_t** expr_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *expr_parser = new_combinator();
    (*expr_parser)->extra_to_free = expr_parser;
    init_pascal_expression_parser(expr_parser);
    
    // Create the main statement parser pointer for recursive references
    combinator_t** stmt_parser = p;
    
    // Assignment statement: identifier := expression (no semicolon here)
    combinator_t* assignment = seq(new_combinator(), PASCAL_T_ASSIGNMENT,
        token(cident(PASCAL_T_IDENTIFIER)),    // variable name
        token(match(":=")),                    // assignment operator
        lazy(expr_parser),                     // expression
        NULL
    );
    
    // Simple expression statement: expression (no semicolon here)
    combinator_t* expr_stmt = seq(new_combinator(), PASCAL_T_STATEMENT,
        lazy(expr_parser),                     // expression
        NULL
    );
    
    // Begin-end block: begin [statement_list] end  
    // Handle empty begin-end blocks explicitly to avoid recursive parsing issues
    combinator_t* empty_begin_end = seq(new_combinator(), PASCAL_T_BEGIN_BLOCK,
        token(match_ci("begin")),              // begin keyword  
        token(match_ci("end")),                // end keyword (immediately after begin)
        NULL
    );
    
    // Parse statements separated by semicolons (semicolon is separator, not terminator)
    // This allows for optional trailing semicolon as well
    combinator_t* stmt_list = sep_end_by(lazy(stmt_parser), token(match(";")));
    
    combinator_t* non_empty_begin_end = seq(new_combinator(), PASCAL_T_BEGIN_BLOCK,
        token(match_ci("begin")),              // begin keyword
        stmt_list,                             // statements each terminated by semicolon
        token(match_ci("end")),                // end keyword
        NULL
    );
    
    // Try empty block first, then non-empty
    combinator_t* begin_end_block = multi(new_combinator(), PASCAL_T_NONE,
        empty_begin_end,                       // try empty block first
        non_empty_begin_end,                   // then try non-empty block
        NULL
    );
    
    // If statement: if expression then statement [else statement]
    combinator_t* if_stmt = seq(new_combinator(), PASCAL_T_IF_STMT,
        token(match_ci("if")),                     // if keyword (case-insensitive)
        lazy(expr_parser),                         // condition
        token(match_ci("then")),                   // then keyword (case-insensitive)
        lazy(stmt_parser),                         // then statement
        optional(seq(new_combinator(), PASCAL_T_ELSE,    // optional else part
            token(match_ci("else")),               // else keyword (case-insensitive)
            lazy(stmt_parser),
            NULL
        )),
        NULL
    );
    
    // For statement: for identifier := expression (to|downto) expression do statement
    combinator_t* for_direction = multi(new_combinator(), PASCAL_T_NONE,
        token(match_ci("to")),                 // to keyword (case-insensitive)
        token(match_ci("downto")),             // downto keyword (case-insensitive)
        NULL
    );
    combinator_t* for_stmt = seq(new_combinator(), PASCAL_T_FOR_STMT,
        token(match_ci("for")),                // for keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),    // loop variable
        token(match(":=")),                    // assignment
        lazy(expr_parser),                     // start expression
        for_direction,                         // to or downto
        lazy(expr_parser),                     // end expression
        token(match_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // loop body statement
        NULL
    );
    
    // While statement: while expression do statement
    combinator_t* while_stmt = seq(new_combinator(), PASCAL_T_WHILE_STMT,
        token(match_ci("while")),              // while keyword (case-insensitive)
        lazy(expr_parser),                     // condition
        token(match_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // body statement
        NULL
    );
    
    // ASM block: asm ... end
    combinator_t* asm_stmt = seq(new_combinator(), PASCAL_T_ASM_BLOCK,
        token(match("asm")),                   // asm keyword
        asm_body(PASCAL_T_NONE),               // asm body content
        token(match("end")),                   // end keyword  
        NULL
    );
    
    // Main statement parser: try different types of statements (order matters!)
    // Note: VAR sections are handled by the complete program parser context
    multi(*stmt_parser, PASCAL_T_NONE,
        begin_end_block,                      // compound statements (must come before expr_stmt)
        asm_stmt,                             // inline assembly blocks
        if_stmt,                              // if statements
        for_stmt,                             // for statements
        while_stmt,                           // while statements
        assignment,                            // assignment statements
        expr_stmt,                            // expression statements (must be last)
        NULL
    );
}

// Pascal Program/Terminated Statement Parser - for standalone statements with semicolons
void init_pascal_program_parser(combinator_t** p) {
    // Create the base statement parser
    combinator_t** base_stmt = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *base_stmt = new_combinator();
    (*base_stmt)->extra_to_free = base_stmt;
    init_pascal_statement_parser(base_stmt);
    
    // Terminated statement: statement followed by semicolon
    seq(*p, PASCAL_T_NONE,
        lazy(base_stmt),                       // any statement
        token(match(";")),                     // followed by semicolon
        NULL
    );
}

// Pascal Procedure/Function Declaration Parser
void init_pascal_procedure_parser(combinator_t** p) {
    // Create expression parser for default values and type references
    combinator_t** expr_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *expr_parser = new_combinator();
    (*expr_parser)->extra_to_free = expr_parser;
    init_pascal_expression_parser(expr_parser);
    
    // Create statement parser for procedure/function bodies
    combinator_t** stmt_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *stmt_parser = new_combinator();
    (*stmt_parser)->extra_to_free = stmt_parser;
    init_pascal_statement_parser(stmt_parser);
    
    // Parameter: identifier : type (simplified - just use identifier for type)
    combinator_t* param = seq(new_combinator(), PASCAL_T_PARAM,
        token(cident(PASCAL_T_IDENTIFIER)),      // parameter name
        token(match(":")),                       // colon
        token(cident(PASCAL_T_IDENTIFIER)),      // type name (simplified)
        NULL
    );
    
    // Parameter list: optional ( param ; param ; ... )
    combinator_t* param_list = optional(between(
        token(match("(")),
        token(match(")")),
        sep_by(param, token(match(";")))
    ));
    
    // Return type: : type (for functions)  
    combinator_t* return_type = seq(new_combinator(), PASCAL_T_RETURN_TYPE,
        token(match(":")),                       // colon
        token(cident(PASCAL_T_IDENTIFIER)),      // return type (simplified)
        NULL
    );
    
    // Procedure declaration: procedure name [(params)] ; body
    combinator_t* procedure_decl = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(match("procedure")),               // procedure keyword
        token(cident(PASCAL_T_IDENTIFIER)),      // procedure name
        param_list,                              // optional parameter list
        token(match(";")),                       // semicolon
        lazy(stmt_parser),                       // procedure body
        NULL
    );
    
    // Function declaration: function name [(params)] : return_type ; body
    combinator_t* function_decl = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(match("function")),                // function keyword
        token(cident(PASCAL_T_IDENTIFIER)),      // function name
        param_list,                              // optional parameter list
        return_type,                             // return type
        token(match(";")),                       // semicolon
        lazy(stmt_parser),                       // function body
        NULL
    );
    
    // Main procedure parser: try function or procedure declaration
    multi(*p, PASCAL_T_NONE,
        function_decl,                           // function declarations first
        procedure_decl,                          // procedure declarations second
        NULL
    );
}

// Pascal Complete Program Parser - for full Pascal programs  
// Custom parser for main block content that parses statements properly
static ParseResult main_block_content_fn(input_t* in, void* args) {
    // First, capture content until "end" (like the original hack)
    int start_offset = in->start;
    int content_start = in->start;
    int content_end = -1;
    
    // Find the "end" keyword (case-insensitive)
    while (in->start < in->length) {
        if (in->start + 3 <= in->length &&
            strncasecmp(in->buffer + in->start, "end", 3) == 0) {
            // Check if it's a word boundary
            if (in->start + 3 == in->length || !isalnum(in->buffer[in->start + 3])) {
                content_end = in->start;
                break;
            }
        }
        in->start++;
    }
    
    if (content_end == -1) {
        return make_failure(in, strdup("Expected 'end' keyword"));
    }
    
    // Extract the content between begin and end
    int content_len = content_end - content_start;
    if (content_len == 0) {
        // Empty main block - return ast_nil
        in->start = content_end; // Position at "end"
        return make_success(ast_nil);
    }
    
    // Create input for parsing the main block content
    input_t* content_input = new_input();
    content_input->buffer = strndup(in->buffer + content_start, content_len);
    content_input->length = content_len;
    content_input->line = in->line;
    content_input->col = in->col;
    
    // Create statement parser to parse the content
    combinator_t* stmt_parser = new_combinator();
    init_pascal_statement_parser(&stmt_parser);
    
    // Parse as many statements as possible
    combinator_t* stmt_list = sep_end_by(stmt_parser, token(match(";")));
    ParseResult stmt_result = parse(content_input, stmt_list);
    
    // Position input at the end of content for the main parser to continue
    in->start = content_end;

    if (stmt_result.is_success && content_input->start < content_input->length) {
        free_ast(stmt_result.value.ast);
        char* err_msg;
        asprintf(&err_msg, "Syntax error at line %d, col %d", content_input->line, content_input->col);
        stmt_result = make_failure(content_input, err_msg);
    }
    
    // Clean up
    free(content_input->buffer);
    free(content_input);
    free_combinator(stmt_list);
    
    return stmt_result;
}

combinator_t* main_block_content(tag_t tag) {
    combinator_t* comb = new_combinator();
    prim_args* args = safe_malloc(sizeof(prim_args));
    args->tag = tag;
    comb->args = args;
    comb->fn = main_block_content_fn;
    return comb;
}

void init_pascal_complete_program_parser(combinator_t** p) {
    // Enhanced main block parser that tries proper statement parsing first, falls back to hack
    combinator_t* enhanced_main_block_content = main_block_content(PASCAL_T_NONE);
    combinator_t* main_block = seq(new_combinator(), PASCAL_T_MAIN_BLOCK,
        token(match_ci("begin")),                       // begin keyword
        enhanced_main_block_content,                 // properly parsed content (or fallback to raw text)
        token(match_ci("end")),                         // end keyword
        NULL
    );
    
    // Program parameter list: (identifier, identifier, ...)
    combinator_t* program_param = token(cident(PASCAL_T_IDENTIFIER));
    combinator_t* program_param_list = optional(between(
        token(match("(")),
        token(match(")")),
        sep_by(program_param, token(match(",")))
    ));
    
    // Enhanced Variable declaration: var1, var2, var3 : type;
    combinator_t* var_identifier_list = sep_by(token(cident(PASCAL_T_IDENTIFIER)), token(match(",")));
    combinator_t* type_spec = multi(new_combinator(), PASCAL_T_TYPE_SPEC,
        class_type(PASCAL_T_CLASS_TYPE),                // class types like class ... end
        array_type(PASCAL_T_ARRAY_TYPE),                // array types like ARRAY[0..9] OF integer
        range_type(PASCAL_T_RANGE_TYPE),                // range types like -1..1
        type_name(PASCAL_T_IDENTIFIER),                 // built-in types
        token(cident(PASCAL_T_IDENTIFIER)),             // custom types
        NULL
    );
    
    combinator_t* var_decl = seq(new_combinator(), PASCAL_T_VAR_DECL,
        var_identifier_list,                            // multiple variable names
        token(match(":")),                              // colon  
        type_spec,                                      // type specification
        token(match(";")),                              // semicolon
        NULL
    );
    
    // Var section: var var_decl var_decl ...
    combinator_t* var_section = seq(new_combinator(), PASCAL_T_VAR_SECTION,
        token(match_ci("var")),                         // var keyword
        many(var_decl),                              // multiple variable declarations
        NULL
    );
    
    // Type declaration: TypeName = TypeSpec;
    combinator_t* type_decl = seq(new_combinator(), PASCAL_T_TYPE_DECL,
        token(cident(PASCAL_T_IDENTIFIER)),          // type name
        token(match("=")),                           // equals sign  
        type_spec,                                   // type specification
        token(match(";")),                           // semicolon
        NULL
    );
    
    // Type section: type type_decl type_decl ...
    combinator_t* type_section = seq(new_combinator(), PASCAL_T_TYPE_SECTION,
        token(match_ci("type")),                        // type keyword
        many(type_decl),                             // multiple type declarations
        NULL
    );
    
    // Uses section: uses unit1, unit2, unit3;
    combinator_t* uses_unit = token(cident(PASCAL_T_USES_UNIT));
    combinator_t* uses_section = seq(new_combinator(), PASCAL_T_USES_SECTION,
        token(match_ci("uses")),                        // uses keyword  
        sep_by(uses_unit, token(match(","))),        // unit names separated by commas
        token(match(";")),                           // semicolon
        NULL
    );
    
    // Const section: const name : type = value; ...
    // For now, we'll create a simplified const parser that accepts basic values
    // plus a fallback for complex expressions
    combinator_t* simple_const_value = multi(new_combinator(), PASCAL_T_NONE,
        integer(PASCAL_T_INTEGER),
        string(PASCAL_T_STRING), 
        cident(PASCAL_T_IDENTIFIER),
        NULL
    );
    
    // Fallback: consume everything until semicolon for complex array literals
    combinator_t* complex_const_value = until(match(";"), PASCAL_T_STRING);
    
    combinator_t* const_value = multi(new_combinator(), PASCAL_T_NONE,
        simple_const_value,
        complex_const_value,  // fallback for complex literals  
        NULL
    );
    
    combinator_t* const_decl = seq(new_combinator(), PASCAL_T_CONST_DECL,
        token(cident(PASCAL_T_IDENTIFIER)),          // constant name
        optional(seq(new_combinator(), PASCAL_T_NONE,
            token(match(":")),                       // colon
            token(cident(PASCAL_T_IDENTIFIER)),      // type (simplified)
            NULL
        )),
        token(match("=")),                           // equals
        const_value,                                 // constant value (simplified for now)
        token(match(";")),                           // semicolon
        NULL
    );
    
    combinator_t* const_section = seq(new_combinator(), PASCAL_T_CONST_SECTION,
        token(match_ci("const")),                       // const keyword
        many(const_decl),                            // multiple const declarations
        NULL
    );
    
    // Create procedure/function parsers for use in complete program
    // Need to create a modified procedure parser that supports var parameters
    combinator_t** stmt_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *stmt_parser = new_combinator();
    (*stmt_parser)->extra_to_free = stmt_parser;
    init_pascal_statement_parser(stmt_parser);
    
    // Enhanced parameter: [var] identifier : type (support for var parameters)
    combinator_t* var_keyword = optional(token(match("var")));
    combinator_t* param = seq(new_combinator(), PASCAL_T_PARAM,
        var_keyword,                                 // optional var keyword
        token(cident(PASCAL_T_IDENTIFIER)),          // parameter name
        token(match(":")),                           // colon
        token(cident(PASCAL_T_IDENTIFIER)),          // type name (simplified)
        NULL
    );
    
    // Parameter list: optional ( param ; param ; ... )
    combinator_t* param_list = optional(between(
        token(match("(")),
        token(match(")")),
        sep_by(param, token(match(";")))
    ));
    
    // Return type: : type (for functions)
    combinator_t* return_type = seq(new_combinator(), PASCAL_T_RETURN_TYPE,
        token(match(":")),                           // colon
        token(cident(PASCAL_T_IDENTIFIER)),          // return type (simplified)
        NULL
    );
    
    // Function body parser: handles local sections followed by begin-end block
    // This is different from statement parsing - functions can have local declarations
    
    // Local VAR section - reuse the existing var_section parser
    combinator_t* local_var_section = var_section;
    
    // Function body for complete programs: uses the statement parser directly without lazy reference
    combinator_t** direct_stmt_parser = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *direct_stmt_parser = new_combinator();
    (*direct_stmt_parser)->extra_to_free = direct_stmt_parser;
    init_pascal_statement_parser(direct_stmt_parser);
    
    combinator_t* program_function_body = seq(new_combinator(), PASCAL_T_NONE,
        optional(local_var_section),                 // now enabled - functions can have local VAR sections
        *direct_stmt_parser,                         // use statement parser directly (not lazy)
        token(match(";")),                           // terminating semicolon after function body
        NULL
    );
    
    // Function body for standalone parsing (no terminating semicolon)
    combinator_t* standalone_function_body = seq(new_combinator(), PASCAL_T_NONE,
        optional(local_var_section),                 // optional local var section
        lazy(stmt_parser),                           // begin-end block handled by statement parser
        NULL
    );
    
    // Procedure declaration: procedure name [(params)] ; body
    combinator_t* procedure_decl = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(match_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon after parameters
        program_function_body,                       // procedure body with terminating semicolon for programs
        NULL
    );
    
    // Function declaration (header only): function name [(params)] : return_type;
    combinator_t* function_declaration = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(match_ci("function")),                 // function keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // function name
        param_list,                                  // optional parameter list
        return_type,                                 // return type
        token(match(";")),                           // terminating semicolon (no body)
        NULL
    );
    
    // Procedure declaration (header only): procedure name [(params)];
    combinator_t* procedure_declaration = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(match_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // terminating semicolon (no body)
        NULL
    );

    // Function definition (with body): function name [(params)] : return_type ; body ;
    combinator_t* function_definition = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(match_ci("function")),                 // function keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // function name
        param_list,                                  // optional parameter list
        return_type,                                 // return type
        token(match(";")),                           // semicolon after return type (like standalone functions)
        program_function_body,                       // function body with terminating semicolon for programs
        NULL
    );
    
    // Procedure definition (with body): procedure name [(params)] ; body ;
    combinator_t* procedure_definition = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(match_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon after parameters
        program_function_body,                       // procedure body with terminating semicolon for programs
        NULL
    );

    // Procedure or function definitions - only include definitions with bodies for complete programs
    // Function/procedure declarations (headers only) are not used in complete programs
    combinator_t* proc_or_func = multi(new_combinator(), PASCAL_T_NONE,
        function_definition,                         // function definitions (with bodies)
        procedure_definition,                        // procedure definitions (with bodies)
        NULL
    );
    
    // Complete program: program Name(params); [uses clause] [type section] [const section] [var section] [procedures/functions] begin end.
    seq(*p, PASCAL_T_PROGRAM_DECL,
        token(match_ci("program")),                     // program keyword
        token(cident(PASCAL_T_IDENTIFIER)),          // program name  
        program_param_list,                          // optional parameter list
        token(match(";")),                           // semicolon
        optional(uses_section),                      // optional uses section
        optional(type_section),                      // optional type section
        optional(const_section),                     // optional const section
        optional(var_section),                       // optional var section
        many(proc_or_func),                          // zero or more procedure/function declarations
        main_block,                                  // main program block
        token(match(".")),                           // final period
        NULL
    );
}
