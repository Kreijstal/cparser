#include "pascal_declaration.h"
#include "pascal_parser.h"
#include "pascal_statement.h"
#include "pascal_expression.h"
#include "pascal_type.h"
#include "pascal_keywords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static combinator_t* main_block_content(tag_t tag) {
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
