#ifndef OMNICORE_OIR_AST_MAP_H
#define OMNICORE_OIR_AST_MAP_H

#include <stdint.h>
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_AST_NODE_KIND_COUNT 100u

/* Stable OIR IDs are independent of ASTNodeType enum ordinals. */
uint16_t oir_ast_node_id_from_frontend(ASTNodeType type);
int oir_ast_node_id_to_frontend(uint16_t node_id, ASTNodeType* out_type);

#ifdef __cplusplus
}
#endif

#endif
