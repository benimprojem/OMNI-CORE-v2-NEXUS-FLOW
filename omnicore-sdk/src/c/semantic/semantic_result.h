#ifndef OCC_SEMANTIC_RESULT_H
#define OCC_SEMANTIC_RESULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEMANTIC_RESULT_MAX_RECORDS 1000000u

typedef enum SemanticResultStatus {
    SEMANTIC_RESULT_OK = 0,
    SEMANTIC_RESULT_INVALID_ARGUMENT,
    SEMANTIC_RESULT_INVALID_DATA,
    SEMANTIC_RESULT_LIMIT_EXCEEDED,
    SEMANTIC_RESULT_OUT_OF_MEMORY
} SemanticResultStatus;

typedef enum SemanticResultTypeKind {
    SEMANTIC_RESULT_TYPE_PRIMITIVE = 1,
    SEMANTIC_RESULT_TYPE_USER_DEFINED = 2,
    SEMANTIC_RESULT_TYPE_FUNCTION = 3,
    SEMANTIC_RESULT_TYPE_AGGREGATE = 4,
    SEMANTIC_RESULT_TYPE_TUPLE = 5,
    SEMANTIC_RESULT_TYPE_INFERRED = 6,
    SEMANTIC_RESULT_TYPE_UNKNOWN = 7,
    SEMANTIC_RESULT_TYPE_POINTER = 8,
    SEMANTIC_RESULT_TYPE_REFERENCE = 9
} SemanticResultTypeKind;

#define SEMANTIC_RESULT_TYPE_FLAG_CONST 0x0001u
#define SEMANTIC_RESULT_TYPE_FLAG_HARD  0x0002u

#define SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER 0x0001u

typedef enum SemanticResultSymbolKind {
    SEMANTIC_RESULT_SYMBOL_VARIABLE = 1,
    SEMANTIC_RESULT_SYMBOL_FUNCTION = 2,
    SEMANTIC_RESULT_SYMBOL_STRUCT = 3,
    SEMANTIC_RESULT_SYMBOL_UNION = 4,
    SEMANTIC_RESULT_SYMBOL_ENUM = 5,
    SEMANTIC_RESULT_SYMBOL_NEWTYPE = 6,
    SEMANTIC_RESULT_SYMBOL_LABEL = 7,
    SEMANTIC_RESULT_SYMBOL_GROUP = 8
} SemanticResultSymbolKind;

#define SEMANTIC_RESULT_SYMBOL_FLAG_CONST    0x0001u
#define SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC   0x0002u
#define SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED 0x0004u
#define SEMANTIC_RESULT_SYMBOL_FLAG_PACKED   0x0008u
#define SEMANTIC_RESULT_SYMBOL_FLAG_HARD     0x0010u
#define SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT 0x0020u
#define SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE 0x0040u

typedef enum SemanticResultRelationKind {
    SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER = 1,
    SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD = 2,
    SEMANTIC_RESULT_RELATION_ENUM_VARIANT = 3,
    SEMANTIC_RESULT_RELATION_ENUM_PAYLOAD_FIELD = 4,
    SEMANTIC_RESULT_RELATION_GROUP_MEMBER = 5,
    SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING = 6,
    SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION = 7,
    SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER = 8
} SemanticResultRelationKind;

typedef enum SemanticResultValueCategory {
    SEMANTIC_RESULT_VALUE_UNSPECIFIED = 0,
    SEMANTIC_RESULT_VALUE_LVALUE = 1,
    SEMANTIC_RESULT_VALUE_RVALUE = 2,
    SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR = 3
} SemanticResultValueCategory;

typedef enum SemanticResultConversionKind {
    SEMANTIC_RESULT_CONVERSION_IDENTITY = 1,
    SEMANTIC_RESULT_CONVERSION_WIDENING = 2,
    SEMANTIC_RESULT_CONVERSION_NARROWING = 3,
    SEMANTIC_RESULT_CONVERSION_SIGNEDNESS = 4,
    SEMANTIC_RESULT_CONVERSION_EXPLICIT_CAST = 5,
    SEMANTIC_RESULT_CONVERSION_HARD_TYPE = 6
} SemanticResultConversionKind;

#define SEMANTIC_RESULT_CONVERSION_FLAG_IMPLICIT 0x0001u
#define SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT 0x0002u

typedef struct SemanticResultScope {
    uint32_t scope_id;
    uint32_t parent_scope_id;
    uint32_t depth;
    uint32_t flags;
    uint32_t owner_node_id;
} SemanticResultScope;

typedef struct SemanticResultType {
    uint32_t type_id;
    uint16_t kind;
    uint16_t flags;
    char* name;
    uint32_t referenced_symbol_id;
    uint32_t base_type_id;
} SemanticResultType;

