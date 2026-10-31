#ifndef OMNICORE_OIR_AST_FRONTEND_READER_H
#define OMNICORE_OIR_AST_FRONTEND_READER_H

#include "oir_ast_graph.h"
#include "oir_ast_field_schema.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Validate the entire payload and graph before allocating any frontend nodes.
 * node_storage and graph scratch arrays are caller-owned and must have at
 * least node_count entries. On success, ownership of the returned root and
 * all strings transfers to the caller; release it with ast_destroy().
 *
 * Nodes and strings are allocated while the graph is disconnected. Pointer
 * fields and sibling links are connected only after all allocations and
 * scalar/string decoding succeed. Failure cleanup never traverses pointers.
 */
OirStatus oir_ast_frontend_read_payload(
    const uint8_t* payload,
    size_t payload_length,
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
