#ifndef OMNICORE_OIR_SEMANTIC_AST_H
#define OMNICORE_OIR_SEMANTIC_AST_H

#include "oir.h"
#include "oir_ast_node_table.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_SEMANTIC_AST_SCHEMA_MAJOR 1u
#define OIR_SEMANTIC_AST_SCHEMA_MINOR 0u

/* SCOPES section payload: u32 record_count, u32 record_size, then 20-byte records. */
#define OIR_SEMANTIC_SCOPES_HEADER_SIZE 8u
#define OIR_SEMANTIC_SCOPE_RECORD_SIZE 20u
#define OIR_SEMANTIC_MAX_SCOPE_COUNT 1000000u
#define OIR_SEMANTIC_MAX_TYPE_COUNT 1000000u
#define OIR_SEMANTIC_MAX_SYMBOL_COUNT 1000000u
#define OIR_SEMANTIC_MAX_ANNOTATION_COUNT 1000000u
#define OIR_SEMANTIC_MAX_SCOPE_DEPTH 512u

/* TYPES section payload: count, record size, string-table byte length, records, UTF-8 strings. */
#define OIR_SEMANTIC_TYPES_HEADER_SIZE 12u
#define OIR_SEMANTIC_TYPE_RECORD_SIZE 24u
#define OIR_SEMANTIC_SYMBOLS_HEADER_SIZE 12u
#define OIR_SEMANTIC_SYMBOL_RECORD_SIZE 36u
#define OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE 8u
#define OIR_SEMANTIC_ANNOTATION_RECORD_SIZE 20u
#define OIR_SEMANTIC_RELATIONS_HEADER_SIZE 12u
#define OIR_SEMANTIC_RELATION_RECORD_SIZE 32u
#define OIR_SEMANTIC_MAX_RELATION_COUNT 1000000u
#define OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE 8u
#define OIR_SEMANTIC_CONVERSION_RECORD_SIZE 20u
#define OIR_SEMANTIC_MAX_CONVERSION_COUNT 1000000u
#define OIR_SEMANTIC_MAX_STRING_TABLE_SIZE (16u * 1024u * 1024u)

typedef enum OirSemanticTypeKind {
    OIR_SEMANTIC_TYPE_PRIMITIVE = 1,
    OIR_SEMANTIC_TYPE_USER_DEFINED = 2,
    OIR_SEMANTIC_TYPE_FUNCTION = 3,
    OIR_SEMANTIC_TYPE_AGGREGATE = 4,
    OIR_SEMANTIC_TYPE_TUPLE = 5,
    OIR_SEMANTIC_TYPE_INFERRED = 6,
    OIR_SEMANTIC_TYPE_UNKNOWN = 7
} OirSemanticTypeKind;

#define OIR_SEMANTIC_TYPE_FLAG_CONST 0x0001u
#define OIR_SEMANTIC_TYPE_FLAG_HARD  0x0002u

typedef enum OirSemanticSymbolKind {
    OIR_SEMANTIC_SYMBOL_VARIABLE = 1,
    OIR_SEMANTIC_SYMBOL_FUNCTION = 2,
    OIR_SEMANTIC_SYMBOL_STRUCT = 3,
    OIR_SEMANTIC_SYMBOL_UNION = 4,
    OIR_SEMANTIC_SYMBOL_ENUM = 5,
    OIR_SEMANTIC_SYMBOL_NEWTYPE = 6,
    OIR_SEMANTIC_SYMBOL_LABEL = 7
} OirSemanticSymbolKind;

#define OIR_SEMANTIC_SYMBOL_FLAG_CONST    0x0001u
#define OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC   0x0002u
#define OIR_SEMANTIC_SYMBOL_FLAG_EXPORTED 0x0004u
#define OIR_SEMANTIC_SYMBOL_FLAG_PACKED   0x0008u
#define OIR_SEMANTIC_SYMBOL_FLAG_HARD     0x0010u
#define OIR_SEMANTIC_SYMBOL_FLAG_IMPLICIT 0x0020u

typedef enum OirSemanticConversionKind {
    OIR_SEMANTIC_CONVERSION_IDENTITY = 1,
    OIR_SEMANTIC_CONVERSION_WIDENING = 2,
    OIR_SEMANTIC_CONVERSION_NARROWING = 3,
    OIR_SEMANTIC_CONVERSION_SIGNEDNESS = 4,
    OIR_SEMANTIC_CONVERSION_EXPLICIT_CAST = 5,
    OIR_SEMANTIC_CONVERSION_HARD_TYPE = 6
} OirSemanticConversionKind;

#define OIR_SEMANTIC_CONVERSION_FLAG_IMPLICIT 0x0001u
#define OIR_SEMANTIC_CONVERSION_FLAG_EXPLICIT 0x0002u

typedef enum OirSemanticRelationKind {
    OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER = 1,
    OIR_SEMANTIC_RELATION_AGGREGATE_FIELD = 2,
    OIR_SEMANTIC_RELATION_ENUM_VARIANT = 3,
    OIR_SEMANTIC_RELATION_ENUM_PAYLOAD_FIELD = 4
} OirSemanticRelationKind;

