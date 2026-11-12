#ifdef NDEBUG
#undef NDEBUG
#endif
#include "semantic.h"
#include "lexer.h"
#include "oir_ast_frontend_writer.h"
#include "oir_semantic_ast_artifact.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ResolverContext {
    const OirAstPointerIndexEntry* entries;
    size_t capacity;
} ResolverContext;

static uint32_t resolve_node_id(void* user_context, const ASTNode* node) {
    const ResolverContext* resolver = (const ResolverContext*)user_context;
    return oir_ast_frontend_index_lookup(
        resolver->entries, resolver->capacity, node);
}

static uint32_t find_scope_owner(const SemanticResult* result,uint32_t owner_node_id) {
    uint32_t i;
    for(i=0u;i<result->scope_count;i++)
        if(result->scopes[i].owner_node_id==owner_node_id)return i+1u;
    return 0u;
}

static ASTVarDecl* find_local(ASTNode* statements,const char* name) {
    ASTNode* current;
    for(current=statements;current;current=current->next) {
        if(current->type==AST_VAR_DECL) {
            ASTVarDecl* variable=(ASTVarDecl*)current;
            if(variable->name && strcmp(variable->name,name)==0)return variable;
        }
    }
    return NULL;
}

static const SemanticResultType* find_result_type(
    const SemanticResult* result, uint32_t type_id
) {
    uint32_t i;
    if (!result || type_id == 0u) return NULL;
    for (i = 0u; i < result->type_count; ++i)
        if (result->types[i].type_id == type_id) return &result->types[i];
    return NULL;
}

static int conversion_matches(
    const SemanticResult* result,
    uint32_t node_id,
    const char* destination_name,
    uint16_t conversion_kind
) {
    uint32_t i;
    if (!result || !destination_name) return 0;
    for (i = 0u; i < result->conversion_count; ++i) {
        const SemanticResultConversion* conversion = &result->conversions[i];
        const SemanticResultType* source =
            find_result_type(result, conversion->source_type_id);
        const SemanticResultType* destination =
            find_result_type(result, conversion->destination_type_id);
        if (conversion->node_id == node_id && source && destination &&
            source->name && destination->name &&
            strcmp(destination->name, destination_name) == 0 &&
            conversion->kind == conversion_kind &&
            conversion->flags == SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT)
            return 1;
    }
    return 0;
}

static int test_bit_byte_semantic_conversion_records(void) {
    static const char source[] =
        "f:main() { v:one!bit = 1; v:raw!byte = 255; "
        "v:flags!8bit = 255; v:wide!16bit = flags; }";
    ParserContext* parser = parser_init_source(
        "bit-byte-conversions.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    OirAstPointerIndexEntry index[128];
    ResolverContext resolver;
    SemanticContext semantic;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTVarDecl *one, *raw, *flags, *wide;
    uint32_t node_count = 0u;
    OirAstFrontendRequirements requirements;
    OirAstNodeRecord* ast_records = NULL;
    uint8_t *ast_fields = NULL, *ast_strings = NULL;
    uint8_t *ast_payload = NULL, *artifact = NULL;
    uint32_t *incoming_counts = NULL, *owner_scope_by_node = NULL;
    uint8_t* visited = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    OirSemanticAstArtifactView artifact_view;
    size_t ast_payload_written = 0u, artifact_written = 0u;
    int semantic_initialized = 0;
    int ok = 0;
    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK ||
        parser->error_count != 0 || !parser->ast_root) goto done;
    memset(index, 0, sizeof(index));
    if (oir_ast_frontend_index_build(
            (ASTNode*)parser->ast_root, index, 128u, &node_count) != OIR_STATUS_OK)
        goto done;
    resolver.entries = index;
    resolver.capacity = 128u;
    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK) goto done;
    body = (ASTBlock*)function->body;
    one = find_local(body->statements, "one");
    raw = find_local(body->statements, "raw");
    flags = find_local(body->statements, "flags");
    wide = find_local(body->statements, "wide");
    if (!one || !raw || !flags || !wide) goto done;

    semantic_init(&semantic, "bit-byte-conversions.nxf", NULL);
    semantic_initialized = 1;
    semantic_attach_node_id_resolver(&semantic, resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    if (semantic.error_count != 0 ||
        semantic.result_status != SEMANTIC_RESULT_OK ||
        semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK) {
        goto done;
    }

    if (oir_ast_frontend_measure((ASTNode*)parser->ast_root, index, 128u,
            node_count, &requirements) != OIR_STATUS_OK) goto done;
    ast_records = (OirAstNodeRecord*)calloc(node_count, sizeof(*ast_records));
    ast_fields = (uint8_t*)malloc(requirements.field_data_length ?
        requirements.field_data_length : 1u);
    ast_strings = (uint8_t*)malloc(requirements.string_table_length ?
        requirements.string_table_length : 1u);
    ast_payload = (uint8_t*)malloc(requirements.payload_size);
    artifact = (uint8_t*)malloc(262144u);
    incoming_counts = (uint32_t*)calloc(node_count, sizeof(*incoming_counts));
    visited = (uint8_t*)calloc(node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)calloc(node_count, sizeof(*graph_stack));
    owner_scope_by_node = (uint32_t*)calloc(
        (size_t)node_count + 1u, sizeof(*owner_scope_by_node));
    if (!ast_records || !ast_fields || !ast_strings || !ast_payload || !artifact ||
        !incoming_counts || !visited || !graph_stack || !owner_scope_by_node)
        goto done;
    if (oir_ast_frontend_write_payload((ASTNode*)parser->ast_root,
            index, 128u, node_count, ast_records, node_count,
            ast_fields, requirements.field_data_length,
            ast_strings, requirements.string_table_length,
            ast_payload, requirements.payload_size, &ast_payload_written,
            &requirements) != OIR_STATUS_OK ||
        oir_semantic_ast_artifact_write(ast_payload, ast_payload_written,
            &semantic.result, 1701u, artifact, 262144u,
            &artifact_written) != OIR_STATUS_OK ||
        oir_semantic_ast_artifact_open(artifact, artifact_written,
            &artifact_view, incoming_counts, node_count, visited, node_count,
            graph_stack, node_count, owner_scope_by_node,
            (size_t)node_count + 1u) != OIR_STATUS_OK ||
        !artifact_view.has_conversions ||
        artifact_view.conversions.record_count != 4u)
        goto done;
    {
        uint32_t conversion_index;
        for (conversion_index = 0u; conversion_index < 4u; ++conversion_index) {
            OirSemanticConversion encoded;
            const SemanticResultConversion* original =
                &semantic.result.conversions[conversion_index];
            if (oir_semantic_conversions_get(
                    &artifact_view.conversions, conversion_index, &encoded)
                    != OIR_STATUS_OK ||
                encoded.node_id != original->node_id ||
                encoded.operand_ordinal != original->operand_ordinal ||
                encoded.source_type_id != original->source_type_id ||
                encoded.destination_type_id != original->destination_type_id ||
                encoded.kind != original->kind || encoded.flags != original->flags)
                goto done;
        }
    }

    ok = semantic.result.conversion_count == 4u &&
        conversion_matches(&semantic.result, resolve_node_id(&resolver, (ASTNode*)one),
            "bit", SEMANTIC_RESULT_CONVERSION_NARROWING) &&
        conversion_matches(&semantic.result, resolve_node_id(&resolver, (ASTNode*)raw),
            "byte", SEMANTIC_RESULT_CONVERSION_NARROWING) &&
        conversion_matches(&semantic.result, resolve_node_id(&resolver, (ASTNode*)flags),
            "8bit", SEMANTIC_RESULT_CONVERSION_NARROWING) &&
        conversion_matches(&semantic.result, resolve_node_id(&resolver, (ASTNode*)wide),
            "16bit", SEMANTIC_RESULT_CONVERSION_WIDENING);
    if (!ok) fprintf(stderr, "Bit/byte conversion records: count=%u\n",
        semantic.result.conversion_count);
done:
    if (semantic_initialized) semantic_destroy(&semantic);
    free(ast_records);
    free(ast_fields);
    free(ast_strings);
    free(ast_payload);
    free(artifact);
    free(incoming_counts);
    free(visited);
    free(graph_stack);
    free(owner_scope_by_node);
    parser_destroy(parser);
    return ok;
}

static const SemanticResultAnnotation* find_annotation_by_node(
    const SemanticResult* result, uint32_t node_id
) {
    uint32_t i;
    if (!result || node_id == 0u) return NULL;
    for (i = 0u; i < result->annotation_count; ++i)
        if (result->annotations[i].node_id == node_id)
            return &result->annotations[i];
    return NULL;
}

