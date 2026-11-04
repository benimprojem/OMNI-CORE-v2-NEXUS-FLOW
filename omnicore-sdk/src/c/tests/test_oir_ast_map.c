#include "oir_ast_map.h"

#include <stdio.h>

int main(void) {
    uint16_t id;
    unsigned int seen[OIR_AST_NODE_KIND_COUNT + 1u] = { 0u };

    for (id = 1u; id <= OIR_AST_NODE_KIND_COUNT; ++id) {
        ASTNodeType type;
        uint16_t roundtrip;
        if (!oir_ast_node_id_to_frontend(id, &type)) {
            fprintf(stderr, "FAIL: OIR AST ID %u has no frontend mapping\n", id);
            return 1;
        }
        roundtrip = oir_ast_node_id_from_frontend(type);
        if (roundtrip != id || ++seen[roundtrip] != 1u) {
            fprintf(stderr, "FAIL: OIR AST ID %u is not bijective\n", id);
            return 1;
        }
    }

    for (id = 1u; id <= OIR_AST_NODE_KIND_COUNT; ++id) {
        if (seen[id] != 1u) {
            fprintf(stderr, "FAIL: OIR AST ID %u is missing or duplicated\n", id);
            return 1;
        }
    }

    {
        ASTNodeType type;
        if (oir_ast_node_id_to_frontend(0u, &type) ||
            oir_ast_node_id_to_frontend(106u, &type) ||
            oir_ast_node_id_from_frontend((ASTNodeType)-1) != 0u) {
            fprintf(stderr, "FAIL: invalid AST node IDs/tags must be rejected\n");
            return 1;
        }
    }

    puts("OIR AST node registry mapping tests passed (105/105).");
    return 0;
}