typedef enum OirSemanticValueCategory {
    OIR_SEMANTIC_VALUE_UNSPECIFIED = 0,
    OIR_SEMANTIC_VALUE_LVALUE = 1,
    OIR_SEMANTIC_VALUE_RVALUE = 2,
    OIR_SEMANTIC_VALUE_FUNCTION_DESIGNATOR = 3
} OirSemanticValueCategory;

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

typedef struct OirSemanticType {
    uint32_t type_id;
    uint16_t kind;
    uint16_t flags;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t referenced_symbol_id;
    uint32_t base_type_id;
} OirSemanticType;

typedef struct OirSemanticTypesView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
    size_t strings_offset;
    uint32_t string_table_length;
} OirSemanticTypesView;

OirStatus oir_semantic_types_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticTypesView* out_view
);

OirStatus oir_semantic_types_get(
    const OirSemanticTypesView* view,
    uint32_t index,
    OirSemanticType* out_type,
    const uint8_t** out_name
);

/* Input records/string bytes and output buffer must be disjoint. */
OirStatus oir_semantic_types_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticType* types,
    uint32_t type_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
);

typedef struct OirSemanticSymbol {
    uint32_t symbol_id;
    uint32_t scope_id;
    uint32_t declaration_node_id;
    uint16_t kind;
    uint16_t flags;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t type_id;
    uint32_t first_relation;
    uint32_t relation_count;
} OirSemanticSymbol;

typedef struct OirSemanticSymbolsView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
    size_t strings_offset;
    uint32_t string_table_length;
} OirSemanticSymbolsView;

OirStatus oir_semantic_symbols_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticSymbolsView* out_view
);

OirStatus oir_semantic_symbols_get(
    const OirSemanticSymbolsView* view,
    uint32_t index,
    OirSemanticSymbol* out_symbol,
    const uint8_t** out_name
);

/* Input records/string bytes and output buffer must be disjoint. */
OirStatus oir_semantic_symbols_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticSymbol* symbols,
    uint32_t symbol_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
);

typedef struct OirSemanticAnnotation {
    uint32_t node_id;
    uint32_t scope_id;
    uint32_t resolved_symbol_id;
    uint32_t resolved_type_id;
    uint16_t value_category;
    uint16_t flags;
} OirSemanticAnnotation;

typedef struct OirSemanticAnnotationsView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
} OirSemanticAnnotationsView;

OirStatus oir_semantic_annotations_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticAnnotationsView* out_view
);

OirStatus oir_semantic_annotations_get(
    const OirSemanticAnnotationsView* view,
    uint32_t index,
    OirSemanticAnnotation* out_annotation
);

OirStatus oir_semantic_annotations_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticAnnotation* annotations,
    uint32_t annotation_count,
    size_t* out_written
);

typedef struct OirSemanticRelation {
    uint32_t owner_symbol_id;
    uint16_t kind;
    uint16_t flags;
    uint32_t ordinal;
    uint32_t related_symbol_id;
    uint32_t related_node_id;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t type_id;
} OirSemanticRelation;

typedef struct OirSemanticRelationsView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
    size_t strings_offset;
    uint32_t string_table_length;
} OirSemanticRelationsView;

OirStatus oir_semantic_relations_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticRelationsView* out_view
);

OirStatus oir_semantic_relations_get(
    const OirSemanticRelationsView* view,
    uint32_t index,
    OirSemanticRelation* out_relation,
    const uint8_t** out_name
);

/* Input records/string bytes and output buffer must be disjoint. */
OirStatus oir_semantic_relations_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticRelation* relations,
    uint32_t relation_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
);

typedef struct OirSemanticConversion {
    uint32_t node_id;
    uint32_t operand_ordinal;
    uint32_t source_type_id;
    uint32_t destination_type_id;
    uint16_t kind;
    uint16_t flags;
} OirSemanticConversion;

typedef struct OirSemanticConversionsView {
    const uint8_t* bytes;
    size_t length;
    uint32_t record_count;
    size_t records_offset;
} OirSemanticConversionsView;

OirStatus oir_semantic_conversions_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticConversionsView* out_view
);

OirStatus oir_semantic_conversions_get(
    const OirSemanticConversionsView* view,
    uint32_t index,
    OirSemanticConversion* out_conversion
);

OirStatus oir_semantic_conversions_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticConversion* conversions,
    uint32_t conversion_count,
    size_t* out_written
);

/*
 * Validate references across already-opened semantic tables and a validated AST
 * node-table view. owner_scope_by_node is caller-owned scratch with at least
 * ast->node_count + 1 entries; it is cleared and used by this call.
 * conversions may be NULL when the optional CONVERSIONS section is absent.
 */
OirStatus oir_semantic_ast_validate_references(
    const OirAstNodeTableView* ast,
    const OirSemanticScopesView* scopes,
    const OirSemanticTypesView* types,
    const OirSemanticSymbolsView* symbols,
    const OirSemanticAnnotationsView* annotations,
    const OirSemanticRelationsView* relations,
    const OirSemanticConversionsView* conversions,
    uint32_t* owner_scope_by_node,
    size_t owner_scope_capacity
);



#ifdef __cplusplus
}
#endif

#endif
