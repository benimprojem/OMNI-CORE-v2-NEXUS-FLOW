#ifdef NDEBUG
#undef NDEBUG
#endif
#include "semantic.h"
#include "lexer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct TestNodeId {
    const ASTNode* node;
    uint32_t id;
} TestNodeId;

static uint32_t resolve_node_id(void* user_context, const ASTNode* node) {
    const TestNodeId* entries = (const TestNodeId*)user_context;
    size_t count = 6u;
    size_t i;
    for (i = 0u; i < count; ++i) {
        if (entries[i].node == node) return entries[i].id;
    }
    return 0u;
}

int main(void) {
    ASTProgram program;
    ASTFuncDecl function;
    ASTVarDecl parameter;
    ASTBlock body;
    ASTVarDecl local;
    ASTLiteral literal;
    TestNodeId node_ids[6];
    SemanticContext context;

    memset(&program, 0, sizeof(program));
    memset(&function, 0, sizeof(function));
    memset(&parameter, 0, sizeof(parameter));
    memset(&body, 0, sizeof(body));
    memset(&local, 0, sizeof(local));
    memset(&literal, 0, sizeof(literal));

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

    node_ids[0].node = (ASTNode*)&program;
    node_ids[0].id = 1u;
    node_ids[1].node = (ASTNode*)&function;
    node_ids[1].id = 2u;
    node_ids[2].node = (ASTNode*)&parameter;
    node_ids[2].id = 3u;
    node_ids[3].node = (ASTNode*)&body;
    node_ids[3].id = 4u;
    node_ids[4].node = (ASTNode*)&local;
    node_ids[4].id = 5u;
    node_ids[5].node = (ASTNode*)&literal;
    node_ids[5].id = 6u;

    semantic_init(&context, "semantic-result-integration.nxf", NULL);
    semantic_attach_node_id_resolver(&context, resolve_node_id, node_ids);
    semantic_check(&context, (ASTNode*)&program);

    assert(context.error_count == 0);
    assert(context.result_status == SEMANTIC_RESULT_OK);
    assert(context.result.scope_count == 3u);
    assert(context.result.symbol_count == 3u);
    assert(context.result.type_count == 2u);
    assert(context.result.relation_count == 1u);
    assert(context.result.annotation_count >= 4u);

    assert(semantic_finalize_result(&context, 6u) == SEMANTIC_RESULT_OK);
    assert(context.result.finalized == 1u);

    /* Function scope and body block remain in the result after Slot cleanup. */
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

    semantic_destroy(&context);
    puts("Semantic checker persistent result integration test passed.");
    return 0;
}
