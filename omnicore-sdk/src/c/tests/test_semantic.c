#include "parser.h"
#include "ast.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* semantic_test_copy_text(const char* value);

static int check_file(const char* path, int expect_error, ErrorCode expected_code) {
    ParserContext* parser = parser_init(path);
    if (!parser) {
        fprintf(stderr, "Could not initialize parser for %s\n", path);
        return 0;
    }
    if (parser_run(parser) != 0 || !parser->ast_root) {
        fprintf(stderr, "Parser failed for semantic fixture %s\n", path);
        parser_destroy(parser);
        return 0;
    }

    OccDiagnosticCollector diagnostics;
    occ_diagnostics_init(&diagnostics);

    SemanticContext semantic;
    semantic_init(&semantic, path, NULL);
    semantic_attach_diagnostics(&semantic, &diagnostics, OCC_STAGE_SEMANTIC);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);

    int ok = expect_error ? semantic.error_count > 0 : semantic.error_count == 0;
    if (expect_error) {
        ok = ok && occ_diagnostics_count(&diagnostics) > 0 &&
            occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) > 0;
    } else {
        ok = ok && occ_diagnostics_count(&diagnostics) == 0;
    }
    if (ok && expect_error) {
        int found = 0;
        for (Error* error = semantic.error_head; error; error = error->next) {
            if (error->code == expected_code) found = 1;
        }
        ok = found;
    }
    if (!ok) {
        fprintf(stderr, "Unexpected semantic result for %s (errors=%d).\n",
            path, semantic.error_count);
    }

    semantic_destroy(&semantic);
    occ_diagnostics_destroy(&diagnostics);
    parser_destroy(parser);
    return ok;
}

