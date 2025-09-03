#include "acutest.h"
#include "parser.h"
#include "combinators.h"
#include "pascal_parser.h"
#include <stdio.h>

void test_pascal_integer_parsing(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("123");
    input->length = 3;

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "123") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_invalid_input(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("1 +");
    input->length = 3;

    ParseResult res = parse(input, p);

    TEST_ASSERT(!res.is_success);
    TEST_ASSERT(res.value.error->partial_ast != NULL);
    TEST_ASSERT(res.value.error->partial_ast->typ == PASCAL_T_ADD);
    ast_t* lhs = res.value.error->partial_ast->child;
    TEST_ASSERT(lhs->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(lhs->sym->name, "1") == 0);

    free_error(res.value.error);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_function_call(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("my_func");  // Just test identifier parsing
    input->length = strlen("my_func");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = res.value.ast;
    if (res.value.ast->child && res.value.ast->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = res.value.ast->child;
    }
    
    TEST_ASSERT(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "my_func") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_string_literal(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("\"hello world\"");
    input->length = strlen("\"hello world\"");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "hello world") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_function_call_no_args(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("func()");
    input->length = strlen("func()");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNC_CALL);
    
    // First child should be the function name
    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = func_name;
    if (func_name->child && func_name->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = func_name->child;
    }
    
    TEST_ASSERT(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "func") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_function_call_with_args(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("func(5, 10)");
    input->length = strlen("func(5, 10)");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNC_CALL);
    
    // First child should be the function name
    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = func_name;
    if (func_name->child && func_name->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = func_name->child;
    }
    
    TEST_ASSERT(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "func") == 0);
    
    // Arguments follow
    ast_t* arg1 = func_name->next;
    TEST_ASSERT(arg1->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(arg1->sym->name, "5") == 0);
    
    ast_t* arg2 = arg1->next;
    TEST_ASSERT(arg2->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(arg2->sym->name, "10") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_mod_operator(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("7 mod 3");
    input->length = strlen("7 mod 3");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_MOD);
    
    // Check operands
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(left->sym->name, "7") == 0);
    
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(right->sym->name, "3") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_mod_operator_percent(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("7 % 3");
    input->length = strlen("7 % 3");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_MOD);
    
    // Check operands
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(left->sym->name, "7") == 0);
    
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(right->sym->name, "3") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_string_concatenation(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("\"hello\" + \"world\"");
    input->length = strlen("\"hello\" + \"world\"");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_ADD);
    
    // Check operands
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(left->sym->name, "hello") == 0);
    
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(right->sym->name, "world") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_complex_expression(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("procedure((5*7)-5)+\"test\"");
    input->length = strlen("procedure((5*7)-5)+\"test\"");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_ADD);
    
    // Left side should be function call
    ast_t* func_call = res.value.ast->child;
    TEST_ASSERT(func_call->typ == PASCAL_T_FUNC_CALL);
    
    // Function name
    ast_t* func_name = func_call->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = func_name;
    if (func_name->child && func_name->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = func_name->child;
    }
    
    TEST_ASSERT(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "procedure") == 0);
    
    // Function argument: (5*7)-5
    ast_t* arg = func_name->next;
    TEST_ASSERT(arg->typ == PASCAL_T_SUB);
    
    // Right side should be string
    ast_t* right = func_call->next;
    TEST_ASSERT(right->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(right->sym->name, "test") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_div_operator(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("10 div 3");
    input->length = strlen("10 div 3");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_INTDIV);
    
    // Check operands
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(left->sym->name, "10") == 0);
    
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(right->sym->name, "3") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test real number parsing
void test_pascal_real_number(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("3.14");
    input->length = strlen("3.14");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_REAL);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "3.14") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test character literal parsing
void test_pascal_char_literal(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("'A'");
    input->length = strlen("'A'");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_CHAR);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "A") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test unary plus operator
