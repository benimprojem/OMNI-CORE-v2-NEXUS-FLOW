#include "oir_ast_graph.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_NODES 513u
#define MAX_FIELDS (MAX_NODES * 12u)

static void put_u16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void make_child_ref(uint8_t* p, uint32_t id) {
    put_u16(p, 1); p[2] = OIR_AST_VALUE_NODE_REF; p[3] = 0;
    put_u32(p + 4, 4); put_u32(p + 8, id);
}

int main(void) {
    OirAstNodeRecord records[MAX_NODES];
    uint8_t fields[MAX_FIELDS];
    uint8_t payload[OIR_AST_PAYLOAD_HEADER_SIZE +
        MAX_NODES * OIR_AST_NODE_RECORD_SIZE + MAX_FIELDS];
    uint32_t incoming[MAX_NODES];
    uint8_t visited[MAX_NODES];
    OirAstGraphStackEntry stack[MAX_NODES];
    OirAstNodeTableView view;
    size_t written = 0;
    uint32_t i;

    memset(records, 0, sizeof(records));
    memset(fields, 0, sizeof(fields));
    records[0].node_id=1; records[0].node_kind_id=1;
    records[0].field_data_offset=0; records[0].field_data_length=12;
    records[1].node_id=2; records[1].node_kind_id=14;
    records[1].field_data_offset=12; records[1].field_data_length=0;
    records[1].next_node_id=3;
    records[2].node_id=3; records[2].node_kind_id=14;
    records[2].field_data_offset=12; records[2].field_data_length=0;
    make_child_ref(fields, 2);

    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 3, 1,
        fields, 12, NULL, 0, &written) == OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload, written, &view) == OIR_STATUS_OK);
    assert(oir_ast_graph_validate(&view, incoming, MAX_NODES,
        visited, MAX_NODES, stack, MAX_NODES) == OIR_STATUS_OK);

    /* A sibling cycle gives the target a second owner and must fail. */
    records[1].next_node_id=2;
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 3, 1,
        fields, 12, NULL, 0, &written) == OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload, written, &view) == OIR_STATUS_OK);
    assert(oir_ast_graph_validate(&view, incoming, MAX_NODES,
        visited, MAX_NODES, stack, MAX_NODES) == OIR_STATUS_INVALID_SECTION);

    /* An unreachable self-cycle has one incoming edge but is still an orphan. */
    records[1].next_node_id=0;
    records[2].next_node_id=3;
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 3, 1,
        fields, 12, NULL, 0, &written) == OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload, written, &view) == OIR_STATUS_OK);
    assert(oir_ast_graph_validate(&view, incoming, MAX_NODES,
        visited, MAX_NODES, stack, MAX_NODES) == OIR_STATUS_INVALID_SECTION);

    /* Build a child chain whose depth exceeds the schema limit (512). */
    memset(records, 0, sizeof(records));
    memset(fields, 0, sizeof(fields));
    for (i = 0; i < MAX_NODES; ++i) {
        records[i].node_id = i + 1u;
        records[i].node_kind_id = i == 0u ? 1u : 14u;
        records[i].field_data_offset = i * 12u;
        records[i].field_data_length = i + 1u < MAX_NODES ? 12u : 0u;
        if (i + 1u < MAX_NODES) make_child_ref(fields + i * 12u, i + 2u);
    }
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, MAX_NODES, 1,
        fields, (MAX_NODES - 1u) * 12u, NULL, 0, &written) == OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload, written, &view) == OIR_STATUS_OK);
    assert(oir_ast_graph_validate(&view, incoming, MAX_NODES,
        visited, MAX_NODES, stack, MAX_NODES) == OIR_STATUS_INVALID_SECTION);

    puts("OIR AST graph tests passed.");
    return 0;
}
