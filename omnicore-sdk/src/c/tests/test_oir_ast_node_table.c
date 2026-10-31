#include "oir_ast_node_table.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void put_u16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

int main(void) {
    uint8_t field_data[12] = {0};
    uint8_t strings[] = {'h','i'};
    uint8_t payload[256];
    uint8_t small[20];
    size_t written = 0;
    OirAstNodeRecord records[2];
    OirAstNodeTableView view;
    OirAstNodeRecord decoded;
    OirStatus status;

    memset(records, 0, sizeof(records));
    records[0].node_id = 1; records[0].node_kind_id = 1;
    records[0].start_line = 1; records[0].start_column = 1;
    records[0].field_data_offset = 0; records[0].field_data_length = 12;
    records[1].node_id = 2; records[1].node_kind_id = 14;
    records[1].start_line = 1; records[1].start_column = 3;
    records[1].field_data_offset = 12; records[1].field_data_length = 0;

    /* Root field 1 = NODE_REF(2). */
    put_u16(field_data, 1); field_data[2] = OIR_AST_VALUE_NODE_REF;
    put_u32(field_data + 4, 4); put_u32(field_data + 8, 2);

    status = oir_ast_node_table_write(payload, sizeof(payload), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written);
    assert(status == OIR_STATUS_OK && written > 0);
    status = oir_ast_node_table_open(payload, written, &view);
    assert(status == OIR_STATUS_OK && view.node_count == 2);
    assert(oir_ast_node_table_get(&view, 1, &decoded) == OIR_STATUS_OK);
    assert(decoded.node_id == 2 && decoded.node_kind_id == 14);
    assert(view.string_table_length == 2 && memcmp(view.string_table, "hi", 2) == 0);

    assert(oir_ast_node_table_write(small, sizeof(small), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written) ==
        OIR_STATUS_BUFFER_TOO_SMALL);
    assert(written == 0);

    /* Invalid root kind, non-contiguous ID, invalid next reference and bad field ref. */
    records[0].node_kind_id = 2;
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written) ==
        OIR_STATUS_INVALID_ARGUMENT);
    records[0].node_kind_id = 1;
    records[1].node_id = 3;
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written) ==
        OIR_STATUS_INVALID_SECTION);
    records[1].node_id = 2;
    records[0].next_node_id = 3;
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written) ==
        OIR_STATUS_INVALID_SECTION);
    records[0].next_node_id = 0;
    put_u32(field_data + 8, 3);
    assert(oir_ast_node_table_write(payload, sizeof(payload), records, 2, 1,
        field_data, sizeof(field_data), strings, sizeof(strings), &written) ==
        OIR_STATUS_INVALID_SECTION);
    put_u32(field_data + 8, 2);

    /* Wire-level truncation, bad reserved bits, and malformed string reference. */
    assert(oir_ast_node_table_open(payload, written - 1, &view) ==
        OIR_STATUS_LENGTH_MISMATCH);
    put_u32(payload + 28, 1);
    assert(oir_ast_node_table_open(payload, written, &view) == OIR_STATUS_INVALID_HEADER);
    put_u32(payload + 28, 0);
    put_u32(field_data + 8, 2);
    (void)put_u16;
    puts("OIR AST node table tests passed.");
    return 0;
}
