#ifndef OMNICORE_OIR_AST_FRONTEND_INDEX_H
#define OMNICORE_OIR_AST_FRONTEND_INDEX_H

#include "oir_ast_map.h"
#include "oir_ast_node_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OirAstPointerIndexEntry {
    const ASTNode* node;
    uint32_t node_id;
} OirAstPointerIndexEntry;

/* Capacity must be a power of two and at least 2 * OIR_AST_NODE_COUNT_MAX
 * for the maximum-size format. Smaller ASTs may use proportionally smaller
 * tables, but the load factor must never exceed 0.5.
 *
 * The index pass assigns stable 1-based IDs in canonical preorder:
 * visit a node, visit its child fields in ascending field ID, then its next
 * sibling at the same depth. Root must be AST_PROGRAM.
 *
 * The caller owns entries and must zero-initialize them before the call.
 * No allocation or AST mutation occurs. On failure, out_node_count is zero.
 */
OirStatus oir_ast_frontend_index_build(
    const ASTNode* root,
    OirAstPointerIndexEntry* entries,
    size_t entry_capacity,
    uint32_t* out_node_count
);

const ASTNode* oir_ast_frontend_child_at(const ASTNode* node, uint16_t field_id);
uint16_t oir_ast_frontend_child_field_count(ASTNodeType type);

uint32_t oir_ast_frontend_index_lookup(
    const OirAstPointerIndexEntry* entries,
    size_t entry_capacity,
    const ASTNode* node
);

#ifdef __cplusplus
}
#endif

#endif
