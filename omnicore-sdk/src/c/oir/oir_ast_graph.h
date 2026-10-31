#ifndef OMNICORE_OIR_AST_GRAPH_H
#define OMNICORE_OIR_AST_GRAPH_H

#include "oir_ast_node_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OirAstGraphStackEntry {
    uint32_t node_id;
    uint32_t depth;
} OirAstGraphStackEntry;

/* Validate single-owner tree semantics using caller-provided scratch arrays.
 * Each array must contain at least view->node_count elements. No allocation
 * occurs. This expects a successful oir_ast_node_table_open() result.
 */
OirStatus oir_ast_graph_validate(
    const OirAstNodeTableView* view,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* stack,
    size_t stack_capacity
);

#ifdef __cplusplus
}
#endif

#endif
