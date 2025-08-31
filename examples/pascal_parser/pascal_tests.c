#include "../../acutest.h"
#include "pascal_parser.h"
#include <string.h>

void test_pascal_minimal_program(void) {
    const char* input_str = "program MyTestProgram(input, output); begin end.";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_program();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s (line %d, col %d)", result.value.error->message, result.value.error->line, result.value.error->col);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_PROGRAM);

        ast_t* child = ast->child;
        TEST_CHECK(child != NULL);

        // Check program name
        TEST_CHECK(child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(child->sym->name, "MyTestProgram") == 0);

        // Check parameter list
        child = child->next;
        TEST_CHECK(child != NULL);
        if (child) {
            TEST_CHECK(child->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(child->sym->name, "input") == 0);
        }

        child = child->next;
        TEST_CHECK(child != NULL);
        if (child) {
            TEST_CHECK(child->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(child->sym->name, "output") == 0);
        }

        // The next child should be the statement list, which is nil for an empty program, so the list should end here.
        child = child->next;
        TEST_CHECK(child == NULL);

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_asm_block(void) {
    const char* input_str = "asm mov eax, 1; end;";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_asm_block();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s (line %d, col %d)", result.value.error->message, result.value.error->line, result.value.error->col);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ASM_BLOCK);

        ast_t* body_node = ast->child;
        TEST_CHECK(body_node != NULL);
        if (body_node) {
            TEST_CHECK(body_node->typ == PASCAL_T_ASM_BLOCK);
            TEST_CHECK(strcmp(body_node->sym->name, " mov eax, 1; ") == 0);
        }

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_identifier_list(void) {
    const char* input_str = "one, two, three";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_identifier_list();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s", result.value.error->message);
        free_error(result.value.error);
    } else {
        ast_t* list_node = result.value.ast;
        TEST_CHECK(list_node != NULL && list_node != ast_nil);
        TEST_CHECK(list_node->typ == PASCAL_T_IDENT_LIST);

        ast_t* ast = list_node->child;

        // Check first identifier
        TEST_CHECK(ast->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->sym->name, "one") == 0);

        // Check second identifier
        ast = ast->next;
        TEST_CHECK(ast != NULL);
        if (ast) {
            TEST_CHECK(ast->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(ast->sym->name, "two") == 0);
        }

        // Check third identifier
        ast = ast->next;
        TEST_CHECK(ast != NULL);
        if (ast) {
            TEST_CHECK(ast->typ == PASCAL_T_IDENT);
            TEST_CHECK(strcmp(ast->sym->name, "three") == 0);
        }

        // Should be the end of the list
        ast = ast->next;
        TEST_CHECK(ast == NULL);

        free_ast(result.value.ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_expressions(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    // Test case 1: Simple addition
    const char* input1 = "1 + 2";
    input_t *in1 = new_input();
    in1->buffer = strdup(input1);
    in1->length = strlen(input1);
    combinator_t* parser1 = p_expression();
    ParseResult res1 = parse(in1, parser1);
    TEST_CHECK(res1.is_success);
    if(res1.is_success) {
        ast_t* ast = res1.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ADD);
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->sym->name, "1") == 0);
        TEST_CHECK(ast->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->next->sym->name, "2") == 0);
        free_ast(ast);
    }
    free_combinator(parser1);
    free(in1->buffer);
    free(in1);

    // Test case 2: Operator precedence
    const char* input2 = "1 + 2 * 3";
    input_t *in2 = new_input();
    in2->buffer = strdup(input2);
    in2->length = strlen(input2);
    combinator_t* parser2 = p_expression();
    ParseResult res2 = parse(in2, parser2);
    TEST_CHECK(res2.is_success);
    if(res2.is_success) {
        ast_t* ast = res2.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ADD); // Root is +
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM); // Left child is 1
        ast_t* mul_node = ast->child->next;
        TEST_CHECK(mul_node->typ == PASCAL_T_MUL); // Right child is a * expression
        TEST_CHECK(mul_node->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(mul_node->child->sym->name, "2") == 0);
        TEST_CHECK(mul_node->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(mul_node->child->next->sym->name, "3") == 0);
        free_ast(ast);
    }
    free_combinator(parser2);
    free(in2->buffer);
    free(in2);

    free(ast_nil);
}