typedef struct SemanticResultSymbol {
    uint32_t symbol_id;
    uint32_t scope_id;
    uint32_t declaration_node_id;
    uint16_t kind;
    uint16_t flags;
    char* name;
    uint32_t type_id;
    uint32_t first_relation;
    uint32_t relation_count;
} SemanticResultSymbol;

typedef struct SemanticResultAnnotation {
    uint32_t node_id;
    uint32_t scope_id;
    uint32_t resolved_symbol_id;
    uint32_t resolved_type_id;
    uint16_t value_category;
    uint16_t flags;
} SemanticResultAnnotation;

typedef struct SemanticResultRelation {
    uint32_t owner_symbol_id;
    uint16_t kind;
    uint16_t flags;
    uint32_t ordinal;
    uint32_t related_symbol_id;
    uint32_t related_node_id;
    char* name;
    uint32_t type_id;
} SemanticResultRelation;

typedef struct SemanticResultConversion {
    uint32_t node_id;
    uint32_t operand_ordinal;
    uint32_t source_type_id;
    uint32_t destination_type_id;
    uint16_t kind;
    uint16_t flags;
} SemanticResultConversion;

/*
 * Invocation-owned semantic model. IDs are assigned deterministically by
 * insertion order; sorted tables are kept canonical. No AST pointers are stored.
 */
typedef struct SemanticResult {
    SemanticResultScope* scopes;
    uint32_t scope_count;
    uint32_t scope_capacity;

    SemanticResultType* types;
    uint32_t type_count;
    uint32_t type_capacity;

    SemanticResultSymbol* symbols;
    uint32_t symbol_count;
    uint32_t symbol_capacity;

    SemanticResultAnnotation* annotations;
    uint32_t annotation_count;
    uint32_t annotation_capacity;

    SemanticResultRelation* relations;
    uint32_t relation_count;
    uint32_t relation_capacity;

    SemanticResultConversion* conversions;
    uint32_t conversion_count;
    uint32_t conversion_capacity;

    uint8_t finalized;
    /* Per-instance deterministic allocator fault injection; test-only API. */
    uint32_t test_allocation_fail_at;
    uint32_t test_allocation_attempts;
} SemanticResult;

void semantic_result_test_fail_allocation_at(SemanticResult* result, uint32_t allocation_index);
uint32_t semantic_result_test_get_allocation_attempts(const SemanticResult* result);

SemanticResultStatus semantic_result_init(SemanticResult* result);
void semantic_result_destroy(SemanticResult* result);

SemanticResultStatus semantic_result_add_scope(
    SemanticResult* result,
    uint32_t parent_scope_id,
    uint32_t depth,
    uint32_t flags,
    uint32_t owner_node_id,
    uint32_t* out_scope_id
);

SemanticResultStatus semantic_result_intern_type(
    SemanticResult* result,
    uint16_t kind,
    uint16_t flags,
    const char* name,
    uint32_t referenced_symbol_id,
    uint32_t base_type_id,
    uint32_t* out_type_id
);

SemanticResultStatus semantic_result_add_symbol(
    SemanticResult* result,
    uint32_t scope_id,
    uint32_t declaration_node_id,
    uint16_t kind,
    uint16_t flags,
    const char* name,
    uint32_t type_id,
    uint32_t* out_symbol_id
);

SemanticResultStatus semantic_result_set_symbol_type(
    SemanticResult* result,
    uint32_t symbol_id,
    uint32_t type_id
);

uint32_t semantic_result_find_visible_symbol(
    const SemanticResult* result,
    uint32_t scope_id,
    const char* name
);

SemanticResultStatus semantic_result_set_annotation(
    SemanticResult* result,
    uint32_t node_id,
    uint32_t scope_id,
    uint32_t resolved_symbol_id,
    uint32_t resolved_type_id,
    uint16_t value_category,
    uint16_t flags
);

SemanticResultStatus semantic_result_add_relation(
    SemanticResult* result,
    uint32_t owner_symbol_id,
    uint16_t kind,
    uint32_t ordinal,
    uint32_t related_symbol_id,
    uint32_t related_node_id,
    const char* name,
    uint32_t type_id
);

SemanticResultStatus semantic_result_add_conversion(
    SemanticResult* result,
    uint32_t node_id,
    uint32_t operand_ordinal,
    uint32_t source_type_id,
    uint32_t destination_type_id,
    uint16_t kind,
    uint16_t flags
);

/* Finalize relation slices and validate all model references against AST IDs. */
SemanticResultStatus semantic_result_finalize(
    SemanticResult* result,
    uint32_t ast_node_count
);

const char* semantic_result_status_string(SemanticResultStatus status);

#ifdef __cplusplus
}
#endif

#endif