static int check_source(const char* source, int expected_errors) {
    ParserContext* parser = parser_init_source(
        "semantic-control.nxf", (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    int ok;
    if (!parser) return 0;
    if (parser_run(parser) != 0 || !parser->ast_root) {
        Error* parser_error;
        fprintf(stderr, "Parser rejected semantic fixture: %s\\n", source);
        fprintf(stderr, "  parser status=%d errors=%d root=%p allocations=%zu fail_at=%zu\\n",
            (int)parser_get_status(parser), parser->error_count, parser->ast_root,
            parser->test_allocation_count, parser->test_fail_allocation_at);
        for (parser_error = parser->error_head; parser_error; parser_error = parser_error->next) {
            fprintf(stderr, "  parser error %d at %d:%d: %s\\n", parser_error->code,
                parser_error->line, parser_error->col,
                parser_error->message ? parser_error->message : "(null)");
        }
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "semantic-control.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    ok = semantic.error_count == expected_errors;
    if (!ok) { Error* e; fprintf(stderr, "Semantic test mismatch: expected=%d actual=%d source=%s\\n", expected_errors, semantic.error_count, source); for(e=semantic.error_head;e;e=e->next) fprintf(stderr,"  semantic error %d at %d:%d: %s\\n",e->code,e->line,e->col,e->message?e->message:"(null)"); }
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_control_flow_semantics(void) {
    const char* valid_if =
        "f:main() { if (true) { v:x!i32 = 1; } elsif (1 == 1) "
        "v:y!i32 = 2; els v:z!i32 = 3; }";
    const char* invalid_condition = "f:main() { if (1) v:x!i32 = 1; }";
    const char* invalid_break = "f:main() { break; continue; }";
    const char* valid_loop = "f:main() { loop (true) { break; continue; } }";
    const char* nested_function_break =
        "f:main() { loop (true) { f:nested() { break; } break; } }";
    const char* nested_function_continue =
        "f:main() { loop (true) { f:nested() { continue; } break; } }";
    const char* nested_loops =
        "f:main() { loop (true) { loop (true) { break; continue; } break; continue; } }";
    const char* loop_scope_restored =
        "f:main() { loop (true) { break; } break; continue; }";
    const char* valid_conditional_actions =
        "f:main() { loop (true) { (true) ? break; (false) ? continue; } }";
    const char* invalid_loop_condition =
        "f:main() { loop (1) { break; } }";
    const char* valid_block_cleanup =
        "f:main() { { v:resource!i32 = 1; } ?; }";
    const char* invalid_conditional_actions =
        "f:main() { (true) ? break; (false) ? continue; }";
    return check_source(valid_if, 0) &&
        check_source(invalid_condition, 1) &&
        check_source(invalid_break, 2) &&
        check_source(valid_loop, 0) &&
        check_source(nested_loops, 0) &&
        check_source(loop_scope_restored, 2) &&
        check_source(nested_function_break, 1) &&
        check_source(nested_function_continue, 1) &&
        check_source(valid_conditional_actions, 0) &&
        check_source(invalid_loop_condition, 1) &&
        check_source(valid_block_cleanup, 0) &&
        check_source(invalid_conditional_actions, 2);
}


static int check_malformed_conditional_ast(void) {
    ASTIf* missing_condition = (ASTIf*)ast_create(AST_IF);
    ASTIf* missing_primary_branch = (ASTIf*)ast_create(AST_IF);
    SemanticContext semantic;
    int ok = 1;
    if (!missing_condition || !missing_primary_branch) {
        ast_destroy((ASTNode*)missing_condition);
        ast_destroy((ASTNode*)missing_primary_branch);
        return 0;
    }

    missing_condition->then_branch = ast_create(AST_BLOCK);
    if (!missing_condition->then_branch) ok = 0;
    if (ok) {
        semantic_init(&semantic, "semantic-malformed-if.nxf", NULL);
        semantic_check(&semantic, (ASTNode*)missing_condition);
        ok = semantic.error_count == 1 &&
            semantic.error_head &&
            strstr(semantic.error_head->message,
                "if statement is missing its condition expression") != NULL;
        semantic_destroy(&semantic);
    }

    if (ok) {
        /* A non-null condition is required; the missing branch must be diagnosed. */
        missing_primary_branch->condition = ast_create(AST_LITERAL);
        if (!missing_primary_branch->condition) ok = 0;
        else {
            ((ASTLiteral*)missing_primary_branch->condition)->type = TOKEN_TRUE;
            semantic_init(&semantic, "semantic-malformed-if.nxf", NULL);
            semantic_check(&semantic, (ASTNode*)missing_primary_branch);
            ok = semantic.error_count > 0 &&
                semantic.error_head &&
                strstr(semantic.error_head->message,
                    "if statement is missing its primary branch") != NULL;
            semantic_destroy(&semantic);
        }
    }

    ast_destroy((ASTNode*)missing_condition);
    ast_destroy((ASTNode*)missing_primary_branch);
    return ok;
}

static int check_malformed_pattern_ast(void) {
    {
        ASTSelect* malformed=(ASTSelect*)ast_create(AST_SELECT);
        ASTLiteral* literal=(ASTLiteral*)ast_create(AST_LITERAL);
        SemanticContext malformed_semantic;
        int malformed_ok;
        if(!malformed||!literal) {
            ast_destroy((ASTNode*)malformed);ast_destroy((ASTNode*)literal);
            return 0;
        }
        literal->type=TOKEN_NUMBER;
        malformed->selector=(ASTNode*)literal;
        semantic_init(&malformed_semantic,"semantic-missing-literal-text.nxf",NULL);
        semantic_check(&malformed_semantic,(ASTNode*)malformed);
        malformed_ok=malformed_semantic.error_count==1 &&
            malformed_semantic.error_head &&
            strstr(malformed_semantic.error_head->message,
                "Literal AST is missing its string value")!=NULL;
        semantic_destroy(&malformed_semantic);
        ast_destroy((ASTNode*)malformed);
        if(!malformed_ok)return 0;
    }
    ASTSelect* select_node=(ASTSelect*)ast_create(AST_SELECT);
    ASTSelectArm* arm=(ASTSelectArm*)ast_create(AST_SELECT_ARM);
    ASTNode* selector=ast_create(AST_LITERAL);
    ASTNode* invalid_pattern=ast_create(AST_IDENTIFIER);
    ASTNode* body=ast_create(AST_BLOCK);
    SemanticContext semantic;
    int ok=0;
    if(!select_node || !arm || !selector || !invalid_pattern || !body) {
        ast_destroy((ASTNode*)select_node);
        ast_destroy((ASTNode*)arm);
        ast_destroy(selector);
        ast_destroy(invalid_pattern);
        ast_destroy(body);
        return 0;
    }
    ((ASTLiteral*)selector)->type=TOKEN_NUMBER;
    ((ASTLiteral*)selector)->string_value=semantic_test_copy_text("1");
    if (!((ASTLiteral*)selector)->string_value) {
        ast_destroy((ASTNode*)select_node);
        ast_destroy((ASTNode*)arm);
        ast_destroy(selector);
        ast_destroy(invalid_pattern);
        ast_destroy(body);
        return 0;
    }
    select_node->selector=selector;
    select_node->arms=(ASTNode*)arm;
    arm->pattern=invalid_pattern;
    arm->body=body;
    semantic_init(&semantic,"semantic-malformed-pattern.nxf",NULL);
    semantic_check(&semantic,(ASTNode*)select_node);
    ok=semantic.error_count==1 && semantic.error_head &&
        strstr(semantic.error_head->message,
            "Pattern contains an unsupported AST node")!=NULL;
    semantic_destroy(&semantic);
    ast_destroy((ASTNode*)select_node);
    return ok;
}

static int check_malformed_rule_ast(void) {
    static const struct {
        ASTNodeType type;
        const char* message;
    } cases[] = {
        { AST_RULE_BARRIER, "Rule barrier is missing its condition expression" },
        { AST_RULE_TRIGGER, "Rule trigger is missing its condition expression" },
        { AST_RULE_ALWAYS, "Always rule is missing its action or event name" },
        { AST_RULE_DEFAULT, "Default rule is missing its action or event name" },
        { AST_APPLY_RULES, "apply rules is missing its ruleset name" },
        { AST_ON_BLOCK, "on-block is missing its ruleset name" },
        { AST_RULESET_DECL, "Ruleset declaration is missing its name" }
    };
    size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        ASTNode* node=ast_create(cases[i].type);
        SemanticContext semantic;
        int ok;
        if(!node) return 0;
        semantic_init(&semantic,"semantic-malformed-rules.nxf",NULL);
        semantic_check(&semantic,node);
        ok=semantic.error_count==1 && semantic.error_head &&
            strstr(semantic.error_head->message,cases[i].message)!=NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if(!ok) return 0;
    }
    return 1;
}

static int check_malformed_expression_and_loop_ast(void) {
    static const struct {
        ASTNodeType type;
        const char* message;
    } cases[] = {
        { AST_BINARY_EXPR, "Binary expression requires both left and right operands" },
        { AST_NEXUS_FLOW, "Nexus Flow operation requires both left and right operands" },
        { AST_LOOP, "Loop is missing its body" },
        { AST_UNPACK, "Reserved unpack AST node is missing its source expression" },
        { AST_CAST, "cast() is missing its source value expression" }
    };
    size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        ASTNode* node=ast_create(cases[i].type);
        SemanticContext semantic;
        int ok;
        if(!node) return 0;
        semantic_init(&semantic,"semantic-malformed-expression.nxf",NULL);
        semantic_check(&semantic,node);
        ok=semantic.error_count==1 && semantic.error_head &&
            strstr(semantic.error_head->message,cases[i].message)!=NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if(!ok) return 0;
    }
    {
        ASTNode* node=ast_create(AST_LOOP);
        SemanticContext semantic;
        int ok;
        if(!node) return 0;
        ((ASTLoop*)node)->loop_type=3;
        ((ASTLoop*)node)->body=ast_create(AST_BLOCK);
        if(!((ASTLoop*)node)->body) { ast_destroy(node); return 0; }
        semantic_init(&semantic,"semantic-malformed-foreach.nxf",NULL);
        semantic_check(&semantic,node);
        ok=semantic.error_count==1 && semantic.error_head &&
            strstr(semantic.error_head->message,
                "Foreach loop requires both an iterator and a collection")!=NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if(!ok) return 0;
    }
    return 1;
}

static int check_malformed_function_and_control_ast(void) {
    static const struct {
        ASTNodeType type;
        const char* message;
    } cases[] = {
        { AST_FUNC_DECL, "Function declaration requires a non-empty name" },
        { AST_NEXUS_LABEL, "Nexus label declaration requires a non-empty label name" },
        { AST_NEXUS_JUMP, "Nexus jump requires a non-empty target label name" },
        { AST_DEFER, "defer requires a statement or block body" },
        { AST_THREAD_BLOCK, "thread_block requires a non-empty block name" }
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ASTNode* node = ast_create(cases[i].type);
        SemanticContext semantic;
        int ok;
        if (!node) return 0;
        semantic_init(&semantic, "semantic-malformed-function-control.nxf", NULL);
        semantic_check(&semantic, node);
        ok = semantic.error_count == 1 && semantic.error_head &&
            strstr(semantic.error_head->message, cases[i].message) != NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if (!ok) return 0;
    }
    {
        ASTNode* node = ast_create(AST_FUNC_DECL);
        SemanticContext semantic;
        int ok;
        if (!node) return 0;
        ((ASTFuncDecl*)node)->name = semantic_test_copy_text("present");
        if (!((ASTFuncDecl*)node)->name) { ast_destroy(node); return 0; }
        semantic_init(&semantic, "semantic-missing-function-body.nxf", NULL);
        semantic_check(&semantic, node);
        ok = semantic.error_count == 1 && semantic.error_head &&
            strstr(semantic.error_head->message,
                "Function declaration is missing its body") != NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if (!ok) return 0;
    }
    {
        ASTNode* node = ast_create(AST_THREAD_BLOCK);
        SemanticContext semantic;
        int ok;
        if (!node) return 0;
        ((ASTThreadBlock*)node)->name = semantic_test_copy_text("worker");
        if (!((ASTThreadBlock*)node)->name) { ast_destroy(node); return 0; }
        semantic_init(&semantic, "semantic-missing-thread-body.nxf", NULL);
        semantic_check(&semantic, node);
        ok = semantic.error_count == 1 && semantic.error_head &&
            strstr(semantic.error_head->message,
                "thread_block is missing its body") != NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if (!ok) return 0;
    }
    {
        ASTNode* node = ast_create(AST_THREAD_BLOCK);
        SemanticContext semantic;
        int ok;
        if (!node) return 0;
        ((ASTThreadBlock*)node)->name = semantic_test_copy_text("worker");
        if (!((ASTThreadBlock*)node)->name) { ast_destroy(node); return 0; }
        ((ASTThreadBlock*)node)->body = ast_create(AST_LITERAL);
        if (!((ASTThreadBlock*)node)->body) { ast_destroy(node); return 0; }
        semantic_init(&semantic, "semantic-invalid-thread-body-kind.nxf", NULL);
        semantic_check(&semantic, node);
        ok = semantic.error_count == 1 && semantic.error_head &&
            strstr(semantic.error_head->message,
                "thread_block body must be an AST_BLOCK node") != NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if (!ok) return 0;
    }
    if (!check_source(
            "f:main() { (worker)>{ v:inside!i32 = 1; "
            "echo(inside); } }", 0)) return 0;
    if (!check_source(
            "f:main() { (worker)>{ v:inside!i32 = 1; } "
            "v:outside!i32 = inside; }", 1)) return 0;
    if (!check_source(
            "f:main() { (outer)>{ v:x!i32 = 1; "
            "(inner)>{ v:y!i32 = x; } } }", 1)) return 0;
    return 1;
}

static int check_malformed_nexus_and_argument_ast(void) {
    static const struct { ASTNodeType type; const char* message; } cases[] = {
        { AST_NEXUS_CAPTURE, "Nexus capture requires a non-empty handler name and source expression" },
        { AST_NEXUS_PIPE, "Nexus pipe requires both left and right operands" },
        { AST_NAMED_ARG, "Named argument requires a non-empty name and value expression" },
    };
    size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        ASTNode* node=ast_create(cases[i].type);
        SemanticContext semantic;
        int ok;
        if(!node)return 0;
        semantic_init(&semantic,"semantic-malformed-nexus-argument.nxf",NULL);
        semantic_check(&semantic,node);
        ok=semantic.error_count==1 && semantic.error_head &&
            strstr(semantic.error_head->message,cases[i].message)!=NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if(!ok)return 0;
    }
    return 1;
}

static int check_malformed_intrinsic_ast(void) {
    ASTNode* node = ast_create(AST_PEEK);
    SemanticContext semantic;
    int ok;
    if (!node) return 0;
    ((ASTCall*)node)->args = ast_create(AST_LITERAL);
    if (!((ASTCall*)node)->args) { ast_destroy(node); return 0; }
    ((ASTLiteral*)((ASTCall*)node)->args)->type = TOKEN_NUMBER;
    ((ASTLiteral*)((ASTCall*)node)->args)->string_value =
        semantic_test_copy_text("1");
    if (!((ASTLiteral*)((ASTCall*)node)->args)->string_value) {
        ast_destroy(node);
        return 0;
    }
    semantic_init(&semantic, "semantic-implicit-intrinsic-identity.nxf", NULL);
    semantic_check(&semantic, node);
    ok = semantic.error_count == 0;
    semantic_destroy(&semantic);
    ast_destroy(node);
    if (!ok) return 0;

    node = ast_create(AST_AREA);
    if (!node) return 0;
    ((ASTCall*)node)->callee = ast_create(AST_IDENTIFIER);
    if (!((ASTCall*)node)->callee) { ast_destroy(node); return 0; }
    ((ASTIdentifier*)((ASTCall*)node)->callee)->name = (char*)malloc(5u);
    if (!((ASTIdentifier*)((ASTCall*)node)->callee)->name) { ast_destroy(node); return 0; }
    memcpy(((ASTIdentifier*)((ASTCall*)node)->callee)->name, "zone", 5u);
    semantic_init(&semantic, "semantic-wrong-intrinsic-callee.nxf", NULL);
    semantic_check(&semantic, node);
    ok = semantic.error_count >= 1 && semantic.error_head &&
        strstr(semantic.error_head->message,
            "Intrinsic AST callee must be 'area'") != NULL;
    semantic_destroy(&semantic);
    ast_destroy(node);
    return ok;
}

static int check_thread_block_value_captures(void);

static int check_thread_api_is_module_call(void) {
    ASTNode* legacy = ast_create(AST_LISTEN);
    SemanticContext semantic;
    int rejected;
    if (!legacy) return 0;
    semantic_init(&semantic, "semantic-legacy-listen.nxf", NULL);
    semantic_check(&semantic, legacy);
    rejected = semantic.error_count > 0 && semantic.error_head &&
        strstr(semantic.error_head->message,
            "Legacy AST_LISTEN is not emitted by the core Parser") != NULL;
    semantic_destroy(&semantic);
    ast_destroy(legacy);
    if (!rejected) return 0;
    return check_source(
        "f:worker() { } f:main() { spawn(worker); }", 0) &&
        check_source(
        "f:main() { listen(1); send(1); receive(); done(); }", 0) &&
        check_source(
        "f:main() { (worker)>{ v:x!i32 = 1; } spawn(worker); }", 0) &&
        check_source(
        "f:main() { spawn(missing_worker); }", 1) &&
        check_source(
            "f:main() { (worker)>{ } spawn(worker, worker); }", 1) &&
        check_source(
            "f:main() { spawn(1); }", 1) &&
        check_source(
            "f:main() { v:outside!i32 = 1; "
            "(worker)>{ v:inside!i32 = outside; } }", 1) &&
        check_source(
            "f:main() { (worker)>{ (h) <- copy(); } }", 1) &&
        check_source(
            "f:main() { v:x!i32 = 1; "
            "(worker)>{ (h) <- copy(x, x); } }", 1) &&
        check_source(
            "f:main() { (worker)>{ } (worker)>{ } }", 1) &&
        check_thread_block_value_captures();
}

static int check_thread_block_value_captures(void) {
    const char* source =
        "f:main() { v:outside!i32 = 7; "
        "(worker)>{ (h) <- copy(outside); (h2) <- copy(outside); "
        "v:inside!i32 = 1; v:inner!i32 = inside; } spawn(worker); }";
    ParserContext* parser = parser_init_source(
        "semantic-thread-capture.nxf",
        (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    ThreadBlockEntry* block;
    ThreadBlockCapture* capture;
    int ok = 0;
    if (!parser) return 0;
    if (parser_run(parser) != 0 || !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "semantic-thread-capture.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    block = semantic.thread_blocks;
    capture = block ? block->captures : NULL;
    ok = semantic.error_count == 0 &&
        block && strcmp(block->name, "worker") == 0 &&
        block->capture_count == 1u && capture &&
        strcmp(capture->name, "outside") == 0 &&
        strcmp(capture->type_name, "i32") == 0 &&
        capture->source_scope_id != 0u &&
        capture->first_use != NULL &&
        capture->source_declaration != NULL &&
        capture->next == NULL;
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_unnamed_declaration_ast(void) {
    static const struct {
        ASTNodeType type;
        const char* message;
    } cases[] = {
        { AST_MUST_DECL, "immutable m: declaration requires a non-empty name" },
        { AST_VAR_DECL, "Variable declaration requires a non-empty name" },
        { AST_GROUP_DECL, "Group declaration requires a non-empty name" }
    };
    size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        ASTNode* node=ast_create(cases[i].type);
        SemanticContext semantic;
        int ok;
        if(!node) return 0;
        semantic_init(&semantic,"semantic-unnamed-declaration.nxf",NULL);
        semantic_check(&semantic,node);
        ok=semantic.error_count==1 && semantic.error_head &&
            strstr(semantic.error_head->message,cases[i].message)!=NULL;
        semantic_destroy(&semantic);
        ast_destroy(node);
        if(!ok) return 0;
    }
    return 1;
}

static int rejects_semantic_pending_node(ASTNodeType type) {
    ASTNode* node = ast_create(type);
    SemanticContext semantic;
    int rejected;
    if (!node) return 0;
    semantic_init(&semantic, "semantic-pending-node.nxf", NULL);
    semantic_check(&semantic, node);
    rejected = semantic.error_count > 0;
    semantic_destroy(&semantic);
    ast_destroy(node);
    return rejected;
}

static int check_semantic_fail_closed_nodes(void) {
    /* Tuple is supported; raw ASM and reserved AI logic remain fail-closed. */
    return rejects_semantic_pending_node(AST_ASM_BLOCK) &&
        rejects_semantic_pending_node(AST_AI_LOGIC);
}

static int check_postfix_semantics(void) {
    const char* valid =
        "f:main() { v:counter!i32 = 0; counter++; counter--; }";
    const char* constant_operand =
        "f:main() { c:counter!i32 = 0; counter++; }";
    const char* non_numeric_operand =
        "f:main() { v:label!str = \"x\"; label++; }";
    const char* unresolved_operand =
        "f:main() { missing++; }";
    return check_source(valid, 0) &&
        check_source(constant_operand, 1) &&
        check_source(non_numeric_operand, 1) &&
        check_source(unresolved_operand, 1);
}

static int check_aggregate_member_semantics(void) {
    const char* valid =
        "f:main() { struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point; v:a!i32 = p.x; p.y = 2; }";
    const char* nested =
        "f:main() { struct: Inner { v:x!i32 }"
        " struct: Outer { v:inner!Inner } v:o!Outer;"
        " v:a!i32 = o.inner.x; o.inner.x = 3; }";
    const char* constant_object =
        "f:main() { struct: Point { v:x!i32 } c:p!Point = Point { x = 1 }; p.x = 2; }";
    const char* incompatible =
        "f:main() { struct: Point { v:x!i32 } v:p!Point; p.x = \"bad\"; }";
    const char* unknown_member =
        "f:main() { struct: Point { v:x!i32 } v:p!Point; v:a = p.missing; }";
    const char* non_aggregate =
        "f:main() { v:value!i32 = 1; v:bad = value.member; }";
    const char* duplicate_struct_field =
        "f:main() { struct: Point { v:x!i32, v:x!f64 } }";
    const char* duplicate_union_field =
        "f:main() { union: Choice { v:value!i32, v:value!f64 } }";
    const char* valid_union_cross_member_read =
        "f:main() { union: Register { v:word!i32, v:octet!u8 } "
        "v:r!Register; r.word = 0x12345678; v:part!u8 = r.octet; "
        "v:whole!i32 = r.word; }";
    const char* unknown_union_member =
        "f:main() { union: Register { v:word!u32 } "
        "v:r!Register; v:bad = r.missing; }";
    const char* incompatible_union_member_write =
        "f:main() { union: Register { v:word!u32 } "
        "v:r!Register; r.word = \"not-a-number\"; }";
    const char* unknown_union_field_type =
        "f:main() { union: Register { v:word!MissingType } }";
    return check_source(valid, 0) &&
        check_source(nested, 0) &&
        check_source(constant_object, 1) &&
        check_source(incompatible, 1) &&
        check_source(unknown_member, 1) &&
        check_source(non_aggregate, 1) &&
        check_source(duplicate_struct_field, 1) &&
        check_source(duplicate_union_field, 1) &&
        check_source(valid_union_cross_member_read, 0) &&
        check_source(unknown_union_member, 1) &&
        check_source(incompatible_union_member_write, 1) &&
        check_source(unknown_union_field_type, 1);
}

static int check_struct_literal_semantics(void) {
    const char* valid_reordered =
        "f:main() { struct: Point { v:x!i32, v:y!i32 } "
        "v:p!Point = Point { y = 2, x = 1 }; }";
    const char* unknown_field =
        "f:main() { struct: Point { v:x!i32 } "
        "v:p!Point = Point { x = 1, missing = 2 }; }";
    const char* duplicate_field =
        "f:main() { struct: Point { v:x!i32 } "
        "v:p!Point = Point { x = 1, x = 2 }; }";
    const char* missing_field =
        "f:main() { struct: Point { v:x!i32, v:y!i32 } "
        "v:p!Point = Point { x = 1 }; }";
    const char* incompatible_field =
        "f:main() { struct: Point { v:x!i32 } "
        "v:p!Point = Point { x = \"wrong\" }; }";
    const char* numeric_type_mismatch =
        "f:main() { struct: Point { v:x!i32 } "
        "v:p!Point = Point { x = 1.0 }; }";
    const char* non_struct_type =
        "f:main() { enum: State { Ready } v:s!State = State { Ready = 1 }; }";
    const char* empty_struct =
        "f:main() { struct: Empty { } v:e!Empty = Empty { }; }";
    const char* group_owned_type_inside =
        "f:main() { group Store { struct:Local { v:value!i32 } "
        "f:make() { v:item!Local = Local { value = 7 }; ret; } } }";
    const char* group_owned_type_outside =
        "f:main() { group Store { struct:Local { v:value!i32 } } "
        "f:make() { v:item!Local = Local { value = 7 }; ret; } }";
    const char* nested_group_cannot_use_outer_struct =
        "f:main() { group Outer { struct:Private { v:value!i32 } "
        "Inner => group { f:make() { v:item!Private = Private { value = 7 }; ret; } } } }";

    return check_source(valid_reordered, 0) &&
        check_source(unknown_field, 1) &&
        check_source(duplicate_field, 1) &&
        check_source(missing_field, 1) &&
        check_source(incompatible_field, 1) &&
        check_source(numeric_type_mismatch, 1) &&
        check_source(non_struct_type, 1) &&
        check_source(empty_struct, 0) &&
        check_source(group_owned_type_inside, 0) &&
        check_source(group_owned_type_outside, 2) &&
        check_source(nested_group_cannot_use_outer_struct, 2);
}

static int check_array_literal_semantics(void) {
    const char* valid =
        "f:main() { v:values = [1, 2, 3]; v:matrix = [[1, 2], [3, 4]]; v:empty = []; }";
    const char* mixed =
        "f:main() { v:invalid = [1, \"two\"]; }";
    return check_source(valid,0) && check_source(mixed,1);
}

static int check_compound_assignment_semantics(void) {
    const char* valid_numeric = "f:main() { v:x!i32 = 1; x += 2; x *= 3; }";
    const char* invalid_numeric = "f:main() { v:x!str = \"a\"; x += \"b\"; }";
    const char* invalid_bitwise_float = "f:main() { v:x!f32 = 1.0; x &= 1.0; }";
    const char* valid_bitwise = "f:main() { v:x!i32 = 1; x |= 2; x ^= 3; }";
    const char* valid_bitwise_bit =
        "f:main() { v:a!bit; v:b!bit; a &= b; a ^= b; }";
    const char* valid_bitwise_byte =
        "f:main() { v:a!byte; v:b!byte; a |= b; a ^= b; }";
    const char* invalid_bit_arithmetic =
        "f:main() { v:a!bit; v:b!bit; a += b; }";
    const char* invalid_byte_arithmetic =
        "f:main() { v:a!byte; v:b!byte; a += b; }";
    const char* valid_fixed_bit_widths =
        "f:main() { v:a!8bit; v:b!8bit; a |= b; a &= b; a ^= b; "
        "v:c!16bit; v:d!32bit; v:e!64bit; v:f!128bit; }";
    const char* valid_fixed_byte_widths =
        "f:main() { v:a!1byte; a |= a; a &= a; a ^= a; "
        "v:b!2byte; b |= b; b &= b; b ^= b; "
        "v:c!4byte; c |= c; c &= c; c ^= c; "
        "v:d!8byte; d |= d; d &= d; d ^= d; "
        "v:e!16byte; e |= e; e &= e; e ^= e; v:inverse!16byte = ~e; }";
    const char* invalid_fixed_bit_arithmetic =
        "f:main() { v:a!8bit; v:b!8bit; a += b; }";
    const char* invalid_fixed_width_mismatch =
        "f:main() { v:a!8bit; v:b!16bit; a |= b; }";
    const char* valid_bitwise_complement_and_compare =
        "f:main() { v:a!8bit; v:b!8bit; v:inverse = ~a; v:same = a == b; }";
    const char* invalid_bitwise_complement =
        "f:main() { v:a!f32; v:inverse = ~a; }";
    const char* valid_fixed_bitwise_binary =
        "f:main() { v:a!8bit; v:b!8bit; "
        "v:masked = a & b; v:merged = a | b; v:toggled = a ^ b; }";
    const char* invalid_fixed_bitwise_binary_width =
        "f:main() { v:a!8bit; v:b!16bit; v:masked = a & b; }";
    const char* valid_representation_indexing =
        "f:main() { v:flags!16bit; v:one!bit; "
        "v:read!bit = flags[0]; flags[15] = one; "
        "v:flags8!8bit; v:lsb!bit = flags8[0]; v:msb!bit = flags8[7]; "
        "v:second_byte_lsb!bit = flags[8]; "
        "v:data!4byte; v:raw!byte = data[0]; data[3] = raw; }";
    const char* invalid_representation_index_type =
        "f:main() { v:flags!8bit; v:read = flags[1.5]; }";
    const char* invalid_bit_vector_index_range =
        "f:main() { v:flags!8bit; v:read!bit = flags[8]; }";
    const char* invalid_byte_block_index_range =
        "f:main() { v:data!4byte; v:read!byte = data[4]; }";
    const char* invalid_negative_bit_index =
        "f:main() { v:flags!8bit; v:read!bit = flags[-1]; }";
    const char* valid_explicit_bit_byte_coercions =
        "f:main() { v:one!bit = 1; v:zero!bit = 0; "
        "v:raw!byte = 255; v:flags!8bit = 255; "
        "v:word!16bit = 65535; v:packet!2byte = 65535; "
        "v:source!u8; v:from_source!8bit = source; "
        "v:small!8bit; v:widened!16bit = small; }";
    const char* invalid_explicit_bit_coercions =
        "f:main() { v:invalid!bit = 2; }";
    const char* invalid_explicit_byte_coercions =
        "f:main() { v:invalid!byte = 256; }";
    const char* invalid_explicit_bit_vector_coercions =
        "f:main() { v:invalid!8bit = 256; }";
    const char* invalid_explicit_word_coercions =
        "f:main() { v:invalid!16bit = 65536; }";
    const char* invalid_dynamic_narrowing_coercion =
        "f:main() { v:source!u16; v:target!8bit = source; }";
    const char* valid_decimal_widths =
        "f:main() { v:decimal32!d32; v:decimal64!d64; decimal32 += decimal32; decimal64 *= decimal64; }";
    const char* immutable_compound =
        "f:main() { m:value = 1; value += 2; }";
    const char* valid_string_append =
        "f:main() { v:text!str = \"a\"; text .= \"b\"; }";
    const char* invalid_string_append =
        "f:main() { v:value!i32 = 1; value .= 2; }";
    const char* valid_index_compound =
        "f:main() { v:items[2]!i32 = [1,2]; items[0] += 1; }";
    const char* constant_index_compound =
        "f:main() { c:items[2]!i32 = [1,2]; items[0] += 1; }";
    const char* valid_member_compound =
        "f:main() { struct:Point { v:x!i32 } v:p!Point; p.x += 1; }";
    const char* constant_member_compound =
        "f:main() { struct:Point { v:x!i32 } c:p!Point = Point { x = 1 }; p.x += 1; }";

    return check_source(valid_numeric,0) &&
        check_source(invalid_numeric,1) &&
        check_source(invalid_bitwise_float,2) &&
        check_source(valid_bitwise,0) &&
        check_source(valid_bitwise_bit,0) &&
        check_source(valid_bitwise_byte,0) &&
        check_source(invalid_bit_arithmetic,1) &&
        check_source(invalid_byte_arithmetic,1) &&
        check_source(valid_fixed_bit_widths,0) &&
        check_source(valid_fixed_byte_widths,0) &&
        check_source(invalid_fixed_bit_arithmetic,1) &&
        check_source(invalid_fixed_width_mismatch,1) &&
        check_source(valid_bitwise_complement_and_compare,0) &&
        check_source(invalid_bitwise_complement,1) &&
        check_source(valid_fixed_bitwise_binary,0) &&
        check_source(invalid_fixed_bitwise_binary_width,1) &&
        check_source(valid_representation_indexing,0) &&
        check_source(invalid_representation_index_type,1) &&
        check_source(invalid_bit_vector_index_range,1) &&
        check_source(invalid_byte_block_index_range,1) &&
        check_source(invalid_negative_bit_index,1) &&
        check_source(valid_explicit_bit_byte_coercions,0) &&
        check_source(invalid_explicit_bit_coercions,1) &&
        check_source(invalid_explicit_byte_coercions,1) &&
        check_source(invalid_explicit_bit_vector_coercions,1) &&
        check_source(invalid_explicit_word_coercions,1) &&
        check_source(invalid_dynamic_narrowing_coercion,1) &&
        check_source(valid_decimal_widths,0) &&
        check_source(immutable_compound,1) &&
        check_source(valid_string_append,0) &&
        check_source(invalid_string_append,1) &&
        check_source(valid_index_compound,0) &&
        check_source(constant_index_compound,1) &&
        check_source(valid_member_compound,0) &&
        check_source(constant_member_compound,1);
}

static int check_pointer_reference_type_semantics(void) {
    const char* exact_pointer =
        "f:take(value!*i32) {} f:main() { v:a!*i32; v:b!*i32; a = b; take(a); }";
    const char* pointer_base_mismatch =
        "f:take(value!*i32) {} f:main() { v:a!*f32; take(a); }";
    const char* pointer_depth_mismatch =
        "f:take(value!**i32) {} f:main() { v:a!*i32; take(a); }";
    const char* pointer_reference_mismatch =
        "f:take(value!&i32) {} f:main() { v:a!*i32; take(a); }";
    const char* exact_reference =
        "f:take(value!&i32) {} f:main() { v:a!&i32; take(a); }";
    const char* reference_base_mismatch =
        "f:take(value!&i32) {} f:main() { v:a!&f32; take(a); }";
    return check_source(exact_pointer, 0) &&
        check_source(pointer_base_mismatch, 1) &&
        check_source(pointer_depth_mismatch, 1) &&
        check_source(pointer_reference_mismatch, 1) &&
        check_source(exact_reference, 0) &&
        check_source(reference_base_mismatch, 1);
}

static int check_array_type_semantics(void) {
    const char* valid_params = "f:consume(a![], b!i32[], third:i32[5]) {}";
    const char* valid_matrix_param = "f:consume_matrix(matrix_data:f64[2][2]) {}";
    const char* valid =
        "f:consume(a![], b!i32[], third:i32[5]) {}"
        "f:consume_matrix(matrix_data:f64[2][2]) {}"
        "f:main() { v:dynamic[]; v:fixed[5]!i32 = [1,2,3,4,5];"
        " v:matrix[2][2]!f64 = [[1.0,2.0],[3.0,4.0]];"
        " v:computed[2+3]!i32 = [1,2,3,4,5];"
        " v:modulo_extent[17%5]!i32 = [1,2];"
        " consume(dynamic,fixed,computed); consume_matrix(matrix); }";
    const char* zero_extent = "f:main() { v:invalid[0]!i32; }";
    const char* fractional_extent = "f:main() { v:invalid[1.5]!i32; }";
    const char* zero_expression = "f:main() { v:invalid[3-3]!i32; }";
    const char* division_by_zero = "f:main() { v:invalid[8/0]!i32; }";
    const char* overflow_extent = "f:main() { v:invalid[9223372036854775807+1]!i32; }";
    const char* negative_extent = "f:main() { v:invalid[-1]!i32; }";
    const char* wrong_shape = "f:main() { v:fixed[3]!i32 = [1,2]; }";
    const char* wrong_element = "f:main() { v:fixed[2]!i32 = [1.5,2.5]; }";
    const char* call_element_mismatch =
        "f:take(values:i32[4]) {} f:main() { v:wrong[4]!f32; take(wrong); }";
    const char* call_extent_mismatch =
        "f:take(values:i32[4]) {} f:main() { v:wrong[5]!i32; take(wrong); }";
    const char* call_rank_mismatch =
        "f:take(values:i32[2][2]) {} f:main() { v:wrong[4]!i32; take(wrong); }";
    const char* call_literal_valid =
        "f:take(values:i32[2]) {} f:main() { take([1,2]); }";
    const char* call_literal_element_mismatch =
        "f:take(values:i32[1]) {} f:main() { take([1.5]); }";
    const char* valid_index =
        "f:main() { v:items[4]!i32; v:x = items[1+2]; items[0] = x; }";
    const char* valid_narrow_integer_indices =
        "f:main() { v:items[4]!i32; v:small!i8; v:unsigned!u16; "
        "v:a = items[small]; v:b = items[unsigned]; }";
    const char* valid_inferred_array_index =
        "f:main() { v:items = [1,2]; v:first = items[0]; }";
    const char* undefined_index_expression =
        "f:main() { v:items[4]!i32; v:x = items[missing]; }";
    const char* non_integer_index_variable =
        "f:main() { v:items[4]!i32; v:index!f64 = 1.0; v:x = items[index]; }";
    const char* invalid_index_target = "f:main() { v:x!i32 = 2; v:y = x[0]; }";
    const char* invalid_index_type = "f:main() { v:x!i32 = 2; v:y = x[1.5]; }";
    const char* const_array_index_write = "f:main() { c:items[2]!i32 = [1,2]; items[0] = 3; }";
    const char* indexed_element_type_mismatch = "f:main() { v:items[2]!i32 = [1,2]; items[0] = \"x\"; }";
    const char* non_lvalue_assignment = "f:main() { v:x!i32 = 1; (x + 1) = 2; }";
    return check_source(valid_params,0) &&
        check_source(valid_matrix_param,0) &&
        check_source(valid,0) &&
        check_source(zero_extent,1) &&
        check_source(fractional_extent,1) &&
        check_source(zero_expression,1) &&
        check_source(division_by_zero,2) &&
        check_source(overflow_extent,1) &&
        check_source(negative_extent,1) &&
        check_source(wrong_shape,1) &&
        check_source(wrong_element,2) &&
        check_source(call_element_mismatch,1) &&
        check_source(call_extent_mismatch,1) &&
        check_source(call_rank_mismatch,1) &&
        check_source(call_literal_valid,0) &&
        check_source(call_literal_element_mismatch,1) &&
        check_source(valid_index,0) &&
        check_source(valid_narrow_integer_indices,0) &&
        check_source(valid_inferred_array_index,0) &&
        check_source(undefined_index_expression,1) &&
        check_source(non_integer_index_variable,1) &&
        check_source(invalid_index_target,1) &&
        check_source(invalid_index_type,2) &&
        check_source(const_array_index_write,1) &&
        check_source(indexed_element_type_mismatch,1) &&
        check_source(non_lvalue_assignment,1);
}

static int check_extended_ast_semantics(void) {
    const char* valid =
        "f:main() {"
        " v:choice = 1;"
        " select(choice) { Range(s,e) when s == e -> { v:seen = s; } default -> { } }"
        " scan(choice) { Range(a,b) when a == b && b == a -> { v:seen2 = b; } else -> { } }"
        " rules PrimeBorders(n) {"
        "  n < 2 :=> \"out of range\";"
        "  n == 2 -> Success;"
        "  always -> true;"
        "  default -> false;"
        " }"
        " apply rules(PrimeBorders(2));"
        " on PrimeBorders.Success { v:handled = true; }"
        " v:callback = f:(x) > x;"
        " v:callback_block = f:(y) { ret y; };"
        " v:data = 1;"
        " unroll(3) -> loop(item in data) { v:copy = item; }"
        " v:src!i32 = 1; v:dst!i32 = 2; v:moved = src _> dst;"
        "}";
    const char* leaked_pattern =
        "f:main() { v:choice = 1; select(choice) { Range(s,e) -> { } } v:leak = s; }";
    const char* unknown_apply =
        "f:main() { apply rules(Missing(1)); }";
    const char* unknown_handler =
        "f:main() { on Missing.Success { } }";
    const char* unknown_event =
        "f:main() { rules R() { true -> Success; } on R.Missing { } }";
    const char* duplicate_rule_event =
        "f:main() { rules R() { true -> Same; false -> Same; } }";
    const char* duplicate_pattern =
        "f:main() { v:choice = 1; select(choice) { Range(s,s) -> { } } }";
    const char* invalid_guard =
        "f:main() { v:choice = 1; scan(choice) { Range(a,b) when 1 -> { } } }";
    const char* select_default_not_last =
        "f:main() { v:choice = 1; select(choice) { default -> { } 1 -> { } } }";
    const char* valid_scan_else_last =
        "f:main() { v:choice = 1; scan(choice) { Range(a,b) -> { } else -> { } } }";
    const char* valid_named_arguments =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(b:2,a:1); }";
    const char* invalid_named_argument =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(missing:2,a:1); }";
    const char* valid_variadic_call =
        "f:log(first!i32,...)!void { ret; } "
        "f:main() { log(1,2,3); }";
    const char* invalid_variadic_missing_fixed =
        "f:log(first!i32,...)!void { ret; } f:main() { log(); }";
    const char* valid_callback_newtype =
        "nt:Callback = f:(id!u32)!bool; f:main() { v:callback!Callback = null; }";
    const char* invalid_newtype_target =
        "nt:Bad = MissingType; f:main() {}";
    const char* valid_newtype_alias_chain =
        "nt:Base = u64; nt:Alias = Base; f:main() { v:value!Alias; }";
    const char* valid_newtype_identity_assignment =
        "nt:UserId = u64; f:main() { v:first!UserId; v:second!UserId = first; }";
    const char* invalid_newtype_base_assignment =
        "nt:UserId = u64; f:main() { v:raw!u64; v:id!UserId = raw; }";
    const char* invalid_distinct_newtype_assignment =
        "nt:UserId = u64; nt:OrderId = u64; "
        "f:main() { v:user!UserId; v:order!OrderId = user; }";
    const char* valid_newtype_function_argument =
        "nt:UserId = u64; f:accept(id!UserId)!void { ret; } "
        "f:main() { v:id!UserId; accept(id); }";
    const char* invalid_newtype_base_function_argument =
        "nt:UserId = u64; f:accept(id!UserId)!void { ret; } "
        "f:main() { v:raw!u64; accept(raw); }";
    const char* valid_newtype_function_return =
        "nt:UserId = u64; f:make() !UserId { v:id!UserId; ret id; } "
        "f:main() { v:id!UserId = make(); }";
    const char* invalid_newtype_base_function_return =
        "nt:UserId = u64; f:make() !UserId { v:raw!u64; ret raw; } "
        "f:main() {}";
    const char* invalid_newtype_target_not_registered =
        "nt:Bad = MissingType; f:main() { v:value!Bad; }";
    const char* valid_recursive_type_exprs =
        "nt:Pointer = *i32; nt:Reference = &i32; "
        "nt:Callback = f:(id!u32)!bool; f:main() {}";
    const char* invalid_recursive_pointer_type =
        "nt:BadPointer = *MissingType; f:main() {}";
    const char* invalid_recursive_function_parameter =
        "nt:BadCallback = f:(id!MissingType)!bool; f:main() {}";
    const char* invalid_recursive_function_return =
        "nt:BadReturn = f:(id!u32)!MissingType; f:main() {}";
    const char* duplicate_recursive_function_parameter =
        "nt:BadCallback = f:(value!i32,value!u32)!bool; f:main() {}";
    const char* invalid_recursive_variable_type =
        "f:main(value!*MissingType) {}";
    const char* invalid_recursive_array_element =
        "f:main(value!MissingType[4]) {}";
    const char* valid_recursive_aggregate_field =
        "f:main() { struct:Box { v:value!*i32 } }";
    const char* invalid_recursive_aggregate_field =
        "f:main() { struct:Box { v:value!*MissingType } }";
    const char* invalid_recursive_union_field =
        "f:main() { union:Choice { v:payload!*MissingType } }";
    const char* invalid_recursive_enum_payload =
        "nt:enum: Result { Value(value!MissingType) } f:main() {}";
    const char* invalid_function_decl_return_type_with_value =
        "f:main() !MissingType { ret 1; }";
    const char* valid_inferred_array =
        "f:main() { v:items = [1,2,3]; }";
    const char* invalid_heterogeneous_array =
        "f:main() { v:items = [1,\"x\"]; }";
    const char* valid_tuple_indexing =
        "f:main() { v:pair = (1, \"x\"); "
        "v:first!i32 = pair[0]; v:second!str = pair[1]; }";
    const char* invalid_tuple_index =
        "f:main() { v:pair = (1, \"x\"); v:bad = pair[2]; }";
    const char* inferred_generic_identity =
        "f:identity(value!t)!t { ret value; } "
        "f:main() { v:result!i32 = identity(7); }";
    const char* inferred_generic_sum =
        "f:sum(a!t,b!t)!t { ret a + b; } "
        "f:main() { v:result!i32 = sum(2,3); }";
    const char* inferred_select_binding =
        "f:main() { v:choice!i32 = 1; select(choice) { captured -> { "
        "v:copy!i32 = captured; } } }";
    const char* mismatched_select_binding =
        "f:main() { v:choice!i32 = 1; select(choice) { captured -> { "
        "v:copy!str = captured; } } }";
    const char* inferred_scan_binding =
        "f:main() { v:choice!i32 = 1; scan(choice) { captured -> { "
        "v:copy!i32 = captured; } } }";
    const char* mismatched_scan_binding =
        "f:main() { v:choice!i32 = 1; scan(choice) { captured -> { "
        "v:copy!str = captured; } } }";
    const char* relocate_type_mismatch =
        "f:main() { v:src!i32 = 1; v:dst!str = \"x\"; v:moved = src _> dst; }";
    return check_source(valid,0) &&
        check_source(leaked_pattern,1) &&
        check_source(unknown_apply,1) &&
        check_source(unknown_handler,1) &&
        check_source(unknown_event,1) &&
        check_source(duplicate_rule_event,1) &&
        check_source(duplicate_pattern,1) &&
        check_source(invalid_guard,1) &&
        check_source(select_default_not_last,1) && 
        check_source(valid_scan_else_last,0) &&
        check_source(valid_named_arguments,0) &&
        check_source(invalid_named_argument,1) &&
        check_source(valid_variadic_call,0) &&
        check_source(invalid_variadic_missing_fixed,1) &&
        check_source(valid_callback_newtype,0) &&
        check_source(invalid_newtype_target,1) &&
        check_source(valid_newtype_alias_chain,0) &&
        check_source(valid_newtype_identity_assignment,0) &&
        check_source(invalid_newtype_base_assignment,1) &&
        check_source(invalid_distinct_newtype_assignment,1) &&
        check_source(valid_newtype_function_argument,0) &&
        check_source(invalid_newtype_base_function_argument,1) &&
        check_source(valid_newtype_function_return,0) &&
        check_source(invalid_newtype_base_function_return,1) &&
        check_source(invalid_newtype_target_not_registered,2) &&
        check_source(valid_recursive_type_exprs,0) &&
        check_source(invalid_recursive_pointer_type,1) &&
        check_source(invalid_recursive_function_parameter,1) &&
        check_source(invalid_function_decl_return_type_with_value,1) &&
        check_source(invalid_recursive_variable_type,1) &&
        check_source(invalid_recursive_array_element,1) &&
        check_source(valid_recursive_aggregate_field,0) &&
        check_source(invalid_recursive_aggregate_field,1) &&
        check_source(invalid_recursive_union_field,1) &&
        check_source(invalid_recursive_enum_payload,1) &&
        check_source(invalid_recursive_function_return,1) &&
        check_source(duplicate_recursive_function_parameter,1) &&
        check_source(valid_inferred_array,0) &&
        check_source(invalid_heterogeneous_array,1) &&
        check_source(valid_tuple_indexing,0) &&
        check_source(invalid_tuple_index,1) &&
        check_source(inferred_generic_identity,0) &&
        check_source(inferred_generic_sum,0) &&
        check_source(inferred_select_binding,0) &&
        check_source(mismatched_select_binding,1) &&
        check_source(inferred_scan_binding,0) &&
        check_source(mismatched_scan_binding,1) &&
        check_source(relocate_type_mismatch,1);
}


static char* semantic_test_copy_text(const char* value) {
    size_t length;
    char* copy;
    if (!value) return NULL;
    length = strlen(value);
    copy = (char*)malloc(length + 1u);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}


static int check_aggregate_field_ast_case(
    ASTNode* node, int expected_errors, const char* expected_message
) {
    SemanticContext semantic;
    int ok;
    if (!node) return 0;
    semantic_init(&semantic, "semantic-aggregate-fields.nxf", NULL);
    semantic_check(&semantic, node);
    ok = semantic.error_count == expected_errors;
    if (expected_message) {
        int found = 0;
        Error* error;
        for (error = semantic.error_head; error; error = error->next) {
            if (strstr(error->message, expected_message) != NULL) found = 1;
        }
        ok = ok && found;
    }
    semantic_destroy(&semantic);
    ast_destroy(node);
    return ok;
}

static int check_aggregate_field_type_semantics(void) {
    ASTStructDecl* structure = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
    ASTVarDecl* struct_field = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTStructDecl* union_node = (ASTStructDecl*)ast_create(AST_UNION_DECL);
    ASTVarDecl* union_field = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTStructDecl* malformed = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
    ASTNode* malformed_field = ast_create(AST_LITERAL);
    if (!structure || !struct_field || !union_node || !union_field ||
        !malformed || !malformed_field) {
        ast_destroy((ASTNode*)structure);
        ast_destroy((ASTNode*)struct_field);
        ast_destroy((ASTNode*)union_node);
        ast_destroy((ASTNode*)union_field);
        ast_destroy((ASTNode*)malformed);
        ast_destroy(malformed_field);
        return 0;
    }

    structure->name = semantic_test_copy_text("UntypedStruct");
    struct_field->name = semantic_test_copy_text("value");
    structure->fields = (ASTNode*)struct_field;
    if (!structure->name || !struct_field->name ||
        !check_aggregate_field_ast_case((ASTNode*)structure, 1,
            "Aggregate field 'value' requires an explicit type")) return 0;

    union_node->name = semantic_test_copy_text("UntypedUnion");
    union_node->is_union = 1;
    union_field->name = semantic_test_copy_text("value");
    union_node->fields = (ASTNode*)union_field;
    if (!union_node->name || !union_field->name ||
        !check_aggregate_field_ast_case((ASTNode*)union_node, 1,
            "Aggregate field 'value' requires an explicit type")) return 0;

    malformed->name = semantic_test_copy_text("MalformedAggregate");
    malformed->fields = malformed_field;
    if (!malformed->name ||
        !check_aggregate_field_ast_case((ASTNode*)malformed, 1,
            "Aggregate field list contains a non-variable declaration node")) return 0;
    return 1;
}


static int check_enum_scan_pattern_case(
    int test_case, int expected_errors, const char* expected_binding_type
) {
    SemanticContext semantic;
    ASTEnumDecl* enum_decl=(ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    ASTEnumVariant* payload_variant=(ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    ASTEnumVariant* empty_variant=(ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    ASTVarDecl* payload_field=(ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTVarDecl* subject_decl=(ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTScan* scan=(ASTScan*)ast_create(AST_SCAN);
    ASTScanArm* arm=(ASTScanArm*)ast_create(AST_SCAN_ARM);
    ASTScanArm* empty_arm=(ASTScanArm*)ast_create(AST_SCAN_ARM);
    ASTPatternBinding* empty_pattern=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
    ASTIdentifier* subject=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTPatternConstructor* constructor=(ASTPatternConstructor*)ast_create(AST_PATTERN_CONSTRUCTOR);
    ASTPatternBinding* binding_a=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
    ASTPatternBinding* binding_b=NULL;
    int initialized=0,ok=0;
    if(!enum_decl||!payload_variant||!empty_variant||!payload_field||
       !subject_decl||!scan||!arm||!empty_arm||!empty_pattern||
       !subject||!constructor||!binding_a)goto cleanup;
    enum_decl->name=semantic_test_copy_text("ScanResult");
    payload_variant->name=semantic_test_copy_text("Ok");
    empty_variant->name=semantic_test_copy_text("Empty");
    payload_field->name=semantic_test_copy_text("value");
    payload_field->type_name=semantic_test_copy_text("i32");
    subject_decl->name=semantic_test_copy_text("scan_value");
    subject_decl->type_name=semantic_test_copy_text(test_case==4?"i32":"ScanResult");
    subject->name=semantic_test_copy_text("scan_value");
    empty_pattern->name=semantic_test_copy_text("Empty");
    constructor->name=semantic_test_copy_text(test_case==1?"Missing":"Ok");
    binding_a->name=semantic_test_copy_text("captured");
    if(!enum_decl->name||!payload_variant->name||!empty_variant->name||
       !payload_field->name||!payload_field->type_name||!subject_decl->name||
       !subject_decl->type_name||!subject->name||!empty_pattern->name||
       !constructor->name||!binding_a->name)goto cleanup;
    payload_variant->data=(ASTNode*)payload_field;payload_field=NULL;
    payload_variant->base.next=(ASTNode*)empty_variant;empty_variant=NULL;
    enum_decl->variants=(ASTNode*)payload_variant;payload_variant=NULL;
    if(test_case==3) {
        ASTPatternBinding* bare=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
        if(!bare)goto cleanup;
        bare->name=semantic_test_copy_text("Ok");
        if(!bare->name){ast_destroy((ASTNode*)bare);goto cleanup;}
        arm->pattern=(ASTNode*)bare;
    } else {
        if(test_case==2) {
            binding_b=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
            if(!binding_b)goto cleanup;
            binding_b->name=semantic_test_copy_text("extra");
            if(!binding_b->name)goto cleanup;
            binding_a->base.next=(ASTNode*)binding_b;binding_b=NULL;
        }
        constructor->bindings=(ASTNode*)binding_a;binding_a=NULL;
        arm->pattern=(ASTNode*)constructor;constructor=NULL;
    }
    scan->subject=(ASTNode*)subject;subject=NULL;
    arm->body=ast_create(AST_BLOCK);
    empty_arm->pattern=(ASTNode*)empty_pattern;empty_pattern=NULL;
    empty_arm->body=ast_create(AST_BLOCK);
    if(!arm->body||!empty_arm->body)goto cleanup;
    if(expected_binding_type) {
        ASTVarDecl* typed_use=(ASTVarDecl*)ast_create(AST_VAR_DECL);
        ASTIdentifier* captured_use=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
        if(!typed_use||!captured_use) {
            ast_destroy((ASTNode*)typed_use);ast_destroy((ASTNode*)captured_use);
            goto cleanup;
        }
        typed_use->name=semantic_test_copy_text("captured_type_check");
        typed_use->type_name=semantic_test_copy_text(expected_binding_type);
        captured_use->name=semantic_test_copy_text("captured");
        if(!typed_use->name||!typed_use->type_name||!captured_use->name) {
            ast_destroy((ASTNode*)typed_use);ast_destroy((ASTNode*)captured_use);
            goto cleanup;
        }
        typed_use->initializer=(ASTNode*)captured_use;
        ((ASTBlock*)arm->body)->statements=(ASTNode*)typed_use;
    }
    if(test_case==5) {
        ASTScanArm* duplicate=(ASTScanArm*)ast_create(AST_SCAN_ARM);
        ASTPatternBinding* duplicate_pattern=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
        if(!duplicate||!duplicate_pattern) {
            ast_destroy((ASTNode*)duplicate);ast_destroy((ASTNode*)duplicate_pattern);
            goto cleanup;
        }
        duplicate_pattern->name=semantic_test_copy_text("Empty");
        duplicate->pattern=(ASTNode*)duplicate_pattern;
        duplicate->body=ast_create(AST_BLOCK);
        if(!duplicate_pattern->name||!duplicate->body) {
            ast_destroy((ASTNode*)duplicate);goto cleanup;
        }
        empty_arm->base.next=(ASTNode*)duplicate;
    }
    if(test_case==6)empty_arm->has_guard=1;
    if(test_case==7) {
        ast_destroy(empty_arm->pattern);
        empty_arm->pattern=NULL;
    }
    arm->base.next=(ASTNode*)empty_arm;empty_arm=NULL;
    scan->arms=(ASTNode*)arm;arm=NULL;
    semantic_init(&semantic,"semantic-enum-scan-pattern.nxf",NULL);
    initialized=1;
    semantic_check(&semantic,(ASTNode*)enum_decl);
    semantic_check(&semantic,(ASTNode*)subject_decl);
    semantic_check(&semantic,(ASTNode*)scan);
    ok=semantic.error_count==expected_errors;
    if(!ok) {
        Error* detail;
        fprintf(stderr,"Enum scan fixture %d: expected %d errors, got %d\\n",
            test_case,expected_errors,semantic.error_count);
        for(detail=semantic.error_head;detail;detail=detail->next)
            fprintf(stderr,"  %d:%d %s\\n",detail->line,detail->col,
                detail->message?detail->message:"<none>");
    }
cleanup:
    if(initialized)semantic_destroy(&semantic);
    ast_destroy((ASTNode*)enum_decl);ast_destroy((ASTNode*)payload_variant);
    ast_destroy((ASTNode*)empty_variant);ast_destroy((ASTNode*)payload_field);
    ast_destroy((ASTNode*)subject_decl);ast_destroy((ASTNode*)scan);
    ast_destroy((ASTNode*)arm);ast_destroy((ASTNode*)empty_arm);
    ast_destroy((ASTNode*)empty_pattern);ast_destroy((ASTNode*)subject);
    ast_destroy((ASTNode*)constructor);ast_destroy((ASTNode*)binding_a);
    ast_destroy((ASTNode*)binding_b);
    return ok;
}

static int check_enum_scan_pattern_semantics(void) {
    return check_enum_scan_pattern_case(0,0,"i32") &&
        check_enum_scan_pattern_case(1,2,NULL) &&
        check_enum_scan_pattern_case(2,1,NULL) &&
        check_enum_scan_pattern_case(3,2,NULL) &&
        check_enum_scan_pattern_case(4,1,NULL) &&
        check_enum_scan_pattern_case(5,1,NULL) &&
        check_enum_scan_pattern_case(6,1,NULL) &&
        check_enum_scan_pattern_case(7,0,NULL);
}

static int check_enum_ast_case(
    ASTNode* node, int expected_errors, const char* expected_message
) {
    SemanticContext semantic;
    int ok;
    if (!node) return 0;
    semantic_init(&semantic, "semantic-enum-validation.nxf", NULL);
    semantic_check(&semantic, node);
    ok = semantic.error_count == expected_errors;
    if (expected_message) {
        int found = 0;
        Error* error;
        for (error = semantic.error_head; error; error = error->next) {
            if (strstr(error->message, expected_message) != NULL) found = 1;
        }
        ok = ok && found;
    }
    semantic_destroy(&semantic);
    ast_destroy(node);
    return ok;
}


static int check_enum_constructor_case(int test_case,int expected_errors) {
    SemanticContext semantic;
    ASTEnumDecl* decl=(ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    ASTEnumVariant* variant=(ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    ASTVarDecl* field=(ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTVarDecl* arg_decl=(ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTCall* call=(ASTCall*)ast_create(AST_FUNC_CALL);
    ASTMemberAccess* callee=(ASTMemberAccess*)ast_create(AST_MEMBER_ACCESS);
    ASTIdentifier* type_id=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTIdentifier* arg=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTNamedArg* named=NULL;
    int ok=0, initialized=0;
    if(!decl||!variant||!field||!arg_decl||!call||!callee||!type_id||!arg)goto done;
    decl->name=semantic_test_copy_text("BuildState");
    variant->name=semantic_test_copy_text("Running");
    field->name=semantic_test_copy_text("value");
    field->type_name=semantic_test_copy_text("i32");
    arg_decl->name=semantic_test_copy_text("actual");
    arg_decl->type_name=semantic_test_copy_text(test_case==4?"f64":"i32");
    type_id->name=semantic_test_copy_text("BuildState");
    arg->name=semantic_test_copy_text("actual");
    callee->member_name=semantic_test_copy_text(test_case==1?"Missing":"Running");
    if(!decl->name||!variant->name||!field->name||!field->type_name||
       !arg_decl->name||!arg_decl->type_name||!type_id->name||!arg->name||
       !callee->member_name)goto done;
    variant->data=(ASTNode*)field;field=NULL;
    decl->variants=(ASTNode*)variant;variant=NULL;
    callee->object=(ASTNode*)type_id;type_id=NULL;
    call->callee=(ASTNode*)callee;callee=NULL;
    if(test_case==2) {
        /* Missing payload is the arity-negative fixture. */
    } else if(test_case==3) {
        ASTLiteral* literal=(ASTLiteral*)ast_create(AST_LITERAL);
        if(!literal)goto done;
        literal->type=TOKEN_STRING;literal->lit_type=2;
        literal->string_value=semantic_test_copy_text("\"wrong\"");
        if(!literal->string_value){ast_destroy((ASTNode*)literal);goto done;}
        call->args=(ASTNode*)literal;
    } else if(test_case==5||test_case==6) {
        named=(ASTNamedArg*)ast_create(AST_NAMED_ARG);
        if(!named)goto done;
        named->name=semantic_test_copy_text(test_case==5?"unknown":"value");
        named->value=(ASTNode*)arg;arg=NULL;
        if(!named->name)goto done;
        call->args=(ASTNode*)named;named=NULL;
    } else { call->args=(ASTNode*)arg;arg=NULL; }
    semantic_init(&semantic,"semantic-enum-constructor.nxf",NULL);
    initialized=1;
    semantic_check(&semantic,(ASTNode*)decl);
    semantic_check(&semantic,(ASTNode*)arg_decl);
    semantic_check(&semantic,(ASTNode*)call);
    ok=semantic.error_count==expected_errors;
done:
    if(initialized)semantic_destroy(&semantic);
    ast_destroy((ASTNode*)decl);ast_destroy((ASTNode*)variant);
    ast_destroy((ASTNode*)field);ast_destroy((ASTNode*)arg_decl);
    ast_destroy((ASTNode*)call);ast_destroy((ASTNode*)callee);
    ast_destroy((ASTNode*)type_id);ast_destroy((ASTNode*)arg);
    ast_destroy((ASTNode*)named);
    return ok;
}
static int check_enum_constructor_semantics(void) {
    return check_enum_constructor_case(0,0)&&
        check_enum_constructor_case(1,1)&&
        check_enum_constructor_case(2,1)&&
        check_enum_constructor_case(3,1)&&
        check_enum_constructor_case(4,1)&&
        check_enum_constructor_case(5,2)&&
        check_enum_constructor_case(6,0);
}

static int check_enum_discriminant_resolved_values(void) {
    static const char source[] =
        "nt:enum: State { First, Explicit = 4, AfterExplicit, "
        "Expression = 8 + 1, AfterExpression }";
    ParserContext* parser = parser_init_source(
        "enum-discriminant-values.nxf", (const uint8_t*)source,
        sizeof(source) - 1u);
    SemanticContext semantic;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    ASTEnumDecl* declaration;
    ASTEnumVariant* variant;
    static const int64_t expected[] = { 0, 4, 5, 9, 10 };
    size_t index = 0u;
    int semantic_initialized = 0;
    int ok = 0;
    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK || !parser->ast_root)
        goto done;
    semantic_init(&semantic, "enum-discriminant-values.nxf", NULL);
    semantic_initialized = 1;
    semantic_check(&semantic, parser->ast_root);
    declaration = (ASTEnumDecl*)((ASTProgram*)parser->ast_root)->declarations;
    variant = declaration ? (ASTEnumVariant*)declaration->variants : NULL;
    ok = semantic.error_count == 0;
    while (ok && variant && index < sizeof(expected) / sizeof(expected[0])) {
        ok = variant->has_resolved_discriminant &&
            variant->discriminant_value == expected[index];
        variant = (ASTEnumVariant*)variant->base.next;
        ++index;
    }
    ok = ok && index == sizeof(expected) / sizeof(expected[0]) &&
        variant == NULL;
    if (!ok) goto done;
    stream = tmpfile();
    if (!stream || !ast_serialize(parser->ast_root, stream) ||
        fseek(stream, 0L, SEEK_SET) != 0)
        { ok = 0; goto done; }
    decoded = ast_deserialize(stream);
    if (!decoded || !ast_validate(decoded, NULL)) { ok = 0; goto done; }
    declaration = (ASTEnumDecl*)((ASTProgram*)decoded)->declarations;
    variant = declaration ? (ASTEnumVariant*)declaration->variants : NULL;
    index = 0u;
    while (variant && index < sizeof(expected) / sizeof(expected[0])) {
        if (!variant->has_resolved_discriminant ||
            variant->discriminant_value != expected[index]) {
            ok = 0;
            break;
        }
        variant = (ASTEnumVariant*)variant->base.next;
        ++index;
    }
    ok = ok && index == sizeof(expected) / sizeof(expected[0]) &&
        variant == NULL;
done:
    if (stream) fclose(stream);
    if (decoded) ast_destroy(decoded);
    if (semantic_initialized) semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_enum_discriminant_semantics(void) {
    const char* valid_implicit_explicit =
        "f:main() { enum:State { Idle, Running = 4, Done } }";
    const char* valid_negative_and_expression =
        "f:main() { enum:State { Before = -3, Current = 2 + 3, Next } }";
    const char* duplicate_explicit =
        "f:main() { enum:State { A = 7, B = 7 } }";
    const char* duplicate_implicit_after_explicit =
        "f:main() { enum:State { A = 1, B, C = 2 } }";
    const char* non_constant =
        "f:main() { enum:State { A = missing } }";
    const char* non_integer =
        "f:main() { enum:State { A = 1.5 } }";
    const char* implicit_overflow =
        "f:main() { enum:State { Max = 9223372036854775807, Next } }";
    const char* explicit_reset_after_max =
        "f:main() { enum:State { Max = 9223372036854775807, Reset = 0, Next } }";
    const char* signed_minimum =
        "f:main() { enum:State { Min = -9223372036854775808, Reset = 0, Next } }";
    const char* valid_division_remainder =
        "f:main() { enum:State { Divided = 21 / 3, Remainder = 17 % 5, Next } }";
    const char* valid_hexadecimal =
        "f:main() { enum:State { Hex = 0x2A, Next } }";
    const char* arithmetic_overflow =
        "f:main() { enum:State { Overflow = 9223372036854775807 + 1 } }";
    const char* multiplication_overflow =
        "f:main() { enum:State { Overflow = 3037000500 * 3037000500 } }";
    const char* division_by_zero =
        "f:main() { enum:State { Invalid = 1 / 0 } }";
    const char* signed_division_overflow =
        "f:main() { enum:State { Invalid = -9223372036854775808 / -1 } }";
    return check_enum_discriminant_resolved_values() &&
        check_source(valid_implicit_explicit, 0) &&
        check_source(valid_negative_and_expression, 0) &&
        check_source(valid_division_remainder, 0) &&
        check_source(valid_hexadecimal, 0) &&
        check_source(duplicate_explicit, 1) &&
        check_source(duplicate_implicit_after_explicit, 1) &&
        check_source(non_constant, 1) &&
        check_source(non_integer, 1) &&
        check_source(implicit_overflow, 1) &&
        check_source(arithmetic_overflow, 1) &&
        check_source(multiplication_overflow, 1) &&
        check_source(division_by_zero, 1) &&
        check_source(signed_division_overflow, 1) &&
        check_source(explicit_reset_after_max, 0) &&
        check_source(signed_minimum, 0);
}

static int check_enum_declaration_semantics(void) {
    ASTEnumDecl* valid = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    ASTEnumVariant* ok_variant = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    ASTEnumVariant* error_variant = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    ASTVarDecl* ok_payload = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTVarDecl* error_payload = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    ASTEnumDecl* duplicate = NULL;
    ASTEnumVariant* duplicate_a = NULL;
    ASTEnumVariant* duplicate_b = NULL;
    ASTEnumDecl* malformed_variant = NULL;
    ASTEnumDecl* malformed_payload = NULL;
    ASTEnumVariant* payload_variant = NULL;
    ASTVarDecl* payload_a = NULL;
    ASTVarDecl* payload_b = NULL;
    ASTEnumDecl* missing_type = NULL;
    ASTEnumVariant* missing_type_variant = NULL;
    ASTVarDecl* untyped_payload = NULL;
    ASTEnumDecl* missing_enum_name = NULL;
    ASTEnumDecl* missing_variant_name = NULL;
    ASTEnumVariant* unnamed_variant = NULL;
    ASTEnumDecl* duplicate_payload = NULL;
    ASTEnumVariant* duplicate_payload_variant = NULL;
    int ok = 0;

    if (!valid || !ok_variant || !error_variant || !ok_payload || !error_payload)
        goto cleanup_valid;
    valid->name = semantic_test_copy_text("Result");
    ok_variant->name = semantic_test_copy_text("Ok");
    error_variant->name = semantic_test_copy_text("Error");
    ok_payload->name = semantic_test_copy_text("value");
    ok_payload->type_name = semantic_test_copy_text("i32");
    error_payload->name = semantic_test_copy_text("message");
    error_payload->type_name = semantic_test_copy_text("str");
    if (!valid->name || !ok_variant->name || !error_variant->name ||
        !ok_payload->name || !ok_payload->type_name ||
        !error_payload->name || !error_payload->type_name) goto cleanup_valid;
    ok_variant->data = (ASTNode*)ok_payload;
    error_variant->data = (ASTNode*)error_payload;
    ok_variant->base.next = (ASTNode*)error_variant;
    valid->variants = (ASTNode*)ok_variant;
    ok_variant = NULL;
    error_variant = NULL;
    ok_payload = NULL;
    error_payload = NULL;
    if (!check_enum_ast_case((ASTNode*)valid, 0, NULL)) return 0;
    valid = NULL;
    ok = 1;

    duplicate = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    duplicate_a = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    duplicate_b = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    if (!duplicate || !duplicate_a || !duplicate_b) goto cleanup;
    duplicate->name = semantic_test_copy_text("State");
    duplicate_a->name = semantic_test_copy_text("Ready");
    duplicate_b->name = semantic_test_copy_text("Ready");
    if (!duplicate->name || !duplicate_a->name || !duplicate_b->name) goto cleanup;
    duplicate_a->base.next = (ASTNode*)duplicate_b;
    duplicate->variants = (ASTNode*)duplicate_a;
    duplicate_a = NULL;
    duplicate_b = NULL;
    {
        ASTNode* owned = (ASTNode*)duplicate;
        duplicate = NULL;
        if (!check_enum_ast_case(owned, 1,
                "declares variant 'Ready' more than once")) goto cleanup;
    }

    malformed_variant = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    if (!malformed_variant) goto cleanup;
    malformed_variant->name = semantic_test_copy_text("State");
    malformed_variant->variants = ast_create(AST_LITERAL);
    if (!malformed_variant->name || !malformed_variant->variants) goto cleanup;
    {
        ASTNode* owned = (ASTNode*)malformed_variant;
        malformed_variant = NULL;
        if (!check_enum_ast_case(owned, 1,
                "non-variant AST node")) goto cleanup;
    }

    malformed_payload = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    payload_variant = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    if (!malformed_payload || !payload_variant) goto cleanup;
    malformed_payload->name = semantic_test_copy_text("Result");
    payload_variant->name = semantic_test_copy_text("Ok");
    payload_variant->data = ast_create(AST_LITERAL);
    malformed_payload->variants = (ASTNode*)payload_variant;
    payload_variant = NULL;
    if (!malformed_payload->name) goto cleanup;
    {
        ASTNode* owned = (ASTNode*)malformed_payload;
        malformed_payload = NULL;
        if (!check_enum_ast_case(owned, 1,
                "non-field AST node")) goto cleanup;
    }

    missing_type = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    missing_type_variant = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    untyped_payload = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    if (!missing_type || !missing_type_variant || !untyped_payload) goto cleanup;
    missing_type->name = semantic_test_copy_text("Result");
    missing_type_variant->name = semantic_test_copy_text("Ok");
    untyped_payload->name = semantic_test_copy_text("value");
    missing_type_variant->data = (ASTNode*)untyped_payload;
    missing_type->variants = (ASTNode*)missing_type_variant;
    missing_type_variant = NULL;
    untyped_payload = NULL;
    if (!missing_type->name) goto cleanup;
    {
        ASTNode* owned = (ASTNode*)missing_type;
        missing_type = NULL;
        if (!check_enum_ast_case(owned, 1,
                "payload field requires an explicit type")) goto cleanup;
    }

    missing_enum_name = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    if (!missing_enum_name) goto cleanup;
    {
        ASTNode* owned = (ASTNode*)missing_enum_name;
        missing_enum_name = NULL;
        if (!check_enum_ast_case(owned, 1,
                "Enum declaration requires a non-empty type name")) goto cleanup;
    }

    missing_variant_name = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    unnamed_variant = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    if (!missing_variant_name || !unnamed_variant) goto cleanup;
    missing_variant_name->name = semantic_test_copy_text("State");
    missing_variant_name->variants = (ASTNode*)unnamed_variant;
    unnamed_variant = NULL;
    if (!missing_variant_name->name) goto cleanup;
    {
        ASTNode* owned = (ASTNode*)missing_variant_name;
        missing_variant_name = NULL;
        if (!check_enum_ast_case(owned, 1,
                "Enum variant requires a non-empty name")) goto cleanup;
    }

    duplicate_payload = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    duplicate_payload_variant =
        (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    payload_a = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    payload_b = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    if (!duplicate_payload || !duplicate_payload_variant ||
        !payload_a || !payload_b) goto cleanup;
    duplicate_payload->name = semantic_test_copy_text("Result");
    duplicate_payload_variant->name = semantic_test_copy_text("Ok");
    payload_a->name = semantic_test_copy_text("value");
    payload_a->type_name = semantic_test_copy_text("i32");
    payload_b->name = semantic_test_copy_text("value");
    payload_b->type_name = semantic_test_copy_text("i32");
    if (!duplicate_payload->name || !duplicate_payload_variant->name ||
        !payload_a->name || !payload_a->type_name ||
        !payload_b->name || !payload_b->type_name) goto cleanup;
    payload_a->base.next = (ASTNode*)payload_b;
    duplicate_payload_variant->data = (ASTNode*)payload_a;
    duplicate_payload->variants = (ASTNode*)duplicate_payload_variant;
    duplicate_payload_variant = NULL;
    payload_a = NULL;
    payload_b = NULL;
    {
        ASTNode* owned = (ASTNode*)duplicate_payload;
        duplicate_payload = NULL;
        if (!check_enum_ast_case(owned, 1,
                "declares payload field 'value' more than once")) goto cleanup;
    }

    return ok;

cleanup_valid:
    ast_destroy((ASTNode*)valid);
    ast_destroy((ASTNode*)ok_variant);
    ast_destroy((ASTNode*)error_variant);
    ast_destroy((ASTNode*)ok_payload);
    ast_destroy((ASTNode*)error_payload);
cleanup:
    ast_destroy((ASTNode*)duplicate);
    ast_destroy((ASTNode*)duplicate_a);
    ast_destroy((ASTNode*)duplicate_b);
    ast_destroy((ASTNode*)malformed_variant);
    ast_destroy((ASTNode*)malformed_payload);
    ast_destroy((ASTNode*)payload_variant);
    ast_destroy((ASTNode*)missing_type);
    ast_destroy((ASTNode*)missing_type_variant);
    ast_destroy((ASTNode*)untyped_payload);
    ast_destroy((ASTNode*)missing_enum_name);
    ast_destroy((ASTNode*)missing_variant_name);
    ast_destroy((ASTNode*)unnamed_variant);
    ast_destroy((ASTNode*)duplicate_payload);
    ast_destroy((ASTNode*)duplicate_payload_variant);
    ast_destroy((ASTNode*)payload_a);
    ast_destroy((ASTNode*)payload_b);
    return 0;
}

static int check_malformed_group_method_ast(void) {
    ASTMethod* missing_name = (ASTMethod*)ast_create(AST_METHOD);
    ASTMethod* missing_implementation = (ASTMethod*)ast_create(AST_METHOD);
    ASTMethod* invalid_implementation = (ASTMethod*)ast_create(AST_METHOD);
    SemanticContext semantic;
    int ok = 0;
    if (!missing_name || !missing_implementation || !invalid_implementation)
        goto cleanup;
    missing_name->implementation = ast_create(AST_FUNC_DECL);
    if (!missing_name->implementation) goto cleanup;
    ((ASTFuncDecl*)missing_name->implementation)->name = semantic_test_copy_text("implementation");
    ((ASTFuncDecl*)missing_name->implementation)->body = ast_create(AST_BLOCK);
    if (!((ASTFuncDecl*)missing_name->implementation)->name ||
        !((ASTFuncDecl*)missing_name->implementation)->body) goto cleanup;
    missing_implementation->public_name = semantic_test_copy_text("open");
    invalid_implementation->public_name = semantic_test_copy_text("open");
    invalid_implementation->implementation = ast_create(AST_BLOCK);
    if (!missing_implementation->public_name || !invalid_implementation->public_name ||
        !invalid_implementation->implementation) goto cleanup;

    semantic_init(&semantic, "semantic-group-method-ast.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)missing_name);
    ok = semantic.error_count == 1 && semantic.error_head &&
        strstr(semantic.error_head->message, "non-empty public name") != NULL;
    semantic_destroy(&semantic);
    if (!ok) goto cleanup;

    semantic_init(&semantic, "semantic-group-method-ast.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)missing_implementation);
    ok = semantic.error_count == 1 && semantic.error_head &&
        strstr(semantic.error_head->message, "missing its implementation") != NULL;
    semantic_destroy(&semantic);
    if (!ok) goto cleanup;

    semantic_init(&semantic, "semantic-group-method-ast.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)invalid_implementation);
    ok = semantic.error_count == 1 && semantic.error_head &&
        strstr(semantic.error_head->message, "must be a function or extern declaration") != NULL;
    semantic_destroy(&semantic);

cleanup:
    ast_destroy((ASTNode*)missing_name);
    ast_destroy((ASTNode*)missing_implementation);
    ast_destroy((ASTNode*)invalid_implementation);
    return ok;
}
static int check_group_self_receiver_semantics(void) {
    const char* inferred_self_before_struct =
        "f:main() { group Storage { "
        "open => f:open_impl(self) { v:value!i32 = self.value; ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* explicit_self_after_struct =
        "f:main() { group Storage { struct:Storage { v:value!i32 } "
        "open => f:open_impl(self!Storage) { v:value!i32 = self.value; ret; } } }";
    const char* static_method_without_self =
        "f:main() { group Storage { struct:Storage { v:value!i32 } "
        "parse => f:parse_impl(text!str) { ret; } } }";
    const char* wrong_explicit_self_type =
        "f:main() { group Storage { struct:Storage { v:value!i32 } "
        "struct:Other { v:other!i32 } "
        "open => f:open_impl(self!Other) { ret; } } }";
    const char* self_not_first =
        "f:main() { group Storage { struct:Storage { v:value!i32 } "
        "open => f:open_impl(value!i32,self) { ret; } } }";
    const char* namespace_group_self =
        "f:main() { group Namespace { run => f:run_impl(self) { ret; } } }";
    const char* enum_group_self =
        "f:main() { group State { enum: State { Ready } "
        "run => f:run_impl(self) { ret; } } }";

    return check_source(inferred_self_before_struct, 0) &&
        check_source(explicit_self_after_struct, 0) &&
        check_source(static_method_without_self, 0) &&
        check_source(wrong_explicit_self_type, 1) &&
        check_source(self_not_first, 1) &&
        check_source(namespace_group_self, 1) &&
        check_source(enum_group_self, 1);
}

static int check_group_method_dispatch_semantics(void) {
    const char* instance_call_to_later_method =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(7); ret; } "
        "store_value => f:store_value_impl(self,value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* anonymous_instance_method_implementation =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.open(); ret; } "
        "open => f:(self) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* anonymous_static_factory_returning_bound_type =
        "f:main() { group Storage { "
        "create => f:(instance!Storage)!Storage { ret instance; } "
        "run => f:run_impl(instance!Storage) { "
        "v:created!Storage = Storage.create(instance); ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* static_factory_returns_struct_literal =
        "f:main() { group Point { "
        "create => f:(x!i32)!Point { ret Point { x = x }; } "
        "run => f:run_impl() { v:p!Point = Point.create(7); ret; } "
        "struct:Point { v:x!i32 } } }";
    const char* type_qualified_static_call =
        "f:main() { group Storage { "
        "run => f:run_impl() { Storage.parse(\"x\"); ret; } "
        "parse => f:parse_impl(text!str) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* namespace_static_call =
        "f:main() { group Namespace { "
        "run => f:run_impl() { Namespace.parse(\"x\"); ret; } "
        "parse => f:parse_impl(text!str) { ret; } } }";
    const char* bare_group_expression_dispatches_default =
        "f:main() { group Namespace { default { } } Namespace; ret; }";
    const char* bare_group_expression_requires_default =
        "f:main() { group Namespace { f:helper() { ret; } } Namespace; ret; }";
    const char* direct_group_function_call =
        "f:main() { group Namespace { "
        "run => f:run_impl() { Namespace.helper(7); ret; } "
        "f:helper(value!i32) { ret; } } }";
    const char* direct_group_function_called_from_enclosing_scope =
        "f:main() { group Namespace { f:helper(value!i32) { ret; } } "
        "Namespace.helper(7); ret; }";
    const char* method_alias_called_from_enclosing_scope =
        "f:main() { group Namespace { "
        "parse => f:parse_impl(text!str) { ret; } } "
        "Namespace.parse(\"x\"); ret; }";
    const char* group_implementation_name_does_not_leak =
        "f:main() { group Namespace { f:helper() { ret; } } "
        "helper(); ret; }";
    const char* non_binding_group_type_has_no_instance_dispatch =
        "f:main() { group Storage { "
        "struct:Other { v:other!i32 } "
        "run => f:run_impl() { v:other!Other; other.open(); ret; } "
        "open => f:open_impl(self) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* instance_call_to_static_method =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { self.parse(\"x\"); ret; } "
        "parse => f:parse_impl(text!str) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* type_call_to_instance_method =
        "f:main() { group Storage { "
        "run => f:run_impl() { Storage.open(); ret; } "
        "open => f:open_impl(self) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* wrong_method_argument_type =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(\"bad\"); ret; } "
        "store_value => f:store_value_impl(self,value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* wrong_method_argument_count =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(); ret; } "
        "store_value => f:store_value_impl(self,value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* named_method_argument_after_implicit_self =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(value:7); ret; } "
        "store_value => f:store_value_impl(self,value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* named_implicit_self_argument_is_hidden =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(self:other,value:7); ret; } "
        "store_value => f:store_value_impl(self,value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* unknown_group_method =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.no_such_method(); ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* duplicate_public_method_alias =
        "f:main() { group Storage { "
        "open => f:first_impl(self) { ret; } "
        "open => f:second_impl(self) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* duplicate_method_implementation_name =
        "f:main() { group Storage { "
        "open => f:shared_impl(self) { ret; } "
        "close => f:shared_impl(self) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* method_implementation_collides_with_direct_function =
        "f:main() { group Storage { "
        "open => f:shared_impl(self) { ret; } "
        "f:shared_impl(value!i32) { ret; } "
        "struct:Storage { v:value!i32 } } }";

    return check_source(instance_call_to_later_method, 0) &&
        check_source(anonymous_instance_method_implementation, 0) &&
        check_source(anonymous_static_factory_returning_bound_type, 0) &&
        check_source(static_factory_returns_struct_literal, 0) &&
        check_source(type_qualified_static_call, 0) &&
        check_source(namespace_static_call, 0) &&
        check_source(bare_group_expression_dispatches_default, 0) &&
        check_source(bare_group_expression_requires_default, 1) &&
        check_source(direct_group_function_call, 0) &&
        check_source(direct_group_function_called_from_enclosing_scope, 0) &&
        check_source(method_alias_called_from_enclosing_scope, 0) &&
        check_source(group_implementation_name_does_not_leak, 1) &&
        check_source(non_binding_group_type_has_no_instance_dispatch, 1) &&
        check_source(instance_call_to_static_method, 1) &&
        check_source(type_call_to_instance_method, 1) &&
        check_source(wrong_method_argument_type, 1) &&
        check_source(wrong_method_argument_count, 1) &&
        check_source(named_method_argument_after_implicit_self, 0) &&
        check_source(named_implicit_self_argument_is_hidden, 1) &&
        check_source(unknown_group_method, 1) &&
        check_source(duplicate_public_method_alias, 1) &&
        check_source(duplicate_method_implementation_name, 1) &&
        check_source(method_implementation_collides_with_direct_function, 1);
}

static int check_group_type_scope_semantics(void) {
    const char* own_struct =
        "f:main() { group Alpha { struct:LocalStruct { v:value!i32 } "
        "consume => f:consume(value!LocalStruct) { ret; } } }";
    const char* own_enum =
        "f:main() { group Alpha { enum:LocalEnum { Value } "
        "consume => f:consume(value!LocalEnum) { ret; } } }";
    const char* own_union =
        "f:main() { group Alpha { union:LocalUnion { v:value!i32 } "
        "consume => f:consume(value!LocalUnion) { ret; } } }";
    const char* own_method =
        "f:main() { group Alpha { struct:MethodType { v:value!i32 } "
        "run => f:run(value!MethodType) { ret; } } }";
    const char* nested_cannot_use_parent_type =
        "f:main() { group Alpha { struct:Hidden { v:value!i32 } "
        "child => group { consume => f:consume(value!Hidden) { ret; } } } }";
    const char* nested_owns_its_type =
        "f:main() { group Alpha { child => group { "
        "struct:Local { v:value!i32 } consume => f:consume(value!Local) { ret; } } } }";
    const char* nested_same_name_is_distinct =
        "f:main() { group Alpha { struct:Item { v:parent!i32 } "
        "child => group { struct:Item { v:child!u32 } "
        "consume => f:child_use(value!Item) { ret; } } "
        "use_parent => f:parent_use(value!Item) { ret; } } }";
    const char* sibling_cannot_use_group_type =
        "f:main() { group Alpha { struct:Hidden { v:value!i32 } } "
        "group Beta { consume => f:consume(value!Hidden) { ret; } } }";
    const char* outside_cannot_use_group_type =
        "f:main() { group Alpha { struct:Hidden { v:value!i32 } } "
        "f:outside(value!Hidden) { ret; } }";
    const char* group_can_use_external_type =
        "f:main() { struct:ExternalType { v:value!i32 } "
        "group Alpha { consume => f:consume(value!ExternalType) { ret; } } }";
    const char* duplicate_public_method_name =
        "f:main() { group Alpha { run => f:first() { ret; } "
        "run => f:second() { ret; } } }";
    const char* duplicate_default_handler =
        "f:main() { group Alpha { default { } default { } } }";
    const char* method_alias_conflicts_with_function =
        "f:main() { group Alpha { f:run() { ret; } "
        "run => f:implementation() { ret; } } }";
    return check_source(own_struct, 0) &&
        check_source(own_enum, 0) &&
        check_source(own_union, 0) &&
        check_source(own_method, 0) &&
        check_source(nested_cannot_use_parent_type, 1) &&
        check_source(nested_owns_its_type, 0) &&
        check_source(nested_same_name_is_distinct, 0) &&
        check_source(sibling_cannot_use_group_type, 1) &&
        check_source(outside_cannot_use_group_type, 1) &&
        check_source(group_can_use_external_type, 0) &&
        check_source(duplicate_public_method_name, 1) &&
        check_source(duplicate_default_handler, 1) &&
        check_source(method_alias_conflicts_with_function, 1);
}

static int check_m_immutable_value_semantics(void) {
    return
        check_source("m:answer = 42; f:main() { echo(answer); }", 0) &&
        check_source("f:main() { m:local = 5; echo(local); }", 0) &&
        check_source("f:main() { m:value = 5; value = 6; }", 1) &&
        check_source("f:main() { m:value = 5; value++; }", 1) &&
        check_source("f:main() { m:value = 5; m:value = 6; }", 1);
}


static int check_c_compile_time_constant_semantics(void) {
    const char* valid_literals_and_expressions =
        "c:PI = 3.14159; c:DOUBLE_PI = PI * 2.0; "
        "c:LIMIT!i32 = 2 + 3 * 4;";
    const char* valid_aggregate_constant =
        "c:VALUES = [1, 2, 3]; c:PAIR = (1, \"two\");";
    const char* missing_initializer = "c:missing!i32;";
    const char* runtime_call =
        "f:get_value()!i32 { ret 7; } c:value = get_value();";
    const char* mutable_read =
        "v:mutable_value!i32 = 1; c:value = mutable_value;";
    const char* immutable_runtime_read =
        "f:get_value()!i32 { ret 7; } "
        "m:runtime_value = get_value(); c:value = runtime_value;";
    return check_source(valid_literals_and_expressions, 0) &&
        check_source(valid_aggregate_constant, 0) &&
        check_source(missing_initializer, 1) &&
        check_source(runtime_call, 1) &&
        check_source(mutable_read, 1) &&
        check_source(immutable_runtime_read, 1);
}

static int check_fast_exec_asm_semantics(void) {
    const char* valid_forward_labels =
        "f:main() { fastexec { v:x!i32 = 10; "
        "asmcall(\"FORWARD\"); asmjmp(\"EXIT\"); "
        "asm: FORWARD { mov rax, %x:reg } asm: EXIT { ret } } ret; }";
    const char* call_outside =
        "f:main() { asmcall(\"OUTSIDE\"); }";
    const char* missing_label =
        "f:main() { fastexec { asmcall(\"MISSING\"); } }";
    const char* duplicate_label =
        "f:main() { fastexec { asm: SAME { nop } asm: SAME { ret } } }";
    const char* missing_argument =
        "f:main() { fastexec { asmcall(); asm: TARGET { nop } } }";
    const char* non_string_argument =
        "f:main() { fastexec { asmjmp(TARGET); asm: TARGET { nop } } }";
    const char* fast_exec_scope_leak =
        "f:main() { fastexec { v:inside!i32 = 1; } "
        "v:outside!i32 = inside; }";
    const char* nested_scope_valid =
        "f:main() { fastexec { v:outer!i32 = 1; "
        "{ v:inner!i32 = outer + 1; } } }";
    return check_source(valid_forward_labels, 0) &&
        check_source(call_outside, 1) &&
        check_source(missing_label, 1) &&
        check_source(duplicate_label, 1) &&
        check_source(missing_argument, 1) &&
        check_source(non_string_argument, 1) &&
        check_source(fast_exec_scope_leak, 1) &&
        check_source(nested_scope_valid, 0);
}

static int check_intr_semantics(void) {
    return check_source("f:main() { intr(4); }", 0) &&
        check_source("f:main() { intr(); }", 1) &&
        check_source("f:main() { intr(\"irq\"); }", 1) &&
        check_source("f:main() { intr(1, 2); }", 1);
}

static int check_irq_reg_semantics(void) {
    const char* valid_irq = "f:handler() {} f:main() { irq(3, handler); }";
    const char* invalid_irq_count = "f:main() { irq(1); }";
    const char* invalid_irq_number = "f:handler() {} f:main() { irq(\"x\", handler); }";
    const char* invalid_irq_handler = "f:main() { irq(2, missing_handler); }";
    const char* valid_reg_read = "f:main() { v:x = reg(r, \"rax\"); }";
    const char* valid_reg_write = "f:main() { reg(w, \"rax\", 1); }";
    const char* invalid_reg_op = "f:main() { reg(q, \"rax\"); }";
    const char* invalid_reg_name = "f:main() { reg(r, 42); }";
    const char* invalid_reg_count = "f:main() { reg(r); reg(w, \"rax\"); }";
    const char* invalid_reg_value = "f:main() { reg(w, \"rax\", \"bad\"); }";
    return check_source(valid_irq, 0) &&
        check_source(invalid_irq_count, 1) &&
        check_source(invalid_irq_number, 1) &&
        check_source(invalid_irq_handler, 1) &&
        check_source(valid_reg_read, 0) &&
        check_source(valid_reg_write, 0) &&
        check_source(invalid_reg_op, 1) &&
        check_source(invalid_reg_name, 1) &&
        check_source(invalid_reg_count, 2) &&
        check_source(invalid_reg_value, 1);
}

static int check_intrinsic_semantics(void) {
    const char* valid_memory =
        "f:main() { v:x!i32 = 1; v:p = addr(x); v:n!u64 = len(\"abc\"); "
        "v:b!u8 = peek(p); poke(p, b); }";
    const char* invalid_addr =
        "f:main() { v:p = addr(1 + 2); }";
    const char* invalid_len =
        "f:main() { v:n = len(42); }";
    const char* invalid_peek =
        "f:main() { v:b = peek(\"address\"); }";
    const char* invalid_poke_value =
        "f:main() { poke(1, \"text\"); }";
    const char* valid_ports =
        "f:main(p!u16) { v:b!u8 = inb(p); outb(p, b); }";
    const char* invalid_inb =
        "f:main() { v:b = inb(\"port\"); }";
    const char* invalid_outb =
        "f:main() { outb(1, \"value\"); }";
    const char* valid_alloc =
        "f:main() { v:a = area(64); v:z = zone(128); free(a); free(z); }";
    const char* invalid_area =
        "f:main() { v:a = area(0); }";
    const char* invalid_zone_type =
        "f:main() { v:a = zone(\"size\"); }";
    const char* invalid_free =
        "f:main() { free(\"not a handle\"); }";
    const char* invalid_count =
        "f:main() { peek(); poke(1); inb(); outb(1); area(); zone(1, 2); free(); }";
    return check_source(valid_memory, 0) &&
        check_source(invalid_addr, 1) &&
        check_source(invalid_len, 1) &&
        check_source(invalid_peek, 1) &&
        check_source(invalid_poke_value, 1) &&
        check_source(valid_ports, 0) &&
        check_source(invalid_inb, 1) &&
        check_source(invalid_outb, 1) &&
        check_source(valid_alloc, 0) &&
        check_source(invalid_area, 1) &&
        check_source(invalid_zone_type, 1) &&
        check_source(invalid_free, 1) &&
        check_source(invalid_count, 7);
}


static int check_program_and_block_scope_semantics(void) {
    const char* global_visible_to_function =
        "v:global_value!i32 = 7; "
        "f:main() { v:local_value!i32 = global_value; }";
    const char* nested_block_does_not_leak =
        "f:main() { { v:inner_value!i32 = 1; } "
        "v:outer_value!i32 = inner_value; }";
    const char* sibling_blocks_are_isolated =
        "f:main() { { v:private_value!i32 = 1; } "
        "{ v:use_value!i32 = private_value; } }";
    const char* nested_scope_shadowing =
        "f:main() { v:value!i32 = 1; "
        "{ v:value!i32 = 2; v:inner_read!i32 = value; } "
        "v:outer_read!i32 = value; }";
    const char* duplicate_program_declaration =
        "v:global_value!i32 = 1; v:global_value!i32 = 2;";
    return check_source(global_visible_to_function, 0) &&
        check_source(nested_block_does_not_leak, 1) &&
        check_source(sibling_blocks_are_isolated, 1) &&
        check_source(nested_scope_shadowing, 0) &&
        check_source(duplicate_program_declaration, 1);
}

int main(int argc, char** argv) {
    if (!check_program_and_block_scope_semantics()) {
        fprintf(stderr, "Program/block declaration scope checks failed.\\n");
        return 1;
    }
    if (!check_intr_semantics()) {
        fprintf(stderr, "INTR semantic checks failed.\n");
        return 1;
    }
    if (!check_irq_reg_semantics()) {
        fprintf(stderr, "IRQ/register semantic checks failed.\n");
        return 1;
    }
    if (!check_intrinsic_semantics()) {
        fprintf(stderr, "Memory/port/allocation intrinsic semantic checks failed.\\n");
        return 1;
    }
    if (!check_c_compile_time_constant_semantics()) {
        fprintf(stderr, "c: compile-time constant semantic checks failed.\\n");
        return 1;
    }
    if (!check_fast_exec_asm_semantics()) {
        fprintf(stderr, "FASTEXEC/ASM semantic checks failed.\\n");
        return 1;
    }
    if (!check_m_immutable_value_semantics()) {
        fprintf(stderr, "Immutable m: value semantic checks failed.\\n");
        return 1;
    }
    if (!check_aggregate_field_type_semantics()) {
        fprintf(stderr, "Aggregate field type semantic checks failed.\\n");
        return 1;
    }
    if (!check_enum_scan_pattern_semantics()) {
        fprintf(stderr, "Enum scan constructor pattern semantic checks failed.\\n");
        return 1;
    }
    if (!check_enum_constructor_semantics()) {
        fprintf(stderr, "Enum constructor call semantic checks failed.\n");
        return 1;
    }
    if (!check_enum_discriminant_semantics()) {
        fprintf(stderr, "Enum discriminant Semantic tests failed.\n");
        return 1;
    }
    if (!check_enum_declaration_semantics()) {
        fprintf(stderr, "Enum declaration and payload semantic checks failed.\\n");
        return 1;
    }
    if (!check_malformed_group_method_ast()) {
        fprintf(stderr, "Malformed Group method AST checks failed.\n");
        return 1;
    }
    if (!check_group_self_receiver_semantics()) {
        fprintf(stderr, "Group self receiver inference/validation checks failed.\\n");
        return 1;
    }
    if (!check_group_method_dispatch_semantics()) {
        fprintf(stderr, "Group method dispatch checks failed.\\n");
        return 1;
    }
    if (!check_group_type_scope_semantics()) {
        fprintf(stderr, "Group-owned nominal type scope checks failed.\n");
        return 1;
    }

    if (!check_malformed_conditional_ast()) {
        fprintf(stderr, "Malformed conditional AST checks failed.\n");
        return 1;
    }
    if (!check_malformed_pattern_ast()) {
        fprintf(stderr, "Malformed pattern AST checks failed.\n");
        return 1;
    }
    if (!check_malformed_rule_ast()) {
        fprintf(stderr, "Malformed rule AST checks failed.\n");
        return 1;
    }
    if (!check_malformed_expression_and_loop_ast()) {
        fprintf(stderr, "Malformed expression/loop AST checks failed.\n");
        return 1;
    }
    if (!check_malformed_nexus_and_argument_ast()) {
        fprintf(stderr, "Malformed Nexus/argument AST checks failed.\\n");
        return 1;
    }
    if (!check_malformed_function_and_control_ast()) {
        fprintf(stderr, "Malformed function/control AST checks failed.\\n");
        return 1;
    }
    if (!check_malformed_intrinsic_ast()) {
        fprintf(stderr, "Malformed intrinsic AST checks failed.\n");
        return 1;
    }
    if (!check_thread_api_is_module_call()) {
        fprintf(stderr, "Thread module call boundary checks failed.\n");
        return 1;
    }
    if (!check_unnamed_declaration_ast()) {
        fprintf(stderr, "Unnamed declaration AST checks failed.\n");
        return 1;
    }
    if (!check_semantic_fail_closed_nodes()) {
        fprintf(stderr, "Semantic pending AST nodes were not rejected.\n");
        return 1;
    }
    if (!check_postfix_semantics()) {
        fprintf(stderr, "Postfix semantic checks failed.\n");
        return 1;
    }
    if (!check_compound_assignment_semantics()) {
        fprintf(stderr, "Compound assignment semantic checks failed.\n");
        return 1;
    }
    if (!check_pointer_reference_type_semantics()) {
        fprintf(stderr, "Pointer/reference compatibility checks failed.\n");
        return 1;
    }
    if (!check_array_type_semantics()) {
        fprintf(stderr, "Array type semantic checks failed.\n");
        return 1;
    }
    if (!check_aggregate_member_semantics()) {
        fprintf(stderr, "Aggregate member semantic checks failed.\n");
        return 1;
    }
    if (!check_struct_literal_semantics()) {
        fprintf(stderr, "Struct literal semantic checks failed.\\n");
        return 1;
    }
    if (!check_array_literal_semantics()) {
        fprintf(stderr, "Array literal semantic checks failed.\n");
        return 1;
    }
    if (!check_extended_ast_semantics()) {
        fprintf(stderr, "Extended AST semantic scope/effect checks failed.\n");
        return 1;
    }
    if (!check_control_flow_semantics()) {
        fprintf(stderr, "Control-flow semantic checks failed.\n");
        return 1;
    }
    if (argc != 3) return 2;
    if (!check_file(argv[1], 0, ERR_SEMANTIC_UNDEFINED)) return 1;
    if (!check_file(argv[2], 1, ERR_SEMANTIC_UNDEFINED)) return 1;
    return 0;
}