void test_pascal_declarations(void) {
    const char* input_str = "program a(i,o); var x, y : integer; begin end.";

    input_t *in = new_input();
    in->buffer = strdup(input_str);
    in->length = strlen(input_str);

    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    combinator_t* parser = p_program();
    ParseResult result = parse(in, parser);

    TEST_CHECK(result.is_success == true);
    if (!result.is_success) {
        TEST_MSG("Parser failed: %s", result.value.error->message);
        free_error(result.value.error);
    } else {
        ast_t* ast = result.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_PROGRAM);

        // Children of program: prog_name -> param1 -> param2 -> var_decls -> stmts
        ast_t* prog_name = ast->child;
        TEST_CHECK(prog_name->typ == PASCAL_T_IDENT);

        ast_t* param1 = prog_name->next;
        TEST_CHECK(param1->typ == PASCAL_T_IDENT);

        ast_t* param2 = param1->next;
        TEST_CHECK(param2->typ == PASCAL_T_IDENT);

        ast_t* var_decls = param2->next;
        TEST_CHECK(var_decls != NULL);
        if(var_decls) {
            TEST_CHECK(var_decls->typ == PASCAL_T_VAR_DECL);

            // Children of VAR_DECL are: an IDENT_LIST node, and a TYPE node
            ast_t* ident_list_node = var_decls->child;
            TEST_CHECK(ident_list_node->typ == PASCAL_T_IDENT_LIST);

            ast_t* type_node = ident_list_node->next;
            TEST_CHECK(type_node->typ == PASCAL_T_TYPE_INTEGER);

            // Children of IDENT_LIST are the identifiers
            ast_t* id1 = ident_list_node->child;
            TEST_CHECK(id1->typ == PASCAL_T_IDENT && strcmp(id1->sym->name, "x") == 0);
            ast_t* id2 = id1->next;
            TEST_CHECK(id2->typ == PASCAL_T_IDENT && strcmp(id2->sym->name, "y") == 0);
        }

        free_ast(ast);
    }

    free_combinator(parser);
    free(in->buffer);
    free(in);
    free(ast_nil);
}

void test_pascal_mod_operator(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    const char* input = "10 mod 3";
    input_t *in = new_input();
    in->buffer = strdup(input);
    in->length = strlen(input);
    combinator_t* parser = p_expression();
    ParseResult res = parse(in, parser);
    TEST_CHECK(res.is_success);
    if(res.is_success) {
        ast_t* ast = res.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_MOD);
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->sym->name, "10") == 0);
        TEST_CHECK(ast->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->next->sym->name, "3") == 0);
        free_ast(ast);
    }
    free_combinator(parser);
    free(in->buffer);
    free(in);

    free(ast_nil);
}

void test_pascal_assignment(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    const char* input = "x := 5";
    input_t *in = new_input();
    in->buffer = strdup(input);
    in->length = strlen(input);
    combinator_t* parser = p_statement();
    ParseResult res = parse(in, parser);
    TEST_CHECK(res.is_success);
    if(res.is_success) {
        ast_t* ast = res.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ASSIGN);
        TEST_CHECK(ast->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->child->sym->name, "x") == 0);
        TEST_CHECK(ast->child->next->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->next->sym->name, "5") == 0);
        free_ast(ast);
    }
    free_combinator(parser);
    free(in->buffer);
    free(in);

    free(ast_nil);
}

void test_pascal_standard_procedures(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    // Test read(x)
    const char* input_read = "read(x);";
    input_t *in_read = new_input();
    in_read->buffer = strdup(input_read);
    in_read->length = strlen(input_read);
    combinator_t* parser_read = p_statement();
    ParseResult res_read = parse(in_read, parser_read);
    TEST_CHECK(res_read.is_success);
    if(res_read.is_success) {
        ast_t* ast = res_read.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_PROC_CALL);
        TEST_CHECK(strcmp(ast->sym->name, "read") == 0);
        TEST_CHECK(ast->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->child->sym->name, "x") == 0);
        free_ast(ast);
    }
    free_combinator(parser_read);
    free(in_read->buffer);
    free(in_read);

    // Test writeln('hello')
    const char* input_writeln = "writeln('hello');";
    input_t *in_writeln = new_input();
    in_writeln->buffer = strdup(input_writeln);
    in_writeln->length = strlen(input_writeln);
    combinator_t* parser_writeln = p_statement();
    ParseResult res_writeln = parse(in_writeln, parser_writeln);
    TEST_CHECK(res_writeln.is_success);
    if(res_writeln.is_success) {
        ast_t* ast = res_writeln.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_PROC_CALL);
        TEST_CHECK(strcmp(ast->sym->name, "writeln") == 0);
        TEST_CHECK(ast->child->typ == PASCAL_T_STRING);
        TEST_CHECK(strcmp(ast->child->sym->name, "hello") == 0);
        free_ast(ast);
    }
    free_combinator(parser_writeln);
    free(in_writeln->buffer);
    free(in_writeln);

    free(ast_nil);
}

