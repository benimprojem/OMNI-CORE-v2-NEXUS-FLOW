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

int main(void) {
    ASTProgram program;
    ASTFuncDecl function;
    ASTVarDecl parameter;
    ASTBlock body;
    ASTVarDecl local;
    ASTLiteral literal;

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
    size_t semantic_artifact_written = 0u;
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

    local.base.type = AST_VAR_DECL;
    local.base.line = 3;
    local.base.col = 3;
    local.name = "local";
    local.type_name = "i32";
    local.is_hard_typed = 1;
    local.initializer = (ASTNode*)&literal;

    literal.base.type = AST_LITERAL;
    literal.base.line = 3;
    literal.base.col = 11;
    literal.type = TOKEN_NUMBER;
    literal.string_value = "7";

    assert(oir_ast_frontend_index_build(
        (ASTNode*)&program, ast_index, 32u, &ast_node_count) == OIR_STATUS_OK);
    assert(ast_node_count == 6u);
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

    assert(context.error_count == 0);
    assert(context.result_status == SEMANTIC_RESULT_OK);
    assert(context.result.scope_count == 3u);
    assert(context.result.symbol_count == 3u);
    assert(context.result.type_count == 2u);
    assert(context.result.relation_count == 1u);
    assert(context.result.annotation_count >= 4u);

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
    assert(context.result.symbols[0].relation_count == 1u);
    assert(context.result.relations[0].related_node_id == 3u);
    assert(context.result.relations[0].related_symbol_id == 2u);

    assert(semantic_result_find_visible_symbol(
        &context.result, 1u, "sum") == 1u);
    assert(semantic_result_find_visible_symbol(
        &context.result, 2u, "input") == 2u);
    assert(semantic_result_find_visible_symbol(
        &context.result, 3u, "local") == 3u);

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
    assert(artifact_view.symbols.record_count == 3u);
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

    semantic_destroy(&context);
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
