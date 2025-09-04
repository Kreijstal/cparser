#include "pascal_parser.h"
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

// Pascal reserved keywords that should NEVER be identifiers (case-insensitive) 
// This is a conservative list of keywords that should never be parsed as identifiers
static const char* pascal_reserved_keywords[] = {
    "begin", "end", "if", "then", "else", "while", "do", "for", "to", "downto",
    "repeat", "until", "case", "of", "var", "const", "type", 
    "and", "or", "not", "xor", "div", "mod", "in", "nil", "true", "false",
    "array", "record", "set", "packed",
    // Exception handling keywords
    "try", "finally", "except", "raise", "on",
    // Class and object-oriented keywords  
    "class", "object", "private", "public", "protected", "published",
    "property", "inherited", "self", "constructor", "destructor",
    // Additional Pascal keywords
    "function", "procedure", "program", "unit", "uses", "interface", "implementation",
    NULL
};

// Helper function to check if a string is a Pascal reserved keyword (case-insensitive)
static bool is_pascal_keyword(const char* str) {
    for (int i = 0; pascal_reserved_keywords[i] != NULL; i++) {
        if (strcasecmp(str, pascal_reserved_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
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

// Range type parser: start..end (e.g., -1..1)
static ParseResult range_type_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Try parsing integer (including negative) or identifier for start value
    combinator_t* start_parser = multi(new_combinator(), PASCAL_T_NONE,
        integer(PASCAL_T_INTEGER),
        seq(new_combinator(), PASCAL_T_INTEGER,
            match("-"),
            integer(PASCAL_T_INTEGER),
            NULL),
        cident(PASCAL_T_IDENTIFIER),
        NULL);
    
    ParseResult start_result = parse(in, start_parser);
    if (!start_result.is_success) {
        free_combinator(start_parser);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected range start value"));
    }
    
    // Parse the ".." separator with whitespace handling
    combinator_t* range_sep = token(match(".."));
    ParseResult sep_result = parse(in, range_sep);
    if (!sep_result.is_success) {
        free_ast(start_result.value.ast);
        free_combinator(start_parser);
        free_combinator(range_sep);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected '..' in range type"));
    }
    free_combinator(range_sep);
    free_ast(sep_result.value.ast);
    
    // Parse end value using the same parser
    ParseResult end_result = parse(in, start_parser);
    if (!end_result.is_success) {
        free_ast(start_result.value.ast);
        free_combinator(start_parser);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected range end value"));
    }
    
    // Create range AST
    ast_t* range_ast = new_ast();
    range_ast->typ = pargs->tag;
    range_ast->child = start_result.value.ast;
    start_result.value.ast->next = end_result.value.ast;
    range_ast->sym = NULL;
    range_ast->next = NULL;
    
    free_combinator(start_parser);
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
    combinator_t* array_keyword = token(keyword_ci("array"));
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
    combinator_t* of_keyword = token(keyword_ci("of"));
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

// Enhanced class type parser: class [access_sections] end
static ParseResult class_type_fn(input_t* in, void* args) {
    prim_args* pargs = (prim_args*)args;
    InputState state;
    save_input_state(in, &state);
    
    // Parse "class" keyword (case insensitive)
    combinator_t* class_keyword = token(keyword_ci("class"));
    ParseResult class_res = parse(in, class_keyword);
    if (!class_res.is_success) {
        free_combinator(class_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'class'"));
    }
    free_ast(class_res.value.ast);
    free_combinator(class_keyword);
    
    // Build class body parser
    ast_t* class_body = NULL;
    
    // Field declaration: field_name: Type;
    combinator_t* field_name = token(cident(PASCAL_T_IDENTIFIER));
    combinator_t* field_type = token(cident(PASCAL_T_IDENTIFIER)); // simplified type for now
    combinator_t* field_decl = seq(new_combinator(), PASCAL_T_FIELD_DECL,
        field_name,
        token(match(":")),
        field_type,
        token(match(";")),
        NULL
    );
    
    // Method declarations (simplified - just headers for now)
    combinator_t* method_name = token(cident(PASCAL_T_IDENTIFIER));
    combinator_t* param_list = optional(between(
        token(match("(")),
        token(match(")")),
        sep_by(token(cident(PASCAL_T_IDENTIFIER)), token(match(";")))
    ));
    
    // Constructor declaration: constructor Name;
    combinator_t* constructor_decl = seq(new_combinator(), PASCAL_T_CONSTRUCTOR_DECL,
        token(keyword_ci("constructor")),
        method_name,
        param_list,
        token(match(";")),
        NULL
    );
    
    // Destructor declaration: destructor Name; override;  
    combinator_t* destructor_decl = seq(new_combinator(), PASCAL_T_DESTRUCTOR_DECL,
        token(keyword_ci("destructor")),
        method_name,
        param_list,
        token(match(";")),
        optional(token(keyword_ci("override"))),
        optional(token(match(";"))),
        NULL
    );
    
    // Procedure declaration: procedure Name;
    combinator_t* procedure_decl = seq(new_combinator(), PASCAL_T_METHOD_DECL,
        token(keyword_ci("procedure")),
        method_name,
        param_list,
        token(match(";")),
        NULL
    );
    
    // Function declaration: function Name: ReturnType;  
    combinator_t* function_decl = seq(new_combinator(), PASCAL_T_METHOD_DECL,
        token(keyword_ci("function")),
        method_name,
        param_list,
        token(match(":")),
        token(cident(PASCAL_T_IDENTIFIER)), // return type
        token(match(";")),
        NULL
    );
    
    // Property declaration: property Name: Type read ReadField write WriteField;
    combinator_t* property_decl = seq(new_combinator(), PASCAL_T_PROPERTY_DECL,
        token(keyword_ci("property")),
        token(cident(PASCAL_T_IDENTIFIER)), // property name
        token(match(":")),
        token(cident(PASCAL_T_IDENTIFIER)), // property type
        optional(seq(new_combinator(), PASCAL_T_NONE,
            token(keyword_ci("read")),
            token(cident(PASCAL_T_IDENTIFIER)), // read field/method
            NULL
        )),
        optional(seq(new_combinator(), PASCAL_T_NONE,
            token(keyword_ci("write")),
            token(cident(PASCAL_T_IDENTIFIER)), // write field/method
            NULL
        )),
        token(match(";")),
        NULL
    );
    
    // Class member: field, method, constructor, destructor, or property
    combinator_t* class_member = multi(new_combinator(), PASCAL_T_CLASS_MEMBER,
        constructor_decl,
        destructor_decl,
        procedure_decl,
        function_decl,
        property_decl,
        field_decl,
        NULL
    );
    
    // Comment handling - simple line comment parser
    combinator_t* line_comment = seq(new_combinator(), PASCAL_T_COMMENT,
        match("//"),
        until(match("\n"), PASCAL_T_STRING),
        NULL
    );
    
    // Skip comments and whitespace in class body
    combinator_t* class_element = multi(new_combinator(), PASCAL_T_NONE,
        class_member,
        line_comment,
        NULL
    );
    
    // Access sections: private, public, protected, published
    combinator_t* access_keyword = multi(new_combinator(), PASCAL_T_ACCESS_MODIFIER,
        token(keyword_ci("private")),
        token(keyword_ci("public")),
        token(keyword_ci("protected")),
        token(keyword_ci("published")),
        NULL
    );
    
    // Access section: access_keyword followed by members
    combinator_t* access_section = seq(new_combinator(), PASCAL_T_NONE,
        access_keyword,
        many(class_element),
        NULL
    );
    
    // Class body: optional access sections and members
    combinator_t* class_body_parser = many(multi(new_combinator(), PASCAL_T_NONE,
        access_section,
        class_element,
        NULL
    ));
    
    // Parse class body
    ParseResult body_res = parse(in, class_body_parser);
    if (body_res.is_success) {
        class_body = body_res.value.ast;
    } else {
        // Class body parsing failed, but this might be acceptable for empty class
        class_body = NULL;
        free_error(body_res.value.error);
    }
    free_combinator(class_body_parser);
    
    // Parse "end" keyword  
    combinator_t* end_keyword = token(keyword_ci("end"));
    ParseResult end_res = parse(in, end_keyword);
    if (!end_res.is_success) {
        if (class_body) free_ast(class_body);
        free_combinator(end_keyword);
        restore_input_state(in, &state);
        return make_failure(in, strdup("Expected 'end' after class"));
    }
    free_ast(end_res.value.ast);
    free_combinator(end_keyword);
    
    // Build final class AST
    ast_t* class_ast = new_ast();
    class_ast->typ = pargs->tag;
    class_ast->sym = NULL;
    class_ast->child = class_body; // Include parsed class body
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
    
    while (true) {
        c = read1(in);
        
        if (c == EOF) {
            free(str_val);
            return make_failure(in, strdup("Unterminated string"));
        }
        
        // Handle Pascal-style escaped single quotes (doubled quotes)
        if (quote_char == '\'' && c == '\'') {
            // Check if the next character is also a single quote
            char next_c = read1(in);
            if (next_c == '\'') {
                // This is an escaped single quote, keep the single quote character
                c = '\'';
            } else {
                // This is the end of the string, put back the character and exit loop
                if (next_c != EOF) in->start--;
                break;
            }
        }
        // Handle double quote termination
        else if (quote_char == '"' && c == quote_char) {
            break;
        }
        // Handle escape sequences for double quotes
        else if (quote_char == '"' && c == '\\') {
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
    // Pascal identifier parser - use expression identifier that allows some keywords in expression contexts
    combinator_t* identifier = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));
    
    // Function name: use expression identifier parser that allows certain keywords as function names
    combinator_t* func_name = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));
    
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

    // Tuple constructor: (expr, expr, ...) - for nested array constants like ((1,2),(3,4))
    combinator_t* tuple = seq(new_combinator(), PASCAL_T_TUPLE,
        token(match("(")),
        sep_by(lazy(p), token(match(","))),  // comma-separated list of expressions
        token(match(")")),
        NULL
    );

    combinator_t *factor = multi(new_combinator(), PASCAL_T_NONE,
        token(real_number(PASCAL_T_REAL)),        // Real numbers (3.14) - try first
        token(integer(PASCAL_T_INTEGER)),         // Integers (123)
        token(char_literal(PASCAL_T_CHAR)),       // Characters ('A')
        token(pascal_string(PASCAL_T_STRING)),    // Strings ("hello" or 'hello')
        set_constructor(PASCAL_T_SET, p),         // Set constructors [1, 2, 3]
        token(boolean_true),                      // Boolean true
        token(boolean_false),                     // Boolean false
        typecast,                                 // Type casts Integer(x) - try before func_call
        array_access,                             // Array access table[i,j] - try before func_call
        func_call,                                // Function calls func(x)
        between(token(match("(")), token(match(")")), lazy(p)), // Parenthesized expressions - try before tuple
        tuple,                                    // Tuple constants (a,b,c) - try after parenthesized expressions
        identifier,                               // Identifiers (variables, built-ins)
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
    
    // Precedence 7: Unary operators
    expr_insert(*p, 7, PASCAL_T_NEG, EXPR_PREFIX, ASSOC_NONE, token(match("-")));
    expr_insert(*p, 7, PASCAL_T_POS, EXPR_PREFIX, ASSOC_NONE, token(match("+")));
    expr_insert(*p, 7, PASCAL_T_NOT, EXPR_PREFIX, ASSOC_NONE, token(match("not")));
    expr_insert(*p, 7, PASCAL_T_ADDR, EXPR_PREFIX, ASSOC_NONE, token(match("@")));
    
    // Field width operator for formatted output: expression:width (same precedence as unary)
    expr_insert(*p, 7, PASCAL_T_FIELD_WIDTH, EXPR_INFIX, ASSOC_LEFT, token(match(":")));
    
    // Precedence 8: Member access (highest precedence) - but not if followed by another dot
    combinator_t* member_access_op = seq(new_combinator(), PASCAL_T_NONE,
        match("."),
        pnot(match(".")),  // not followed by another dot
        NULL
    );
    expr_insert(*p, 8, PASCAL_T_MEMBER_ACCESS, EXPR_INFIX, ASSOC_LEFT, token(member_access_op));

    // Range operator (..) - SECOND insertion to override member access priority
    // (Multi-character operators added last = tried first in expr parser)
    expr_altern(*p, 4, PASCAL_T_RANGE, token(match("..")));
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
    
    // Left-value parser: accepts identifiers, member access expressions, and array access
    combinator_t* simple_identifier = token(pascal_expression_identifier(PASCAL_T_IDENTIFIER));
    combinator_t* member_access_lval = seq(new_combinator(), PASCAL_T_MEMBER_ACCESS,
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // object name
        token(match(".")),                                            // dot
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // field/property name
        NULL
    );
    // Array access for lvalue: identifier[index, ...]
    combinator_t* array_access_lval = seq(new_combinator(), PASCAL_T_ARRAY_ACCESS,
        token(pascal_expression_identifier(PASCAL_T_IDENTIFIER)),     // array name
        between(token(match("[")), token(match("]")), 
            sep_by(lazy(expr_parser), token(match(",")))),             // indices
        NULL
    );
    combinator_t* lvalue = multi(new_combinator(), PASCAL_T_NONE,
        array_access_lval,       // try array access first
        member_access_lval,      // try member access second
        simple_identifier,       // then simple identifier
        NULL
    );
    
    // Assignment statement: lvalue := expression (no semicolon here)
    combinator_t* assignment = seq(new_combinator(), PASCAL_T_ASSIGNMENT,
        lvalue,                                // left-hand side (identifier or member access)
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
        token(keyword_ci("begin")),              // begin keyword  
        token(keyword_ci("end")),                // end keyword (immediately after begin)
        NULL
    );
    
    // Standard BEGIN-END block for main statement parser (uses full recursive parsing)
    // Create custom statement list parser to properly handle Pascal semicolon rules
    // Pascal semicolons are separators, but there can be an optional trailing semicolon
    
    // Simplified statement list parser - just use sep_by with optional trailing semicolon
    combinator_t* stmt_list = seq(new_combinator(), PASCAL_T_NONE,
        sep_by(lazy(stmt_parser), token(match(";"))),     // statements separated by semicolons
        optional(token(match(";"))),                      // optional trailing semicolon
        NULL
    );
    
    combinator_t* non_empty_begin_end = seq(new_combinator(), PASCAL_T_BEGIN_BLOCK,
        token(keyword_ci("begin")),              // begin keyword
        stmt_list,                             // statements each terminated by semicolon
        token(keyword_ci("end")),                // end keyword
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
        token(keyword_ci("if")),                     // if keyword (case-insensitive)
        lazy(expr_parser),                         // condition
        token(keyword_ci("then")),                   // then keyword (case-insensitive)
        lazy(stmt_parser),                         // then statement
        optional(seq(new_combinator(), PASCAL_T_ELSE,    // optional else part
            token(keyword_ci("else")),               // else keyword (case-insensitive)
            lazy(stmt_parser),
            NULL
        )),
        NULL
    );
    
    // For statement: for identifier := expression (to|downto) expression do statement
    combinator_t* for_direction = multi(new_combinator(), PASCAL_T_NONE,
        token(keyword_ci("to")),                 // to keyword (case-insensitive)
        token(keyword_ci("downto")),             // downto keyword (case-insensitive)
        NULL
    );
    combinator_t* for_stmt = seq(new_combinator(), PASCAL_T_FOR_STMT,
        token(keyword_ci("for")),                // for keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),    // loop variable
        token(match(":=")),                    // assignment
        lazy(expr_parser),                     // start expression
        for_direction,                         // to or downto
        lazy(expr_parser),                     // end expression
        token(keyword_ci("do")),                 // do keyword (case-insensitive)
        lazy(stmt_parser),                     // loop body statement
        NULL
    );
    
    // While statement: while expression do statement
    combinator_t* while_stmt = seq(new_combinator(), PASCAL_T_WHILE_STMT,
        token(keyword_ci("while")),              // while keyword (case-insensitive)
        lazy(expr_parser),                     // condition
        token(keyword_ci("do")),                 // do keyword (case-insensitive)
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
    
    // Try-finally block: try statements finally statements end
    // Use more lenient statement parsing that doesn't require semicolons
    combinator_t* try_finally = seq(new_combinator(), PASCAL_T_TRY_BLOCK,
        token(keyword_ci("try")),              // try keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in try block
        token(keyword_ci("finally")),          // finally keyword (case-insensitive) 
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in finally block
        token(keyword_ci("end")),              // end keyword (case-insensitive)
        NULL
    );
    
    // Try-except block: try statements except statements end
    combinator_t* try_except = seq(new_combinator(), PASCAL_T_TRY_BLOCK,
        token(keyword_ci("try")),              // try keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in try block
        token(keyword_ci("except")),           // except keyword (case-insensitive)
        many(seq(new_combinator(), PASCAL_T_NONE,
            lazy(stmt_parser),
            optional(token(match(";"))),       // optional semicolon after each statement
            NULL
        )),                                    // statements in except block
        token(keyword_ci("end")),              // end keyword (case-insensitive)
        NULL
    );
    
    // Raise statement: raise [expression]
    combinator_t* raise_stmt = seq(new_combinator(), PASCAL_T_RAISE_STMT,
        token(keyword_ci("raise")),            // raise keyword (case-insensitive)
        optional(lazy(expr_parser)),           // optional exception expression
        NULL
    );
    
    // Inherited statement: inherited [method_call]
    combinator_t* inherited_stmt = seq(new_combinator(), PASCAL_T_INHERITED_STMT,
        token(keyword_ci("inherited")),        // inherited keyword (case-insensitive)
        optional(lazy(expr_parser)),           // optional method call expression (e.g., inherited Destroy)
        NULL
    );
    
    // Main statement parser: try different types of statements (order matters!)
    // Note: VAR sections are handled by the complete program parser context
    multi(*stmt_parser, PASCAL_T_NONE,
        begin_end_block,                      // compound statements (must come before expr_stmt)
        try_finally,                          // try-finally blocks
        try_except,                           // try-except blocks
        raise_stmt,                           // raise statements
        inherited_stmt,                       // inherited statements
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

// Pascal Method Implementation Parser - for constructor/destructor/procedure implementations
void init_pascal_method_implementation_parser(combinator_t** p) {
    // Create statement parser for method bodies
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
    
    // Method name with class: ClassName.MethodName
    combinator_t* method_name_with_class = seq(new_combinator(), PASCAL_T_NONE,
        token(cident(PASCAL_T_IDENTIFIER)),      // class name
        token(match(".")),                       // dot
        token(cident(PASCAL_T_IDENTIFIER)),      // method name
        NULL
    );
    
    // Constructor implementation: constructor ClassName.MethodName[(params)]; body
    combinator_t* constructor_impl = seq(new_combinator(), PASCAL_T_CONSTRUCTOR_DECL,
        token(keyword_ci("constructor")),        // constructor keyword
        method_name_with_class,                  // ClassName.MethodName
        param_list,                              // optional parameter list
        token(match(";")),                       // semicolon
        lazy(stmt_parser),                       // method body
        optional(token(match(";"))),             // optional terminating semicolon
        NULL
    );
    
    // Destructor implementation: destructor ClassName.MethodName[(params)]; body
    combinator_t* destructor_impl = seq(new_combinator(), PASCAL_T_DESTRUCTOR_DECL,
        token(keyword_ci("destructor")),         // destructor keyword
        method_name_with_class,                  // ClassName.MethodName
        param_list,                              // optional parameter list
        token(match(";")),                       // semicolon
        lazy(stmt_parser),                       // method body
        optional(token(match(";"))),             // optional terminating semicolon
        NULL
    );
    
    // Procedure implementation: procedure ClassName.MethodName[(params)]; body
    combinator_t* procedure_impl = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),          // procedure keyword
        method_name_with_class,                  // ClassName.MethodName
        param_list,                              // optional parameter list
        token(match(";")),                       // semicolon
        lazy(stmt_parser),                       // method body
        optional(token(match(";"))),             // optional terminating semicolon
        NULL
    );
    
    // Method implementation parser: constructor, destructor, or procedure implementation
    multi(*p, PASCAL_T_NONE,
        constructor_impl,
        destructor_impl,
        procedure_impl,
        NULL
    );
}

// Pascal Complete Program Parser - for full Pascal programs  
// Custom parser for main block content that parses statements properly
static ParseResult main_block_content_fn(input_t* in, void* args) {
    // Parse statements until we can't parse any more
    // Don't look for "end" - that's handled by the parent main_block parser
    
    // Create statement parser to parse the content
    combinator_t* stmt_parser = new_combinator();
    init_pascal_statement_parser(&stmt_parser);
    
    // Parse as many statements as possible, separated by semicolons
    combinator_t* stmt_list = many(seq(new_combinator(), PASCAL_T_NONE,
        stmt_parser,
        optional(token(match(";"))),  // optional semicolon after each statement
        NULL
    ));
    
    ParseResult stmt_result = parse(in, stmt_list);
    
    // Clean up
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
        token(keyword_ci("begin")),                     // begin keyword (with word boundary check)
        enhanced_main_block_content,                 // properly parsed content (or fallback to raw text)
        token(keyword_ci("end")),                       // end keyword (with word boundary check)
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
        token(keyword_ci("var")),                       // var keyword (with word boundary check)
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
        token(keyword_ci("type")),                      // type keyword (with word boundary check)
        many(type_decl),                             // multiple type declarations
        NULL
    );
    
    // Uses section: uses unit1, unit2, unit3;
    combinator_t* uses_unit = token(cident(PASCAL_T_USES_UNIT));
    combinator_t* uses_section = seq(new_combinator(), PASCAL_T_USES_SECTION,
        token(keyword_ci("uses")),                      // uses keyword (with word boundary check)
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
        token(keyword_ci("const")),                     // const keyword (with word boundary check)
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
    
    // Create a specialized function body parser that avoids circular references
    // This parser handles the most common function body patterns without full recursive complexity
    
    // Function body for standalone parsing (no terminating semicolon)
    // Function body that can contain nested function/procedure declarations
    combinator_t** nested_proc_or_func = (combinator_t**)safe_malloc(sizeof(combinator_t*));
    *nested_proc_or_func = new_combinator();
    (*nested_proc_or_func)->extra_to_free = nested_proc_or_func;
    
    // Forward declaration for nested functions - these will refer to function_definition and procedure_definition below
    combinator_t* nested_function_decl = lazy(nested_proc_or_func);
    
    combinator_t* nested_function_body = seq(new_combinator(), PASCAL_T_NONE,
        optional(local_var_section),                 // optional local var section
        many(nested_function_decl),                  // zero or more nested function/procedure declarations
        lazy(stmt_parser),                           // begin-end block handled by statement parser
        NULL
    );

    combinator_t* standalone_function_body = seq(new_combinator(), PASCAL_T_NONE,
        optional(local_var_section),                 // optional local var section
        lazy(stmt_parser),                           // begin-end block handled by statement parser
        NULL
    );

    // Use the nested function body parser for complete programs to support nested functions
    // Use the standalone function body for standalone parsing
    combinator_t* program_function_body = nested_function_body;
    
    // Procedure declaration: procedure name [(params)] ; body
    combinator_t* procedure_decl = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon after parameters
        program_function_body,                       // procedure body with terminating semicolon for programs
        NULL
    );
    
    // Function declaration (header only): function name [(params)] : return_type;
    combinator_t* function_declaration = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(keyword_ci("function")),               // function keyword (with word boundary check)
        token(cident(PASCAL_T_IDENTIFIER)),          // function name
        param_list,                                  // optional parameter list
        return_type,                                 // return type
        token(match(";")),                           // terminating semicolon (no body)
        NULL
    );
    
    // Procedure declaration (header only): procedure name [(params)];
    combinator_t* procedure_declaration = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // terminating semicolon (no body)
        NULL
    );

    // Function definition (with body): function name [(params)] : return_type ; body ;
    combinator_t* function_definition = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(keyword_ci("function")),                 // function keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // function name
        param_list,                                  // optional parameter list
        return_type,                                 // return type
        token(match(";")),                           // semicolon after return type (like standalone functions)
        program_function_body,                       // function body without terminating semicolon
        token(match(";")),                           // terminating semicolon after function definition
        NULL
    );
    
    // Procedure definition (with body): procedure name [(params)] ; body ;
    combinator_t* procedure_definition = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon after parameters
        program_function_body,                       // procedure body without terminating semicolon
        token(match(";")),                           // terminating semicolon after procedure definition
        NULL
    );

    // Create simple working function and procedure parsers based on the standalone versions
    // These work because they use the simpler statement parsing approach
    
    // Working function parser: function name [(params)] : return_type ; body ;  
    combinator_t* working_function = seq(new_combinator(), PASCAL_T_FUNCTION_DECL,
        token(keyword_ci("function")),               // function keyword (with word boundary check)
        token(cident(PASCAL_T_IDENTIFIER)),          // function name
        param_list,                                  // optional parameter list
        return_type,                                 // return type
        token(match(";")),                           // semicolon after signature
        program_function_body,                       // function body with VAR section support
        optional(token(match(";"))),                 // optional terminating semicolon after function body
        NULL
    );
    
    // Working procedure parser: procedure name [(params)] ; body ;
    combinator_t* working_procedure = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),                // procedure keyword (case-insensitive)
        token(cident(PASCAL_T_IDENTIFIER)),          // procedure name
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon after signature
        program_function_body,                       // procedure body with VAR section support
        optional(token(match(";"))),                 // optional terminating semicolon after procedure body
        NULL
    );

    // Object Pascal method implementations (constructor/destructor/procedure with class.method syntax)
    // These are Object Pascal extensions, not standard Pascal
    combinator_t* method_name_with_class = seq(new_combinator(), PASCAL_T_NONE,
        token(cident(PASCAL_T_IDENTIFIER)),          // class name
        token(match(".")),                           // dot
        token(cident(PASCAL_T_IDENTIFIER)),          // method name
        NULL
    );
    
    combinator_t* constructor_impl = seq(new_combinator(), PASCAL_T_CONSTRUCTOR_DECL,
        token(keyword_ci("constructor")),              // constructor keyword (with word boundary check)
        method_name_with_class,                      // ClassName.MethodName
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon
        lazy(stmt_parser),                           // use statement parser for method body
        optional(token(match(";"))),                 // optional terminating semicolon
        NULL
    );
    
    combinator_t* destructor_impl = seq(new_combinator(), PASCAL_T_DESTRUCTOR_DECL,
        token(keyword_ci("destructor")),               // destructor keyword (with word boundary check)
        method_name_with_class,                      // ClassName.MethodName
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon
        lazy(stmt_parser),                           // use statement parser for method body
        optional(token(match(";"))),                 // optional terminating semicolon
        NULL
    );
    
    combinator_t* procedure_impl = seq(new_combinator(), PASCAL_T_PROCEDURE_DECL,
        token(keyword_ci("procedure")),              // procedure keyword (with word boundary check)
        method_name_with_class,                      // ClassName.MethodName
        param_list,                                  // optional parameter list
        token(match(";")),                           // semicolon
        lazy(stmt_parser),                           // use statement parser for method body
        optional(token(match(";"))),                 // optional terminating semicolon
        NULL
    );

    // Object Pascal method implementations - separate from standard Pascal proc_or_func
    combinator_t* method_impl = multi(new_combinator(), PASCAL_T_NONE,
        constructor_impl,
        destructor_impl,
        procedure_impl,
        NULL
    );

    // Standard Pascal procedure or function definitions
    combinator_t* proc_or_func = multi(new_combinator(), PASCAL_T_NONE,
        working_function,                            // working function parser
        working_procedure,                           // working procedure parser
        NULL
    );
    
    // Combined Pascal and Object Pascal declarations
    combinator_t* all_declarations = multi(new_combinator(), PASCAL_T_NONE,
        proc_or_func,                                // standard Pascal procedures/functions
        method_impl,                                 // Object Pascal method implementations
        NULL
    );
    
    // Set up the nested function parser to point to the working function/procedure parsers
    // This allows nested function/procedure declarations within function bodies
    multi(*nested_proc_or_func, PASCAL_T_NONE,
        working_function,                            // nested functions
        working_procedure,                           // nested procedures
        NULL
    );
    
    // Complete program: program Name(params); [uses clause] [type section] [const section] [var section] [procedures/functions] [var section] begin end.
    seq(*p, PASCAL_T_PROGRAM_DECL,
        token(keyword_ci("program")),                   // program keyword (with word boundary check)
        token(cident(PASCAL_T_IDENTIFIER)),          // program name  
        program_param_list,                          // optional parameter list
        token(match(";")),                           // semicolon
        optional(uses_section),                      // optional uses section
        optional(type_section),                      // optional type section
        optional(const_section),                     // optional const section
        optional(var_section),                       // optional var section (before functions)
        many(all_declarations),                      // zero or more procedure/function/method declarations
        optional(var_section),                       // optional var section (after functions) - Pascal allows this
        optional(main_block),                        // optional main program block
        token(match(".")),                           // final period
        NULL
    );
}
