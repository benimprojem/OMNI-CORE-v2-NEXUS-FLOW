#ifndef OMNICORE_OIR_AST_FRONTEND_WRITER_H
#define OMNICORE_OIR_AST_FRONTEND_WRITER_H

#include "oir_ast_frontend_index.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OirAstFrontendRequirements {
    uint32_t node_count;
    uint32_t field_data_length;
    uint32_t string_table_length;
    size_t payload_size;
} OirAstFrontendRequirements;

/* Measure the exact schema-1.0 payload requirements without writing bytes.
 * The pointer index must be the successful result of index_build() for root.
 */
OirStatus oir_ast_frontend_measure(
    const ASTNode* root,
    const OirAstPointerIndexEntry* index_entries,
    size_t index_capacity,
    uint32_t node_count,
    OirAstFrontendRequirements* out_requirements
);

/* Encode a complete AST payload using caller-owned scratch and output buffers.
 * First performs a complete validation/size pass. If any capacity is too small,
 * returns BUFFER_TOO_SMALL without modifying records, field/string buffers or
 * output. Input AST and index must remain unchanged for the duration.
 *
 * Scratch buffers and output must be disjoint. No allocation occurs.
 */
OirStatus oir_ast_frontend_write_payload(
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
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written,
    OirAstFrontendRequirements* out_requirements
);

#ifdef __cplusplus
}
#endif

#endif
