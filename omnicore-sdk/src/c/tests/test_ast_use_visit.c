#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t seen;
    int stop_after_first;
    int valid;
} VisitState;

static char* copy_text(const char* text) {
    size_t n = strlen(text);
    char* result = (char*)malloc(n + 1u);
    if (result) memcpy(result, text, n + 1u);
    return result;
}

static ASTNode* make_use(const char* module, const char* alias, const char* ns) {
    ASTUse* use = (ASTUse*)ast_create(AST_USE);
    if (!use) return NULL;
    use->module_name = copy_text(module);
    use->alias = alias ? copy_text(alias) : NULL;
    use->namespace_name = ns ? copy_text(ns) : NULL;
    if (!use->module_name || (alias && !use->alias) || (ns && !use->namespace_name)) {
        ast_destroy((ASTNode*)use);
        return NULL;
    }
    return (ASTNode*)use;
}

static int visit_use(const ASTUse* use, uint32_t ordinal, void* context) {
    VisitState* state = (VisitState*)context;
    static const char* modules[] = {"io", "db.mysql.sql"};
    static const char* aliases[] = {NULL, "connector"};
    static const char* namespaces[] = {NULL, "sql"};
    size_t index = state->seen;
    if (index >= 2u || ordinal != (uint32_t)(index + 1u) ||
        strcmp(use->module_name, modules[index]) != 0 ||
        ((use->alias == NULL) != (aliases[index] == NULL)) ||
        (use->alias && strcmp(use->alias, aliases[index]) != 0) ||
        ((use->namespace_name == NULL) != (namespaces[index] == NULL)) ||
        (use->namespace_name && strcmp(use->namespace_name, namespaces[index]) != 0)) {
        state->valid = 0;
        return 0;
    }
    state->seen++;
    return !state->stop_after_first || state->seen < 1u;
}

int main(void) {
    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    ASTNode* first = make_use("io", NULL, NULL);
    ASTFuncDecl* function = (ASTFuncDecl*)ast_create(AST_FUNC_DECL);
    ASTBlock* body = (ASTBlock*)ast_create(AST_BLOCK);
    ASTNode* nested = make_use("nested.must.not.appear", NULL, NULL);
    ASTNode* second = make_use("db.mysql.sql", "connector", "sql");
    VisitState state;
    size_t count = 0u;
    ASTUseVisitStatus status;

    if (!program || !first || !function || !body || !nested || !second) return 2;
    program->declarations = first;
    first->next = (ASTNode*)function;
    function->body = (ASTNode*)body;
    body->statements = nested;
    function->base.next = second;

    memset(&state, 0, sizeof(state));
    state.valid = 1;
    status = ast_visit_top_level_uses((ASTNode*)program, visit_use, &state, &count);
    if (status != AST_USE_VISIT_OK || !state.valid || state.seen != 2u || count != 2u) {
        fprintf(stderr, "top-level USE traversal failed\n");
        ast_destroy((ASTNode*)program);
        return 1;
    }

    memset(&state, 0, sizeof(state));
    state.valid = 1;
    state.stop_after_first = 1;
    status = ast_visit_top_level_uses((ASTNode*)program, visit_use, &state, &count);
    if (status != AST_USE_VISIT_STOPPED || !state.valid || state.seen != 1u || count != 1u) {
        fprintf(stderr, "USE visitor stop contract failed\n");
        ast_destroy((ASTNode*)program);
        return 1;
    }

    if (ast_visit_top_level_uses(NULL, NULL, NULL, &count) != AST_USE_VISIT_INVALID_ARGUMENT ||
        ast_visit_top_level_uses(first, NULL, NULL, &count) != AST_USE_VISIT_INVALID_ROOT) {
        fprintf(stderr, "USE visitor argument validation failed\n");
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ast_destroy((ASTNode*)program);
    puts("AST top-level USE visitor tests passed.");
    return 0;
}
