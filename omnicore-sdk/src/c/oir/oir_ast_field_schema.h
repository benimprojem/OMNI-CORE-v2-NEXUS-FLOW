#ifndef OMNICORE_OIR_AST_FIELD_SCHEMA_H
#define OMNICORE_OIR_AST_FIELD_SCHEMA_H

#include "oir_ast_node_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Validate required fields and wire types against OIR_AST_NODE_REGISTRY v1.
 * Unknown fields are accepted only when their TLV OPTIONAL flag is set.
 */
OirStatus oir_ast_field_schema_validate(const OirAstNodeTableView* view);

#ifdef __cplusplus
}
#endif

#endif
