#ifndef OMNICORE_OIR_SEMANTIC_AST_H
#define OMNICORE_OIR_SEMANTIC_AST_H

#include "oir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_SEMANTIC_AST_SCHEMA_MAJOR 1u
#define OIR_SEMANTIC_AST_SCHEMA_MINOR 0u

/* SCOPES section payload: u32 record_count, u32 record_size, then 20-byte records. */
#define OIR_SEMANTIC_SCOPES_HEADER_SIZE 8u
#define OIR_SEMANTIC_SCOPE_RECORD_SIZE 20u
#define OIR_SEMANTIC_MAX_SCOPE_COUNT 1000000u
#define OIR_SEMANTIC_MAX_SCOPE_DEPTH 512u

/* Logical API view only. This structure is never serialized with sizeof/memcpy. */
typedef struct OirSemanticScope {
    uint32_t scope_id;
    uint32_t parent_scope_id;
    uint32_t depth;
    uint32_t flags;
    uint32_t owner_node_id;
} OirSemanticScope;

/* Zero-copy view over a validated SCOPES section payload. */
typedef struct OirSemanticScopesView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
} OirSemanticScopesView;

/* Validate a SCOPES section payload without allocation or mutation. */
OirStatus oir_semantic_scopes_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticScopesView* out_view
);

/* Decode one validated scope record field-by-field. */
OirStatus oir_semantic_scopes_get(
    const OirSemanticScopesView* view,
    uint32_t index,
    OirSemanticScope* out_scope
);

/*
 * Encode logical scope records into caller-owned storage, field-by-field.
 * The input scope array and output byte buffer must be disjoint.
 */
OirStatus oir_semantic_scopes_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticScope* scopes,
    uint32_t scope_count,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