void test_pascal_if_statement(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    const char* input = "if x > 10 then x := 1 else x := 2";
    input_t *in = new_input();
    in->buffer = strdup(input);
    in->length = strlen(input);
    combinator_t* parser = p_if_statement();
    ParseResult res = parse(in, parser);
    TEST_CHECK(res.is_success);
    if(res.is_success) {
        ast_t* ast = res.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_IF);

        ast_t* cond = ast->child;
        TEST_CHECK(cond->typ == PASCAL_T_GT);

        ast_t* then_stmt = cond->next;
        TEST_CHECK(then_stmt->typ == PASCAL_T_ASSIGN);

        ast_t* else_stmt = then_stmt->next;
        TEST_CHECK(else_stmt->typ == PASCAL_T_ASSIGN);

        free_ast(ast);
    }
    free_combinator(parser);
    free(in->buffer);
    free(in);

    free(ast_nil);
}

void test_pascal_for_statement(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    // Test FOR TO statement
    const char* input1 = "for i := 1 to 10 do x := x + i";
    input_t *in1 = new_input();
    in1->buffer = strdup(input1);
    in1->length = strlen(input1);
    combinator_t* parser1 = p_for_statement();
    ParseResult res1 = parse(in1, parser1);
    TEST_CHECK(res1.is_success);
    if(res1.is_success) {
        ast_t* ast = res1.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_FOR);
        
        // Check structure: ident -> start_expr -> direction -> statement
        ast_t* ident = ast->child;
        TEST_CHECK(ident->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ident->sym->name, "i") == 0);
        
        ast_t* start_expr = ident->next;
        TEST_CHECK(start_expr->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(start_expr->sym->name, "1") == 0);
        
        ast_t* direction = start_expr->next;
        TEST_CHECK(direction->typ == PASCAL_T_FOR_TO);
        
        // The end expression is a child of the direction node
        TEST_CHECK(direction->child != NULL);
        ast_t* end_expr = direction->child;
        TEST_CHECK(end_expr->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(end_expr->sym->name, "10") == 0);
        
        ast_t* statement = direction->next;
        TEST_CHECK(statement->typ == PASCAL_T_ASSIGN);
        
        free_ast(ast);
    }
    free_combinator(parser1);
    free(in1->buffer);
    free(in1);

    // Test FOR DOWNTO statement
    const char* input2 = "for i := 10 downto 1 do x := x - i";
    input_t *in2 = new_input();
    in2->buffer = strdup(input2);
    in2->length = strlen(input2);
    combinator_t* parser2 = p_for_statement();
    ParseResult res2 = parse(in2, parser2);
    TEST_CHECK(res2.is_success);
    if(res2.is_success) {
        ast_t* ast = res2.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_FOR);
        
        // Check structure: ident -> start_expr -> direction -> statement
        ast_t* ident = ast->child;
        TEST_CHECK(ident->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ident->sym->name, "i") == 0);
        
        ast_t* start_expr = ident->next;
        TEST_CHECK(start_expr->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(start_expr->sym->name, "10") == 0);
        
        ast_t* direction = start_expr->next;
        TEST_CHECK(direction->typ == PASCAL_T_FOR_DOWNTO);
        
        // The end expression is a child of the direction node
        TEST_CHECK(direction->child != NULL);
        ast_t* end_expr = direction->child;
        TEST_CHECK(end_expr->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(end_expr->sym->name, "1") == 0);
        
        ast_t* statement = direction->next;
        TEST_CHECK(statement->typ == PASCAL_T_ASSIGN);
        
        free_ast(ast);
    }
    free_combinator(parser2);
    free(in2->buffer);
    free(in2);

    free(ast_nil);
}

