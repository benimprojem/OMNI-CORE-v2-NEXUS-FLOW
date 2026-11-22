#include "parser.h"
#include "ast.h"
#include "semantic.h"
#include "../semantic/semantic_import_environment.h"

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


static int check_imported_function_call_semantics(void) {
    OccModuleInterfaceSymbol symbol;
    OccModuleInterfaceType types[2];
    OccModuleInterfaceRelation relations[2];
    OccModuleInterface module_interface;
    OccSemanticImportEnvironmentBinding binding;
    OccSemanticImportEnvironmentBinding duplicate_binding;
    OccSemanticImportEnvironmentBinding bindings[2];
    OccSemanticImportEnvironment environment;
    const char* valid_source =
        "use math as m; f:main() { v:sum!i32 = m.add(10, 20); }";
    const char* named_source =
        "use math as m; f:main() { v:sum!i32 = m.add(right:20, left:10); }";
    const char* namespace_source =
        "use math ns mathns; f:main() { v:sum!i32 = mathns.add(10, 20); }";
    const char* wrong_type_source =
        "use math as m; f:main() { v:sum!i32 = m.add(10, \"bad\"); }";
    const char* missing_symbol_source =
        "use math as m; f:main() { m.missing(10, 20); }";
    const char* wrong_count_source =
        "use math as m; f:main() { m.add(10); }";
    const char* hidden_symbol_source =
        "use math as m; f:main() { m.add(10, 20); }";
    const char* local_selector_collision_source =
        "use math as m; f:main() { v:m!i32 = 1; m.add(10, 20); }";
    const char* safe_reference_return_source =
        "use math as m; f:main(owner!i32) { v:result!&i32 = m.add(10, 20); }";
    const char* ambiguous_alias_source =
        "use math as m; f:main() { m.add(10, 20); }";
    const char* ambiguous_namespace_source =
        "use math ns mathns; f:main() { mathns.add(10, 20); }";
    const char* alias_namespace_collision_source =
        "use math as m; f:main() { m.add(10, 20); }";
    const char* sources[] = {
        valid_source, named_source, namespace_source, wrong_type_source,
        missing_symbol_source, wrong_count_source, hidden_symbol_source,
        local_selector_collision_source, safe_reference_return_source,
        ambiguous_alias_source, ambiguous_namespace_source,
        alias_namespace_collision_source
    };
    const int expected_errors[] = {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    size_t source_index;
    memset(&symbol, 0, sizeof(symbol));
    memset(types, 0, sizeof(types));
    memset(relations, 0, sizeof(relations));
    memset(&module_interface, 0, sizeof(module_interface));
    memset(&binding, 0, sizeof(binding));
    memset(&duplicate_binding, 0, sizeof(duplicate_binding));
    memset(bindings, 0, sizeof(bindings));
    memset(&environment, 0, sizeof(environment));

    symbol.source_symbol_id = 4u;
    symbol.type_id = 1u;
    symbol.kind = SEMANTIC_RESULT_SYMBOL_FUNCTION;
    symbol.flags = SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED;
    symbol.name = "add";
    types[0].source_type_id = 1u;
    types[0].kind = SEMANTIC_RESULT_TYPE_PRIMITIVE;
    types[0].name = "i32";
    types[1].source_type_id = 2u;
    types[1].kind = SEMANTIC_RESULT_TYPE_REFERENCE;
    types[1].base_type_id = 1u;
    types[1].name = "&i32";
    relations[0].owner_source_symbol_id = 4u;
    relations[0].type_id = 1u;
    relations[0].ordinal = 0u;
    relations[0].kind = SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER;
    relations[0].name = "left";
    relations[1].owner_source_symbol_id = 4u;
    relations[1].type_id = 1u;
    relations[1].ordinal = 1u;
    relations[1].kind = SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER;
    relations[1].name = "right";
    module_interface.canonical_identity = "file:///lib/math.nxf";
    module_interface.schema_major = OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    module_interface.schema_minor = OCC_MODULE_INTERFACE_SCHEMA_MINOR;
    module_interface.symbols = &symbol;
    module_interface.symbol_count = 1u;
    module_interface.types = types;
    module_interface.type_count = 2u;
    module_interface.relations = relations;
    module_interface.relation_count = 2u;
    binding.use_ordinal = 1u;
    binding.requested_path = "math";
    binding.dependency_identity = "file:///lib/math.nxf";
    binding.alias = "m";
    binding.namespace_name = "mathns";
    binding.module_interface = &module_interface;
    duplicate_binding.use_ordinal = 2u;
    duplicate_binding.requested_path = "math_duplicate";
    duplicate_binding.dependency_identity = "file:///lib/math_duplicate.nxf";
    duplicate_binding.alias = "other";
    duplicate_binding.namespace_name = "otherns";
    duplicate_binding.module_interface = &module_interface;
    bindings[0] = binding;
    bindings[1] = duplicate_binding;
    environment.bindings = bindings;
    environment.binding_count = 1u;

    for (source_index = 0u;
         source_index < sizeof(sources) / sizeof(sources[0]);
         ++source_index) {
        ParserContext* parser = parser_init_source(
            "semantic-import-call.nxf",
            (const uint8_t*)sources[source_index],
            strlen(sources[source_index]));
        SemanticContext semantic;
        int ok;
        if (!parser || parser_run(parser) != 0 || !parser->ast_root) {
            fprintf(stderr, "Parser rejected imported-call fixture: %s\n",
                sources[source_index]);
            if (parser) parser_destroy(parser);
            return 0;
        }
        semantic_init(&semantic, "semantic-import-call.nxf", NULL);
        symbol.flags = source_index == 6u
            ? 0u : SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED;
        symbol.type_id = source_index == 8u ? 2u : 1u;
        environment.binding_count = source_index >= 9u ? 2u : 1u;
        bindings[1] = duplicate_binding;
        if (source_index == 9u) bindings[1].alias = "m";
        if (source_index == 10u) bindings[1].namespace_name = "mathns";
        if (source_index == 11u) bindings[1].namespace_name = "m";
        semantic_attach_import_environment(&semantic, &environment);
        semantic_check(&semantic, (ASTNode*)parser->ast_root);
        ok = semantic.error_count == expected_errors[source_index];
        if (!ok) {
            Error* error;
            fprintf(stderr,
                "Imported-call semantic mismatch: expected=%d actual=%d source=%s\n",
                expected_errors[source_index], semantic.error_count,
                sources[source_index]);
            for (error = semantic.error_head; error; error = error->next)
                fprintf(stderr, "  semantic error %d at %d:%d: %s\n",
                    error->code, error->line, error->col,
                    error->message ? error->message : "(null)");
        }
        semantic_destroy(&semantic);
        parser_destroy(parser);
        if (!ok) return 0;
    }
    return 1;
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

static int check_handler_capture_semantics(void) {
    const char* captured_type_is_preserved =
        "f:main() { (text) <- \"handler-data\"; v:copy!str = text; }";
    const char* self_capture_is_undefined =
        "f:main() { (self) <- self; }";
    const char* duplicate_handler_capture =
        "f:main() { (h) <- 1; (h) <- 2; }";
    return check_source(captured_type_is_preserved, 0) &&
        check_source(self_capture_is_undefined, 1) &&
        check_source(duplicate_handler_capture, 1);
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

static int check_malformed_recursive_type_expression_ast(void) {
    static const struct {
        uint32_t kind;
        uint32_t flags;
        int child_mode;
        const char* expected;
    } cases[] = {
        { AST_TYPE_POINTER, 0u, 0,
          "Pointer type expression requires a target type." },
        { AST_TYPE_REFERENCE, 0u, 1,
          "Recursive type expression contains a node that is not a type expression." },
        { AST_TYPE_QUALIFIED, AST_TYPE_FLAG_HARD_TYPED | AST_TYPE_FLAG_MUTABLE_REFERENCE, 2,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_FUNCTION, 0u, 3,
          "Function type expression requires a return type." },
        { AST_TYPE_NAMED, AST_TYPE_FLAG_HARD_TYPED, 4,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_POINTER, AST_TYPE_FLAG_HARD_TYPED, 2,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_REFERENCE, AST_TYPE_FLAG_HARD_TYPED, 2,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_ARRAY, AST_TYPE_FLAG_HARD_TYPED, 5,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_FUNCTION, AST_TYPE_FLAG_MUTABLE_REFERENCE, 2,
          "Type expression contains flags that are not valid for its kind." },
        { AST_TYPE_REFERENCE, AST_TYPE_FLAG_MUTABLE_REFERENCE, 2, NULL }
    };
    size_t i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ASTTypeExpr* expression = (ASTTypeExpr*)ast_create(AST_TYPE_EXPR);
        SemanticContext semantic;
        int ok;
        if (!expression) return 0;
        expression->kind = cases[i].kind;
        expression->flags = cases[i].flags;
        if (cases[i].child_mode == 1) {
            expression->child = ast_create(AST_LITERAL);
            if (!expression->child) {
                ast_destroy((ASTNode*)expression);
                return 0;
            }
        } else if (cases[i].child_mode == 2 || cases[i].child_mode == 5) {
            expression->child = (ASTNode*)ast_create(AST_TYPE_EXPR);
            if (!expression->child) {
                ast_destroy((ASTNode*)expression);
                return 0;
            }
            ((ASTTypeExpr*)expression->child)->kind = AST_TYPE_NAMED;
            ((ASTTypeExpr*)expression->child)->name =
                semantic_test_copy_text("i32");
            if (!((ASTTypeExpr*)expression->child)->name) {
                ast_destroy((ASTNode*)expression);
                return 0;
            }
            if (cases[i].child_mode == 5) {
                expression->dimensions = ast_create(AST_ARRAY_TYPE);
                if (!expression->dimensions) {
                    ast_destroy((ASTNode*)expression);
                    return 0;
                }
            }
        } else if (cases[i].child_mode == 4) {
            expression->name = semantic_test_copy_text("i32");
            if (!expression->name) {
                ast_destroy((ASTNode*)expression);
                return 0;
            }
        }
        semantic_init(&semantic, "semantic-malformed-type-expression.nxf", NULL);
        semantic_check(&semantic, (ASTNode*)expression);
        ok = cases[i].expected
            ? semantic.error_count == 1 && semantic.error_head &&
              strstr(semantic.error_head->message, cases[i].expected) != NULL
            : semantic.error_count == 0;
        semantic_destroy(&semantic);
        ast_destroy((ASTNode*)expression);
        if (!ok) return 0;
    }
    return 1;
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
    {
        ASTMustDecl* missing_initializer =
            (ASTMustDecl*)ast_create(AST_MUST_DECL);
        SemanticContext semantic;
        int ok;
        if (!missing_initializer) return 0;
        missing_initializer->name = (char*)malloc(sizeof("missing"));
        if (!missing_initializer->name) {
            ast_destroy((ASTNode*)missing_initializer);
            return 0;
        }
        memcpy(missing_initializer->name, "missing", sizeof("missing"));
        semantic_init(&semantic, "semantic-missing-immutable-initializer.nxf", NULL);
        semantic_check(&semantic, (ASTNode*)missing_initializer);
        ok = semantic.error_count == 1 && semantic.error_head &&
            strstr(semantic.error_head->message,
                "An immutable m: value requires an initializer expression.") != NULL;
        semantic_destroy(&semantic);
        ast_destroy((ASTNode*)missing_initializer);
        if (!ok) return 0;
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
        "f:main() { struct:Point { v:x!i32, v:y!i32 }"
        " v:p!Point; v:a!i32 = p.x; p.y = 2; }";
    const char* nested =
        "f:main() { struct: Inner { v:x!i32 }"
        " struct: Outer { v:inner!Inner } v:o!Outer;"
        " v:a!i32 = o.inner.x; o.inner.x = 3; }";
    const char* constant_object =
        "f:main() { struct:Point { v:x!i32 } c:p!Point = Point { x = 1 }; p.x = 2; }";
    const char* incompatible =
        "f:main() { struct:Point { v:x!i32 } v:p!Point; p.x = \"bad\"; }";
    const char* unknown_member =
        "f:main() { struct:Point { v:x!i32 } v:p!Point; v:a = p.missing; }";
    const char* non_aggregate =
        "f:main() { v:value!i32 = 1; v:bad = value.member; }";
    const char* duplicate_struct_field =
        "f:main() { struct:Point { v:x!i32, v:x!f64 } }";
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

static int check_aggregate_reference_field_rejection_semantics(void) {
    const char* shared_reference_field_is_rejected =
        "struct: Holder { v:reference!&i32 } f:main() {}";
    const char* mutable_reference_field_is_rejected =
        "struct: Holder { v:reference!&muti32 } f:main() {}";
    const char* newtype_reference_field_is_rejected =
        "nt:Ref = &i32; struct: Holder { v:reference!Ref } f:main() {}";
    const char* nested_union_reference_field_is_rejected =
        "union: RefUnion { v:reference!&i32, v:raw!u64 } "
        "struct: Wrapper { v:inner!RefUnion } f:main() {}";
    const char* nested_struct_reference_field_is_rejected =
        "struct: RefInner { v:reference!&i32 } "
        "struct: RefOuter { v:inner!RefInner } f:main() {}";
    return check_source(shared_reference_field_is_rejected, 1) &&
        check_source(mutable_reference_field_is_rejected, 1) &&
        check_source(newtype_reference_field_is_rejected, 1) &&
        check_source(nested_union_reference_field_is_rejected, 2) &&
        check_source(nested_struct_reference_field_is_rejected, 2);
}

static int check_struct_literal_semantics(void) {
    const char* valid_reordered =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:p!Point = Point { y = 2, x = 1 }; }";
    const char* unknown_field =
        "f:main() { struct:Point { v:x!i32 } "
        "v:p!Point = Point { x = 1, missing = 2 }; }";
    const char* duplicate_field =
        "f:main() { struct:Point { v:x!i32 } "
        "v:p!Point = Point { x = 1, x = 2 }; }";
    const char* missing_field =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:p!Point = Point { x = 1 }; }";
    const char* incompatible_field =
        "f:main() { struct:Point { v:x!i32 } "
        "v:p!Point = Point { x = \"wrong\" }; }";
    const char* numeric_type_mismatch =
        "f:main() { struct:Point { v:x!i32 } "
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
    const char* valid_fixed_bitwise_binary_width_promotion =
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
        check_source(valid_fixed_bitwise_binary_width_promotion,0) &&
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
        "f:take(value!*i32) {} f:main() { v:a!*i32 = null; v:b!*i32 = null; a = b; take(a); }";
    const char* pointer_base_mismatch =
        "f:take(value!*i32) {} f:main(a!*f32) { take(a); }";
    const char* pointer_depth_mismatch =
        "f:take(value!**i32) {} f:main(a!*i32) { take(a); }";
    const char* pointer_reference_mismatch =
        "f:take(value!&i32) {} f:main(a!*i32) { take(a); }";
    const char* exact_reference =
        "f:take(value!&i32) {} f:main() { v:owner!i32 = 1; v:a!&i32 = &owner; take(a); }";
    const char* reference_base_mismatch =
        "f:take(value!&i32) {} f:main(owner!f32) { v:a!&f32 = &owner; take(a); }";
    const char* exact_nested_pointer_assignment =
        "f:main(a!**i32, b!**i32) { a = b; }";
    const char* nested_pointer_depth_mismatch =
        "f:main(a!**i32, b!*i32) { a = b; }";
    const char* exact_reference_assignment =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; v:a!&i32 = &first; v:b!&i32 = &second; a = b; }";
    const char* pointer_integer_mismatch =
        "f:take(value!*i32) {} f:main(address!u64) { take(address); }";
    const char* unknown_nested_pointer_target =
        "f:take(value!**MissingType) {} f:main() {}";
    const char* exact_pointer_return =
        "f:identity(value!*i32)!*i32 { ret value; } "
        "f:main(p!*i32) { v:q!*i32 = identity(p); }";
    const char* pointer_return_base_mismatch =
        "f:identity(value!*i32)!*f32 { ret value; } f:main() {}";
    const char* exact_reference_return =
        "f:identity(value!&i32)!&i32 { ret value; } f:main() {}";
    const char* reference_return_base_mismatch =
        "f:identity(value!&i32)!&f32 { ret value; } f:main() {}";
    const char* pointer_dereference =
        "f:main(p!*i32) { v:value!i32 = *p; }";
    const char* nested_pointer_dereference =
        "f:main(p!**i32) { v:q!*i32 = *p; v:value!i32 = **p; }";
    const char* non_pointer_dereference =
        "f:main() { v:value!i32 = 4; v:bad!i32 = *value; }";
    const char* pointer_dereference_assignment =
        "f:main(p!*i32) { *p = 7; }";
    const char* pointer_dereference_assignment_mismatch =
        "f:main(p!*i32) { *p = \"wrong\"; }";
    const char* reference_dereference =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; v:value!i32 = *r; }";
    const char* reference_dereference_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r = 7; }";
    const char* reference_compound_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r += 1; }";
    const char* pointer_compound_assignment =
        "f:main(p!*i32) { *p += 1; }";
    const char* reference_subtract_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r -= 1; }";
    const char* reference_multiply_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r *= 2; }";
    const char* reference_divide_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r /= 2; }";
    const char* reference_bitwise_assignment =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; *r &= 1; }";
    const char* pointer_bitwise_assignment =
        "f:main(p!*i32) { *p &= 1; }";
    return check_source(exact_pointer, 0) &&
        check_source(pointer_base_mismatch, 1) &&
        check_source(pointer_depth_mismatch, 1) &&
        check_source(pointer_reference_mismatch, 1) &&
        check_source(exact_reference, 0) &&
        check_source(reference_base_mismatch, 1) &&
        check_source(exact_nested_pointer_assignment, 0) &&
        check_source(nested_pointer_depth_mismatch, 1) &&
        check_source(exact_reference_assignment, 0) &&
        check_source(pointer_integer_mismatch, 1) &&
        check_source(unknown_nested_pointer_target, 1) &&
        check_source(exact_pointer_return, 0) &&
        check_source(pointer_return_base_mismatch, 1) &&
        check_source(exact_reference_return, 0) &&
        check_source(reference_return_base_mismatch, 1) &&
        check_source(pointer_dereference, 0) &&
        check_source(nested_pointer_dereference, 0) &&
        check_source(non_pointer_dereference, 1) &&
        check_source(pointer_dereference_assignment, 0) &&
        check_source(pointer_dereference_assignment_mismatch, 1) &&
        check_source(reference_dereference, 0) &&
        check_source(reference_dereference_assignment, 1) &&
        check_source(reference_compound_assignment, 1) &&
        check_source(pointer_compound_assignment, 0) &&
        check_source(reference_subtract_assignment, 1) &&
        check_source(reference_multiply_assignment, 1) &&
        check_source(reference_divide_assignment, 1) &&
        check_source(reference_bitwise_assignment, 1) &&
        check_source(pointer_bitwise_assignment, 0);
}

