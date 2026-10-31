#ifndef OMNICORE_OIR_SEMANTIC_AST_ARTIFACT_H
#define OMNICORE_OIR_SEMANTIC_AST_ARTIFACT_H

#include "oir_semantic_ast.h"
#include "oir_ast_graph.h"
#include "oir_ast_field_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OirSemanticAstArtifactView {
    OirArtifactView envelope;
    OirAstNodeTableView ast;
    OirSemanticScopesView scopes;
    OirSemanticTypesView types;
    OirSemanticSymbolsView symbols;
    OirSemanticAnnotationsView annotations;
    OirSemanticRelationsView relations;
    OirSemanticConversionsView conversions;
    uint8_t has_conversions;
} OirSemanticAstArtifactView;

/*
 * Validate a complete SEMANTIC_AST OIR artifact:
 * - envelope kind/schema/validity and required section uniqueness/version;
 * - embedded AST payload, field schema and graph;
 * - all semantic section codecs and cross-table references.
 *
 * All scratch arrays are caller-owned. The returned views borrow artifact_bytes,
 * which must outlive out_view. On failure, out_view is zeroed.
 */
OirStatus oir_semantic_ast_artifact_open(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    OirSemanticAstArtifactView* out_view,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* graph_stack,
    size_t graph_stack_capacity,
    uint32_t* owner_scope_by_node,
    size_t owner_scope_capacity
);

#ifdef __cplusplus
}
#endif

#endif
