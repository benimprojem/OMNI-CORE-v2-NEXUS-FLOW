#ifndef OMNICORE_OIR_AST_ARTIFACT_H
#define OMNICORE_OIR_AST_ARTIFACT_H

#include "oir_ast_frontend_writer.h"
#include "oir_ast_frontend_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wrap a schema-1.0 AST payload in the canonical OIR container.
 * payload_scratch and artifact_output must be disjoint; all scratch is caller-owned.
 */
OirStatus oir_ast_artifact_write_frontend_ast(
    const ASTNode* root,
    const OirAstPointerIndexEntry* index_entries,
    size_t index_capacity,
    uint32_t node_count,
    OirAstNodeRecord* records,
    size_t records_capacity,
    uint8_t* field_data,
    size_t field_capacity,
    uint8_t* string_table,
    size_t string_capacity,
    uint8_t* payload_scratch,
    size_t payload_capacity,
    uint8_t* artifact_output,
    size_t artifact_capacity,
    uint64_t job_id,
    size_t* out_written,
    OirAstFrontendRequirements* out_requirements
);

/* Validate the OIR envelope and AST schema/graph, then materialize frontend AST. */
OirStatus oir_ast_artifact_read_frontend_ast(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    ASTNode** node_storage,
    size_t node_capacity,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* graph_stack,
    size_t graph_stack_capacity,
    ASTNode** out_root
);

#ifdef __cplusplus
}
#endif

#endif
