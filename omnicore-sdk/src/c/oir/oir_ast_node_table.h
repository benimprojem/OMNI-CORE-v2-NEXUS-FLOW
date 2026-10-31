#ifndef OMNICORE_OIR_AST_NODE_TABLE_H
#define OMNICORE_OIR_AST_NODE_TABLE_H

#include "oir_ast_tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_AST_PAYLOAD_HEADER_SIZE 32u
#define OIR_AST_NODE_RECORD_SIZE 32u
#define OIR_AST_NODE_COUNT_MAX 1000000u
#define OIR_AST_NODE_DEPTH_MAX 512u
#define OIR_AST_NODE_KIND_MAX 80u
#define OIR_AST_STRING_MAX 16777216u

typedef struct OirAstNodeRecord {
    uint32_t node_id;
    uint16_t node_kind_id;
    uint16_t flags;
    uint32_t start_line;
    uint32_t start_column;
    uint32_t next_node_id;
    uint32_t field_data_offset;
    uint32_t field_data_length;
} OirAstNodeRecord;

typedef struct OirAstNodeTableView {
    uint32_t node_count;
    uint32_t root_node_id;
    const uint8_t* node_records;
    const uint8_t* field_data;
    uint32_t field_data_length;
    const uint8_t* string_table;
    uint32_t string_table_length;
} OirAstNodeTableView;

/* Open and validate AST payload layout, records, TLVs and table references.
 * The payload remains caller-owned and must outlive the returned view.
 * Tree ownership/cycle/reachability validation is a separate graph pass.
 */
OirStatus oir_ast_node_table_open(
    const uint8_t* payload,
    size_t payload_length,
    OirAstNodeTableView* out_view
);

/* Decode one record from an already validated table view. */
OirStatus oir_ast_node_table_get(
    const OirAstNodeTableView* view,
    uint32_t index,
    OirAstNodeRecord* out_record
);

/* Encode a complete AST payload into caller-owned storage. */
OirStatus oir_ast_node_table_write(
    uint8_t* output,
    size_t capacity,
    const OirAstNodeRecord* records,
    uint32_t node_count,
    uint32_t root_node_id,
    const uint8_t* field_data,
    uint32_t field_data_length,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