void test_pascal_unary_minus(void) {
    ast_nil = new_ast();
    ast_nil->typ = PASCAL_T_NONE;

    // Test case 1: Simple unary minus
    const char* input1 = "-5";
    input_t *in1 = new_input();
    in1->buffer = strdup(input1);
    in1->length = strlen(input1);
    combinator_t* parser1 = p_expression();
    ParseResult res1 = parse(in1, parser1);
    TEST_CHECK(res1.is_success);
    if(res1.is_success) {
        ast_t* ast = res1.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_NEG);
        TEST_CHECK(ast->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(ast->child->sym->name, "5") == 0);
        free_ast(ast);
    }
    free_combinator(parser1);
    free(in1->buffer);
    free(in1);

    // Test case 2: Unary minus with variable
    const char* input2 = "-x";
    input_t *in2 = new_input();
    in2->buffer = strdup(input2);
    in2->length = strlen(input2);
    combinator_t* parser2 = p_expression();
    ParseResult res2 = parse(in2, parser2);
    TEST_CHECK(res2.is_success);
    if(res2.is_success) {
        ast_t* ast = res2.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_NEG);
        TEST_CHECK(ast->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->child->sym->name, "x") == 0);
        free_ast(ast);
    }
    free_combinator(parser2);
    free(in2->buffer);
    free(in2);

    // Test case 3: Complex expression with unary minus
    const char* input3 = "a + -b * -c";
    input_t *in3 = new_input();
    in3->buffer = strdup(input3);
    in3->length = strlen(input3);
    combinator_t* parser3 = p_expression();
    ParseResult res3 = parse(in3, parser3);
    TEST_CHECK(res3.is_success);
    if(res3.is_success) {
        ast_t* ast = res3.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_ADD);
        
        // Left child should be identifier 'a'
        TEST_CHECK(ast->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(ast->child->sym->name, "a") == 0);
        
        // Right child should be multiplication
        ast_t* mul_node = ast->child->next;
        TEST_CHECK(mul_node->typ == PASCAL_T_MUL);
        
        // Left child of multiplication should be unary minus on 'b'
        ast_t* neg_b = mul_node->child;
        TEST_CHECK(neg_b->typ == PASCAL_T_NEG);
        TEST_CHECK(neg_b->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(neg_b->child->sym->name, "b") == 0);
        
        // Right child of multiplication should be unary minus on 'c'
        ast_t* neg_c = neg_b->next;
        TEST_CHECK(neg_c->typ == PASCAL_T_NEG);
        TEST_CHECK(neg_c->child->typ == PASCAL_T_IDENT);
        TEST_CHECK(strcmp(neg_c->child->sym->name, "c") == 0);
        
        free_ast(ast);
    }
    free_combinator(parser3);
    free(in3->buffer);
    free(in3);

    // Test case 4: Double unary minus
    const char* input4 = "--5";
    input_t *in4 = new_input();
    in4->buffer = strdup(input4);
    in4->length = strlen(input4);
    combinator_t* parser4 = p_expression();
    ParseResult res4 = parse(in4, parser4);
    TEST_CHECK(res4.is_success);
    if(res4.is_success) {
        ast_t* ast = res4.value.ast;
        TEST_CHECK(ast->typ == PASCAL_T_NEG);
        
        // Child should be another unary minus
        ast_t* inner_neg = ast->child;
        TEST_CHECK(inner_neg->typ == PASCAL_T_NEG);
        
        // Grandchild should be the number 5
        TEST_CHECK(inner_neg->child->typ == PASCAL_T_INT_NUM);
        TEST_CHECK(strcmp(inner_neg->child->sym->name, "5") == 0);
        
        free_ast(ast);
    }
    free_combinator(parser4);
    free(in4->buffer);
    free(in4);

    free(ast_nil);
}

TEST_LIST = {
    { "test_pascal_minimal_program", test_pascal_minimal_program },
    { "test_pascal_asm_block", test_pascal_asm_block },
    { "test_pascal_identifier_list", test_pascal_identifier_list },
    { "test_pascal_expressions", test_pascal_expressions },
    { "test_pascal_declarations", test_pascal_declarations },
    { "test_pascal_mod_operator", test_pascal_mod_operator },
    { "test_pascal_assignment", test_pascal_assignment },
    { "test_pascal_standard_procedures", test_pascal_standard_procedures },
    { "test_pascal_if_statement", test_pascal_if_statement },
    { "test_pascal_for_statement", test_pascal_for_statement },
    { "test_pascal_unary_minus", test_pascal_unary_minus },
    { NULL, NULL }
};