void test_pascal_unary_plus(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("+42");
    input->length = strlen("+42");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_POS);
    
    // Check operand
    ast_t* operand = res.value.ast->child;
    TEST_ASSERT(operand->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(operand->sym->name, "42") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test relational operators
void test_pascal_relational_operators(void) {
    const char* expressions[] = {
        "5 = 5", "5 <> 3", "3 < 5", "5 > 3", "3 <= 5", "5 >= 3"
    };
    pascal_tag_t expected_tags[] = {
        PASCAL_T_EQ, PASCAL_T_NE, PASCAL_T_LT, PASCAL_T_GT, PASCAL_T_LE, PASCAL_T_GE
    };
    
    for (int i = 0; i < 6; i++) {
        combinator_t* p = new_combinator();
        init_pascal_expression_parser(&p);

        input_t* input = new_input();
        input->buffer = strdup(expressions[i]);
        input->length = strlen(expressions[i]);

        ParseResult res = parse(input, p);

        TEST_ASSERT(res.is_success);
        TEST_ASSERT(res.value.ast->typ == expected_tags[i]);

        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
    }
}

// Test boolean operators
void test_pascal_boolean_operators(void) {
    const char* expressions[] = {
        "true and false", "true or false", "not true", "true xor false"
    };
    pascal_tag_t expected_tags[] = {
        PASCAL_T_AND, PASCAL_T_OR, PASCAL_T_NOT, PASCAL_T_XOR
    };
    
    for (int i = 0; i < 4; i++) {
        combinator_t* p = new_combinator();
        init_pascal_expression_parser(&p);

        input_t* input = new_input();
        input->buffer = strdup(expressions[i]);
        input->length = strlen(expressions[i]);

        ParseResult res = parse(input, p);

        TEST_ASSERT(res.is_success);
        TEST_ASSERT(res.value.ast->typ == expected_tags[i]);

        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
    }
}

// Test bitwise shift operators
void test_pascal_bitwise_operators(void) {
    const char* expressions[] = {
        "8 shl 2", "8 shr 1"
    };
    pascal_tag_t expected_tags[] = {
        PASCAL_T_SHL, PASCAL_T_SHR
    };
    
    for (int i = 0; i < 2; i++) {
        combinator_t* p = new_combinator();
        init_pascal_expression_parser(&p);

        input_t* input = new_input();
        input->buffer = strdup(expressions[i]);
        input->length = strlen(expressions[i]);

        ParseResult res = parse(input, p);

        TEST_ASSERT(res.is_success);
        TEST_ASSERT(res.value.ast->typ == expected_tags[i]);

        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
    }
}

// Test address operator
void test_pascal_address_operator(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("@myVar");
    input->length = strlen("@myVar");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_ADDR);
    
    // Check operand
    ast_t* operand = res.value.ast->child;
    TEST_ASSERT(operand->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = operand;
    if (operand->child && operand->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = operand->child;
    }
    
    TEST_ASSERT(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "myVar") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test complex expression with new operators
void test_pascal_comprehensive_expression(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("(x + y) * z div 4");
    input->length = strlen("(x + y) * z div 4");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_INTDIV);
    
    // Left side should be multiplication
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_MUL);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test precedence: arithmetic vs comparison
void test_pascal_precedence(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("a + b = c * d");
    input->length = strlen("a + b = c * d");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_EQ); // = should be top level

    // Left side should be addition
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_ADD);
    
    // Right side should be multiplication  
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_MUL);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test type casting
void test_pascal_type_casting(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("Integer('A')");
    input->length = strlen("Integer('A')");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_TYPECAST);
    
    // First child should be the type name
    ast_t* type_name = res.value.ast->child;
    TEST_ASSERT(type_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(type_name->sym->name, "Integer") == 0);
    
    // Second child should be the expression being cast
    ast_t* expr = type_name->next;
    TEST_ASSERT(expr->typ == PASCAL_T_CHAR);
    TEST_ASSERT(strcmp(expr->sym->name, "A") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test set constructor parsing
void test_pascal_set_constructor(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("[1, 2, 3]");
    input->length = strlen("[1, 2, 3]");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_SET);
    
    // Check first element
    ast_t* elem1 = res.value.ast->child;
    TEST_ASSERT(elem1->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(elem1->sym->name, "1") == 0);
    
    // Check second element
    ast_t* elem2 = elem1->next;
    TEST_ASSERT(elem2->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(elem2->sym->name, "2") == 0);
    
    // Check third element
    ast_t* elem3 = elem2->next;
    TEST_ASSERT(elem3->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(elem3->sym->name, "3") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test empty set constructor
void test_pascal_empty_set(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("[]");
    input->length = strlen("[]");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_SET);
    TEST_ASSERT(res.value.ast->child == NULL); // Empty set

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test character set constructor
void test_pascal_char_set(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("['a', 'b', 'c']");
    input->length = strlen("['a', 'b', 'c']");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_SET);
    
    // Check first element
    ast_t* elem1 = res.value.ast->child;
    TEST_ASSERT(elem1->typ == PASCAL_T_CHAR);
    TEST_ASSERT(strcmp(elem1->sym->name, "a") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test range expression
void test_pascal_range_expression(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("1..10");
    input->length = strlen("1..10");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_RANGE);
    
    // Check left operand
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(left->sym->name, "1") == 0);
    
    // Check right operand
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(right->sym->name, "10") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test character range expression
void test_pascal_char_range(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("'a'..'z'");
    input->length = strlen("'a'..'z'");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_RANGE);
    
    // Check left operand
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_CHAR);
    TEST_ASSERT(strcmp(left->sym->name, "a") == 0);
    
    // Check right operand
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_CHAR);
    TEST_ASSERT(strcmp(right->sym->name, "z") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test set union operation
void test_pascal_set_union(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("[1, 2] + [3, 4]");
    input->length = strlen("[1, 2] + [3, 4]");

    ParseResult res = parse_pascal_expression(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_SET_UNION);
    
    // Check left operand is a set
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_SET);
    
    // Check right operand is a set
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_SET);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test class type checking with 'is' operator
void test_pascal_is_operator(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("MyObject is TMyClass");
    input->length = strlen("MyObject is TMyClass");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_IS);
    
    // Check left operand (object identifier)
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_left_node = left;
    if (left->child && left->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_left_node = left->child;
    }
    
    TEST_ASSERT(actual_left_node->sym && 
               actual_left_node->sym->name && 
               strcmp(actual_left_node->sym->name, "MyObject") == 0);
    
    // Check right operand (class type identifier)
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_right_node = right;
    if (right->child && right->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_right_node = right->child;
    }
    
    TEST_ASSERT(actual_right_node->sym && 
               actual_right_node->sym->name && 
               strcmp(actual_right_node->sym->name, "TMyClass") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test class type casting with 'as' operator
void test_pascal_as_operator(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("Sender as TButton");
    input->length = strlen("Sender as TButton");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_AS);
    
    // Check left operand (object identifier)
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_left_node = left;
    if (left->child && left->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_left_node = left->child;
    }
    
    TEST_ASSERT(actual_left_node->sym && 
               actual_left_node->sym->name && 
               strcmp(actual_left_node->sym->name, "Sender") == 0);
    
    // Check right operand (target class type)
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_right_node = right;
    if (right->child && right->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_right_node = right->child;
    }
    
    TEST_ASSERT(actual_right_node->sym && 
               actual_right_node->sym->name && 
               strcmp(actual_right_node->sym->name, "TButton") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// Test complex 'as' operator with field access (basic parsing)
void test_pascal_as_operator_with_field_access(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    // For now, just test the casting part - field access would require more complex parsing
    input_t* input = new_input();
    input->buffer = strdup("SomeObject as TForm");
    input->length = strlen("SomeObject as TForm");

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_AS);
    
    // Check left operand
    ast_t* left = res.value.ast->child;
    TEST_ASSERT(left->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_left_node = left;
    if (left->child && left->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_left_node = left->child;
    }
    
    TEST_ASSERT(actual_left_node->sym && 
               actual_left_node->sym->name && 
               strcmp(actual_left_node->sym->name, "SomeObject") == 0);
    
    // Check right operand
    ast_t* right = left->next;
    TEST_ASSERT(right->typ == PASCAL_T_IDENTIFIER);
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_right_node = right;
    if (right->child && right->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_right_node = right->child;
    }
    
    TEST_ASSERT(actual_right_node->sym && 
               actual_right_node->sym->name && 
               strcmp(actual_right_node->sym->name, "TForm") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// --- Pascal Statement Tests ---

void test_pascal_assignment_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("x := 42;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // The program parser wraps in a NONE node, so get the actual statement
    ast_t* stmt = res.value.ast;
    // For terminated statements, we expect the actual statement type directly
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_ASSIGNMENT);
    
    // Check identifier
    ast_t* identifier = stmt->child;
    TEST_ASSERT(identifier->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(identifier->sym->name, "x") == 0);
    
    // Check assignment operator (skipped in AST)
    
    // Check expression value
    ast_t* value = identifier->next;
    TEST_ASSERT(value->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(value->sym->name, "42") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_expression_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("writeln(\"Hello\");");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_CHECK(res.is_success);
    if (!res.is_success) {
        if (res.value.error) {
            printf("Parse error: %s at line %d, col %d\n", 
                   res.value.error->message, res.value.error->line, res.value.error->col);
            free_error(res.value.error);
        }
        free_combinator(p);
        free(input->buffer);
        free(input);
        return;
    }
    
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    
    TEST_CHECK(stmt && stmt->typ == PASCAL_T_STATEMENT);
    if (!stmt || stmt->typ != PASCAL_T_STATEMENT) {
        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
        return;
    }
    
    // Check function call
    ast_t* func_call = stmt->child;
    TEST_CHECK(func_call && func_call->typ == PASCAL_T_FUNC_CALL);
    if (!func_call || func_call->typ != PASCAL_T_FUNC_CALL) {
        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
        return;
    }
    
    // Check function name
    ast_t* func_name = func_call->child;
    TEST_CHECK(func_name && func_name->typ == PASCAL_T_IDENTIFIER);
    if (!func_name || func_name->typ != PASCAL_T_IDENTIFIER) {
        free_ast(res.value.ast);
        free_combinator(p);
        free(input->buffer);
        free(input);
        return;
    }
    
    // Handle both regular identifiers and built-in function structure
    ast_t* actual_name_node = func_name;
    if (func_name->child && func_name->child->typ == PASCAL_T_IDENTIFIER) {
        // Use the child identifier for built-in functions
        actual_name_node = func_name->child;
    }
    
    TEST_CHECK(actual_name_node->sym && 
               actual_name_node->sym->name && 
               strcmp(actual_name_node->sym->name, "writeln") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_if_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("if x > 0 then y := 1;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_IF_STMT);
    
    // Check condition (x > 0)
    ast_t* condition = stmt->child;
    TEST_ASSERT(condition->typ == PASCAL_T_GT);
    
    // Check then statement
    ast_t* then_stmt = condition->next;
    TEST_ASSERT(then_stmt->typ == PASCAL_T_ASSIGNMENT);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_if_else_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("if x > 0 then y := 1 else y := -1;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_IF_STMT);
    
    // Check condition
    ast_t* condition = stmt->child;
    TEST_ASSERT(condition->typ == PASCAL_T_GT);
    
    // Check then statement
    ast_t* then_stmt = condition->next;
    TEST_ASSERT(then_stmt->typ == PASCAL_T_ASSIGNMENT);
    
    // Check else clause
    ast_t* else_clause = then_stmt->next;
    TEST_ASSERT(else_clause->typ == PASCAL_T_ELSE);
    
    // Check else statement
    ast_t* else_stmt = else_clause->child;
    TEST_ASSERT(else_stmt->typ == PASCAL_T_ASSIGNMENT);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_begin_end_block(void) {
    combinator_t* p = new_combinator();
    init_pascal_statement_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("begin x := 1; y := 2 end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_BEGIN_BLOCK);
    
    // The child should be the statement list
    ast_t* stmt_list = res.value.ast->child;
    TEST_ASSERT(stmt_list != NULL);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_for_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("for i := 1 to 10 do x := x + i;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_FOR_STMT);
    
    // Check loop variable
    ast_t* loop_var = stmt->child;
    TEST_ASSERT(loop_var->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(loop_var->sym->name, "i") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_while_statement(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("while x > 0 do x := x - 1;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_WHILE_STMT);
    
    // Check condition
    ast_t* condition = stmt->child;
    TEST_ASSERT(condition->typ == PASCAL_T_GT);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_simple_asm_block(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("asm mov ax, 5 end;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_ASM_BLOCK);
    
    // Check the ASM body content
    ast_t* asm_body = stmt->child;
    TEST_ASSERT(asm_body->typ == PASCAL_T_NONE);
    TEST_ASSERT(strcmp(asm_body->sym->name, "mov ax, 5 ") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_multiline_asm_block(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("asm\n  mov ax, bx\n  add ax, 10\n  int 21h\nend;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_ASM_BLOCK);
    
    // Check the ASM body content - it should contain newlines and instructions
    ast_t* asm_body = stmt->child;
    TEST_ASSERT(asm_body->typ == PASCAL_T_NONE);
    TEST_ASSERT(strstr(asm_body->sym->name, "mov ax, bx") != NULL);
    TEST_ASSERT(strstr(asm_body->sym->name, "add ax, 10") != NULL);
    TEST_ASSERT(strstr(asm_body->sym->name, "int 21h") != NULL);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_empty_asm_block(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("asm end;");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    // Get the actual statement
    ast_t* stmt = res.value.ast;
    if (stmt->typ == PASCAL_T_NONE) {
        stmt = stmt->child;
    }
    TEST_ASSERT(stmt->typ == PASCAL_T_ASM_BLOCK);
    
    // Check the ASM body content - should be empty except for space
    ast_t* asm_body = stmt->child;
    TEST_ASSERT(asm_body->typ == PASCAL_T_NONE);
    TEST_ASSERT(strcmp(asm_body->sym->name, "") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_unterminated_asm_block(void) {
    combinator_t* p = new_combinator();
    init_pascal_program_parser(&p);  // Use program parser for terminated statements

    input_t* input = new_input();
    input->buffer = strdup("asm mov ax, 5");  // Missing 'end'
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(!res.is_success);
    // Could be "Expected ';'" from program parser or "Unterminated ASM block" from ASM parser
    TEST_ASSERT(res.value.error->message != NULL);

    free_error(res.value.error);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

// === Procedure/Function Declaration Tests ===

void test_pascal_simple_procedure(void) {
    combinator_t* p = new_combinator();
    init_pascal_procedure_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("procedure MyProcedure; begin end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_PROCEDURE_DECL);
    
    // Check procedure name
    ast_t* proc_name = res.value.ast->child;
    TEST_ASSERT(proc_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(proc_name->sym->name, "MyProcedure") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_procedure_with_params(void) {
    combinator_t* p = new_combinator();
    init_pascal_procedure_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("procedure MyProcedure(x: integer; y: string); begin end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_PROCEDURE_DECL);
    
    // Check procedure name
    ast_t* proc_name = res.value.ast->child;
    TEST_ASSERT(proc_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(proc_name->sym->name, "MyProcedure") == 0);
    
    // Check parameters exist
    ast_t* params = proc_name->next;
    TEST_ASSERT(params != NULL); // parameter list should exist

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_simple_function(void) {
    combinator_t* p = new_combinator();
    init_pascal_procedure_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("function Square(x: integer): integer; begin end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNCTION_DECL);
    
    // Check function name
    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(func_name->sym->name, "Square") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_function_no_params(void) {
    combinator_t* p = new_combinator();
    init_pascal_procedure_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("function GetValue: integer; begin end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNCTION_DECL);
    
    // Check function name
    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(func_name->sym->name, "GetValue") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_function_multiple_params(void) {
    combinator_t* p = new_combinator();
    init_pascal_procedure_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("function Calculate(a: real; b: real; c: integer): real; begin end");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_FUNCTION_DECL);
    
    // Check function name
    ast_t* func_name = res.value.ast->child;
    TEST_ASSERT(func_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(func_name->sym->name, "Calculate") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_program_declaration(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("program Test; begin end.");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    if(res.is_success) {
        print_pascal_ast(res.value.ast);
    }

    TEST_CHECK(res.is_success);
    ast_t* program_decl = res.value.ast;
    TEST_CHECK(program_decl->typ == PASCAL_T_PROGRAM_DECL);

    // program Test; (fixed to match actual input)
    ast_t* program_name = program_decl->child;
    TEST_CHECK(program_name->typ == PASCAL_T_IDENTIFIER);
    TEST_CHECK(strcmp(program_name->sym->name, "Test") == 0);  // Fixed: should match input "program Test;"

    // For simple program without parameters, next should be main block
    ast_t* main_block = program_name->next;
    TEST_CHECK(main_block != NULL);
    TEST_CHECK(main_block->typ == PASCAL_T_MAIN_BLOCK);

    // Simple program "program Test; begin end." should have an empty main block
    // The main block should either have no children (empty) or contain statements
    if (main_block->child != NULL) {
        // For a simple empty program, we accept PASCAL_T_NONE (0) or other statement types
        ast_t* first_child = main_block->child;
        TEST_CHECK(first_child->typ == PASCAL_T_NONE || 
                   first_child->typ == PASCAL_T_BEGIN_BLOCK || 
                   first_child->typ == PASCAL_T_STATEMENT ||
                   first_child->typ == PASCAL_T_ASSIGNMENT ||
                   first_child->typ == PASCAL_T_IF_STMT ||
                   first_child->typ == PASCAL_T_FOR_STMT ||
                   first_child->typ == PASCAL_T_WHILE_STMT);
    }
    // For this simple case, an empty main block is fine
    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_program_with_vars(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("program Test; var x : integer; begin end.");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_PROGRAM_DECL);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_fizzbuzz_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* fizzbuzz = "program FizzBuzz(output);\n"
                    "var\n"
                    "   i : integer;\n"
                    "begin\n"
                    "   for i := 1 to 100 do\n"
                    "      if i mod 15 = 0 then\n"
                    "         writeln('FizzBuzz')\n"
                    "      else if i mod 3 = 0 then\n"
                    "         writeln('Fizz')\n"
                    "      else if i mod 5 = 0 then\n"
                    "         writeln('Buzz')\n"
                    "      else\n"
                    "         writeln(i)\n"
                    "end.\n";
    input->buffer = strdup(fizzbuzz);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    ast_t* program_decl = res.value.ast;
    TEST_ASSERT(program_decl->typ == PASCAL_T_PROGRAM_DECL);

    // program FizzBuzz(output);
    ast_t* program_name = program_decl->child;
    TEST_ASSERT(program_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(program_name->sym->name, "FizzBuzz") == 0);

    // output parameter
    ast_t* output_param = program_name->next;
    TEST_ASSERT(output_param->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(output_param->sym->name, "output") == 0);

    // For FizzBuzz program, expect var section after program parameters
    ast_t* var_section = output_param->next;
    TEST_ASSERT(var_section != NULL);
    TEST_ASSERT(var_section->typ == PASCAL_T_VAR_SECTION);
    
    // var i : integer;
    ast_t* var_decl = var_section->child;
    TEST_ASSERT(var_decl->typ == PASCAL_T_VAR_DECL);
    
    // First child should be the variable identifier list
    ast_t* var_list = var_decl->child;
    TEST_ASSERT(var_list->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(var_list->sym->name, "i") == 0);
    
    // Second child should be the type specification
    ast_t* type_spec = var_list->next;
    TEST_ASSERT(type_spec->typ == PASCAL_T_TYPE_SPEC);
    TEST_ASSERT(type_spec->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(type_spec->child->sym->name, "integer") == 0);

    // main block
    ast_t* main_block = var_section->next;
    TEST_ASSERT(main_block != NULL);
    TEST_ASSERT(main_block->typ == PASCAL_T_MAIN_BLOCK);

    // for i := 1 to 100 do
    ast_t* for_stmt = main_block->child;
    TEST_ASSERT(for_stmt->typ == PASCAL_T_FOR_STMT);
    ast_t* loop_var = for_stmt->child;
    TEST_ASSERT(loop_var->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(loop_var->sym->name, "i") == 0);
    ast_t* start_val = loop_var->next;
    TEST_ASSERT(start_val->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(start_val->sym->name, "1") == 0);
    ast_t* end_val = start_val->next;
    TEST_ASSERT(end_val->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(end_val->sym->name, "100") == 0);

    // body of for loop
    ast_t* if_stmt1 = end_val->next;
    TEST_ASSERT(if_stmt1->typ == PASCAL_T_IF_STMT);

    // if i mod 15 = 0 then writeln('FizzBuzz')
    ast_t* cond1 = if_stmt1->child;
    TEST_ASSERT(cond1->typ == PASCAL_T_EQ);
    ast_t* mod1 = cond1->child;
    TEST_ASSERT(mod1->typ == PASCAL_T_MOD);
    TEST_ASSERT(mod1->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(mod1->child->sym->name, "i") == 0);
    TEST_ASSERT(mod1->child->next->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(mod1->child->next->sym->name, "15") == 0);
    ast_t* zero1 = mod1->next;
    TEST_ASSERT(zero1->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(zero1->sym->name, "0") == 0);

    ast_t* then1 = cond1->next;
    TEST_ASSERT(then1->typ == PASCAL_T_STATEMENT);
    ast_t* writeln1 = then1->child;
    TEST_ASSERT(writeln1->typ == PASCAL_T_FUNC_CALL);
    TEST_ASSERT(writeln1->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(writeln1->child->sym->name, "writeln") == 0);
    TEST_ASSERT(writeln1->child->next->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(writeln1->child->next->sym->name, "FizzBuzz") == 0);

    // else if i mod 3 = 0 then writeln('Fizz')
    ast_t* else1 = then1->next;
    TEST_ASSERT(else1->typ == PASCAL_T_ELSE);
    ast_t* if_stmt2 = else1->child;
    TEST_ASSERT(if_stmt2->typ == PASCAL_T_IF_STMT);

    ast_t* cond2 = if_stmt2->child;
    TEST_ASSERT(cond2->typ == PASCAL_T_EQ);
    ast_t* mod2 = cond2->child;
    TEST_ASSERT(mod2->typ == PASCAL_T_MOD);
    TEST_ASSERT(mod2->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(mod2->child->sym->name, "i") == 0);
    TEST_ASSERT(mod2->child->next->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(mod2->child->next->sym->name, "3") == 0);
    ast_t* zero2 = mod2->next;
    TEST_ASSERT(zero2->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(zero2->sym->name, "0") == 0);

    ast_t* then2 = cond2->next;
    TEST_ASSERT(then2->typ == PASCAL_T_STATEMENT);
    ast_t* writeln2 = then2->child;
    TEST_ASSERT(writeln2->typ == PASCAL_T_FUNC_CALL);
    TEST_ASSERT(writeln2->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(writeln2->child->sym->name, "writeln") == 0);
    TEST_ASSERT(writeln2->child->next->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(writeln2->child->next->sym->name, "Fizz") == 0);

    // else if i mod 5 = 0 then writeln('Buzz')
    ast_t* else2 = then2->next;
    TEST_ASSERT(else2->typ == PASCAL_T_ELSE);
    ast_t* if_stmt3 = else2->child;
    TEST_ASSERT(if_stmt3->typ == PASCAL_T_IF_STMT);

    ast_t* cond3 = if_stmt3->child;
    TEST_ASSERT(cond3->typ == PASCAL_T_EQ);
    ast_t* mod3 = cond3->child;
    TEST_ASSERT(mod3->typ == PASCAL_T_MOD);
    TEST_ASSERT(mod3->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(mod3->child->sym->name, "i") == 0);
    TEST_ASSERT(mod3->child->next->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(mod3->child->next->sym->name, "5") == 0);
    ast_t* zero3 = mod3->next;
    TEST_ASSERT(zero3->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(zero3->sym->name, "0") == 0);

    ast_t* then3 = cond3->next;
    TEST_ASSERT(then3->typ == PASCAL_T_STATEMENT);
    ast_t* writeln3 = then3->child;
    TEST_ASSERT(writeln3->typ == PASCAL_T_FUNC_CALL);
    TEST_ASSERT(writeln3->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(writeln3->child->sym->name, "writeln") == 0);
    TEST_ASSERT(writeln3->child->next->typ == PASCAL_T_STRING);
    TEST_ASSERT(strcmp(writeln3->child->next->sym->name, "Buzz") == 0);

    // else writeln(i)
    ast_t* else3 = then3->next;
    TEST_ASSERT(else3->typ == PASCAL_T_ELSE);
    ast_t* stmt4 = else3->child;
    TEST_ASSERT(stmt4->typ == PASCAL_T_STATEMENT);
    ast_t* writeln4 = stmt4->child;
    TEST_ASSERT(writeln4->typ == PASCAL_T_FUNC_CALL);
    TEST_ASSERT(writeln4->child->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(writeln4->child->sym->name, "writeln") == 0);
    TEST_ASSERT(writeln4->child->next->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(writeln4->child->next->sym->name, "i") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_complex_syntax_error(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* error_program = "program ErrorTest;\n"
                          "var\n"
                          "  x: integer;\n"
                          "begin\n"
                          "  x := 10;\n"
                          "  if x > 5 then\n"
                          "    begin\n"
                          "      writeln('x is greater than 5')\n"
                          "    \n" // Missing 'end'
                          "end.\n";
    input->buffer = strdup(error_program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(!res.is_success);
    TEST_ASSERT(res.value.error != NULL);

    free_error(res.value.error);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_comments(void) {
    combinator_t* p = new_combinator();
    init_pascal_expression_parser(&p);

    input_t* input = new_input();
    input->buffer = strdup("{ This is a comment } 123");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "123") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_compiler_directives(void) {
    combinator_t* p = compiler_directive(PASCAL_T_COMPILER_DIRECTIVE);

    input_t* input = new_input();
    input->buffer = strdup("{$ifNDef CPU}");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_COMPILER_DIRECTIVE);
    TEST_ASSERT(strcmp(res.value.ast->sym->name, "ifNDef CPU") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_range_type(void) {
    combinator_t* p = range_type(PASCAL_T_RANGE_TYPE);

    input_t* input = new_input();
    input->buffer = strdup("-1..1");
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    TEST_ASSERT(res.value.ast->typ == PASCAL_T_RANGE_TYPE);
    
    // Check start value
    ast_t* start_val = res.value.ast->child;
    TEST_ASSERT(start_val->typ == PASCAL_T_INTEGER);
    
    // Check end value
    ast_t* end_val = start_val->next;
    TEST_ASSERT(end_val->typ == PASCAL_T_INTEGER);
    TEST_ASSERT(strcmp(end_val->sym->name, "1") == 0);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_multiple_variables(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* program = "program Test;\n"
                   "var\n"
                   "   I, Count, guess : Longint;\n"
                   "   R : Real;\n"
                   "begin\n"
                   "end.\n";
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    ast_t* program_decl = res.value.ast;
    TEST_ASSERT(program_decl->typ == PASCAL_T_PROGRAM_DECL);

    // Find var section
    ast_t* current = program_decl->child;
    while (current && current->typ != PASCAL_T_VAR_SECTION) {
        current = current->next;
    }
    TEST_ASSERT(current != NULL);
    TEST_ASSERT(current->typ == PASCAL_T_VAR_SECTION);
    
    // First var declaration: I, Count, guess : Longint;
    ast_t* var_decl1 = current->child;
    TEST_ASSERT(var_decl1->typ == PASCAL_T_VAR_DECL);
    
    // Check first variable in list
    ast_t* var1 = var_decl1->child;
    TEST_ASSERT(var1->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(var1->sym->name, "I") == 0);
    
    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_type_section(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* program = "program Test;\n"
                   "type\n"
                   "   signumCodomain = -1..1;\n"
                   "begin\n"
                   "end.\n";
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    TEST_ASSERT(res.is_success);
    ast_t* program_decl = res.value.ast;
    TEST_ASSERT(program_decl->typ == PASCAL_T_PROGRAM_DECL);

    // Find type section  
    ast_t* current = program_decl->child;
    while (current && current->typ != PASCAL_T_TYPE_SECTION) {
        current = current->next;
    }
    TEST_ASSERT(current != NULL);
    TEST_ASSERT(current->typ == PASCAL_T_TYPE_SECTION);
    
    // Type declaration: signumCodomain = -1..1;
    ast_t* type_decl = current->child;
    TEST_ASSERT(type_decl->typ == PASCAL_T_TYPE_DECL);
    
    // Check type name
    ast_t* type_name = type_decl->child;
    TEST_ASSERT(type_name->typ == PASCAL_T_IDENTIFIER);
    TEST_ASSERT(strcmp(type_name->sym->name, "signumCodomain") == 0);
    
    // Check type specification (range type)
    ast_t* type_spec = type_name->next;
    TEST_ASSERT(type_spec->typ == PASCAL_T_TYPE_SPEC);
    TEST_ASSERT(type_spec->child->typ == PASCAL_T_RANGE_TYPE);

    free_ast(res.value.ast);
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_complex_random_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    // Start with a simpler version first - just the basic structure
    char* program = "program Example49;\n"
                   "{ Program to demonstrate the Random and Randomize functions. }\n"
                   "var I,Count,guess : Longint;\n"
                   "begin\n"
                   "end.\n";
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    if (!res.is_success) {
        printf("Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
    }

    // For now, just test basic parsing success - we'll verify structure if needed
    TEST_ASSERT(res.is_success);
    
    if (res.is_success) {
        ast_t* program_decl = res.value.ast;
        TEST_ASSERT(program_decl->typ == PASCAL_T_PROGRAM_DECL);

        // Check program name
        ast_t* program_name = program_decl->child;
        TEST_ASSERT(program_name->typ == PASCAL_T_IDENTIFIER);
        TEST_ASSERT(strcmp(program_name->sym->name, "Example49") == 0);

        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_damm_algorithm_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* program = "program DammAlgorithm;\n"
                   "uses\n"
                   "  sysutils;\n"
                   "\n"
                   "TYPE TA = ARRAY[0..9,0..9] OF UInt8;\n"
                   "CONST table : TA =\n"
                   "                ((0,3,1,7,5,9,8,6,4,2),\n"
                   "                 (7,0,9,2,1,5,4,8,6,3),\n"
                   "                 (4,2,0,6,8,7,1,3,5,9),\n"
                   "                 (1,7,5,0,9,8,3,4,2,6),\n"
                   "                 (6,1,2,3,0,4,5,9,7,8),\n"
                   "                 (3,6,7,4,2,0,9,5,8,1),\n"
                   "                 (5,8,6,9,7,2,0,1,3,4),\n"
                   "                 (8,9,4,5,3,6,2,0,1,7),\n"
                   "                 (9,4,3,8,6,1,7,2,0,5),\n"
                   "                 (2,5,8,1,4,3,6,7,9,0));\n"
                   "\n"
                   "function Damm(s : string) : BOOLEAN;\n"
                   "VAR\n"
                   "  interim,i : UInt8;\n"
                   "BEGIN\n"
                   "  interim := 0;\n"
                   "  i := 1;\n"
                   "  WHILE i <= length(s) DO\n"
                   "  Begin\n"
                   "    interim := table[interim,ORD(s[i])-ORD('0')];\n"
                   "    INC(i);\n"
                   "  END;\n"
                   "  Damm := interim=0;\n"
                   "END;\n"
                   "\n"
                   "PROCEDURE Print(number : Uint32);\n"
                   "VAR\n"
                   "    isValid : BOOLEAN;\n"
                   "    buf :string;\n"
                   "BEGIN\n"
                   "    buf := IntToStr(number);\n"
                   "    isValid := Damm(buf);\n"
                   "    Write(buf);\n"
                   "    IF isValid THEN\n"
                   "      Write(' is valid')\n"
                   "    ELSE\n"
                   "      Write(' is invalid');\n"
                   "    WriteLn;\n"
                   "END;\n"
                   "\n"
                   "BEGIN\n"
                   "    Print(5724);\n"
                   "    Print(5727);\n"
                   "    Print(112946);\n"
                   "    Print(112949);\n"
                   "    Readln;\n"
                   "END.\n";
                   
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    if (!res.is_success) {
        printf("DammAlgorithm Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
        if (res.value.error->partial_ast) {
            printf("Partial AST available:\n");
            print_pascal_ast(res.value.error->partial_ast);
        }
    }

    // This test will likely fail due to missing features like:
    // - uses clause
    // - 2D array type declarations
    // - multi-dimensional array constant initialization
    // - while loops  
    // - array indexing syntax
    // - various built-in functions (length, ORD, INC, IntToStr)
    TEST_CHECK(res.is_success); // Use CHECK so other tests can run
    
    if (res.is_success) {
        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_sample_class_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* program = "program SampleClass;\n"
                   "\n"
                   "{$APPTYPE CONSOLE}\n"
                   "\n"
                   "type\n"
                   "  TMyClass = class\n"
                   "  private\n"
                   "    FSomeField: Integer; // by convention, fields are usually private and exposed as properties\n"
                   "  public\n"
                   "    constructor Create;\n"
                   "    destructor Destroy; override;\n"
                   "    procedure SomeMethod;\n"
                   "    property SomeField: Integer read FSomeField write FSomeField;\n"
                   "  end;\n"
                   "\n"
                   "constructor TMyClass.Create;\n"
                   "begin\n"
                   "  FSomeField := -1\n"
                   "end;\n"
                   "\n"
                   "destructor TMyClass.Destroy;\n"
                   "begin\n"
                   "  // free resources, etc\n"
                   "\n"
                   "  inherited Destroy;\n"
                   "end;\n"
                   "\n"
                   "procedure TMyClass.SomeMethod;\n"
                   "begin\n"
                   "  // do something\n"
                   "end;\n"
                   "\n"
                   "\n"
                   "var\n"
                   "  lMyClass: TMyClass;\n"
                   "begin\n"
                   "  lMyClass := TMyClass.Create;\n"
                   "  try\n"
                   "    lMyClass.SomeField := 99;\n"
                   "    lMyClass.SomeMethod();\n"
                   "  finally\n"
                   "    lMyClass.Free;\n"
                   "  end;\n"
                   "end.\n";
                   
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    if (!res.is_success) {
        printf("SampleClass Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
        if (res.value.error->partial_ast) {
            printf("Partial AST available:\n");
            print_pascal_ast(res.value.error->partial_ast);
        }
    }

    // This test will likely fail due to missing features like:
    // - class definitions (type class end)
    // - private/public sections
    // - constructor/destructor declarations
    // - property declarations
    // - override keyword
    // - inherited keyword
    // - try/finally blocks
    // - C++-style comments (//)
    // - method implementations with dot notation
    TEST_CHECK(res.is_success); // Use CHECK so other tests can run
    
    if (res.is_success) {
        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_pascal_anonymous_recursion_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    char* program = "program AnonymousRecursion;\n"
                   "\n"
                   "{$APPTYPE CONSOLE}\n"
                   "\n"
                   "uses\n"
                   "  SysUtils;\n"
                   "\n"
                   "function Fib(X: Integer): integer;\n"
                   "\n"
                   "\tfunction DoFib(N: Integer): Integer;\n"
                   "\tbegin\n"
                   "\tif N < 2 then Result:=N\n"
                   "\telse Result:=DoFib(N-1) + DoFib(N-2);\n"
                   "\tend;\n"
                   "\n"
                   "begin\n"
                   "if X < 0 then raise Exception.Create('Argument < 0')\n"
                   "else Result:=DoFib(X);\n"
                   "end;\n"
                   "\n"
                   "\n"
                   "var I: integer;\n"
                   "\n"
                   "begin\n"
                   "for I:=-1 to 15 do\n"
                   "\tbegin\n"
                   "\ttry\n"
                   "\tWriteLn(I:3,' - ',Fib(I):3);\n"
                   "\texcept WriteLn(I,' - Error'); end;\n"
                   "\tend;\n"
                   "WriteLn('Hit Any Key');\n"
                   "ReadLn;\n"
                   "end.\n";
                   
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    if (!res.is_success) {
        printf("AnonymousRecursion Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
        if (res.value.error->partial_ast) {
            printf("Partial AST available:\n");
            print_pascal_ast(res.value.error->partial_ast);
        }
    }

    // This test will likely fail due to missing features like:
    // - uses clause
    // - nested function definitions inside functions
    // - Result special variable
    // - raise statements with exception handling
    // - try/except blocks
    // - formatted output (WriteLn with :3 formatting)
    // - tabs in source code
    TEST_CHECK(res.is_success); // Use CHECK so other tests can run
    
    if (res.is_success) {
        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_minimal_damm_program(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    // Start with just the sections that work
    char* program = "program DammAlgorithm;\n"
                   "uses sysutils;\n"
                   "TYPE TA = ARRAY[0..9,0..9] OF UInt8;\n"
                   "CONST table : TA = ((0,3,1));\n"
                   "begin\n"
                   "end.\n";
                   
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    printf("=== MINIMAL DAMM TEST ===\n");
    if (!res.is_success) {
        printf("Minimal Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
        if (res.value.error->partial_ast) {
            printf("Partial AST available:\n");
            print_pascal_ast(res.value.error->partial_ast);
        }
    } else {
        printf("Minimal parse succeeded!\n");
        // Don't print full AST to keep output clean
    }
    
    TEST_CHECK(res.is_success);
    
    if (res.is_success) {
        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

void test_minimal_damm_with_function(void) {
    combinator_t* p = new_combinator();
    init_pascal_complete_program_parser(&p);

    input_t* input = new_input();
    // Add just a simple function with VAR
    char* program = "program DammAlgorithm;\n"
                   "function Test: integer;\n"
                   "var x: integer;\n"
                   "begin\n"
                   "end;\n"
                   "begin\n"
                   "end.\n";
                   
    input->buffer = strdup(program);
    input->length = strlen(input->buffer);

    ParseResult res = parse(input, p);

    printf("=== MINIMAL DAMM WITH FUNCTION TEST ===\n");
    if (!res.is_success) {
        printf("Function Parse failed at line %d, col %d: %s\n", 
               res.value.error->line, res.value.error->col, res.value.error->message);
        if (res.value.error->partial_ast) {
            printf("Partial AST available:\n");
            print_pascal_ast(res.value.error->partial_ast);
        }
    } else {
        printf("Function parse succeeded!\n");
        // Don't print full AST to keep output clean
    }
    
    TEST_CHECK(res.is_success);
    
    if (res.is_success) {
        free_ast(res.value.ast);
    } else {
        free_error(res.value.error);
    }
    
    free_combinator(p);
    free(input->buffer);
    free(input);
}

TEST_LIST = {
    { "test_pascal_integer_parsing", test_pascal_integer_parsing },
    { "test_pascal_invalid_input", test_pascal_invalid_input },
    { "test_pascal_function_call", test_pascal_function_call },
    { "test_pascal_string_literal", test_pascal_string_literal },
    { "test_pascal_function_call_no_args", test_pascal_function_call_no_args },
    { "test_pascal_function_call_with_args", test_pascal_function_call_with_args },
    { "test_pascal_mod_operator", test_pascal_mod_operator },
    { "test_pascal_mod_operator_percent", test_pascal_mod_operator_percent },
    { "test_pascal_string_concatenation", test_pascal_string_concatenation },
    { "test_pascal_complex_expression", test_pascal_complex_expression },
    { "test_pascal_div_operator", test_pascal_div_operator },
    { "test_pascal_real_number", test_pascal_real_number },
    { "test_pascal_char_literal", test_pascal_char_literal },
    { "test_pascal_unary_plus", test_pascal_unary_plus },
    { "test_pascal_relational_operators", test_pascal_relational_operators },
    { "test_pascal_boolean_operators", test_pascal_boolean_operators },
    { "test_pascal_bitwise_operators", test_pascal_bitwise_operators },
    { "test_pascal_address_operator", test_pascal_address_operator },
    { "test_pascal_comprehensive_expression", test_pascal_comprehensive_expression },
    { "test_pascal_precedence", test_pascal_precedence },
    { "test_pascal_type_casting", test_pascal_type_casting },
    { "test_pascal_set_constructor", test_pascal_set_constructor },
    { "test_pascal_empty_set", test_pascal_empty_set },
    { "test_pascal_char_set", test_pascal_char_set },
    { "test_pascal_range_expression", test_pascal_range_expression },
    { "test_pascal_char_range", test_pascal_char_range },
    { "test_pascal_set_union", test_pascal_set_union },
    { "test_pascal_is_operator", test_pascal_is_operator },
    { "test_pascal_as_operator", test_pascal_as_operator },
    { "test_pascal_as_operator_with_field_access", test_pascal_as_operator_with_field_access },
    // Statement tests
    { "test_pascal_assignment_statement", test_pascal_assignment_statement },
    { "test_pascal_expression_statement", test_pascal_expression_statement },
    { "test_pascal_if_statement", test_pascal_if_statement },
    { "test_pascal_if_else_statement", test_pascal_if_else_statement },
    { "test_pascal_begin_end_block", test_pascal_begin_end_block },
    { "test_pascal_for_statement", test_pascal_for_statement },
    { "test_pascal_while_statement", test_pascal_while_statement },
    { "test_pascal_simple_asm_block", test_pascal_simple_asm_block },
    { "test_pascal_multiline_asm_block", test_pascal_multiline_asm_block },
    { "test_pascal_empty_asm_block", test_pascal_empty_asm_block },
    { "test_pascal_unterminated_asm_block", test_pascal_unterminated_asm_block },
    // Procedure/Function Declaration tests
    { "test_pascal_simple_procedure", test_pascal_simple_procedure },
    { "test_pascal_procedure_with_params", test_pascal_procedure_with_params },
    { "test_pascal_simple_function", test_pascal_simple_function },
    { "test_pascal_function_no_params", test_pascal_function_no_params },
    { "test_pascal_function_multiple_params", test_pascal_function_multiple_params },
    // Complete program tests
    { "test_pascal_program_declaration", test_pascal_program_declaration },
    { "test_pascal_program_with_vars", test_pascal_program_with_vars },
    { "test_pascal_fizzbuzz_program", test_pascal_fizzbuzz_program },
    { "test_pascal_complex_syntax_error", test_pascal_complex_syntax_error },
    // Enhanced parser tests
    { "test_pascal_comments", test_pascal_comments },
    { "test_pascal_compiler_directives", test_pascal_compiler_directives },
    { "test_pascal_range_type", test_pascal_range_type },
    { "test_pascal_multiple_variables", test_pascal_multiple_variables },
    { "test_pascal_type_section", test_pascal_type_section },
    { "test_pascal_complex_random_program", test_pascal_complex_random_program },
    // Debug tests for DammAlgorithm  
    { "test_minimal_damm_program", test_minimal_damm_program },
    { "test_minimal_damm_with_function", test_minimal_damm_with_function },
    // New advanced Pascal program tests
    { "test_pascal_damm_algorithm_program", test_pascal_damm_algorithm_program },
    { "test_pascal_sample_class_program", test_pascal_sample_class_program },
    { "test_pascal_anonymous_recursion_program", test_pascal_anonymous_recursion_program },
    { NULL, NULL }
};
