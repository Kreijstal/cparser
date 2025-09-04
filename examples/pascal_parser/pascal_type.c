#include "pascal_type.h"
#include "pascal_parser.h"
#include "pascal_keywords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