static int check_named_function_callback_value_semantics(void) {
    const char* exact_named_function_value =
        "nt:Callback = f:(id!u32)!bool; "
        "f:is_even(id!u32)!bool { ret true; } "
        "f:main(id!u32) { v:callback!Callback = is_even; callback(id); }";
    const char* exact_direct_callback_value =
        "f:check(id!u32)!bool { ret true; } "
        "f:main(id!u32) { v:callback!f:(id!u32)!bool = check; callback(id); }";
    const char* mismatched_parameter_type =
        "nt:Callback = f:(id!u32)!bool; "
        "f:check(id!i32)!bool { ret true; } "
        "f:main() { v:callback!Callback = check; }";
    const char* mismatched_return_type =
        "nt:Callback = f:(id!u32)!bool; "
        "f:check(id!u32)!i32 { ret 1; } "
        "f:main() { v:callback!Callback = check; }";
    const char* mismatched_parameter_name =
        "nt:Callback = f:(id!u32)!bool; "
        "f:check(value!u32)!bool { ret true; } "
        "f:main() { v:callback!Callback = check; }";
    const char* callback_reassignment_from_function =
        "nt:Callback = f:(id!u32)!bool; "
        "f:first(id!u32)!bool { ret true; } "
        "f:second(id!u32)!bool { ret false; } "
        "f:main(id!u32) { v:callback!Callback = first; callback = second; callback(id); }";
    return check_source(exact_named_function_value, 0) &&
        check_source(exact_direct_callback_value, 0) &&
        check_source(mismatched_parameter_type, 1) &&
        check_source(mismatched_return_type, 1) &&
        check_source(mismatched_parameter_name, 1) &&
        check_source(callback_reassignment_from_function, 0);
}

static int check_variadic_argument_safety_semantics(void) {
    const char* variadic_values_are_allowed =
        "f:consume(...) {} f:main() { consume(1, 2.0, true, \"text\"); }";
    const char* variadic_raw_pointer_is_explicitly_allowed =
        "f:consume(...) {} f:main(pointer!*i32) { consume(pointer); }";
    const char* variadic_safe_reference_is_rejected =
        "f:consume(...) {} f:main(owner!i32) { consume(&owner); }";
    const char* variadic_mutable_reference_is_rejected =
        "f:consume(...) {} f:main(owner!i32) { consume(&mut owner); }";
    return check_source(variadic_values_are_allowed, 0) &&
        check_source(variadic_raw_pointer_is_explicitly_allowed, 0) &&
        check_source(variadic_safe_reference_is_rejected, 1) &&
        check_source(variadic_mutable_reference_is_rejected, 1);
}

static int check_callback_target_control_flow_merge_semantics(void) {
    const char* both_if_paths_keep_same_target =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:first(value!&i32)!&i32 { ret value; } "
        "f:main(owner!i32, flag!bool) { v:callback!RefGetter = first; "
        "if (flag) { callback = first; } els { callback = first; } "
        "v:result!&i32 = callback(&owner); }";
    const char* if_paths_choose_different_targets =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:first(value!&i32)!&i32 { ret value; } "
        "f:second(value!&i32)!&i32 { ret value; } "
        "f:main(owner!i32, flag!bool) { v:callback!RefGetter = first; "
        "if (flag) { callback = first; } els { callback = second; } "
        "v:result!&i32 = callback(&owner); }";
    const char* missing_else_keeps_unknown_target =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:first(value!&i32)!&i32 { ret value; } "
        "f:second(value!&i32)!&i32 { ret value; } "
        "f:main(owner!i32, flag!bool) { v:callback!RefGetter = first; "
        "if (flag) { callback = second; } "
        "v:result!&i32 = callback(&owner); }";
    return check_source(both_if_paths_keep_same_target, 0) &&
        check_source(if_paths_choose_different_targets, 1) &&
        check_source(missing_else_keeps_unknown_target, 1);
}