static int test_thread_payload_capture_annotations(void) {
    static const char source[] =
        "f:main() { v:outside!i32 = 7; "
        "(worker)>{ (h) <- copy(outside); (h2) <- copy(outside); "
        "v:inside!i32 = 1; v:local_copy!i32 = inside; } "
        "spawn(worker); }";
    ParserContext* parser = parser_init_source(
        "thread-payload-capture-result.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    OirAstPointerIndexEntry index[128];
    ResolverContext resolver;
    SemanticContext semantic;
    ASTFuncDecl* function;
    ASTBlock* main_body;
    ASTThreadBlock* thread_block;
    ASTBlock* payload_body;
    ASTNexusCapture* first_capture;
    ASTVarDecl *inside, *local_copy;
    const SemanticResultAnnotation *outer_use = NULL, *local_use = NULL;
    uint32_t node_count = 0u;
    int semantic_initialized = 0;
    int ok = 0;
    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK ||
        parser->error_count != 0 || !parser->ast_root) goto done;
    memset(index, 0, sizeof(index));
    if (oir_ast_frontend_index_build(
            (ASTNode*)parser->ast_root, index, 128u, &node_count) != OIR_STATUS_OK)
        goto done;
    resolver.entries = index;
    resolver.capacity = 128u;
    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    main_body = (ASTBlock*)function->body;
    thread_block = main_body->statements && main_body->statements->next &&
        main_body->statements->next->type == AST_THREAD_BLOCK
            ? (ASTThreadBlock*)main_body->statements->next : NULL;
    if (!thread_block || !thread_block->body ||
        thread_block->body->type != AST_BLOCK) goto done;
    payload_body = (ASTBlock*)thread_block->body;
    first_capture = payload_body->statements &&
        payload_body->statements->type == AST_NEXUS_CAPTURE
            ? (ASTNexusCapture*)payload_body->statements : NULL;
    inside = first_capture && first_capture->base.next &&
        first_capture->base.next->type == AST_NEXUS_CAPTURE &&
        first_capture->base.next->next &&
        first_capture->base.next->next->type == AST_VAR_DECL
            ? (ASTVarDecl*)first_capture->base.next->next : NULL;
    local_copy = inside && inside->base.next &&
        inside->base.next->type == AST_VAR_DECL
            ? (ASTVarDecl*)inside->base.next : NULL;
    if (!first_capture || !first_capture->expr ||
        first_capture->expr->type != AST_FUNC_CALL ||
        !((ASTCall*)first_capture->expr)->args ||
        ((ASTCall*)first_capture->expr)->args->type != AST_IDENTIFIER ||
        !inside || !inside->initializer ||
        !local_copy || !local_copy->initializer ||
        local_copy->initializer->type != AST_IDENTIFIER) goto done;

    semantic_init(&semantic, "thread-payload-capture-result.nxf", NULL);
    semantic_initialized = 1;
    semantic_attach_node_id_resolver(&semantic, resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    if (semantic.error_count != 0 ||
        semantic.result_status != SEMANTIC_RESULT_OK ||
        semantic.thread_blocks == NULL ||
        semantic.thread_blocks->capture_count != 1u ||
        semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK)
        goto done;
    outer_use = find_annotation_by_node(
        &semantic.result, resolve_node_id(&resolver, ((ASTCall*)first_capture->expr)->args));
    local_use = find_annotation_by_node(
        &semantic.result, resolve_node_id(&resolver, local_copy->initializer));
    ok = outer_use && local_use &&
        (outer_use->flags & SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE) != 0u &&
        (local_use->flags & SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE) == 0u;
done:
    if (!ok) {
        fprintf(stderr,
            "Thread capture debug: initialized=%d errors=%d result_status=%d "
            "capture_count=%u outer_flags=%u local_flags=%u\n",
            semantic_initialized,
            semantic_initialized ? semantic.error_count : -1,
            semantic_initialized ? (int)semantic.result_status : -1,
            semantic_initialized && semantic.thread_blocks
                ? semantic.thread_blocks->capture_count : 0u,
            outer_use ? outer_use->flags : 0u,
            local_use ? local_use->flags : 0u);
    }
    if (semantic_initialized) semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int test_extended_semantic_scopes(void) {
    static const char source[] =
        "f:main() {"
        " v:choice = 1;"
        " select(choice) { Range(s,e) -> { v:seen = s; } default -> { } }"
        " scan(choice) { Range(a,b) -> { v:seen2 = b; } }"
        " rules PrimeBorders(n) { n < 2 :=> \"out of range\"; n == 2 -> Success; always -> true; default -> false; }"
        " apply rules(PrimeBorders(2));"
        " on PrimeBorders.Success { v:handled = true; }"
        " v:callback = f:(x) > x;"
        " v:callback_block = f:(y) { ret y; };"
        " v:data = 1;"
        " unroll(3) -> loop(item in data) { v:copy = item; }"
        " v:src!i32 = 1; v:dst!i32 = 2; v:moved = src _> dst;"
        " v:fixed[1+2]!i32 = [1,2,3];"
        " v:matrix[2][2]!f64 = [[1.0,2.0],[3.0,4.0]];"
        "}"
        "f:array_params(a![], b!i32[]) {}";
    ParserContext* parser=parser_init_source(
        "extended-semantic-scopes.nxf",(const uint8_t*)source,sizeof(source)-1u);
    OirAstPointerIndexEntry index[1024];
    ResolverContext resolver;
    SemanticContext semantic;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTSelect* select_node;
    ASTSelectArm* select_arm;
    ASTPatternConstructor* constructor;
    ASTLambda* lambda;
    ASTVarDecl* callback;
    ASTRulesetDecl* ruleset=NULL;
    ASTNode* statement;
    uint32_t node_count=0u,select_scope,lambda_scope,ruleset_scope;
    int ok=0;
    int saw_fixed_array_type=0,saw_matrix_array_type=0;
    int saw_untyped_dynamic_array=0,saw_typed_dynamic_array=0;
    uint32_t type_index;
    if(!parser)return 0;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count || !parser->ast_root)goto done;
    memset(index,0,sizeof(index));
    if(oir_ast_frontend_index_build((ASTNode*)parser->ast_root,index,1024u,&node_count)!=OIR_STATUS_OK)goto done;
    resolver.entries=index;resolver.capacity=1024u;
    function=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    if(!function || !function->body || function->body->type!=AST_BLOCK)goto done;
    body=(ASTBlock*)function->body;
    select_node=(ASTSelect*)body->statements->next;
    if(!select_node || select_node->base.type!=AST_SELECT || !select_node->arms)goto done;
    select_arm=(ASTSelectArm*)select_node->arms;
    if(!select_arm->pattern || select_arm->pattern->type!=AST_PATTERN_CONSTRUCTOR)goto done;
    constructor=(ASTPatternConstructor*)select_arm->pattern;
    callback=find_local(body->statements,"callback");
    if(!callback || !callback->initializer || callback->initializer->type!=AST_LAMBDA)goto done;
    lambda=(ASTLambda*)callback->initializer;
    for(statement=body->statements;statement;statement=statement->next)
        if(statement->type==AST_RULESET_DECL)ruleset=(ASTRulesetDecl*)statement;
    if(!ruleset)goto done;

    semantic_init(&semantic,"extended-semantic-scopes.nxf",NULL);
    semantic_attach_node_id_resolver(&semantic,resolve_node_id,&resolver);
    semantic_check(&semantic,(ASTNode*)parser->ast_root);
    if(semantic.error_count!=0 || semantic.result_status!=SEMANTIC_RESULT_OK ||
       semantic.current_scope_id!=1u || semantic.current_depth!=0) {
        semantic_destroy(&semantic);
        goto done;
    }
    if(semantic_finalize_result(&semantic,node_count)!=SEMANTIC_RESULT_OK) {
        semantic_destroy(&semantic);
        goto done;
    }
    for(type_index=0u;type_index<semantic.result.type_count;type_index++) {
        const SemanticResultType* type=&semantic.result.types[type_index];
        if(type->name && strcmp(type->name,"array[3]")==0 && type->base_type_id!=0u)
            saw_fixed_array_type=1;
        if(type->name && strcmp(type->name,"array[2][2]")==0 && type->base_type_id!=0u)
            saw_matrix_array_type=1;
        if(type->name && strcmp(type->name,"array[]")==0 && type->base_type_id==0u)
            saw_untyped_dynamic_array=1;
        if(type->name && strcmp(type->name,"array[]")==0 && type->base_type_id!=0u)
            saw_typed_dynamic_array=1;
    }
    select_scope=find_scope_owner(&semantic.result,resolve_node_id(&resolver,(ASTNode*)select_arm));
    lambda_scope=find_scope_owner(&semantic.result,resolve_node_id(&resolver,(ASTNode*)lambda));
    ruleset_scope=find_scope_owner(&semantic.result,resolve_node_id(&resolver,(ASTNode*)ruleset));
    ok=select_scope!=0u && lambda_scope!=0u && ruleset_scope!=0u &&
        saw_fixed_array_type && saw_matrix_array_type &&
        saw_untyped_dynamic_array && saw_typed_dynamic_array &&
        constructor->bindings!=NULL &&
        semantic_result_find_visible_symbol(&semantic.result,select_scope,"s")!=0u &&
        semantic_result_find_visible_symbol(&semantic.result,1u,"s")==0u &&
        semantic_result_find_visible_symbol(&semantic.result,lambda_scope,"x")!=0u &&
        semantic_result_find_visible_symbol(&semantic.result,1u,"x")==0u &&
        semantic_result_find_visible_symbol(&semantic.result,ruleset_scope,"n")!=0u &&
        semantic_result_find_visible_symbol(&semantic.result,1u,"n")==0u;
    if(!ok) fprintf(stderr,"Array result flags: fixed=%d matrix=%d dynamic=%d typed_dynamic=%d scopes=%u/%u/%u\\n",
        saw_fixed_array_type,saw_matrix_array_type,saw_untyped_dynamic_array,
        saw_typed_dynamic_array,select_scope,lambda_scope,ruleset_scope);
    semantic_destroy(&semantic);
done:
    parser_destroy(parser);
    return ok;
}

static int test_group_semantic_records(void) {
    static const char source[] =
        "f:main() {"
        " group Storage {"
        "   open => f:open_impl(self) { v:field_value!i32 = self.value; self.store_value(7); ret; }"
        "   store_value => f:store_value_impl(self, value!i32) { ret; }"
        "   static_info => f:static_info_impl() { ret; }"
        "   struct:Storage { v:value!i32 }"
        "   default { }"
        " }"
        " Storage.static_info();"
        " Storage;"
        "}";
    ParserContext* parser = parser_init_source(
        "group-semantic-result.nxf", (const uint8_t*)source,
        sizeof(source) - 1u);
    OirAstPointerIndexEntry index[256];
    ResolverContext resolver;
    SemanticContext semantic;
    uint32_t node_count = 0u, group_symbol_id = 0u;
    uint32_t i, group_relation_count = 0u;
    int saw_member = 0, saw_binding = 0;
    int saw_method = 0, saw_default = 0;
    ASTFuncDecl* main_function;
    ASTBlock* main_body;
    ASTGroupDecl* group_node;
    ASTVarDecl* self_parameter = NULL;
    ASTFuncDecl* store_implementation = NULL;
    ASTFuncDecl* static_info_implementation = NULL;
    ASTCall* store_call = NULL;
    ASTCall* external_static_call = NULL;
    ASTNode* default_reference = NULL;
    uint32_t self_node_id = 0u, self_symbol_id = 0u, bound_type_id = 0u;
    uint32_t store_node_id = 0u, store_callee_node_id = 0u;
    uint32_t store_symbol_id = 0u, static_info_symbol_id = 0u;
    uint32_t external_call_node_id = 0u, default_reference_node_id = 0u;
    int saw_receiver_annotation = 0, saw_receiver_parameter_relation = 0;
    int saw_address_receiver_annotation = 0;
    int saw_store_call_target = 0, saw_store_callee_target = 0;
    int saw_store_method_relation = 0, saw_external_static_call_target = 0;
    int saw_static_info_method_relation = 0;
    int saw_default_dispatch_annotation = 0;
    OirAstFrontendRequirements ast_requirements;
    OirAstNodeRecord* ast_records = NULL;
    uint8_t *ast_fields = NULL, *ast_strings = NULL;
    uint8_t *ast_payload = NULL, *artifact = NULL, *legacy_artifact = NULL;
    uint32_t *incoming_counts = NULL, *owner_scope_by_node = NULL;
    uint8_t* visited = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    OirSemanticAstArtifactView artifact_view;
    size_t ast_payload_written = 0u, artifact_written = 0u;
    size_t legacy_artifact_written = 0u;
    int ok = 0;

    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK ||
        parser->error_count != 0 || !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }
    memset(index, 0, sizeof(index));
    if (oir_ast_frontend_index_build((ASTNode*)parser->ast_root,
            index, 256u, &node_count) != OIR_STATUS_OK) {
        parser_destroy(parser);
        return 0;
    }
    resolver.entries = index;
    resolver.capacity = 256u;
    main_function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    if (!main_function || !main_function->body ||
        main_function->body->type != AST_BLOCK) {
        parser_destroy(parser);
        return 0;
    }
    main_body = (ASTBlock*)main_function->body;
    group_node = main_body->statements &&
        main_body->statements->type == AST_GROUP_DECL
            ? (ASTGroupDecl*)main_body->statements : NULL;
    if (!group_node) {
        parser_destroy(parser);
        return 0;
    }
    {
        ASTNode* group_member;
        for (group_member = group_node->members; group_member;
             group_member = group_member->next) {
            if (group_member->type == AST_METHOD) {
                ASTMethod* method = (ASTMethod*)group_member;
                if (method->public_name &&
                    strcmp(method->public_name, "open") == 0 &&
                    method->implementation &&
                    method->implementation->type == AST_FUNC_DECL) {
                    ASTFuncDecl* implementation =
                        (ASTFuncDecl*)method->implementation;
                    ASTNode* statement;
                    if (implementation->params &&
                        implementation->params->type == AST_VAR_DECL) {
                        self_parameter = (ASTVarDecl*)implementation->params;
                    }
                    if (implementation->body &&
                        implementation->body->type == AST_BLOCK) {
                        for (statement = ((ASTBlock*)implementation->body)->statements;
                             statement; statement = statement->next) {
                            if (statement->type == AST_FUNC_CALL) {
                                ASTCall* candidate = (ASTCall*)statement;
                                if (candidate->callee &&
                                    candidate->callee->type == AST_MEMBER_ACCESS &&
                                    strcmp(((ASTMemberAccess*)candidate->callee)->member_name,
                                        "store_value") == 0) {
                                    store_call = candidate;
                                    break;
                                }
                            }
                        }
                    }
                } else if (method->public_name &&
                    strcmp(method->public_name, "store_value") == 0 &&
                    method->implementation &&
                    method->implementation->type == AST_FUNC_DECL) {
                    store_implementation = (ASTFuncDecl*)method->implementation;
                } else if (method->public_name &&
                    strcmp(method->public_name, "static_info") == 0 &&
                    method->implementation &&
                    method->implementation->type == AST_FUNC_DECL) {
                    static_info_implementation =
                        (ASTFuncDecl*)method->implementation;
                }
            }
        }
    }
    if (!self_parameter || !self_parameter->name ||
        strcmp(self_parameter->name, "self") != 0 ||
        !store_implementation || !store_call ||
        !static_info_implementation || !group_node->base.next ||
        group_node->base.next->type != AST_FUNC_CALL ||
        !group_node->base.next->next ||
        group_node->base.next->next->type != AST_IDENTIFIER) {
        parser_destroy(parser);
        return 0;
    }
    external_static_call = (ASTCall*)group_node->base.next;
    default_reference = group_node->base.next->next;
    self_node_id = resolve_node_id(&resolver, (ASTNode*)self_parameter);
    store_node_id = resolve_node_id(&resolver, (ASTNode*)store_call);
    store_callee_node_id = resolve_node_id(
        &resolver, store_call ? store_call->callee : NULL);
    external_call_node_id = resolve_node_id(
        &resolver, (ASTNode*)external_static_call);
    default_reference_node_id = resolve_node_id(
        &resolver, default_reference);
    semantic_init(&semantic, "group-semantic-result.nxf", NULL);
    semantic_attach_node_id_resolver(&semantic, resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    if (semantic.error_count != 0 ||
        semantic.result_status != SEMANTIC_RESULT_OK) {
        fprintf(stderr,
            "Group receiver Semantic failure: errors=%d result_status=%d\\n",
            semantic.error_count, (int)semantic.result_status);
        semantic_print_errors(&semantic);
        semantic_destroy(&semantic);
        parser_destroy(parser);
        return 0;
    }
    {
        SemanticResultStatus finalize_status =
            semantic_finalize_result(&semantic, node_count);
        if (finalize_status != SEMANTIC_RESULT_OK) {
            fprintf(stderr,
                "Group receiver SemanticResult finalize failed: %d (%s)\\n",
                (int)finalize_status,
                semantic_result_status_string(finalize_status));
            semantic_destroy(&semantic);
            parser_destroy(parser);
            return 0;
        }
    }

    for (i = 0u; i < semantic.result.symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic.result.symbols[i];
        if (symbol->kind == SEMANTIC_RESULT_SYMBOL_GROUP &&
            symbol->name && strcmp(symbol->name, "Storage") == 0) {
            group_symbol_id = symbol->symbol_id;
            group_relation_count = symbol->relation_count;
            break;
        }
    }
    for (i = 0u; i < semantic.result.relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic.result.relations[i];
        if (relation->owner_symbol_id != group_symbol_id) continue;
        if (relation->kind == SEMANTIC_RESULT_RELATION_GROUP_MEMBER &&
            relation->name && strcmp(relation->name, "Storage") == 0) {
            saw_member = 1;
        } else if (relation->kind ==
                SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING &&
            relation->related_symbol_id != 0u &&
            relation->name && strcmp(relation->name, "Storage") == 0) {
            saw_binding = 1;
            bound_type_id = relation->type_id;
        } else if (relation->kind ==
                SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION &&
            relation->related_symbol_id != 0u &&
            relation->name && strcmp(relation->name, "open") == 0) {
            saw_method = 1;
        } else if (relation->kind ==
                SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER &&
            relation->related_symbol_id == 0u &&
            relation->name && strcmp(relation->name, "default") == 0) {
            saw_default = 1;
        }
    }
    for (i = 0u; i < semantic.result.symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic.result.symbols[i];
        if (symbol->declaration_node_id == self_node_id &&
            symbol->name && strcmp(symbol->name, "self") == 0) {
            self_symbol_id = symbol->symbol_id;
            if (symbol->type_id == bound_type_id && bound_type_id != 0u)
                saw_receiver_annotation = 1;
        }
    }
    for (i = 0u; i < semantic.result.annotation_count; ++i) {
        const SemanticResultAnnotation* annotation =
            &semantic.result.annotations[i];
        if (annotation->node_id == self_node_id &&
            annotation->resolved_symbol_id == self_symbol_id &&
            annotation->resolved_type_id == bound_type_id &&
            bound_type_id != 0u) {
            saw_receiver_annotation = 1;
            if ((annotation->flags &
                    SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER) != 0u)
                saw_address_receiver_annotation = 1;
        }
    }
    for (i = 0u; i < semantic.result.relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic.result.relations[i];
        if (relation->kind == SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER &&
            relation->related_node_id == self_node_id &&
            relation->related_symbol_id == self_symbol_id &&
            relation->type_id == bound_type_id &&
            bound_type_id != 0u) {
            saw_receiver_parameter_relation = 1;
        }
    }
    for (i = 0u; i < semantic.result.symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic.result.symbols[i];
        if (symbol->declaration_node_id ==
                resolve_node_id(&resolver, (ASTNode*)store_implementation) &&
            symbol->name && strcmp(symbol->name, "store_value_impl") == 0) {
            store_symbol_id = symbol->symbol_id;
        }
        if (symbol->declaration_node_id ==
                resolve_node_id(&resolver, (ASTNode*)static_info_implementation) &&
            symbol->name && strcmp(symbol->name, "static_info_impl") == 0) {
            static_info_symbol_id = symbol->symbol_id;
        }
    }
    for (i = 0u; i < semantic.result.annotation_count; ++i) {
        const SemanticResultAnnotation* annotation =
            &semantic.result.annotations[i];
        if (annotation->node_id == store_node_id &&
            annotation->resolved_symbol_id == store_symbol_id &&
            store_symbol_id != 0u) {
            saw_store_call_target = 1;
        }
        if (annotation->node_id == store_callee_node_id &&
            annotation->resolved_symbol_id == store_symbol_id &&
            store_symbol_id != 0u) {
            saw_store_callee_target = 1;
        }
        if (annotation->node_id == external_call_node_id &&
            annotation->resolved_symbol_id == static_info_symbol_id &&
            static_info_symbol_id != 0u) {
            saw_external_static_call_target = 1;
        }
        if (annotation->node_id == default_reference_node_id &&
            annotation->resolved_symbol_id == group_symbol_id &&
            annotation->value_category == SEMANTIC_RESULT_VALUE_RVALUE &&
            group_symbol_id != 0u) {
            saw_default_dispatch_annotation = 1;
        }
    }
    for (i = 0u; i < semantic.result.relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic.result.relations[i];
        if (relation->kind ==
                SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION &&
            relation->related_symbol_id == store_symbol_id &&
            relation->name && strcmp(relation->name, "store_value") == 0) {
            saw_store_method_relation = 1;
        }
        if (relation->kind ==
                SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION &&
            relation->related_symbol_id == static_info_symbol_id &&
            relation->name && strcmp(relation->name, "static_info") == 0) {
            saw_static_info_method_relation = 1;
        }
    }
    ok = group_symbol_id != 0u && group_relation_count == 10u &&
        saw_member && saw_binding && saw_method && saw_default &&
        self_symbol_id != 0u && saw_receiver_annotation &&
        saw_address_receiver_annotation &&
        saw_receiver_parameter_relation && saw_store_call_target &&
        saw_store_callee_target && saw_store_method_relation &&
        static_info_symbol_id != 0u && saw_external_static_call_target &&
        saw_static_info_method_relation && saw_default_dispatch_annotation;
    if (!ok) {
        fprintf(stderr,
            "Group SemanticResult mismatch: symbol=%u relations=%u member=%d binding=%d method=%d default=%d receiver_symbol=%u receiver_type=%u receiver_annotation=%d receiver_relation=%d store_symbol=%u store_target=%d callee_target=%d alias_relation=%d static_symbol=%u external_target=%d static_relation=%d default_dispatch=%d\n",
            group_symbol_id, group_relation_count, saw_member, saw_binding,
            saw_method, saw_default, self_symbol_id, bound_type_id,
            saw_receiver_annotation, saw_receiver_parameter_relation,
            store_symbol_id, saw_store_call_target, saw_store_callee_target,
            saw_store_method_relation, static_info_symbol_id,
            saw_external_static_call_target, saw_static_info_method_relation,
            saw_default_dispatch_annotation);
    }

    /* Serialize the real Group AST and validate the resulting schema 1.1 artifact. */
    if (ok && oir_ast_frontend_measure((ASTNode*)parser->ast_root,
            index, 256u, node_count, &ast_requirements) == OIR_STATUS_OK) {
        ast_records = (OirAstNodeRecord*)calloc(node_count, sizeof(*ast_records));
        ast_fields = (uint8_t*)malloc(ast_requirements.field_data_length ?
            ast_requirements.field_data_length : 1u);
        ast_strings = (uint8_t*)malloc(ast_requirements.string_table_length ?
            ast_requirements.string_table_length : 1u);
        ast_payload = (uint8_t*)malloc(ast_requirements.payload_size);
        artifact = (uint8_t*)malloc(1024u * 1024u);
        legacy_artifact = (uint8_t*)malloc(1024u * 1024u);
        incoming_counts = (uint32_t*)calloc(node_count, sizeof(*incoming_counts));
        visited = (uint8_t*)calloc(node_count, sizeof(*visited));
        graph_stack = (OirAstGraphStackEntry*)calloc(node_count, sizeof(*graph_stack));
        owner_scope_by_node = (uint32_t*)calloc(
            (size_t)node_count + 1u, sizeof(*owner_scope_by_node));
        if (!ast_records || !ast_fields || !ast_strings || !ast_payload ||
            !artifact || !legacy_artifact || !incoming_counts || !visited || !graph_stack ||
            !owner_scope_by_node) {
            ok = 0;
        } else if (oir_ast_frontend_write_payload(
                (ASTNode*)parser->ast_root, index, 256u, node_count,
                ast_records, node_count, ast_fields,
                ast_requirements.field_data_length, ast_strings,
                ast_requirements.string_table_length, ast_payload,
                ast_requirements.payload_size, &ast_payload_written,
                &ast_requirements) != OIR_STATUS_OK ||
            oir_semantic_ast_artifact_write(ast_payload, ast_payload_written,
                &semantic.result, 77u, artifact, 1024u * 1024u,
                &artifact_written) != OIR_STATUS_OK ||
            oir_semantic_ast_artifact_open(artifact, artifact_written,
                &artifact_view, incoming_counts, node_count, visited,
                node_count, graph_stack, node_count, owner_scope_by_node,
                (size_t)node_count + 1u) != OIR_STATUS_OK ||
            artifact_view.envelope.header.payload_schema_minor != 4u ||
            artifact_view.symbols.record_count != semantic.result.symbol_count ||
            artifact_view.relations.record_count != 14u) {
            ok = 0;
        } else {
            uint32_t annotation_index;
            int artifact_has_address_receiver = 0;
            for (annotation_index = 0u;
                annotation_index < artifact_view.annotations.record_count;
                ++annotation_index) {
                OirSemanticAnnotation encoded_annotation;
                if (oir_semantic_annotations_get(&artifact_view.annotations,
                        annotation_index, &encoded_annotation) != OIR_STATUS_OK) {
                    ok = 0;
                    break;
                }
                if (encoded_annotation.node_id == self_node_id &&
                    (encoded_annotation.flags &
                        SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER) != 0u)
                    artifact_has_address_receiver = 1;
            }
            if (!artifact_has_address_receiver) ok = 0;
            OirArtifactHeader legacy_header = artifact_view.envelope.header;
            OirSectionInput legacy_sections[8];
            uint32_t section_index;
            uint32_t legacy_section_count = artifact_view.envelope.section_count;
            if (legacy_section_count > 8u) {
                ok = 0;
            } else {
                for (section_index = 0u;
                    section_index < legacy_section_count;
                    ++section_index) {
                    OirSectionView section_view;
                    if (oir_artifact_get_section(&artifact_view.envelope,
                            section_index, &section_view) != OIR_STATUS_OK) {
                        ok = 0;
                        break;
                    }
                    legacy_sections[section_index].type = section_view.type;
                    legacy_sections[section_index].version = section_view.version;
                    legacy_sections[section_index].flags = section_view.flags;
                    legacy_sections[section_index].data = section_view.data;
                    legacy_sections[section_index].length = section_view.length;
                }
                if (ok) {
                    legacy_header.payload_schema_minor = 1u;
                    if (oir_artifact_write(legacy_artifact, 1024u * 1024u,
                            &legacy_header, legacy_sections,
                            legacy_section_count,
                            &legacy_artifact_written) != OIR_STATUS_OK ||
                        oir_semantic_ast_artifact_open(legacy_artifact,
                            legacy_artifact_written, &artifact_view,
                            incoming_counts, node_count, visited, node_count,
                            graph_stack, node_count, owner_scope_by_node,
                            (size_t)node_count + 1u) != OIR_STATUS_INVALID_SECTION) {
                        ok = 0;
                    }
                }
                if (ok) {
                    legacy_header.payload_schema_minor = 0u;
                    if (oir_artifact_write(legacy_artifact, 1024u * 1024u,
                            &legacy_header, legacy_sections,
                            legacy_section_count,
                            &legacy_artifact_written) != OIR_STATUS_OK ||
                        oir_semantic_ast_artifact_open(legacy_artifact,
                            legacy_artifact_written, &artifact_view,
                            incoming_counts, node_count, visited, node_count,
                            graph_stack, node_count, owner_scope_by_node,
                            (size_t)node_count + 1u) != OIR_STATUS_INVALID_SECTION) {
                        ok = 0;
                    }
                }
            }
        }
    } else if (ok) {
        ok = 0;
    }

    free(ast_records);
    free(ast_fields);
    free(ast_strings);
    free(ast_payload);
    free(artifact);
    free(legacy_artifact);
    free(incoming_counts);
    free(visited);
    free(graph_stack);
    free(owner_scope_by_node);
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int test_invalid_struct_literal_not_finalized(void) {
    static const char source[] =
        "f:main() {"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { x = 1 };"
        "}";
    ParserContext* parser=parser_init_source(
        "invalid-struct-literal-result.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    OirAstPointerIndexEntry index[128];
    ResolverContext resolver;
    SemanticContext semantic;
    uint32_t node_count=0u;
    int ok=0;
    if(!parser)return 0;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count ||
        !parser->ast_root)goto done;
    memset(index,0,sizeof(index));
    if(oir_ast_frontend_index_build((ASTNode*)parser->ast_root,
        index,128u,&node_count)!=OIR_STATUS_OK)goto done;
    resolver.entries=index;
    resolver.capacity=128u;
    semantic_init(&semantic,"invalid-struct-literal-result.nxf",NULL);
    semantic_attach_node_id_resolver(&semantic,resolve_node_id,&resolver);
    semantic_check(&semantic,(ASTNode*)parser->ast_root);
    if(semantic.error_count==0 ||
        semantic_finalize_result(&semantic,node_count)!=SEMANTIC_RESULT_INVALID_DATA) {
        fprintf(stderr,
            "Invalid Struct literal unexpectedly finalized: errors=%d status=%d\\n",
            semantic.error_count,(int)semantic.result_status);
        semantic_destroy(&semantic);
        goto done;
    }
    ok=1;
    semantic_destroy(&semantic);
done:
    parser_destroy(parser);
    return ok;
}

static int test_struct_literal_semantic_records(void) {
    static const char source[] =
        "f:main() {"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { y = 2, x = 1 };"
        "}";
    ParserContext* parser = parser_init_source(
        "struct-literal-semantic-result.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    OirAstPointerIndexEntry index[128];
    ResolverContext resolver;
    SemanticContext semantic;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTStructDecl* declaration;
    ASTVarDecl* variable;
    ASTStructLiteral* literal;
    ASTNode* field_node;
    uint32_t node_count = 0u, struct_symbol_id = 0u, nominal_type_id = 0u;
    uint32_t i, i32_type_id = 0u;
    int saw_literal_annotation = 0;
    int saw_x_relation = 0, saw_y_relation = 0;
    int saw_x_initializer = 0, saw_y_initializer = 0;
    int ok = 0;
    int stage = 1;

    if (!parser) return 0;
    stage = 2;
    if (parser_run(parser) != PARSER_STATUS_OK ||
        parser->error_count != 0 || !parser->ast_root) goto done;
    stage = 3;
    memset(index, 0, sizeof(index));
    if (oir_ast_frontend_index_build((ASTNode*)parser->ast_root,
            index, 128u, &node_count) != OIR_STATUS_OK) goto done;
    stage = 4;
    resolver.entries = index;
    resolver.capacity = 128u;

    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    body = function && function->body && function->body->type == AST_BLOCK
        ? (ASTBlock*)function->body : NULL;
    declaration = body && body->statements &&
        body->statements->type == AST_STRUCT_DECL
            ? (ASTStructDecl*)body->statements : NULL;
    variable = declaration && declaration->base.next &&
        declaration->base.next->type == AST_VAR_DECL
            ? (ASTVarDecl*)declaration->base.next : NULL;
    literal = variable && variable->initializer &&
        variable->initializer->type == AST_STRUCT_LITERAL
            ? (ASTStructLiteral*)variable->initializer : NULL;
    if (!declaration || !variable || !literal || !literal->fields) goto done;
    stage = 5;

    semantic_init(&semantic, "struct-literal-semantic-result.nxf", NULL);
    semantic_attach_node_id_resolver(&semantic, resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    stage = 6;
    if (semantic.error_count != 0 ||
        semantic.result_status != SEMANTIC_RESULT_OK) {
        fprintf(stderr, "Struct literal SemanticResult checker: errors=%d status=%d\\n",
            semantic.error_count, (int)semantic.result_status);
        semantic_destroy(&semantic);
        goto done;
    }
    if (semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK) {
        fprintf(stderr, "Struct literal SemanticResult finalize failed: status=%d\\n",
            (int)semantic.result_status);
        semantic_destroy(&semantic);
        goto done;
    }
    stage = 7;

    for (i = 0u; i < semantic.result.symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic.result.symbols[i];
        if (symbol->declaration_node_id ==
                resolve_node_id(&resolver, (ASTNode*)declaration) &&
            symbol->kind == SEMANTIC_RESULT_SYMBOL_STRUCT &&
            symbol->name && strcmp(symbol->name, "Point") == 0) {
            struct_symbol_id = symbol->symbol_id;
            nominal_type_id = symbol->type_id;
        }
    }
    for (i = 0u; i < semantic.result.type_count; ++i) {
        const SemanticResultType* type = &semantic.result.types[i];
        if (type->name && strcmp(type->name, "i32") == 0 &&
            type->kind == SEMANTIC_RESULT_TYPE_PRIMITIVE &&
            (type->flags & SEMANTIC_RESULT_TYPE_FLAG_HARD) == 0u) {
            i32_type_id = type->type_id;
        }
    }
    for (i = 0u; i < semantic.result.annotation_count; ++i) {
        const SemanticResultAnnotation* annotation =
            &semantic.result.annotations[i];
        if (annotation->node_id ==
                resolve_node_id(&resolver, (ASTNode*)literal) &&
            annotation->resolved_symbol_id == struct_symbol_id &&
            annotation->resolved_type_id == nominal_type_id &&
            annotation->value_category == SEMANTIC_RESULT_VALUE_RVALUE &&
            struct_symbol_id != 0u && nominal_type_id != 0u) {
            saw_literal_annotation = 1;
        }
    }
    for (i = 0u; i < semantic.result.relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic.result.relations[i];
        if (relation->owner_symbol_id != struct_symbol_id ||
            relation->kind != SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD)
            continue;
        if (relation->name && strcmp(relation->name, "x") == 0 &&
            relation->ordinal == 0u && relation->related_symbol_id == 0u &&
            relation->related_node_id ==
                resolve_node_id(&resolver, declaration->fields)) {
            saw_x_relation = 1;
        }
        if (relation->name && strcmp(relation->name, "y") == 0 &&
            relation->ordinal == 1u && relation->related_symbol_id == 0u &&
            relation->related_node_id ==
                resolve_node_id(&resolver, declaration->fields->next)) {
            saw_y_relation = 1;
        }
    }
    for (field_node = literal->fields; field_node; field_node = field_node->next) {
        ASTStructFieldInit* initializer = (ASTStructFieldInit*)field_node;
        uint32_t initializer_id = resolve_node_id(&resolver, field_node);
        uint32_t value_id = resolve_node_id(&resolver, initializer->value);
        int found_initializer = 0, found_value = 0;
        for (i = 0u; i < semantic.result.annotation_count; ++i) {
            const SemanticResultAnnotation* annotation =
                &semantic.result.annotations[i];
            if (annotation->node_id == initializer_id &&
                annotation->resolved_symbol_id == 0u &&
                annotation->resolved_type_id == i32_type_id &&
                i32_type_id != 0u) found_initializer = 1;
            if (annotation->node_id == value_id &&
                annotation->resolved_type_id == i32_type_id &&
                i32_type_id != 0u) found_value = 1;
        }
        if (initializer->name && strcmp(initializer->name, "y") == 0) {
            saw_y_initializer = found_initializer && found_value;
        } else if (initializer->name && strcmp(initializer->name, "x") == 0) {
            saw_x_initializer = found_initializer && found_value;
        }
    }

    ok = saw_literal_annotation && saw_x_relation && saw_y_relation &&
        saw_x_initializer && saw_y_initializer;
    if (!ok) {
        fprintf(stderr,
            "Struct literal SemanticResult mismatch: symbol=%u type=%u i32=%u literal=%d relations=%d/%d initializers=%d/%d\\n",
            struct_symbol_id, nominal_type_id, i32_type_id,
            saw_literal_annotation, saw_x_relation, saw_y_relation,
            saw_x_initializer, saw_y_initializer);
    }
    semantic_destroy(&semantic);
done:
    if (!ok) fprintf(stderr, "Struct literal SemanticResult test stopped at stage %d\\n", stage);
    parser_destroy(parser);
    return ok;
}

static int test_immutable_vs_compile_time_constant_flags(void) {
    static const char source[] =
        "c:CompileValue = 5; m:RuntimeValue = 6;";
    ParserContext* parser = NULL;
    OirAstPointerIndexEntry index[32];
    OirAstFrontendRequirements requirements;
    OirAstNodeRecord* records = NULL;
    uint8_t *fields = NULL, *strings = NULL, *ast_payload = NULL;
    uint8_t *artifact = NULL, *visited = NULL;
    uint32_t *incoming = NULL, *owner_scope = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    OirSemanticAstArtifactView view;
    OirArtifactView envelope;
    OirSectionView section_view;
    OirSectionInput legacy_sections[8];
    uint8_t* legacy_artifact = NULL;
    ResolverContext resolver;
    SemanticContext semantic;
    uint32_t node_count = 0u, i;
    size_t ast_written = 0u, artifact_written = 0u;
    int semantic_initialized = 0;
    int saw_compile_time_const = 0, saw_immutable = 0, ok = 0;

    memset(index, 0, sizeof(index));
    memset(&requirements, 0, sizeof(requirements));
    memset(&view, 0, sizeof(view));
    parser = parser_init_source("m-vs-c-flags.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
        parser->error_count != 0 || !parser->ast_root ||
        !ast_validate((ASTNode*)parser->ast_root, NULL))
        goto done;
    if (oir_ast_frontend_index_build((ASTNode*)parser->ast_root,
            index, 32u, &node_count) != OIR_STATUS_OK)
        goto done;
    resolver.entries = index;
    resolver.capacity = 32u;
    semantic_init(&semantic, "m-vs-c-flags.nxf", NULL);
    semantic_initialized = 1;
    semantic_attach_node_id_resolver(&semantic, resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    if (semantic.error_count != 0 ||
        semantic.result_status != SEMANTIC_RESULT_OK ||
        semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK)
        goto done;

    for (i = 0u; i < semantic.result.symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic.result.symbols[i];
        if (strcmp(symbol->name, "CompileValue") == 0) {
            saw_compile_time_const =
                (symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_CONST) != 0u &&
                (symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE) == 0u;
        } else if (strcmp(symbol->name, "RuntimeValue") == 0) {
            saw_immutable =
                (symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE) != 0u &&
                (symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_CONST) == 0u;
        }
    }
    if (!saw_compile_time_const || !saw_immutable) goto done;

    if (oir_ast_frontend_measure((ASTNode*)parser->ast_root, index, 32u,
            node_count, &requirements) != OIR_STATUS_OK)
        goto done;
    records = (OirAstNodeRecord*)calloc(node_count, sizeof(*records));
    fields = (uint8_t*)malloc(requirements.field_data_length ?
        requirements.field_data_length : 1u);
    strings = (uint8_t*)malloc(requirements.string_table_length ?
        requirements.string_table_length : 1u);
    ast_payload = (uint8_t*)malloc(requirements.payload_size);
    artifact = (uint8_t*)malloc(65536u);
    legacy_artifact = (uint8_t*)malloc(65536u);
    incoming = (uint32_t*)calloc(node_count, sizeof(*incoming));
    visited = (uint8_t*)calloc(node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)calloc(node_count, sizeof(*graph_stack));
    owner_scope = (uint32_t*)calloc((size_t)node_count + 1u, sizeof(*owner_scope));
    if (!records || !fields || !strings || !ast_payload || !artifact ||
        !legacy_artifact || !incoming || !visited || !graph_stack || !owner_scope)
        goto done;
    if (oir_ast_frontend_write_payload((ASTNode*)parser->ast_root,
            index, 32u, node_count, records, node_count,
            fields, requirements.field_data_length,
            strings, requirements.string_table_length,
            ast_payload, requirements.payload_size, &ast_written,
            &requirements) != OIR_STATUS_OK ||
        oir_semantic_ast_artifact_write(ast_payload, ast_written,
            &semantic.result, 9001u, artifact, 65536u,
            &artifact_written) != OIR_STATUS_OK ||
        oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, node_count, visited, node_count,
            graph_stack, node_count, owner_scope, (size_t)node_count + 1u)
            != OIR_STATUS_OK ||
        view.envelope.header.payload_schema_minor != 4u)
        goto done;

    saw_compile_time_const = 0;
    saw_immutable = 0;
    for (i = 0u; i < view.symbols.record_count; ++i) {
        OirSemanticSymbol symbol;
        const uint8_t* name = NULL;
        if (oir_semantic_symbols_get(&view.symbols, i, &symbol, &name)
                != OIR_STATUS_OK)
            goto done;
        if (symbol.name_length == 12u &&
            memcmp(name, "CompileValue", 12u) == 0) {
            saw_compile_time_const =
                (symbol.flags & OIR_SEMANTIC_SYMBOL_FLAG_CONST) != 0u &&
                (symbol.flags & OIR_SEMANTIC_SYMBOL_FLAG_IMMUTABLE) == 0u;
        } else if (symbol.name_length == 12u &&
            memcmp(name, "RuntimeValue", 12u) == 0) {
            saw_immutable =
                (symbol.flags & OIR_SEMANTIC_SYMBOL_FLAG_IMMUTABLE) != 0u &&
                (symbol.flags & OIR_SEMANTIC_SYMBOL_FLAG_CONST) == 0u;
        }
    }
    if (!saw_compile_time_const || !saw_immutable ||
        oir_artifact_open(artifact, artifact_written, &envelope) != OIR_STATUS_OK ||
        envelope.section_count > 8u)
        goto done;
    for (i = 0u; i < envelope.section_count; ++i) {
        if (oir_artifact_get_section(&envelope, i, &section_view) != OIR_STATUS_OK)
            goto done;
        legacy_sections[i].type = section_view.type;
        legacy_sections[i].version = section_view.version;
        legacy_sections[i].flags = section_view.flags;
        legacy_sections[i].data = section_view.data;
        legacy_sections[i].length = section_view.length;
    }
    envelope.header.payload_schema_minor = 3u;
    {
        size_t legacy_written = 0u;
        if (oir_artifact_write(legacy_artifact, 65536u, &envelope.header,
                legacy_sections, envelope.section_count, &legacy_written)
                != OIR_STATUS_OK ||
            oir_semantic_ast_artifact_open(legacy_artifact, legacy_written,
                &view, incoming, node_count, visited, node_count,
                graph_stack, node_count, owner_scope,
                (size_t)node_count + 1u) != OIR_STATUS_INVALID_SECTION)
            goto done;
    }
    ok = 1;
done:
    if (semantic_initialized) semantic_destroy(&semantic);
    if (parser) parser_destroy(parser);
    free(records); free(fields); free(strings); free(ast_payload); free(artifact); free(legacy_artifact);
    free(incoming); free(visited); free(graph_stack); free(owner_scope);
    if (!ok) fprintf(stderr, "c: compile-time constant and m: immutable flags were conflated.\n");
    return ok;
}

int main(void) {
    if (!test_bit_byte_semantic_conversion_records()) {
        fprintf(stderr, "Bit/byte SemanticResult conversion records failed.\\n");
        return 1;
    }
    if (!test_thread_payload_capture_annotations()) {
        fprintf(stderr, "Thread payload by-value capture annotations failed.\\n");
        return 1;
    }
    if(!test_immutable_vs_compile_time_constant_flags()) {
        fprintf(stderr,"c:/m: SemanticResult flag separation failed.\n");
        return 1;
    }

    if(!test_invalid_struct_literal_not_finalized()) {
        fprintf(stderr,"Invalid Struct literal SemanticResult gate failed.\\n"); return 1;
    }
    if(!test_struct_literal_semantic_records()) {
        fprintf(stderr,"Struct literal SemanticResult relation integration failed.\\n");
        return 1;
    }
    if(!test_group_semantic_records()) {
        fprintf(stderr,"Group SemanticResult record integration failed.\n");
        return 1;
    }
    if(!test_extended_semantic_scopes()) {
        fprintf(stderr,"Extended AST semantic scope/result integration failed.\\n");
        return 1;
    }
    ASTProgram program;
    ASTFuncDecl function;
    ASTVarDecl parameter;
    ASTBlock body;
    ASTVarDecl local;
    ASTLiteral literal;
    ASTVarDecl shadowed;
    ASTIdentifier shadowed_reference;

    OirAstPointerIndexEntry ast_index[32];
    OirAstFrontendRequirements ast_requirements;
    OirAstNodeRecord* ast_records = NULL;
    uint8_t* ast_fields = NULL;
    uint8_t* ast_strings = NULL;
    uint8_t* ast_payload = NULL;
    size_t ast_payload_written = 0u;
    uint32_t ast_node_count = 0u;
    ResolverContext resolver;

    uint8_t semantic_artifact[8192];
    uint8_t repeated_artifact[8192];
    size_t semantic_artifact_written = 0u;
    size_t injected_written = 0u;
    uint32_t writer_allocation_count = 0u;
    uint32_t fail_index;
    size_t repeated_artifact_written = 0u;
    OirSemanticAstArtifactView artifact_view;
    uint32_t* incoming_counts = NULL;
    uint8_t* visited = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    uint32_t* owner_scope_by_node = NULL;
    OirSectionView ast_payload_section;
    OirSemanticSymbol decoded_function;
    const uint8_t* decoded_function_name = NULL;

    SemanticContext context;

    memset(&program, 0, sizeof(program));
    memset(&function, 0, sizeof(function));
    memset(&parameter, 0, sizeof(parameter));
    memset(&body, 0, sizeof(body));
    memset(&local, 0, sizeof(local));
    memset(&literal, 0, sizeof(literal));
    memset(&shadowed, 0, sizeof(shadowed));
    memset(&shadowed_reference, 0, sizeof(shadowed_reference));
    memset(ast_index, 0, sizeof(ast_index));

    program.base.type = AST_PROGRAM;
    program.base.line = 1;
    program.base.col = 1;
    program.declarations = (ASTNode*)&function;

    function.base.type = AST_FUNC_DECL;
    function.base.line = 1;
    function.base.col = 1;
    function.name = "sum";
    function.return_type = "i32";
    function.is_public = 1;
    function.params = (ASTNode*)&parameter;
    function.body = (ASTNode*)&body;

    parameter.base.type = AST_VAR_DECL;
    parameter.base.line = 1;
    parameter.base.col = 5;
    parameter.name = "input";
    parameter.type_name = "i32";

    body.base.type = AST_BLOCK;
    body.base.line = 2;
    body.base.col = 1;
    body.statements = (ASTNode*)&local;
    local.base.next = (ASTNode*)&shadowed;

    local.base.type = AST_VAR_DECL;
    local.base.line = 3;
    local.base.col = 3;
    local.name = "input";
    local.type_name = "i32";
    local.is_hard_typed = 1;
    local.initializer = (ASTNode*)&literal;

    literal.base.type = AST_LITERAL;
    literal.base.line = 3;
    literal.base.col = 11;
    literal.type = TOKEN_NUMBER;
    literal.string_value = "7";

    shadowed.base.type = AST_VAR_DECL;
    shadowed.base.line = 4;
    shadowed.base.col = 3;
    shadowed.name = "copy";
    shadowed.type_name = NULL;
    shadowed.initializer = (ASTNode*)&shadowed_reference;

    shadowed_reference.base.type = AST_IDENTIFIER;
    shadowed_reference.base.line = 4;
    shadowed_reference.base.col = 10;
    shadowed_reference.name = "input";

    assert(oir_ast_frontend_index_build(
        (ASTNode*)&program, ast_index, 32u, &ast_node_count) == OIR_STATUS_OK);
    assert(ast_node_count == 8u);
    assert(oir_ast_frontend_measure(
        (ASTNode*)&program, ast_index, 32u, ast_node_count,
        &ast_requirements) == OIR_STATUS_OK);

    ast_records = (OirAstNodeRecord*)calloc(
        ast_node_count, sizeof(*ast_records));
    ast_fields = (uint8_t*)malloc(
        ast_requirements.field_data_length
            ? ast_requirements.field_data_length : 1u);
    ast_strings = (uint8_t*)malloc(
        ast_requirements.string_table_length
            ? ast_requirements.string_table_length : 1u);
    ast_payload = (uint8_t*)malloc(ast_requirements.payload_size);
    assert(ast_records && ast_fields && ast_strings && ast_payload);

    assert(oir_ast_frontend_write_payload(
        (ASTNode*)&program,
        ast_index,
        32u,
        ast_node_count,
        ast_records,
        ast_node_count,
        ast_fields,
        ast_requirements.field_data_length,
        ast_strings,
        ast_requirements.string_table_length,
        ast_payload,
        ast_requirements.payload_size,
        &ast_payload_written,
        &ast_requirements) == OIR_STATUS_OK);
    assert(ast_payload_written == ast_requirements.payload_size);

    resolver.entries = ast_index;
    resolver.capacity = 32u;
    semantic_init(&context, "semantic-result-integration.nxf", NULL);
    semantic_attach_node_id_resolver(&context, resolve_node_id, &resolver);
    semantic_check(&context, (ASTNode*)&program);

    /* Function parameters and block locals are transient; the root function remains. */
    assert(context.current_depth == 0);
    assert(context.current_scope_id == 1u);
    assert(context.head != NULL && strcmp(context.head->name, "sum") == 0);
    assert(context.head->next == NULL);

    assert(context.error_count == 0);
    assert(context.result_status == SEMANTIC_RESULT_OK);
    assert(context.result.scope_count == 3u);
    assert(context.result.symbol_count == 4u);
    assert(context.result.type_count == 2u);
    assert(context.result.relation_count == 1u);
    assert(context.result.annotation_count >= 6u);

    assert(semantic_finalize_result(&context, ast_node_count) ==
        SEMANTIC_RESULT_OK);
    assert(context.result.finalized == 1u);

    /* Function scope and body block remain after transient Slot cleanup. */
    assert(context.result.scopes[1].owner_node_id == 2u);
    assert(context.result.scopes[1].parent_scope_id == 1u);
    assert(context.result.scopes[2].owner_node_id == 4u);
    assert(context.result.scopes[2].parent_scope_id == 2u);

    assert(context.result.symbols[0].name &&
        strcmp(context.result.symbols[0].name, "sum") == 0);
    assert((context.result.symbols[0].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC) != 0u);
    assert((context.result.symbols[2].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_HARD) != 0u);
    assert(context.result.symbols[2].type_id == 2u);
    assert(strcmp(context.result.symbols[2].name, "input") == 0);
    assert(strcmp(context.result.symbols[3].name, "copy") == 0);
    /* An untyped local initialized from an identifier inherits its resolved type. */
    assert(context.result.symbols[3].type_id == 1u);
    assert(context.result.symbols[0].relation_count == 1u);
    assert(context.result.relations[0].related_node_id == 3u);
    assert(context.result.relations[0].related_symbol_id == 2u);

    assert(semantic_result_find_visible_symbol(
        &context.result, 1u, "sum") == 1u);
    assert(semantic_result_find_visible_symbol(
        &context.result, 2u, "input") == 2u);
    assert(semantic_result_find_visible_symbol(
        &context.result, 3u, "input") == 3u);
    assert(semantic_result_find_visible_symbol(
        &context.result, 3u, "copy") == 4u);

    /* The inferred declaration annotation must carry its canonical type and symbol. */
    {
        uint32_t declaration_node_id = resolve_node_id(&resolver, (ASTNode*)&shadowed);
        uint32_t i;
        int found_inferred_declaration = 0;
        for (i = 0u; i < context.result.annotation_count; ++i) {
            const SemanticResultAnnotation* annotation = &context.result.annotations[i];
            if (annotation->node_id == declaration_node_id) {
                assert(annotation->resolved_symbol_id == 4u);
                assert(annotation->resolved_type_id == 1u);
                assert(annotation->value_category == SEMANTIC_RESULT_VALUE_LVALUE);
                found_inferred_declaration = 1;
                break;
            }
        }
        assert(found_inferred_declaration);
    }

    /* Identifier lookup in the nested block must bind to the shadowing local. */
    {
        uint32_t reference_node_id = resolve_node_id(&resolver,
            (ASTNode*)&shadowed_reference);
        uint32_t i;
        int found_shadow_binding = 0;
        for (i = 0u; i < context.result.annotation_count; ++i) {
            const SemanticResultAnnotation* annotation =
                &context.result.annotations[i];
            if (annotation->node_id == reference_node_id) {
                assert(annotation->resolved_symbol_id == 3u);
                assert(annotation->scope_id == 3u);
                found_shadow_binding = 1;
                break;
            }
        }
        assert(found_shadow_binding);
    }

    /* Fail each writer-owned allocation in turn; output must remain unpublished. */
    memset(semantic_artifact, 0xa5, sizeof(semantic_artifact));
    assert(oir_semantic_ast_artifact_write_test_fail_at(
        ast_payload, ast_payload_written, &context.result, 42u,
        semantic_artifact, sizeof(semantic_artifact), &injected_written,
        UINT32_MAX, &writer_allocation_count) == OIR_STATUS_OK);
    assert(injected_written != 0u && writer_allocation_count > 0u);
    for (fail_index = 1u; fail_index <= writer_allocation_count; ++fail_index) {
        size_t j;
        memset(semantic_artifact, 0xa5, sizeof(semantic_artifact));
        injected_written = 123u;
        assert(oir_semantic_ast_artifact_write_test_fail_at(
            ast_payload, ast_payload_written, &context.result, 42u,
            semantic_artifact, sizeof(semantic_artifact), &injected_written,
            fail_index, NULL) == OIR_STATUS_OUT_OF_MEMORY);
        assert(injected_written == 0u);
        for (j = 0u; j < sizeof(semantic_artifact); ++j) {
            assert(semantic_artifact[j] == 0xa5u);
        }
    }

    /* The semantic result is encoded, then re-opened by the full artifact gate. */
    assert(oir_semantic_ast_artifact_write(
        ast_payload,
        ast_payload_written,
        &context.result,
        42u,
        semantic_artifact,
        sizeof(semantic_artifact),
        &semantic_artifact_written) == OIR_STATUS_OK);
    assert(semantic_artifact_written != 0u);

    /* A short output buffer must not publish or partially modify an artifact. */
    {
        size_t short_written = 123u;
        memset(semantic_artifact, 0xa5, sizeof(semantic_artifact));
        assert(oir_semantic_ast_artifact_write(
            ast_payload,
            ast_payload_written,
            &context.result,
            42u,
            semantic_artifact,
            semantic_artifact_written - 1u,
            &short_written) == OIR_STATUS_BUFFER_TOO_SMALL);
        assert(short_written == 0u && semantic_artifact[0] == 0xa5u);
    }
    assert(oir_semantic_ast_artifact_write(
        ast_payload,
        ast_payload_written,
        &context.result,
        42u,
        semantic_artifact,
        sizeof(semantic_artifact),
        &semantic_artifact_written) == OIR_STATUS_OK);

    /* Same finalized input and job ID must produce byte-identical OIR. */
    assert(oir_semantic_ast_artifact_write(
        ast_payload,
        ast_payload_written,
        &context.result,
        42u,
        repeated_artifact,
        sizeof(repeated_artifact),
        &repeated_artifact_written) == OIR_STATUS_OK);
    assert(repeated_artifact_written == semantic_artifact_written);
    assert(memcmp(repeated_artifact, semantic_artifact,
        semantic_artifact_written) == 0);

    /* A downstream consumer must not depend on the producer's live model. */
    semantic_destroy(&context);

    incoming_counts = (uint32_t*)calloc(ast_node_count, sizeof(*incoming_counts));
    visited = (uint8_t*)calloc(ast_node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)calloc(
        ast_node_count, sizeof(*graph_stack));
    owner_scope_by_node = (uint32_t*)calloc(
        (size_t)ast_node_count + 1u, sizeof(*owner_scope_by_node));
    assert(incoming_counts && visited && graph_stack && owner_scope_by_node);

    assert(oir_semantic_ast_artifact_open(
        semantic_artifact,
        semantic_artifact_written,
        &artifact_view,
        incoming_counts,
        ast_node_count,
        visited,
        ast_node_count,
        graph_stack,
        ast_node_count,
        owner_scope_by_node,
        (size_t)ast_node_count + 1u) == OIR_STATUS_OK);

    assert(artifact_view.envelope.header.artifact_kind == OIR_KIND_SEMANTIC_AST);
    assert(artifact_view.envelope.header.producer_stage_id == OCC_STAGE_SEMANTIC);
    assert(artifact_view.envelope.header.job_id == 42u);
    assert(artifact_view.ast.node_count == ast_node_count);
    assert(artifact_view.scopes.record_count == 3u);
    assert(artifact_view.types.record_count == 2u);
    assert(artifact_view.symbols.record_count == 4u);
    assert(artifact_view.annotations.record_count >= 4u);
    assert(artifact_view.relations.record_count == 1u);
    assert(artifact_view.has_conversions == 0u);

    assert(oir_artifact_get_section(
        &artifact_view.envelope, 0u, &ast_payload_section) == OIR_STATUS_OK);
    assert(ast_payload_section.length == ast_payload_written);
    assert(memcmp(ast_payload_section.data, ast_payload, ast_payload_written) == 0);

    assert(oir_semantic_symbols_get(
        &artifact_view.symbols, 0u, &decoded_function,
        &decoded_function_name) == OIR_STATUS_OK);
    assert(decoded_function.name_length == 3u);
    assert(memcmp(decoded_function_name, "sum", 3u) == 0);

    free(ast_records);
    free(ast_fields);
    free(ast_strings);
    free(ast_payload);
    free(incoming_counts);
    free(visited);
    free(graph_stack);
    free(owner_scope_by_node);

    puts("Semantic checker -> SEMANTIC_AST artifact integration test passed.");
    return 0;
}