static int check_named_callback_reference_return_origin_semantics(void) {
    const char* named_callback_reference_return =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:main(owner!i32) { v:callback!RefGetter = identity; "
        "v:result!&i32 = callback(&owner); v:copy!i32 = *result; }";
    const char* copied_callback_preserves_return_origin =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:main(owner!i32) { v:first!RefGetter = identity; "
        "v:second!RefGetter = first; v:result!&i32 = second(&owner); "
        "v:copy!i32 = *result; }";
    const char* unknown_callback_reference_return_fails_closed =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:use(callback!RefGetter, owner!i32) { "
        "v:result!&i32 = callback(&owner); }";
    return check_source(named_callback_reference_return, 0) &&
        check_source(copied_callback_preserves_return_origin, 0) &&
        check_source(unknown_callback_reference_return_fails_closed, 1);
}

static int check_function_all_path_return_semantics(void) {
    const char* missing_return =
        "f:missing()!i32 { v:value!i32 = 1; }";
    const char* both_if_paths_return =
        "f:choose(flag!bool)!i32 { "
        "if (flag) { ret 1; } els { ret 2; } }";
    const char* one_if_path_missing =
        "f:partial(flag!bool)!i32 { if (flag) { ret 1; } }";
    const char* select_default_all_return =
        "f:choose(value!bool)!i32 { select(value) { "
        "true -> { ret 1; } default -> { ret 2; } } }";
    const char* select_without_default_missing_path =
        "f:choose(value!bool)!i32 { select(value) { "
        "true -> { ret 1; } } }";
    const char* scan_else_all_return =
        "f:choose(value!i32)!i32 { scan(value) { "
        "1 -> { ret 1; } else -> { ret 2; } } }";
    const char* void_function_may_fall_through =
        "f:work()!void { v:value!i32 = 1; }";
    const char* infinite_loop_diverges =
        "f:wait()!i32 { loop { v:value!i32 = 1; } }";
    return check_source(missing_return, 1) &&
        check_source(both_if_paths_return, 0) &&
        check_source(one_if_path_missing, 1) &&
        check_source(select_default_all_return, 0) &&
        check_source(select_without_default_missing_path, 1) &&
        check_source(scan_else_all_return, 0) &&
        check_source(void_function_may_fall_through, 0) &&
        check_source(infinite_loop_diverges, 0);
}

static int check_callback_function_type_semantics(void) {
    const char* direct_callback_parameter =
        "f:apply(callback!f:(id!u32)!bool,id!u32)!bool { ret callback(id); }";
    const char* callback_newtype_alias =
        "nt:Callback = f:(id!u32)!bool; "
        "f:apply(callback!Callback,id!u32)!bool { ret callback(id); }";
    const char* callback_wrong_argument_type =
        "f:apply(callback!f:(id!u32)!bool,id!i32)!bool { ret callback(id); }";
    const char* callback_wrong_argument_count =
        "f:apply(callback!f:(id!u32)!bool)!bool { ret callback(); }";
    const char* callback_reference_result_has_no_origin =
        "f:apply(callback!f:(id!i32)!&i32,id!i32) { callback(id); }";
    const char* non_callable_value =
        "f:main() { v:value!i32 = 1; value(); }";
    return check_source(direct_callback_parameter, 0) &&
        check_source(callback_newtype_alias, 0) &&
        check_source(callback_wrong_argument_type, 1) &&
        check_source(callback_wrong_argument_count, 1) &&
        check_source(callback_reference_result_has_no_origin, 1) &&
        check_source(non_callable_value, 1);
}

static int check_nested_type_variable_semantics(void) {
    const char* pointer_type_variable_return =
        "f:identity(value!*t)!*t { ret value; } "
        "f:main(value!*i32) { v:result!*i32 = identity(value); }";
    const char* reference_type_variable_return =
        "f:identity(value!&t)!&t { ret value; } "
        "f:main(value!&i32) { v:result!&i32 = identity(value); }";
    const char* nested_type_variable_base_conflict =
        "f:combine(pointer!*t, value!t) {} "
        "f:main(pointer!*i32, value!u32) { combine(pointer, value); }";
    const char* nested_type_variable_shape_mismatch =
        "f:consume(pointer!*t) {} f:main(value!i32) { consume(value); }";
    const char* dynamic_array_type_variable_return =
        "f:first(values!t[])!t { ret values[0]; } "
        "f:main(values!i32[2]) { v:result!i32 = first(values); }";
    const char* fixed_array_type_variable_shape_mismatch =
        "f:first(values!t[2])!t { ret values[0]; } "
        "f:main(values!i32[3]) { v:result!i32 = first(values); }";
    const char* direct_t_reference_inference_is_rejected =
        "f:identity(value!t)!t { ret value; } "
        "f:main(owner!i32) { v:result!&i32 = identity(&owner); }";
    const char* unbound_generic_return_type_is_rejected =
        "f:make()!t { ret 1; }";

    return check_source(pointer_type_variable_return, 0) &&
        check_source(reference_type_variable_return, 0) &&
        check_source(nested_type_variable_base_conflict, 1) &&
        check_source(nested_type_variable_shape_mismatch, 1) &&
        check_source(dynamic_array_type_variable_return, 0) &&
        check_source(fixed_array_type_variable_shape_mismatch, 1) &&
        check_source(direct_t_reference_inference_is_rejected, 1) &&
        check_source(unbound_generic_return_type_is_rejected, 1);
}

static int check_vector_index_semantics(void) {
    const char* vector_constant_indexes =
        "f:main() { v:value!Vec3 = [1,2,3]; "
        "v:a!f64 = value[0]; v:b!f64 = value[2]; }";
    const char* vector_dynamic_integer_index =
        "f:main(index!i32) { v:value!Vec2 = [1,2]; "
        "v:component!f64 = value[index]; }";
    const char* vector_index_out_of_range =
        "f:main() { v:value!Vec2 = [1,2]; v:component!f64 = value[2]; }";
    const char* vector_index_negative =
        "f:main() { v:value!Vec2 = [1,2]; v:component!f64 = value[-1]; }";
    const char* vector_index_requires_integer =
        "f:main() { v:value!Vec2 = [1,2]; value[\"x\"]; }";
    const char* vector_index_write_type =
        "f:main() { v:value!Vec2 = [1,2]; value[0] = \"bad\"; }";
    return check_source(vector_constant_indexes, 0) &&
        check_source(vector_dynamic_integer_index, 0) &&
        check_source(vector_index_out_of_range, 1) &&
        check_source(vector_index_negative, 1) &&
        check_source(vector_index_requires_integer, 1) &&
        check_source(vector_index_write_type, 1);
}

static int check_safe_reference_type_erasure_semantics(void) {
    const char* any_initializer_rejected =
        "f:main(owner!i32) { v:dynamic!any = &owner; }";
    const char* any_reassignment_rejected =
        "f:main(owner!i32) { v:dynamic!any = 1; dynamic = &owner; }";
    const char* any_parameter_rejected =
        "f:consume(value!any) {} f:main(owner!i32) { consume(&owner); }";
    const char* callback_reference_contract_erasure_rejected =
        "nt:RefGetter = f:(value!&i32)!&i32; "
        "f:consume(value!any) {} "
        "f:main(callback!RefGetter) { consume(callback); }";
    const char* any_return_rejected =
        "f:escape(owner!i32)!any { ret &owner; }";
    const char* any_array_initializer_rejected =
        "f:main(owner!i32) { v:items[]!any = [&owner]; }";
    const char* inferred_tuple_reference_initializer_rejected =
        "f:main(owner!i32) { v:items = (&owner, 1); }";
    const char* any_map_value_rejected =
        "f:main(owner!i32) { v:items!map; items[\"key\"] = &owner; }";
    const char* cast_any_to_reference_rejected =
        "f:main(value!any) { v:reference!&i32 = value; }";
    const char* cast_raw_pointer_to_reference_rejected =
        "f:main(value!*i32) { v:reference!&i32 = value; }";
    const char* explicit_reference_to_raw_pointer_allowed =
        "nt:RawI32 = *i32; f:main(owner!i32) { v:pointer!RawI32 = cast(&owner,RawI32); }";
    return check_source(any_initializer_rejected, 1) &&
        check_source(any_reassignment_rejected, 1) &&
        check_source(any_parameter_rejected, 1) &&
        check_source(callback_reference_contract_erasure_rejected, 1) &&
        check_source(any_return_rejected, 1) &&
        check_source(any_array_initializer_rejected, 1) &&
        check_source(inferred_tuple_reference_initializer_rejected, 1) &&
        check_source(any_map_value_rejected, 1) &&
        check_source(cast_any_to_reference_rejected, 1) &&
        check_source(cast_raw_pointer_to_reference_rejected, 1) &&
        check_source(explicit_reference_to_raw_pointer_allowed, 0);
}

static int check_extern_reference_boundary_semantics(void) {
    const char* extern_use_before_declaration =
        "f:main(value!i32) { v:result!i32 = read(value); } "
        "exf:read(value!i32)!i32;";
    const char* extern_value_signature =
        "exf:read(value!i32)!i32; f:main(value!i32) { v:result!i32 = read(value); }";
    const char* extern_raw_pointer_signature =
        "exf:write(value!*i32); f:main(pointer!*i32) { write(pointer); }";
    const char* extern_safe_reference_parameter_rejected =
        "exf:store(value!&i32); f:main() {}";
    const char* extern_safe_reference_return_rejected =
        "exf:get()!&i32; f:main() {}";
    const char* extern_callback_with_safe_reference_rejected =
        "exf:invoke(callback!f:(value!&i32)!void); f:main() {}";
    const char* extern_aggregate_with_callback_reference_rejected =
        "nt:RefGetter = f:(value!&i32)!void; "
        "struct:CallbackBox { v:handler!RefGetter } "
        "exf:invoke(box!CallbackBox); f:main() {}";
    const char* extern_any_safe_reference_argument_rejected =
        "exf:store(value!any); f:main(owner!i32) { store(&owner); }";
    return check_source(extern_use_before_declaration, 1) &&
        check_source(extern_value_signature, 0) &&
        check_source(extern_raw_pointer_signature, 0) &&
        check_source(extern_safe_reference_parameter_rejected, 1) &&
        check_source(extern_safe_reference_return_rejected, 1) &&
        check_source(extern_callback_with_safe_reference_rejected, 1) &&
        check_source(extern_aggregate_with_callback_reference_rejected, 1) &&
        check_source(extern_any_safe_reference_argument_rejected, 1);
}

static int check_extended_numeric_primitive_semantics(void) {
    const char* signed_128_type =
        "f:identity(value!i128)!i128 { ret value; } "
        "f:main(value!i128) { v:copy!i128 = identity(value); }";
    const char* unsigned_128_bitwise =
        "f:main(value!u128) { v:copy!u128 = value; "
        "v:masked!u128 = value & value; v:shifted!u128 = value <<< 1; }";
    const char* float_128_arithmetic =
        "f:main(value!f128) { v:copy!f128 = value + value; }";
    const char* decimal_128_arithmetic =
        "f:main(value!d128) { v:copy!d128 = value * value; }";
    const char* float_128_bitwise_rejected =
        "f:main(value!f128) { v:copy!f128 = value & value; }";
    const char* valid_char_type =
        "f:main() { v:letter!char = 'A'; }";
    const char* invalid_hex_storage_type =
        "f:main() { v:value!hex = 0x2A; }";
    const char* supported_type_spelling_matrix =
        "f:main() { "
        "v:a!i8; v:b!i16; v:c!i32; v:d!i64; v:e!i128; "
        "v:f!u8; v:g!u16; v:h!u32; v:i!u64; v:j!u128; "
        "v:k!f32; v:l!f64; v:m!f128; "
        "v:n!d32; v:o!d64; v:p!d128; "
        "v:q!bit; v:r!byte; v:s!8bit; v:t!16bit; "
        "v:u!32bit; v:v!64bit; v:w!128bit; "
        "v:x!1byte; v:y!2byte; v:z!4byte; "
        "v:aa!8byte; v:ab!16byte; "
        "v:ac!char; v:ad!bool; v:ae!str; "
        "v:af!Vec2; v:ag!Vec3; v:ah!Vec4; "
        "v:ai!map; v:aj!Map; v:ak!any; }";
    return check_source(signed_128_type, 0) &&
        check_source(unsigned_128_bitwise, 0) &&
        check_source(float_128_arithmetic, 0) &&
        check_source(decimal_128_arithmetic, 0) &&
        check_source(float_128_bitwise_rejected, 1) &&
        check_source(valid_char_type, 0) &&
        check_source(invalid_hex_storage_type, 1) &&
        check_source(supported_type_spelling_matrix, 0);
}

static int check_shift_operator_semantics(void) {
    const char* valid_u8_shift_at_width =
        "f:main(value!u8) { "
        "v:left!u8 = value <<< 8; v:right!u8 = value >>> 8; }";
    const char* valid_wide_shift_count =
        "f:main(value!u64,count!u32) { "
        "v:left!u64 = value <<< count; v:right!u64 = value >>> count; }";
    const char* invalid_fixed_bit_vector_shift =
        "f:main(value!8bit) { v:shifted!8bit = value <<< 7; }";
    const char* invalid_negative_shift =
        "f:main(value!u8) { v:shifted!u8 = value <<< -1; }";
    const char* invalid_float_shift_count =
        "f:main(value!u8) { v:shifted!u8 = value <<< 1.5; }";
    const char* invalid_scalar_bit_shift =
        "f:main(value!bit) { v:shifted!bit = value <<< 1; }";
    const char* invalid_raw_byte_shift =
        "f:main(value!byte) { v:shifted!byte = value <<< 1; }";
    const char* invalid_byte_block_shift =
        "f:main(value!2byte) { v:shifted!2byte = value <<< 1; }";
    return check_source(valid_u8_shift_at_width, 0) &&
        check_source(valid_wide_shift_count, 0) &&
        check_source(invalid_fixed_bit_vector_shift, 1) &&
        check_source(invalid_negative_shift, 1) &&
        check_source(invalid_float_shift_count, 1) &&
        check_source(invalid_scalar_bit_shift, 1) &&
        check_source(invalid_raw_byte_shift, 1) &&
        check_source(invalid_byte_block_shift, 1);
}

static uint32_t semantic_test_pointer_node_id(
    void* user_context, const ASTNode* node
) {
    uint64_t bits = (uint64_t)(uintptr_t)node;
    uint32_t id;
    (void)user_context;
    id = (uint32_t)(bits ^ (bits >> 32));
    return id ? id : 1u;
}

static int check_newtype_result_base_metadata(void) {
    const char* source =
        "nt:Base = u64; nt:Alias = Base; "
        "nt:Pointer = *i32; nt:Reference = &i32; "
        "nt:Callback = f:(id!u32)!bool; "
        "f:main() { v:value!Alias; }";
    ParserContext* parser = parser_init_source(
        "semantic-newtype-result-metadata.nxf",
        (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    SemanticResultType *base_type = NULL, *alias_type = NULL, *u64_type = NULL;
    SemanticResultType *pointer_alias = NULL, *reference_alias = NULL;
    SemanticResultType *callback_alias = NULL, *pointer_base = NULL;
    SemanticResultType *reference_base = NULL, *callback_base = NULL;
    uint32_t i;
    int ok;
    if (!parser || parser_run(parser) != 0 || !parser->ast_root) {
        if (parser) parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "semantic-newtype-result-metadata.nxf", NULL);
    semantic_attach_node_id_resolver(
        &semantic, semantic_test_pointer_node_id, NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    for (i = 0u; i < semantic.result.type_count; ++i) {
        SemanticResultType* type = &semantic.result.types[i];
        if (strcmp(type->name, "u64") == 0) u64_type = type;
        else if (strcmp(type->name, "Base") == 0) base_type = type;
        else if (strcmp(type->name, "Alias") == 0) alias_type = type;
        else if (strcmp(type->name, "Pointer") == 0) pointer_alias = type;
        else if (strcmp(type->name, "Reference") == 0) reference_alias = type;
        else if (strcmp(type->name, "Callback") == 0) callback_alias = type;
        if (type->kind == SEMANTIC_RESULT_TYPE_POINTER) pointer_base = type;
        else if (type->kind == SEMANTIC_RESULT_TYPE_REFERENCE) reference_base = type;
        else if (type->kind == SEMANTIC_RESULT_TYPE_FUNCTION) callback_base = type;
    }
    ok = semantic.error_count == 0 &&
        semantic.result_status == SEMANTIC_RESULT_OK &&
        u64_type && base_type && alias_type &&
        u64_type->kind == SEMANTIC_RESULT_TYPE_PRIMITIVE &&
        base_type->kind == SEMANTIC_RESULT_TYPE_USER_DEFINED &&
        alias_type->kind == SEMANTIC_RESULT_TYPE_USER_DEFINED &&
        base_type->base_type_id == u64_type->type_id &&
        alias_type->base_type_id == base_type->type_id &&
        pointer_alias && reference_alias && callback_alias &&
        pointer_base && reference_base && callback_base &&
        pointer_alias->base_type_id == pointer_base->type_id &&
        reference_alias->base_type_id == reference_base->type_id &&
        callback_alias->base_type_id == callback_base->type_id &&
        base_type->referenced_symbol_id != 0u &&
        alias_type->referenced_symbol_id != 0u &&
        pointer_alias->referenced_symbol_id != 0u &&
        reference_alias->referenced_symbol_id != 0u &&
        callback_alias->referenced_symbol_id != 0u &&
        base_type->referenced_symbol_id != alias_type->referenced_symbol_id;
    if (!ok) {
        fprintf(stderr,
            "NewType SemanticResult metadata mismatch: errors=%d status=%d types=%u\n",
            semantic.error_count, (int)semantic.result_status,
            semantic.result.type_count);
    }
    semantic_destroy(&semantic);
    parser_destroy(parser);
    if (!ok) return 0;
    {
        ASTVarDecl* array_decl = (ASTVarDecl*)ast_create(AST_VAR_DECL);
        SemanticResultType* encoded_array_type = NULL;
        int array_ok;
        if (!array_decl) return 0;
        array_decl->name = semantic_test_copy_text("encoded_items");
        array_decl->type_name = semantic_test_copy_text("array|i32[3]");
        array_decl->is_hard_typed = 1;
        if (!array_decl->name || !array_decl->type_name) {
            ast_destroy((ASTNode*)array_decl);
            return 0;
        }
        semantic_init(&semantic, "semantic-encoded-array-result-type.nxf", NULL);
        semantic_attach_node_id_resolver(
            &semantic, semantic_test_pointer_node_id, NULL);
        semantic_check(&semantic, (ASTNode*)array_decl);
        for (i = 0u; i < semantic.result.type_count; ++i) {
            SemanticResultType* type = &semantic.result.types[i];
            if (strcmp(type->name, "array|i32[3]") == 0)
                encoded_array_type = type;
        }
        array_ok = semantic.error_count == 0 && encoded_array_type &&
            encoded_array_type->kind == SEMANTIC_RESULT_TYPE_AGGREGATE;
        semantic_destroy(&semantic);
        ast_destroy((ASTNode*)array_decl);
        if (!array_ok) {
            fprintf(stderr,
                "Encoded array SemanticResult type mismatch: errors=%d\\n",
                semantic.error_count);
            return 0;
        }
    }
    return 1;
}

static int check_map_key_type_semantics(void) {
    const char* map_scalar_keys =
        "f:main() { v:values!map; values[1]; values[1.5]; "
        "values[true]; values['x']; values[\"name\"]; }";
    const char* map_enum_key =
        "enum:Mode { Read, Write } "
        "f:main(mode!Mode) { v:values!map; values[mode]; }";
    const char* map_newtype_scalar_key =
        "nt:UserId = u64; f:main(id!UserId) { v:values!map; values[id]; }";
    const char* map_aggregate_key_rejected =
        "struct:Point { v:x!i32 } "
        "f:main(point!Point) { v:values!map; values[point]; }";
    const char* map_safe_reference_key_rejected =
        "f:main(owner!i32) { v:values!map; values[&owner]; }";
    const char* map_null_key_rejected =
        "f:main() { v:values!map; values[null]; }";
    return check_source(map_scalar_keys, 0) &&
        check_source(map_enum_key, 0) &&
        check_source(map_newtype_scalar_key, 0) &&
        check_source(map_aggregate_key_rejected, 1) &&
        check_source(map_safe_reference_key_rejected, 1) &&
        check_source(map_null_key_rejected, 1);
}

static int check_nested_pointer_and_reference_type_variable_semantics(void) {
    const char* raw_pointer_type_variable =
        "f:identity(value!*t)!*t { ret value; } "
        "f:main(pointer!*i32) { v:copy!*i32 = identity(pointer); }";
    const char* safe_reference_type_variable =
        "f:identity(value!&t)!&t { ret value; } "
        "f:main(owner!i32) { v:copy!&i32 = identity(&owner); "
        "v:read!i32 = *copy; }";
    const char* repeated_pointer_type_variable_mismatch =
        "f:pair(first!*t,second!*t)!*t { ret first; } "
        "f:main(first!*i32,second!*u32) { v:copy!*i32 = pair(first,second); }";
    return check_source(raw_pointer_type_variable, 0) &&
        check_source(safe_reference_type_variable, 0) &&
        check_source(repeated_pointer_type_variable_mismatch, 1);
}

static int check_nested_callback_type_variable_semantics(void) {
    const char* nested_callback_parameter_unifies =
        "f:apply(handler!f:(test!f:(value!t)!bool)!bool, "
        "callback!f:(value!i32)!bool)!bool { ret handler(callback); } "
        "f:main(handler!f:(test!f:(value!i32)!bool)!bool, "
        "callback!f:(value!i32)!bool)!bool { ret apply(handler, callback); }";
    const char* nested_callback_return_unifies =
        "f:invoke(callback!f:(value!t)!bool,value!t)!bool { ret callback(value); } "
        "f:main(callback!f:(value!i32)!bool)!bool { ret invoke(callback, 1); }";
    const char* nested_callback_shape_mismatch =
        "f:apply(handler!f:(test!f:(value!t)!bool)!bool, "
        "callback!f:(value!i32)!bool)!bool { ret handler(callback); } "
        "f:main(handler!f:(test!f:(value!i32)!bool)!bool, "
        "callback!f:(value!u32)!bool)!bool { ret apply(handler, callback); }";
    return check_source(nested_callback_parameter_unifies, 0) &&
        check_source(nested_callback_return_unifies, 0) &&
        check_source(nested_callback_shape_mismatch, 1);
}

static int check_callback_null_refinement_semantics(void) {
    const char* not_null_branch_allows_call =
        "nt:Callback = f:(id!u32)!bool; "
        "f:apply(callback!Callback,id!u32)!bool { "
        "if (callback != null) { ret callback(id); } els { ret false; } }";
    const char* null_guard_return_allows_call_after_if =
        "nt:Callback = f:(id!u32)!bool; "
        "f:apply(callback!Callback,id!u32)!bool { "
        "if (callback == null) { ret false; } ret callback(id); }";
    const char* nullable_callback_argument_requires_guard =
        "nt:Callback = f:(id!u32)!bool; "
        "f:pass(callback!Callback) {} "
        "f:main() { v:local!Callback = null; pass(local); }";
    const char* guarded_callback_argument_is_allowed =
        "nt:Callback = f:(id!u32)!bool; "
        "f:pass(callback!Callback) {} "
        "f:main(callback!Callback) { "
        "if (callback != null) { pass(callback); } }";
    const char* callback_null_return_is_rejected =
        "nt:Callback = f:(id!u32)!bool; f:make()!Callback { ret null; }";
    return check_source(not_null_branch_allows_call, 0) &&
        check_source(null_guard_return_allows_call_after_if, 0) &&
        check_source(nullable_callback_argument_requires_guard, 1) &&
        check_source(guarded_callback_argument_is_allowed, 0) &&
        check_source(callback_null_return_is_rejected, 1);
}

static int check_callback_nullability_semantics(void) {
    const char* callback_null_initializer_is_allowed =
        "nt:Callback = f:(id!u32)!bool; f:main() { v:callback!Callback = null; }";
    const char* callback_null_cannot_be_invoked =
        "nt:Callback = f:(id!i32)!bool; "
        "f:main() { v:callback!Callback = null; callback(1); }";
    const char* callback_alias_copy_preserves_non_null_state =
        "nt:Callback = f:(id!u32)!bool; "
        "f:apply(callback!Callback,id!u32)!bool { "
        "v:local!Callback = callback; ret local(id); }";
    return check_source(callback_null_initializer_is_allowed, 0) &&
        check_source(callback_null_cannot_be_invoked, 1) &&
        check_source(callback_alias_copy_preserves_non_null_state, 0);
}

static int check_reference_initialization_control_flow_semantics(void) {
    const char* return_branch_does_not_block_other_path =
        "f:main(owner!i32, condition!bool) { v:reference!&i32; "
        "if (condition) { ret; } els { reference = &owner; } "
        "v:value!i32 = *reference; }";
    const char* select_default_initializes_all_paths =
        "f:main(choice!i32, owner!i32) { v:reference!&i32; "
        "select(choice) { 1 -> { reference = &owner; } "
        "default -> { reference = &owner; } } "
        "v:value!i32 = *reference; }";
    const char* select_without_default_does_not_initialize =
        "f:main(choice!i32, owner!i32) { v:reference!&i32; "
        "select(choice) { 1 -> { reference = &owner; } } "
        "v:value!i32 = *reference; }";
    const char* scan_else_initializes_all_paths =
        "enum:State { Ready, Empty } "
        "f:main(choice!State, owner!i32) { v:reference!&i32; "
        "scan(choice) { Ready -> { reference = &owner; }, "
        "else -> { reference = &owner; } } "
        "v:value!i32 = *reference; }";
    const char* scan_without_else_does_not_initialize =
        "enum:State { Ready, Empty } "
        "f:main(choice!State, owner!i32) { v:reference!&i32; "
        "scan(choice) { Ready -> { reference = &owner; }, "
        "Empty -> { reference = &owner; } } "
        "v:value!i32 = *reference; }";
    return check_source(return_branch_does_not_block_other_path, 0) &&
        check_source(select_default_initializes_all_paths, 0) &&
        check_source(select_without_default_does_not_initialize, 1) &&
        check_source(scan_else_initializes_all_paths, 0) &&
        check_source(scan_without_else_does_not_initialize, 1);
}

static int check_raw_pointer_initialization_and_nullability_semantics(void) {
    const char* uninitialized_pointer_read =
        "f:main() { v:pointer!*i32; v:value!i32 = *pointer; }";
    const char* uninitialized_pointer_write =
        "f:main() { v:pointer!*i32; *pointer = 1; }";
    const char* null_pointer_initializer_and_assignment =
        "f:main() { v:pointer!*i32 = null; pointer = null; }";
    const char* null_pointer_comparison =
        "f:main() { v:pointer!*i32 = null; v:empty!bool = pointer == null; }";
    const char* safe_reference_rejects_null =
        "f:main() { v:reference!&i32 = null; }";
    const char* raw_pointer_parameter_accepts_null =
        "f:take(pointer!*i32) {} f:main() { take(null); }";
    return check_source(uninitialized_pointer_read, 1) &&
        check_source(uninitialized_pointer_write, 1) &&
        check_source(null_pointer_initializer_and_assignment, 0) &&
        check_source(null_pointer_comparison, 0) &&
        check_source(safe_reference_rejects_null, 1) &&
        check_source(raw_pointer_parameter_accepts_null, 0);
}

static int check_safe_reference_container_storage_rejection_semantics(void) {
    const char* array_of_safe_references =
        "f:main() { v:refs[1]!&i32; }";
    const char* newtype_alias_binding =
        "nt:Ref = &i32; f:main() { v:reference!Ref; }";
    const char* map_safe_reference_value =
        "f:main(owner!i32) { v:items!map; items[\"ref\"] = &owner; }";
    const char* any_member_safe_reference_value =
        "struct:Box { v:value!any } "
        "f:main(owner!i32) { v:box!Box; box.value = &owner; }";
    const char* inferred_array_of_safe_references =
        "f:main(owner!i32) { v:refs = [&owner]; }";
    return check_source(array_of_safe_references, 1) &&
        check_source(newtype_alias_binding, 1) &&
        check_source(map_safe_reference_value, 1) &&
        check_source(any_member_safe_reference_value, 1) &&
        check_source(inferred_array_of_safe_references, 1);
}

static int check_safe_reference_definite_initialization_semantics(void) {
    const char* uninitialized_dereference =
        "f:main() { v:reference!&i32; v:value!i32 = *reference; }";
    const char* uninitialized_call_argument =
        "f:read(value!&i32) {} f:main() { v:reference!&i32; read(reference); }";
    const char* assigned_before_read =
        "f:main() { v:owner!i32 = 1; v:reference!&i32; "
        "reference = &owner; v:value!i32 = *reference; }";
    const char* both_if_paths_initialize =
        "f:main() { v:owner!i32 = 1; v:reference!&i32; "
        "if (true) { reference = &owner; } els { reference = &owner; } "
        "v:value!i32 = *reference; }";
    const char* one_if_path_initializes =
        "f:main() { v:owner!i32 = 1; v:reference!&i32; "
        "if (true) { reference = &owner; } "
        "v:value!i32 = *reference; }";
    const char* invalid_assignment_does_not_initialize =
        "f:main() { v:reference!&i32; reference = 7; "
        "v:value!i32 = *reference; }";
    const char* parameter_is_initialized =
        "f:read(value!&i32) { v:copy!i32 = *value; } "
        "f:main() { v:owner!i32 = 1; read(&owner); }";
    const char* loop_body_initialization_not_definite =
        "f:main() { v:owner!i32 = 1; v:reference!&i32; "
        "loop(true) { reference = &owner; break; } "
        "v:value!i32 = *reference; }";
    const char* loop_preserves_prior_initialization =
        "f:main() { v:owner!i32 = 1; v:reference!&i32 = &owner; "
        "loop(true) { break; } v:value!i32 = *reference; }";
    return check_source(uninitialized_dereference, 1) &&
        check_source(uninitialized_call_argument, 1) &&
        check_source(assigned_before_read, 0) &&
        check_source(both_if_paths_initialize, 0) &&
        check_source(one_if_path_initializes, 1) &&
        check_source(invalid_assignment_does_not_initialize, 2) &&
        check_source(parameter_is_initialized, 0) &&
        check_source(loop_body_initialization_not_definite, 1) &&
        check_source(loop_preserves_prior_initialization, 0);
}

static int check_borrow_expression_semantics(void) {
    const char* shared_borrow =
        "f:main() { v:value!i32 = 1; v:read_only!&i32 = &value; v:copy!i32 = *read_only; }";
    const char* mutable_borrow =
        "f:main() { v:value!i32 = 1; v:editable!&muti32 = &mut value; *editable = 7; }";
    const char* mutable_borrow_const =
        "f:main() { c:value!i32 = 1; v:editable!&muti32 = &mut value; }";
    const char* borrow_temporary =
        "f:main() { v:read_only!&i32 = &(1 + 2); }";
    const char* shared_borrow_write =
        "f:main() { v:value!i32 = 1; v:read_only!&i32 = &value; *read_only = 7; }";
    return check_source(shared_borrow, 0) &&
        check_source(mutable_borrow, 0) &&
        check_source(mutable_borrow_const, 1) &&
        check_source(borrow_temporary, 1) &&
        check_source(shared_borrow_write, 1);
}

static int check_shared_reborrow_semantics(void) {
    const char* shared_dereference_reborrow_allowed =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; v:s!&i32 = &*r; }";
    const char* shared_reborrow_keeps_owner_conflict =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; "
        "v:s!&i32 = &*r; v:edit!&muti32 = &mut owner; }";
    const char* chained_shared_dereference_reborrow_allowed =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; "
        "v:s!&i32 = &*r; v:t!&i32 = &*s; }";
    const char* chained_shared_reborrow_keeps_owner_conflict =
        "f:main() { v:owner!i32 = 1; v:r!&i32 = &owner; "
        "v:s!&i32 = &*r; v:t!&i32 = &*s; "
        "v:edit!&muti32 = &mut owner; }";
    const char* mutable_reference_cannot_be_shared_reborrowed =
        "f:main() { v:owner!i32 = 1; v:r!&muti32 = &mut owner; "
        "v:s!&i32 = &*r; }";
    return check_source(shared_dereference_reborrow_allowed, 0) &&
        check_source(shared_reborrow_keeps_owner_conflict, 1) &&
        check_source(chained_shared_dereference_reborrow_allowed, 0) &&
        check_source(chained_shared_reborrow_keeps_owner_conflict, 1) &&
        check_source(mutable_reference_cannot_be_shared_reborrowed, 1);
}

static int check_reference_member_projection_semantics(void) {
    const char* shared_reference_member_read =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:r!&Point = &point; v:value!i32 = r.x; }";
    const char* shared_reference_unknown_member =
        "f:main() { struct:Point { v:x!i32 } "
        "v:point!Point; v:r!&Point = &point; v:value!i32 = r.missing; }";
    const char* shared_projection_reborrow_blocks_overlapping_mutable =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:r!&Point = &point; "
        "v:field!&i32 = &r.x; v:write!&muti32 = &mut point.x; }";
    const char* shared_projection_reborrow_allows_disjoint_mutable =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:outer!Point; v:other!Point; v:r!&Point = &outer; "
        "v:field!&i32 = &r.x; r = &other; "
        "v:write!&muti32 = &mut outer.y; }";
    const char* child_projection_keeps_original_origin_after_parent_reassignment =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:outer!Point; v:replacement!Point; "
        "v:r!&Point = &outer; v:field!&i32 = &r.x; "
        "r = &replacement; "
        "v:write!&muti32 = &mut outer.x; }";
    const char* child_projection_preserves_first_branch_origin =
        "f:main(flag!bool) { struct:Point { v:x!i32, v:y!i32 } "
        "v:first!Point; v:second!Point; v:replacement!Point; "
        "v:r!&Point = &first; "
        "if (flag) { r = &first; } else { r = &second; } "
        "v:field!&i32 = &r.x; r = &replacement; "
        "v:write!&muti32 = &mut first.x; }";
    const char* child_projection_preserves_second_branch_origin =
        "f:main(flag!bool) { struct:Point { v:x!i32, v:y!i32 } "
        "v:first!Point; v:second!Point; v:replacement!Point; "
        "v:r!&Point = &first; "
        "if (flag) { r = &first; } else { r = &second; } "
        "v:field!&i32 = &r.x; r = &replacement; "
        "v:write!&muti32 = &mut second.x; }";
    const char* array_reference_projection_disjoint_index =
        "f:main() { v:items[3]!i32 = [10, 20, 30]; "
        "v:r!&array|i32[3] = &items; "
        "v:element!&i32 = &r[1]; v:write!&muti32 = &mut items[0]; }";
    const char* array_reference_projection_overlap_index =
        "f:main() { v:items[3]!i32 = [10, 20, 30]; "
        "v:r!&array|i32[3] = &items; "
        "v:element!&i32 = &r[1]; v:write!&muti32 = &mut items[1]; }";
    const char* array_reference_projection_dynamic_index =
        "f:main(index!i32) { v:items[3]!i32 = [10, 20, 30]; "
        "v:r!&array|i32[3] = &items; "
        "v:element!&i32 = &r[index]; v:write!&muti32 = &mut items[2]; }";
    return check_source(shared_reference_member_read, 0) &&
        check_source(shared_reference_unknown_member, 1) &&
        check_source(shared_projection_reborrow_blocks_overlapping_mutable, 1) &&
        check_source(shared_projection_reborrow_allows_disjoint_mutable, 0) &&
        check_source(child_projection_keeps_original_origin_after_parent_reassignment, 1) &&
        check_source(child_projection_preserves_first_branch_origin, 1) &&
        check_source(child_projection_preserves_second_branch_origin, 1) &&
        check_source(array_reference_projection_disjoint_index, 0) &&
        check_source(array_reference_projection_overlap_index, 1) &&
        check_source(array_reference_projection_dynamic_index, 1);
}

static int check_call_temporary_borrow_semantics(void) {
    const char* shared_call_conflicts_with_active_mutable =
        "f:read(value!&i32) {} f:main() { v:owner!i32 = 1; "
        "v:edit!&muti32 = &mut owner; read(&owner); }";
    const char* mutable_call_conflicts_with_active_shared =
        "f:edit(value!&muti32) {} f:main() { v:owner!i32 = 1; "
        "v:read_only!&i32 = &owner; edit(&mut owner); }";
    const char* two_shared_call_arguments_are_allowed =
        "f:read(a!&i32, b!&i32) {} f:main() { v:owner!i32 = 1; read(&owner, &owner); }";
    const char* conflicting_call_arguments_are_rejected =
        "f:edit(a!&muti32, b!&i32) {} f:main() { v:owner!i32 = 1; edit(&mut owner, &owner); }";
    const char* named_temporary_shared_borrow =
        "f:read(value!&i32) {} f:main() { v:owner!i32 = 1; read(value:&owner); }";
    const char* named_temporary_mutable_borrow =
        "f:edit(value!&muti32) {} f:main() { v:owner!i32 = 1; edit(value:&mut owner); }";
    const char* call_borrow_mutability_mismatch =
        "f:read(value!&i32) {} f:main() { v:owner!i32 = 1; read(&mut owner); }";
    const char* call_temporary_borrow_is_released =
        "f:edit(value!&muti32) {} f:main() { v:owner!i32 = 1; "
        "edit(&mut owner); v:read!&i32 = &owner; }";
    const char* disjoint_temporary_mutable_member_borrows =
        "f:edit(a!&muti32, b!&muti32) {} "
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; edit(&mut point.x, &mut point.y); }";
    const char* overlapping_temporary_member_borrows =
        "f:edit(a!&muti32, b!&muti32) {} "
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; edit(&mut point.x, &mut point.x); }";
    return check_source(shared_call_conflicts_with_active_mutable, 1) &&
        check_source(mutable_call_conflicts_with_active_shared, 1) &&
        check_source(two_shared_call_arguments_are_allowed, 0) &&
        check_source(conflicting_call_arguments_are_rejected, 1) &&
        check_source(named_temporary_shared_borrow, 0) &&
        check_source(named_temporary_mutable_borrow, 0) &&
        check_source(call_borrow_mutability_mismatch, 2) &&
        check_source(call_temporary_borrow_is_released, 0) &&
        check_source(disjoint_temporary_mutable_member_borrows, 0) &&
        check_source(overlapping_temporary_member_borrows, 1);
}

static int check_borrow_diagnostic_guidance(void) {
    const char* source =
        "f:main() {\n"
        "  v:owner!i32 = 1;\n"
        "  v:a!&i32 = &owner;\n"
        "  v:b!&muti32 = &mut owner;\n"
        "}";
    ParserContext* parser = parser_init_source(
        "borrow-diagnostic.nxf", (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    Error* error;
    int ok;
    if (!parser) return 0;
    if (parser_run(parser) != 0 || !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "borrow-diagnostic.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    error = semantic.error_head;
    ok = semantic.error_count == 1 && error &&
        error->line == 4 && error->col > 0 &&
        error->message && strstr(error->message, "borrow 'a' at path '<whole value>' is still active") != NULL &&
        error->message && strstr(error->message, "path '<whole value>'") != NULL &&
        error->suggestion && strstr(error->suggestion, "End the scope") != NULL;
    if (!ok) {
        fprintf(stderr,
            "Borrow diagnostic guidance mismatch: count=%d line=%d col=%d message=%s suggestion=%s\n",
            semantic.error_count, error ? error->line : 0,
            error ? error->col : 0,
            error && error->message ? error->message : "(null)",
            error && error->suggestion ? error->suggestion : "(null)");
    }
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_reference_parameter_escape_diagnostic(void) {
    const char* source =
        "v:global_owner!i32 = 0;\n"
        "v:global_ref!&i32 = &global_owner;\n"
        "f:store(value!&i32) {\n"
        "  global_ref = value;\n"
        "}";
    ParserContext* parser = parser_init_source(
        "reference-escape.nxf", (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    Error* error;
    int ok;
    if (!parser) return 0;
    if (parser_run(parser) != 0 || !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "reference-escape.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    error = semantic.error_head;
    ok = semantic.error_count == 1 && error &&
        error->line == 4 && error->col > 0 &&
        error->message &&
        strstr(error->message, "longer-lived binding 'global_ref'") != NULL &&
        error->suggestion &&
        strstr(error->suggestion, "Do not store") != NULL;
    if (!ok) {
        fprintf(stderr,
            "Reference escape diagnostic mismatch: count=%d line=%d col=%d message=%s suggestion=%s\n",
            semantic.error_count, error ? error->line : 0,
            error ? error->col : 0,
            error && error->message ? error->message : "(null)",
            error && error->suggestion ? error->suggestion : "(null)");
    }
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_borrow_overlap_and_scope_semantics(void) {
    const char* multiple_shared =
        "f:main() { v:owner!i32 = 1; v:a!&i32 = &owner; v:b!&i32 = &owner; }";
    const char* shared_then_mutable =
        "f:main() { v:owner!i32 = 1; v:a!&i32 = &owner; v:b!&muti32 = &mut owner; }";
    const char* mutable_then_shared =
        "f:main() { v:owner!i32 = 1; v:a!&muti32 = &mut owner; v:b!&i32 = &owner; }";
    const char* two_mutable =
        "f:main() { v:owner!i32 = 1; v:a!&muti32 = &mut owner; v:b!&muti32 = &mut owner; }";
    const char* owner_write_during_shared =
        "f:main() { v:owner!i32 = 1; v:a!&i32 = &owner; owner = 2; }";
    const char* owner_write_during_mutable =
        "f:main() { v:owner!i32 = 1; v:a!&muti32 = &mut owner; owner = 2; }";
    const char* owner_compound_write_during_borrow =
        "f:main() { v:owner!i32 = 1; v:a!&i32 = &owner; owner += 1; }";
    const char* borrow_released_at_scope_exit =
        "f:main() { v:owner!i32 = 1; { v:a!&muti32 = &mut owner; } "
        "v:b!&muti32 = &mut owner; }";
    return check_source(multiple_shared, 0) &&
        check_source(shared_then_mutable, 1) &&
        check_source(mutable_then_shared, 1) &&
        check_source(two_mutable, 1) &&
        check_source(owner_write_during_shared, 1) &&
        check_source(owner_write_during_mutable, 1) &&
        check_source(owner_compound_write_during_borrow, 1) &&
        check_source(borrow_released_at_scope_exit, 0);
}

static int check_mutable_borrow_owner_access_semantics(void) {
    const char* owner_read_during_mutable_borrow =
        "f:main() { v:owner!i32 = 1; v:reference!&muti32 = &mut owner; "
        "v:copy!i32 = owner; }";
    const char* owner_read_during_shared_borrow =
        "f:main() { v:owner!i32 = 1; v:reference!&i32 = &owner; "
        "v:copy!i32 = owner; }";
    const char* member_read_during_mutable_borrow =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:reference!&muti32 = &mut point.x; "
        "v:copy!i32 = point.y; }";
    const char* index_read_during_mutable_borrow =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:reference!&muti32 = &mut items[0]; v:copy!i32 = items[1]; }";
    const char* overlapping_member_read_during_mutable_borrow =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:reference!&muti32 = &mut point.x; "
        "v:copy!i32 = point.x; }";
    const char* overlapping_index_read_during_mutable_borrow =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:reference!&muti32 = &mut items[0]; v:copy!i32 = items[0]; }";
    const char* assignment_target_read_does_not_duplicate =
        "f:main() { v:owner!i32 = 1; v:reference!&muti32 = &mut owner; "
        "owner = owner + 1; }";
    return check_source(owner_read_during_mutable_borrow, 1) &&
        check_source(owner_read_during_shared_borrow, 0) &&
        check_source(member_read_during_mutable_borrow, 0) &&
        check_source(index_read_during_mutable_borrow, 0) &&
        check_source(overlapping_member_read_during_mutable_borrow, 1) &&
        check_source(overlapping_index_read_during_mutable_borrow, 1) &&
        check_source(assignment_target_read_does_not_duplicate, 1);
}

static int check_reference_initializer_provenance_semantics(void) {
    const char* shared_reference_initializer_copy_tracks_owner =
        "f:main() { v:owner!i32 = 1; "
        "v:reference!&i32 = &owner; v:copy!&i32 = reference; "
        "v:edit!&muti32 = &mut owner; }";
    const char* exclusive_reference_initializer_copy_is_rejected =
        "f:main() { v:owner!i32 = 1; "
        "v:reference!&muti32 = &mut owner; "
        "v:copy!&muti32 = reference; }";
    const char* returned_reference_parameter_preserves_origin =
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:main() { v:owner!i32 = 1; "
        "v:copy!&i32 = identity(&owner); "
        "v:edit!&muti32 = &mut owner; }";
    const char* returned_reference_binding_preserves_origin =
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:main() { v:owner!i32 = 1; v:reference!&i32 = &owner; "
        "v:copy!&i32 = identity(reference); "
        "v:edit!&muti32 = &mut owner; }";
    const char* returned_mutable_reference_parameter_preserves_origin =
        "f:identity(value!&muti32)!&muti32 { ret value; } "
        "f:main() { v:owner!i32 = 1; "
        "v:copy!&muti32 = identity(&mut owner); "
        "v:read!&i32 = &owner; }";
    const char* call_return_reference_without_verified_origin_is_rejected =
        "f:make()!&i32 { ret null; } "
        "f:main() { v:copy!&i32 = make(); }";
    const char* named_return_parameter_maps_to_correct_argument =
        "f:pick_ref(left!&i32,right!&i32)!&i32 { ret right; } "
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:copy!&i32 = pick_ref(right:&second,left:&first); "
        "v:edit!&muti32 = &mut second; }";
    const char* inconsistent_return_origins_are_rejected =
        "f:choose(left!&i32,right!&i32)!&i32 { "
        "if (true) { ret left; } els { ret right; } } "
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:copy!&i32 = choose(&first,&second); }";
    const char* consistent_branch_return_origin_is_preserved =
        "f:identity(value!&i32)!&i32 { "
        "if (true) { ret value; } els { ret value; } } "
        "f:main() { v:owner!i32 = 1; "
        "v:copy!&i32 = identity(&owner); "
        "v:edit!&muti32 = &mut owner; }";
    const char* group_instance_method_return_origin_is_preserved =
        "f:main() { group Storage { "
        "identity => f:identity_impl(self,value!&i32)!&i32 { ret value; } "
        "run => f:run_impl(self) { v:owner!i32 = 1; "
        "v:copy!&i32 = self.identity(&owner); "
        "v:edit!&muti32 = &mut owner; ret; } "
        "struct:Storage { v:value!i32 } } }";
    return check_source(shared_reference_initializer_copy_tracks_owner, 1) &&
        check_source(exclusive_reference_initializer_copy_is_rejected, 1) &&
        check_source(returned_reference_parameter_preserves_origin, 1) &&
        check_source(returned_reference_binding_preserves_origin, 1) &&
        check_source(returned_mutable_reference_parameter_preserves_origin, 1) &&
        check_source(call_return_reference_without_verified_origin_is_rejected, 2) &&
        check_source(named_return_parameter_maps_to_correct_argument, 1) &&
        check_source(inconsistent_return_origins_are_rejected, 1) &&
        check_source(consistent_branch_return_origin_is_preserved, 1) &&
        check_source(group_instance_method_return_origin_is_preserved, 1);
}

static int check_safe_reference_return_lifetime_semantics(void) {
    const char* returning_local_reference_is_rejected =
        "f:get()!&i32 { v:owner!i32 = 1; ret &owner; }";
    const char* returning_local_reference_binding_is_rejected =
        "f:get()!&i32 { v:owner!i32 = 1; "
        "v:reference!&i32 = &owner; ret reference; }";
    const char* returning_reference_parameter_preserves_forwarding =
        "f:identity(value!&i32)!&i32 { ret value; }";
    const char* returning_local_alias_of_reference_parameter_is_supported =
        "f:identity(value!&i32)!&i32 { v:local!&i32 = value; ret local; }";
    const char* returning_local_alias_chain_of_reference_parameter_is_supported =
        "f:identity(value!&i32)!&i32 { v:first!&i32 = value; "
        "v:second!&i32 = first; ret second; }";
    const char* returning_rebound_local_alias_is_rejected =
        "f:identity(value!&i32, other!&i32)!&i32 { "
        "v:local!&i32 = value; local = other; ret local; } "
        "f:main(value!&i32, other!&i32) { "
        "v:copy!&i32 = identity(value, other); }";
    const char* forwarding_local_alias_return_preserves_origin =
        "f:identity(value!&i32)!&i32 { v:local!&i32 = value; ret local; } "
        "f:forward(value!&i32) { v:copy!&i32 = identity(value); }";
    const char* forwarding_call_preserves_parameter_origin =
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:forward(value!&i32) { v:local!&i32 = identity(value); }";
    const char* parameter_reference_cannot_escape_to_global =
        "v:global_owner!i32 = 0; v:global_ref!&i32 = &global_owner; f:store(value!&i32) { global_ref = value; }";
    const char* local_alias_of_parameter_cannot_escape_to_global =
        "v:global_owner!i32 = 0; v:global_ref!&i32 = &global_owner; f:store(value!&i32) { "
        "v:local!&i32 = value; global_ref = local; }";
    const char* returned_call_reference_cannot_escape_to_global =
        "v:global_owner!i32 = 0; v:global_ref!&i32 = &global_owner; "
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:store(value!&i32) { v:local!&i32 = identity(value); "
        "global_ref = local; }";
    const char* returned_shared_reference_preserves_branch_origins =
        "f:identity(value!&i32)!&i32 { ret value; } "
        "f:main(first!i32, second!i32, third!i32, flag!bool) { "
        "v:reference!&i32 = &first; "
        "if (flag) { reference = &second; } els { reference = &third; } "
        "v:returned!&i32 = identity(reference); "
        "v:edit_second!&muti32 = &mut second; "
        "v:edit_third!&muti32 = &mut third; }";
    return check_source(returning_local_reference_is_rejected, 1) &&
        check_source(returning_local_reference_binding_is_rejected, 1) &&
        check_source(returning_reference_parameter_preserves_forwarding, 0) &&
        check_source(returning_local_alias_of_reference_parameter_is_supported, 0) &&
        check_source(returning_local_alias_chain_of_reference_parameter_is_supported, 0) &&
        check_source(returning_rebound_local_alias_is_rejected, 1) &&
        check_source(forwarding_local_alias_return_preserves_origin, 0) &&
        check_source(forwarding_call_preserves_parameter_origin, 0) &&
        check_source(parameter_reference_cannot_escape_to_global, 1) &&
        check_source(local_alias_of_parameter_cannot_escape_to_global, 1) &&
        check_source(returned_call_reference_cannot_escape_to_global, 1) &&
        check_source(returned_shared_reference_preserves_branch_origins, 2);
}

static int check_reference_reassignment_and_lifetime_semantics(void) {
    const char* reassignment_releases_previous_owner =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&i32 = &first; reference = &second; "
        "v:edit!&muti32 = &mut first; }";
    const char* reassignment_tracks_new_owner =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&i32 = &first; reference = &second; "
        "v:edit!&muti32 = &mut second; }";
    const char* copying_shared_reference_tracks_owner =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:a!&i32 = &first; v:b!&i32 = &second; a = b; "
        "v:old_edit!&muti32 = &mut first; "
        "v:new_edit!&muti32 = &mut second; }";
    const char* copying_exclusive_reference_is_rejected =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:a!&muti32 = &mut first; v:b!&muti32 = &mut second; a = b; }";
    const char* outer_reference_cannot_escape_inner_owner =
        "f:main() { v:reference!&i32; "
        "{ v:inner!i32 = 1; reference = &inner; } }";
    const char* later_declared_owner_cleanup_is_safe =
        "f:main() { v:reference!&i32; v:owner!i32 = 1; reference = &owner; }";
    const char* mutable_reassignment_releases_old_owner =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&muti32 = &mut first; reference = &mut second; "
        "v:read!&i32 = &first; }";
    const char* mutable_reassignment_tracks_new_owner =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&muti32 = &mut first; reference = &mut second; "
        "v:edit!&muti32 = &mut second; }";
    const char* failed_reassignment_preserves_old_borrow =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&i32 = &first; reference = second; "
        "v:edit!&muti32 = &mut first; }";
    const char* failed_mutable_reassignment_preserves_old_borrow =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:reference!&muti32 = &mut first; reference = second; "
        "v:edit!&muti32 = &mut first; }";
    const char* conflicting_mutable_reassignment_preserves_old_borrow =
        "f:main() { v:first!i32 = 1; v:second!i32 = 2; "
        "v:blocker!&i32 = &second; "
        "v:reference!&muti32 = &mut first; reference = &mut second; "
        "v:edit!&muti32 = &mut first; }";
    const char* branch_reassignment_preserves_all_possible_mutable_borrows =
        "f:main(first!i32, second!i32, third!i32, flag!bool) { "
        "v:reference!&muti32 = &mut first; "
        "if (flag) { reference = &mut second; } "
        "els { reference = &mut third; } "
        "v:edit_second!&muti32 = &mut second; "
        "v:edit_third!&muti32 = &mut third; }";
    const char* conditional_mutable_reassignment_keeps_incoming_and_new_borrows =
        "f:main(first!i32, second!i32, flag!bool) { "
        "v:reference!&muti32 = &mut first; "
        "if (flag) { reference = &mut second; } "
        "v:edit_first!&muti32 = &mut first; "
        "v:edit_second!&muti32 = &mut second; }";
    const char* select_reassignment_preserves_all_possible_mutable_borrows =
        "f:main(first!i32, second!i32, third!i32, flag!bool) { "
        "v:reference!&muti32 = &mut first; "
        "select(flag) { true -> { reference = &mut second; } "
        "default -> { reference = &mut third; } } "
        "v:edit_second!&muti32 = &mut second; "
        "v:edit_third!&muti32 = &mut third; }";
    const char* scan_reassignment_preserves_all_possible_mutable_borrows =
        "f:main(first!i32, second!i32, third!i32, flag!bool) { "
        "v:reference!&muti32 = &mut first; "
        "scan(flag) { true -> { reference = &mut second; } "
        "else -> { reference = &mut third; } } "
        "v:edit_second!&muti32 = &mut second; "
        "v:edit_third!&muti32 = &mut third; }";
    const char* loop_reassignment_preserves_zero_and_one_iteration_borrows =
        "f:main(first!i32, second!i32) { "
        "v:reference!&muti32 = &mut first; "
        "loop (true) { reference = &mut second; break; } "
        "v:edit_first!&muti32 = &mut first; "
        "v:edit_second!&muti32 = &mut second; }";
    const char* terminating_branch_does_not_leak_unreachable_borrow =
        "f:main(first!i32, second!i32, flag!bool) { "
        "v:reference!&muti32 = &mut first; "
        "if (flag) { reference = &mut second; ret; } "
        "v:edit_second!&muti32 = &mut second; "
        "v:edit_first!&muti32 = &mut first; }";
    const char* stored_reference_member_reassignment_is_rejected =
        "struct: Box { v:reference!&i32 } "
        "f:main() { v:first!i32 = 1; v:box!Box; box.reference = &first; }";
    const char* stored_reference_index_reassignment_is_rejected =
        "f:main() { v:first!i32 = 1; v:refs[1]!&i32; refs[0] = &first; }";
    return check_source(reassignment_releases_previous_owner, 0) &&
        check_source(reassignment_tracks_new_owner, 1) &&
        check_source(copying_shared_reference_tracks_owner, 1) &&
        check_source(copying_exclusive_reference_is_rejected, 1) &&
        check_source(outer_reference_cannot_escape_inner_owner, 1) &&
        check_source(later_declared_owner_cleanup_is_safe, 0) &&
        check_source(mutable_reassignment_releases_old_owner, 0) &&
        check_source(mutable_reassignment_tracks_new_owner, 1) &&
        check_source(failed_reassignment_preserves_old_borrow, 2) &&
        check_source(failed_mutable_reassignment_preserves_old_borrow, 2) &&
        check_source(conflicting_mutable_reassignment_preserves_old_borrow, 2) &&
        check_source(branch_reassignment_preserves_all_possible_mutable_borrows, 2) &&
        check_source(conditional_mutable_reassignment_keeps_incoming_and_new_borrows, 2) &&
        check_source(select_reassignment_preserves_all_possible_mutable_borrows, 2) &&
        check_source(scan_reassignment_preserves_all_possible_mutable_borrows, 2) &&
        check_source(loop_reassignment_preserves_zero_and_one_iteration_borrows, 2) &&
        check_source(terminating_branch_does_not_leak_unreachable_borrow, 1) &&
        check_source(stored_reference_member_reassignment_is_rejected, 3) &&
        check_source(stored_reference_index_reassignment_is_rejected, 2);
}

static int check_borrow_aggregate_provenance_semantics(void) {
    const char* shared_index_and_mutable_other_index_are_independent =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:first!&i32 = &items[0]; "
        "v:second!&muti32 = &mut items[1]; }";
    const char* mutable_index_allows_other_index_write =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:first!&muti32 = &mut items[0]; items[1] = 3; }";
    const char* shared_index_allows_other_index_write =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:first!&i32 = &items[0]; items[1] = 3; }";
    const char* two_mutable_index_borrows_are_independent =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:first!&muti32 = &mut items[0]; "
        "v:second!&muti32 = &mut items[1]; }";
    const char* same_index_mutable_borrows_conflict =
        "f:main() { v:items[2]!i32 = [1,2]; "
        "v:first!&muti32 = &mut items[0]; "
        "v:second!&muti32 = &mut items[0]; }";
    const char* dynamic_index_borrows_are_conservative =
        "f:main() { v:items[2]!i32 = [1,2]; v:i!i32 = 0; v:j!i32 = 1; "
        "v:first!&muti32 = &mut items[i]; "
        "v:second!&muti32 = &mut items[j]; }";
    const char* raw_pointer_dereference_borrow_has_no_provenance =
        "f:main() { v:pointer!*i32 = null; v:reference!&i32 = &*pointer; }";
    const char* shared_member_allows_other_member_write =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:first!&i32 = &point.x; point.y = 3; }";
    const char* mutable_member_allows_other_member_write =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:first!&muti32 = &mut point.x; point.y = 3; }";
    const char* two_mutable_member_borrows_are_independent =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:first!&muti32 = &mut point.x; "
        "v:second!&muti32 = &mut point.y; }";
    const char* same_member_mutable_borrows_conflict =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:first!&muti32 = &mut point.x; "
        "v:second!&muti32 = &mut point.x; }";
    const char* two_shared_member_borrows_are_allowed =
        "f:main() { struct:Point { v:x!i32, v:y!i32 } "
        "v:point!Point; v:first!&i32 = &point.x; "
        "v:second!&i32 = &point.y; }";
    const char* distinct_union_member_mutable_borrows_conflict =
        "f:main() { union: Bits { v:word!i32, v:octet!u8 } "
        "v:value!Bits; v:first!&muti32 = &mut value.word; "
        "v:second!&mutu8 = &mut value.octet; }";
    const char* union_member_and_outer_sibling_are_independent =
        "f:main() { union: Bits { v:word!i32, v:octet!u8 } "
        "struct: Container { v:value!Bits, v:guard!i32 } "
        "v:container!Container; v:first!&muti32 = &mut container.value.word; "
        "v:second!&muti32 = &mut container.guard; }";
    const char* nested_union_member_borrows_conflict =
        "f:main() { union: Bits { v:word!i32, v:octet!u8 } "
        "struct: Container { v:value!Bits, v:guard!i32 } "
        "v:container!Container; v:first!&muti32 = &mut container.value.word; "
        "v:second!&mutu8 = &mut container.value.octet; }";
    return check_source(shared_index_and_mutable_other_index_are_independent, 0) &&
        check_source(mutable_index_allows_other_index_write, 0) &&
        check_source(shared_index_allows_other_index_write, 0) &&
        check_source(two_mutable_index_borrows_are_independent, 0) &&
        check_source(same_index_mutable_borrows_conflict, 1) &&
        check_source(dynamic_index_borrows_are_conservative, 1) &&
        check_source(raw_pointer_dereference_borrow_has_no_provenance, 1) &&
        check_source(shared_member_allows_other_member_write, 0) &&
        check_source(mutable_member_allows_other_member_write, 0) &&
        check_source(two_mutable_member_borrows_are_independent, 0) &&
        check_source(same_member_mutable_borrows_conflict, 1) &&
        check_source(two_shared_member_borrows_are_allowed, 0) &&
        check_source(distinct_union_member_mutable_borrows_conflict, 1) &&
        check_source(union_member_and_outer_sibling_are_independent, 0) &&
        check_source(nested_union_member_borrows_conflict, 1);
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
        "nt:enum: Choice { Range(start!i32, end!i32), Other }"
        "f:main() {"
        " v:choice!Choice = Choice.Range(1, 2);"
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
        "nt:enum: Choice { Range(start!i32, end!i32), Other }"
        "f:main() { v:choice!Choice = Choice.Range(1, 2); "
        "select(choice) { Range(s,e) -> { } Other -> { } } v:leak = s; }";
    const char* unknown_apply =
        "f:main() { apply rules(Missing(1)); }";
    const char* unknown_handler =
        "f:main() { on Missing.Success { } }";
    const char* unknown_event =
        "f:main() { rules R() { true -> Success; } on R.Missing { } }";
    const char* duplicate_rule_event =
        "f:main() { rules R() { true -> Same; false -> Same; } }";
    const char* duplicate_pattern =
        "nt:enum: Choice { Range(start!i32, end!i32), Other }"
        "f:main() { v:choice!Choice = Choice.Range(1, 2); "
        "select(choice) { Range(s,s) -> { } Other -> { } } }";
    const char* invalid_guard =
        "nt:enum: Choice { Range(start!i32, end!i32), Other }"
        "f:main() { v:choice!Choice = Choice.Range(1, 2); "
        "scan(choice) { Range(a,b) when 1 -> { } else -> { } } }";
    const char* select_default_not_last =
        "f:main() { v:choice = 1; select(choice) { default -> { } 1 -> { } } }";
    const char* valid_scan_else_last =
        "nt:enum: Choice { Range(start!i32, end!i32), Other }"
        "f:main() { v:choice!Choice = Choice.Range(1, 2); "
        "scan(choice) { Range(a,b) -> { } else -> { } } }";
    const char* valid_named_arguments =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(b:2,a:1); }";
    const char* valid_default_parameter_omitted =
        "f:sum(a!i32,b:2)!i32 { ret a+b; } "
        "f:main() !i32 { ret sum(1); }";
    const char* valid_default_parameter_explicit =
        "f:sum(a!i32,b:2)!i32 { ret a+b; } "
        "f:main() !i32 { ret sum(1,3); }";
    const char* invalid_default_parameter_missing_required =
        "f:sum(a!i32,b:2)!i32 { ret a+b; } "
        "f:main() { v:result = sum(); }";
    const char* invalid_required_after_default =
        "f:bad(a:1,b!i32)!i32 { ret b; } f:main() {}";
    const char* invalid_named_argument =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(missing:2,a:1); }";
    const char* invalid_positional_named_collision =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(1,a:2); }";
    const char* invalid_named_missing_required =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(b:2); }";
    const char* invalid_default_parameter_call_type =
        "f:sum(a!i32,b:\"wrong\")!i32 { ret a; } "
        "f:main() { v:result!i32 = sum(1,b:2); }";
    const char* valid_omitted_default_parameter =
        "f:sum(a!i32,b:\"fallback\")!i32 { ret a; } "
        "f:main() { v:result!i32 = sum(1); }";
    const char* valid_callback_named_arguments =
        "nt:Callback = f:(id!u32,flag!bool)!bool; "
        "f:main(callback!Callback,id!u32,flag!bool) { "
        "callback(flag:flag,id:id); }";
    const char* invalid_callback_unknown_named_argument =
        "nt:Callback = f:(id!u32,flag!bool)!bool; "
        "f:main(callback!Callback,id!u32,flag!bool) { "
        "callback(missing:id,flag:flag); }";
    const char* invalid_mixed_extra_positional =
        "f:sum(a!i32,b!i32)!i32 { ret a+b; } "
        "f:main() { v:result!i32 = sum(1,2,3,missing:4); }";
    const char* valid_variadic_call =
        "f:log(first!i32,...)!void { ret; } "
        "f:main() { log(1,2,3); }";
    const char* invalid_variadic_missing_fixed =
        "f:log(first!i32,...)!void { ret; } f:main() { log(); }";
    const char* valid_callback_newtype =
        "nt:Callback = f:(id!u32)!bool; f:main() { v:callback!Callback = null; }";
    const char* invalid_newtype_target =
        "nt:Bad = MissingType; f:main() {}";
    const char* invalid_self_referential_newtype =
        "nt:Bad = Bad; f:main() {}";
    const char* invalid_by_value_recursive_struct =
        "f:main() { struct:Node { v:next!Node } }";
    const char* invalid_by_value_recursive_union =
        "f:main() { union:Node { v:next!Node } }";
    const char* invalid_by_value_recursive_array =
        "f:main() { struct:Node { v:next[1]!Node } }";
    const char* valid_dynamic_recursive_array =
        "f:main() { struct:Node { v:next[]!Node } }";
    const char* valid_pointer_recursive_struct =
        "f:main() { struct:Node { v:next!*Node } }";
    const char* valid_newtype_alias_chain =
        "nt:Base = u64; nt:Alias = Base; f:main() { v:value!Alias; }";
    const char* valid_newtype_chain_identity_assignment =
        "nt:Base = u64; nt:Alias = Base; "
        "f:main() { v:first!Alias; v:second!Alias = first; }";
    const char* invalid_newtype_chain_base_assignment =
        "nt:Base = u64; nt:Alias = Base; "
        "f:main() { v:base!Base; v:alias!Alias = base; }";
    const char* invalid_newtype_chain_reverse_assignment =
        "nt:Base = u64; nt:Alias = Base; "
        "f:main() { v:alias!Alias; v:base!Base = alias; }";
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
    const char* valid_map_dynamic_values =
        "f:consume(values:map) {} "
        "f:main() { v:records!map; records[\"number\"] = 7; "
        "records[\"label\"] = \"seven\"; "
        "v:first!any = records[\"number\"]; consume(records); }";
    const char* invalid_map_const_write =
        "f:main() { v:records!map; c:alias!map = records; alias[\"key\"] = 1; }";
    const char* inferred_generic_identity =
        "f:identity(value!t)!t { ret value; } "
        "f:main() { v:result!i32 = identity(7); }";
    const char* inferred_generic_second_parameter =
        "f:second(a!i32,b!t)!t { ret b; } "
        "f:main() { v:result!str = second(1,\"ok\"); }";
    const char* inferred_generic_named_parameter =
        "f:second(a!i32,b!t)!t { ret b; } "
        "f:main() { v:result!str = second(b:\"ok\",a:1); }";
    const char* inferred_generic_sum =
        "f:sum(a!t,b!t)!t { ret a + b; } "
        "f:main() { v:result!i32 = sum(2,3); }";
    const char* mismatched_generic_sum =
        "f:sum(a!t,b!t)!t { ret a + b; } "
        "f:main() { v:result = sum(2,\"wrong\"); }";
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
    const char* relocate_valid =
        "f:main() { v:src!i32 = 1; v:dst!i32 = 0; v:moved = src _> dst; v:read!i32 = dst; }";
    const char* relocate_double_move =
        "f:main() { v:src!i32 = 1; v:dst!i32 = 0; v:dst2!i32 = 0; v:moved = src _> dst; v:again = src _> dst2; }";
    const char* relocate_type_mismatch =
        "f:main() { v:src!i32 = 1; v:dst!str = \"x\"; v:moved = src _> dst; }";
    const char* relocate_use_after_move =
        "f:main() { v:src!i32 = 1; v:dst!i32 = 0; v:moved = src _> dst; v:again!i32 = src; }";
    const char* relocate_same_source_target =
        "f:main() { v:src!i32 = 1; v:moved = src _> src; }";
    const char* relocate_unknown_target =
        "f:main() { v:src!i32 = 1; v:moved = src _> missing; }";
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
        check_source(valid_default_parameter_omitted,0) &&
        check_source(valid_default_parameter_explicit,0) &&
        check_source(invalid_default_parameter_missing_required,1) &&
        check_source(invalid_required_after_default,1) &&
        check_source(invalid_named_argument,1) &&
        check_source(invalid_positional_named_collision,1) &&
        check_source(invalid_named_missing_required,1) &&
        check_source(invalid_default_parameter_call_type,1) &&
        check_source(valid_omitted_default_parameter,0) &&
        check_source(valid_callback_named_arguments,0) &&
        check_source(invalid_callback_unknown_named_argument,1) &&
        check_source(invalid_mixed_extra_positional,2) &&
        check_source(valid_variadic_call,0) &&
        check_source(invalid_variadic_missing_fixed,1) &&
        check_source(valid_callback_newtype,0) &&
        check_source(invalid_newtype_target,1) &&
        check_source(invalid_self_referential_newtype,1) &&
        check_source(invalid_by_value_recursive_struct,1) &&
        check_source(invalid_by_value_recursive_union,1) &&
        check_source(invalid_by_value_recursive_array,1) &&
        check_source(valid_dynamic_recursive_array,0) &&
        check_source(valid_pointer_recursive_struct,0) &&
        check_source(valid_newtype_alias_chain,0) &&
        check_source(valid_newtype_chain_identity_assignment,0) &&
        check_source(invalid_newtype_chain_base_assignment,1) &&
        check_source(invalid_newtype_chain_reverse_assignment,1) &&
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
        check_source(valid_map_dynamic_values,0) &&
        check_source(invalid_map_const_write,2) &&
        check_source(inferred_generic_identity,0) &&
        check_source(inferred_generic_second_parameter,0) &&
        check_source(inferred_generic_named_parameter,0) &&
        check_source(inferred_generic_sum,0) &&
        check_source(mismatched_generic_sum,1) &&
        check_source(relocate_valid,0) &&
        check_source(relocate_double_move,1) &&
        check_source(relocate_type_mismatch,1) &&
        check_source(relocate_use_after_move,1) &&
        check_source(relocate_same_source_target,1) &&
        check_source(relocate_unknown_target,1) &&
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
    ASTMethod* empty_name = (ASTMethod*)ast_create(AST_METHOD);
    ASTMethod* missing_implementation = (ASTMethod*)ast_create(AST_METHOD);
    ASTMethod* invalid_implementation = (ASTMethod*)ast_create(AST_METHOD);
    SemanticContext semantic;
    int ok = 0;
    if (!missing_name || !empty_name || !missing_implementation ||
        !invalid_implementation) goto cleanup;
    missing_name->implementation = ast_create(AST_FUNC_DECL);
    if (!missing_name->implementation) goto cleanup;
    ((ASTFuncDecl*)missing_name->implementation)->name = semantic_test_copy_text("implementation");
    ((ASTFuncDecl*)missing_name->implementation)->body = ast_create(AST_BLOCK);
    if (!((ASTFuncDecl*)missing_name->implementation)->name ||
        !((ASTFuncDecl*)missing_name->implementation)->body) goto cleanup;
    empty_name->public_name = semantic_test_copy_text("");
    empty_name->implementation = ast_create(AST_FUNC_DECL);
    if (!empty_name->implementation) goto cleanup;
    ((ASTFuncDecl*)empty_name->implementation)->name =
        semantic_test_copy_text("empty_name_impl");
    ((ASTFuncDecl*)empty_name->implementation)->body = ast_create(AST_BLOCK);
    if (!empty_name->public_name ||
        !((ASTFuncDecl*)empty_name->implementation)->name ||
        !((ASTFuncDecl*)empty_name->implementation)->body) goto cleanup;
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
    semantic_check(&semantic, (ASTNode*)empty_name);
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
    ast_destroy((ASTNode*)empty_name);
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
    const char* mixed_method_extra_positional_argument =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store_value(1,2,3,missing:4); ret; } "
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
    const char* self_is_not_visible_outside_method =
        "f:main() { v:value!i32 = self; }";
    const char* method_named_default_argument =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store(mode:\"fast\",value:7); ret; } "
        "store => f:store_impl(self,value!i32,mode:\"safe\") { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* method_named_missing_required_argument =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.store(mode:\"fast\"); ret; } "
        "store => f:store_impl(self,value!i32,mode:\"safe\") { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* variadic_instance_method_values =
        "f:main() { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.log(1,2,3); ret; } "
        "log => f:log_impl(self,first!i32,...) { ret; } "
        "struct:Storage { v:value!i32 } } }";
    const char* variadic_instance_method_rejects_safe_reference =
        "f:main(owner!i32) { group Storage { "
        "run => f:run_impl(self) { v:other!Storage = self; other.log(1,&owner); ret; } "
        "log => f:log_impl(self,first!i32,...) { ret; } "
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
        check_source(mixed_method_extra_positional_argument, 2) &&
        check_source(unknown_group_method, 1) &&
        check_source(duplicate_public_method_alias, 1) &&
        check_source(duplicate_method_implementation_name, 1) &&
        check_source(method_implementation_collides_with_direct_function, 1) &&
        check_source(self_is_not_visible_outside_method, 1) &&
        check_source(method_named_default_argument, 0) &&
        check_source(method_named_missing_required_argument, 1) &&
        check_source(variadic_instance_method_values, 0) &&
        check_source(variadic_instance_method_rejects_safe_reference, 1);
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

static int check_declaration_initializer_binding_semantics(void) {
    const char* valid_outer_shadow_initializer =
        "f:main() { v:value!i32 = 7; { v:value!i32 = value; } }";
    const char* invalid_self_initializer =
        "f:main() { v:value!i32 = value; }";
    const char* invalid_inferred_self_initializer =
        "f:main() { v:value = value; }";
    const char* invalid_immutable_self_initializer =
        "f:main() { m:value = value; }";
    const char* invalid_constant_self_initializer =
        "c:value!i32 = value;";
    const char* valid_inferred_declarations =
        "f:main() { v:answer = 42; v:copy = answer; }";
    const char* duplicate_local_variable =
        "f:main() { v:value!i32 = 1; v:value!i32 = 2; }";
    const char* unknown_declared_type =
        "f:main() { v:value!UnknownType = 1; }";
    const char* duplicate_compile_time_constant =
        "c:VALUE!i32 = 1; c:VALUE!i32 = 2;";
    const char* outer_type_must_be_used =
        "f:main() { v:value!str = \"outer\"; "
        "{ v:value!i32 = value; } }";
    return check_source(valid_outer_shadow_initializer, 0) &&
        check_source(invalid_self_initializer, 1) &&
        check_source(invalid_inferred_self_initializer, 1) &&
        check_source(invalid_immutable_self_initializer, 1) &&
        check_source(invalid_constant_self_initializer, 1) &&
        check_source(valid_inferred_declarations, 0) &&
        check_source(duplicate_local_variable, 1) &&
        check_source(unknown_declared_type, 1) &&
        check_source(duplicate_compile_time_constant, 1) &&
        check_source(outer_type_must_be_used, 1);
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
    const char* global_constant_visible_to_function =
        "c:GLOBAL!i32 = 7; f:main() { v:copy!i32 = GLOBAL; }";
    const char* local_constant =
        "f:main() { c:LOCAL!i32 = 7; v:copy!i32 = LOCAL; }";
    const char* local_constant_does_not_leak =
        "f:main() { { c:LOCAL!i32 = 7; } v:copy!i32 = LOCAL; }";
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
        check_source(global_constant_visible_to_function, 0) &&
        check_source(local_constant, 0) &&
        check_source(local_constant_does_not_leak, 1) &&
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
    /* CTest runs from the package root where src/c/lang is staged. */
    msg_init("lang/messages_en.conf");
    (void)atexit(msg_destroy);
    if (!check_imported_function_call_semantics()) {
        fprintf(stderr, "Qualified imported function call checks failed.\\n");
        return 1;
    }

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
    if (!check_declaration_initializer_binding_semantics()) {
        fprintf(stderr, "Declaration initializer binding checks failed.\n");
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
    if (!check_handler_capture_semantics()) {
        fprintf(stderr, "Handler capture semantic tests failed.\\n"); return 1;
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
    if (!check_malformed_recursive_type_expression_ast()) {
        fprintf(stderr, "Malformed recursive type-expression AST checks failed.\n");
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
    if (!check_named_function_callback_value_semantics()) {
        fprintf(stderr, "Named function callback value checks failed.\\n");
        return 1;
    }
    if (!check_variadic_argument_safety_semantics()) {
        fprintf(stderr, "Variadic argument safety checks failed.\\n");
        return 1;
    }
    if (!check_callback_target_control_flow_merge_semantics()) {
        fprintf(stderr, "Callback target control-flow merge checks failed.\\n");
        return 1;
    }
    if (!check_named_callback_reference_return_origin_semantics()) {
        fprintf(stderr, "Named callback reference-return origin checks failed.\\n");
        return 1;
    }
    if (!check_function_all_path_return_semantics()) {
        fprintf(stderr, "Function all-path return checks failed.\\n");
        return 1;
    }
    if (!check_callback_function_type_semantics()) {
        fprintf(stderr, "Callback function-type Semantic checks failed.\\n");
        return 1;
    }
    if (!check_nested_type_variable_semantics()) {
        fprintf(stderr, "Nested type-variable checks failed.\\n");
        return 1;
    }
    if (!check_vector_index_semantics()) {
        fprintf(stderr, "Vector index checks failed.\\n");
        return 1;
    }
    if (!check_safe_reference_type_erasure_semantics()) {
        fprintf(stderr, "Safe-reference type-erasure checks failed.\\n");
        return 1;
    }
    if (!check_extern_reference_boundary_semantics()) {
        fprintf(stderr, "Extern reference boundary checks failed.\\n");
        return 1;
    }
    if (!check_shift_operator_semantics()) {
        fprintf(stderr, "Shift operator Semantic checks failed.\\n");
        return 1;
    }
    if (!check_extended_numeric_primitive_semantics()) {
        fprintf(stderr, "Extended numeric primitive checks failed.\\n");
        return 1;
    }
    if (!check_map_key_type_semantics()) {
        fprintf(stderr, "Map key type checks failed.\\n");
        return 1;
    }
    if (!check_nested_pointer_and_reference_type_variable_semantics()) {
        fprintf(stderr, "Nested pointer/reference type-variable checks failed.\\n");
        return 1;
    }
    if (!check_nested_callback_type_variable_semantics()) {
        fprintf(stderr, "Nested callback type-variable checks failed.\\n");
        return 1;
    }
    if (!check_callback_null_refinement_semantics()) {
        fprintf(stderr, "Callback null-refinement checks failed.\\n");
        return 1;
    }
    if (!check_callback_nullability_semantics()) {
        fprintf(stderr, "Callback nullability checks failed.\\n");
        return 1;
    }
    if (!check_reference_initialization_control_flow_semantics()) {
        fprintf(stderr, "Reference initialization control-flow checks failed.\\n");
        return 1;
    }
    if (!check_raw_pointer_initialization_and_nullability_semantics()) {
        fprintf(stderr, "Raw-pointer initialization/nullability checks failed.\\n");
        return 1;
    }
    if (!check_safe_reference_container_storage_rejection_semantics()) {
        fprintf(stderr, "Safe-reference container-storage rejection checks failed.\\n");
        return 1;
    }
    if (!check_safe_reference_definite_initialization_semantics()) {
        fprintf(stderr, "Safe-reference definite-initialization checks failed.\\n");
        return 1;
    }
    if (!check_borrow_expression_semantics()) {
        fprintf(stderr, "Shared/mutable borrow Semantic checks failed.\n");
        return 1;
    }
    if (!check_shared_reborrow_semantics() ||
        !check_reference_member_projection_semantics()) {
        fprintf(stderr, "Shared dereference reborrow checks failed.\\n");
        return 1;
    }
if (!check_call_temporary_borrow_semantics()) {
        fprintf(stderr, "Call temporary borrow checks failed.\\n"); return 1;
    }
    if (!check_borrow_diagnostic_guidance()) {
        fprintf(stderr, "Borrow diagnostic guidance checks failed.\\n");
        return 1;
    }
    if (!check_reference_parameter_escape_diagnostic()) {
        fprintf(stderr, "Reference parameter escape diagnostic checks failed.\\n");
        return 1;
    }
    if (!check_borrow_overlap_and_scope_semantics()) {
        fprintf(stderr, "Borrow overlap/scope Semantic checks failed.\n");
        return 1;
    }
    if (!check_mutable_borrow_owner_access_semantics()) {
        fprintf(stderr, "Exclusive borrow owner-access checks failed.\\n");
        return 1;
    }
    if (!check_reference_initializer_provenance_semantics()) {
        fprintf(stderr, "Reference initializer provenance checks failed.\\n");
        return 1;
    }
    if (!check_safe_reference_return_lifetime_semantics()) {
        fprintf(stderr, "Safe-reference return lifetime checks failed.\\n");
        return 1;
    }
    if (!check_reference_reassignment_and_lifetime_semantics()) {
        fprintf(stderr, "Reference reassignment/lifetime checks failed.\\n");
        return 1;
    }
    if (!check_borrow_aggregate_provenance_semantics()) {
        fprintf(stderr, "Aggregate/member/index borrow provenance checks failed.\\n");
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
    if (!check_aggregate_reference_field_rejection_semantics()) {
        fprintf(stderr, "Aggregate safe-reference field rejection tests failed.\\n");
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
    if (!check_newtype_result_base_metadata()) {
        fprintf(stderr, "NewType SemanticResult metadata checks failed.\n");
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
