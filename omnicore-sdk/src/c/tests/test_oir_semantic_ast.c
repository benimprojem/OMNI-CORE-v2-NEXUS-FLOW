#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_semantic_ast.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_u32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

int main(void) {
    const OirSemanticScope scopes[] = {
        { 1u, 0u, 0u, 0u, 0u },
        { 2u, 1u, 1u, 0u, 10u },
        { 3u, 2u, 2u, 0u, 14u }
    };
    uint8_t bytes[OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        3u * OIR_SEMANTIC_SCOPE_RECORD_SIZE];
    uint8_t second[sizeof(bytes)];
    uint8_t damaged[sizeof(bytes)];
    size_t written = 0u;
    size_t second_written = 0u;
    OirSemanticScopesView view;
    OirSemanticScope decoded;

    memset(bytes, 0xa5, sizeof(bytes));
    assert(oir_semantic_scopes_write(bytes, sizeof(bytes), scopes, 3u,
        &written) == OIR_STATUS_OK);
    assert(written == sizeof(bytes));
    assert(oir_semantic_scopes_open(bytes, written, &view) == OIR_STATUS_OK);
    assert(view.record_count == 3u);

    assert(oir_semantic_scopes_get(&view, 0u, &decoded) == OIR_STATUS_OK);
    assert(decoded.scope_id == scopes[0].scope_id &&
        decoded.parent_scope_id == scopes[0].parent_scope_id &&
        decoded.depth == scopes[0].depth &&
        decoded.flags == scopes[0].flags &&
        decoded.owner_node_id == scopes[0].owner_node_id);
    assert(oir_semantic_scopes_get(&view, 1u, &decoded) == OIR_STATUS_OK);
    assert(decoded.scope_id == scopes[1].scope_id &&
        decoded.parent_scope_id == scopes[1].parent_scope_id &&
        decoded.depth == scopes[1].depth &&
        decoded.flags == scopes[1].flags &&
        decoded.owner_node_id == scopes[1].owner_node_id);
    assert(oir_semantic_scopes_get(&view, 2u, &decoded) == OIR_STATUS_OK);
    assert(decoded.scope_id == scopes[2].scope_id &&
        decoded.parent_scope_id == scopes[2].parent_scope_id &&
        decoded.depth == scopes[2].depth &&
        decoded.flags == scopes[2].flags &&
        decoded.owner_node_id == scopes[2].owner_node_id);
    assert(oir_semantic_scopes_get(&view, 3u, &decoded) ==
        OIR_STATUS_INVALID_ARGUMENT);

    assert(oir_semantic_scopes_write(second, sizeof(second), scopes, 3u,
        &second_written) == OIR_STATUS_OK);
    assert(second_written == written && memcmp(bytes, second, written) == 0);

    /* Truncated record array. */
    assert(oir_semantic_scopes_open(bytes, written - 1u, &view) ==
        OIR_STATUS_LENGTH_MISMATCH);

    /* Unsupported record width. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + 4, 24u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Count multiplication / payload bound must be rejected before record access. */
    memcpy(damaged, bytes, written);
    write_u32(damaged, UINT32_MAX);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Root must be unique and have no parent/owner. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE + 4u, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Duplicate/noncanonical IDs are rejected. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* A non-root scope must have an owning AST node. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 16u, 0u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Parent must precede child; this also rules out cycles. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 4u, 3u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Depth must be exactly parent depth + 1. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 8u, 2u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Reserved flags are required to be zero in schema 1.0. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE + 12u, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Writer failure must not touch output. */
    memset(second, 0xa5, sizeof(second));
    assert(oir_semantic_scopes_write(second, sizeof(second) - 1u, scopes, 3u,
        &second_written) == OIR_STATUS_BUFFER_TOO_SMALL);
    assert(second_written == 0u && second[0] == 0xa5u);


    {
        const uint8_t names[] = { 'i', '3', '2', 'W', 'i', 'd', 'g', 'e', 't' };
        const OirSemanticType types[] = {
            { 1u, OIR_SEMANTIC_TYPE_PRIMITIVE, 0u, 0u, 3u, 0u, 0u },
            { 2u, OIR_SEMANTIC_TYPE_USER_DEFINED, OIR_SEMANTIC_TYPE_FLAG_HARD,
                3u, 6u, 7u, 1u }
        };
        uint8_t type_bytes[OIR_SEMANTIC_TYPES_HEADER_SIZE +
            2u * OIR_SEMANTIC_TYPE_RECORD_SIZE + sizeof(names)];
        uint8_t type_second[sizeof(type_bytes)];
        uint8_t type_bad[sizeof(type_bytes)];
        size_t type_written = 0u;
        size_t type_second_written = 0u;
        OirSemanticTypesView types_view;
        OirSemanticType decoded_type;
        const uint8_t* decoded_name = NULL;

        assert(oir_semantic_types_write(type_bytes, sizeof(type_bytes), types, 2u,
            names, (uint32_t)sizeof(names), &type_written) == OIR_STATUS_OK);
        assert(type_written == sizeof(type_bytes));
        assert(oir_semantic_types_open(type_bytes, type_written, &types_view) ==
            OIR_STATUS_OK);
        assert(types_view.record_count == 2u);
        assert(oir_semantic_types_get(&types_view, 0u, &decoded_type,
            &decoded_name) == OIR_STATUS_OK);
        assert(decoded_type.type_id == 1u &&
            decoded_type.kind == OIR_SEMANTIC_TYPE_PRIMITIVE &&
            decoded_type.name_length == 3u &&
            decoded_name && memcmp(decoded_name, "i32", 3u) == 0);
        assert(oir_semantic_types_get(&types_view, 1u, &decoded_type,
            &decoded_name) == OIR_STATUS_OK);
        assert(decoded_type.type_id == 2u &&
            decoded_type.kind == OIR_SEMANTIC_TYPE_USER_DEFINED &&
            decoded_type.flags == OIR_SEMANTIC_TYPE_FLAG_HARD &&
            decoded_type.referenced_symbol_id == 7u &&
            decoded_type.base_type_id == 1u &&
            decoded_name && memcmp(decoded_name, "Widget", 6u) == 0);

        assert(oir_semantic_types_write(type_second, sizeof(type_second), types, 2u,
            names, (uint32_t)sizeof(names), &type_second_written) == OIR_STATUS_OK);
        assert(type_second_written == type_written &&
            memcmp(type_bytes, type_second, type_written) == 0);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + 4u, 28u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad, UINT32_MAX);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE, 1u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE + 8u, UINT32_MAX);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE +
            2u * OIR_SEMANTIC_TYPE_RECORD_SIZE] = 0xc0u;
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE + 4u] = 0xffu;
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE + 5u] = 0u;
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE + 16u, 0u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE + 20u, 2u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memset(type_second, 0xa5, sizeof(type_second));
        assert(oir_semantic_types_write(type_second, sizeof(type_second) - 1u,
            types, 2u, names, (uint32_t)sizeof(names), &type_second_written) ==
            OIR_STATUS_BUFFER_TOO_SMALL);
        assert(type_second_written == 0u && type_second[0] == 0xa5u);
    }


    {
        const uint8_t symbol_names[] = {
            'c', 'o', 'u', 'n', 't', 'e', 'r', 'r', 'u', 'n'
        };
        const OirSemanticSymbol symbols[] = {
            { 1u, 1u, 2u, OIR_SEMANTIC_SYMBOL_VARIABLE,
                OIR_SEMANTIC_SYMBOL_FLAG_CONST, 0u, 7u, 1u, 0u, 0u },
            { 2u, 1u, 5u, OIR_SEMANTIC_SYMBOL_FUNCTION,
                OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC, 7u, 3u, 1u, 1u, 2u }
        };
        uint8_t symbol_bytes[OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            2u * OIR_SEMANTIC_SYMBOL_RECORD_SIZE + sizeof(symbol_names)];
        uint8_t symbol_second[sizeof(symbol_bytes)];
        uint8_t symbol_bad[sizeof(symbol_bytes)];
        size_t symbol_written = 0u;
        size_t symbol_second_written = 0u;
        OirSemanticSymbolsView symbols_view;
        OirSemanticSymbol decoded_symbol;
        const uint8_t* decoded_symbol_name = NULL;
        size_t records = OIR_SEMANTIC_SYMBOLS_HEADER_SIZE;

        assert(oir_semantic_symbols_write(symbol_bytes, sizeof(symbol_bytes),
            symbols, 2u, symbol_names, (uint32_t)sizeof(symbol_names),
            &symbol_written) == OIR_STATUS_OK);
        assert(symbol_written == sizeof(symbol_bytes));
        assert(oir_semantic_symbols_open(symbol_bytes, symbol_written,
            &symbols_view) == OIR_STATUS_OK);
        assert(symbols_view.record_count == 2u);
        assert(oir_semantic_symbols_get(&symbols_view, 0u, &decoded_symbol,
            &decoded_symbol_name) == OIR_STATUS_OK);
        assert(decoded_symbol.symbol_id == 1u &&
            decoded_symbol.kind == OIR_SEMANTIC_SYMBOL_VARIABLE &&
            decoded_symbol.scope_id == 1u &&
            decoded_symbol.declaration_node_id == 2u &&
            decoded_symbol.flags == OIR_SEMANTIC_SYMBOL_FLAG_CONST &&
            decoded_symbol.type_id == 1u &&
            decoded_symbol_name && memcmp(decoded_symbol_name, "counter", 7u) == 0);
        assert(oir_semantic_symbols_get(&symbols_view, 1u, &decoded_symbol,
            &decoded_symbol_name) == OIR_STATUS_OK);
        assert(decoded_symbol.symbol_id == 2u &&
            decoded_symbol.kind == OIR_SEMANTIC_SYMBOL_FUNCTION &&
            decoded_symbol.first_relation == 1u &&
            decoded_symbol.relation_count == 2u &&
            decoded_symbol_name && memcmp(decoded_symbol_name, "run", 3u) == 0);

        assert(oir_semantic_symbols_write(symbol_second, sizeof(symbol_second),
            symbols, 2u, symbol_names, (uint32_t)sizeof(symbol_names),
            &symbol_second_written) == OIR_STATUS_OK);
        assert(symbol_second_written == symbol_written &&
            memcmp(symbol_bytes, symbol_second, symbol_written) == 0);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + 4u, 40u);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad, UINT32_MAX);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + OIR_SEMANTIC_SYMBOL_RECORD_SIZE, 1u);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + 4u, 0u);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + 8u, 0u);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        symbol_bad[records + 12u] = 0xffu;
        symbol_bad[records + 13u] = 0u;
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        symbol_bad[records + 14u] = 0x80u;
        symbol_bad[records + 15u] = 0u;
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + 16u, UINT32_MAX);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        symbol_bad[records + 2u * OIR_SEMANTIC_SYMBOL_RECORD_SIZE] = 0xc0u;
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + OIR_SEMANTIC_SYMBOL_RECORD_SIZE +
            28u, UINT32_MAX);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(symbol_bad, symbol_bytes, symbol_written);
        write_u32(symbol_bad + records + OIR_SEMANTIC_SYMBOL_RECORD_SIZE +
            32u, 0u);
        assert(oir_semantic_symbols_open(symbol_bad, symbol_written,
            &symbols_view) == OIR_STATUS_INVALID_SECTION);

        memset(symbol_second, 0xa5, sizeof(symbol_second));
        assert(oir_semantic_symbols_write(symbol_second,
            sizeof(symbol_second) - 1u, symbols, 2u, symbol_names,
            (uint32_t)sizeof(symbol_names), &symbol_second_written) ==
            OIR_STATUS_BUFFER_TOO_SMALL);
        assert(symbol_second_written == 0u && symbol_second[0] == 0xa5u);
    }


    {
        const OirSemanticAnnotation annotations[] = {
            { 2u, 1u, 1u, 1u, OIR_SEMANTIC_VALUE_LVALUE, 0u },
            { 5u, 1u, 0u, 1u, OIR_SEMANTIC_VALUE_RVALUE, 0u }
        };
        uint8_t annotation_bytes[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
            2u * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE];
        uint8_t annotation_second[sizeof(annotation_bytes)];
        uint8_t annotation_bad[sizeof(annotation_bytes)];
        uint8_t empty_table[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE];
        uint8_t empty_types[OIR_SEMANTIC_TYPES_HEADER_SIZE];
        uint8_t empty_symbols[OIR_SEMANTIC_SYMBOLS_HEADER_SIZE];
        size_t annotation_written = 0u;
        size_t annotation_second_written = 0u;
        size_t empty_written = 0u;
        OirSemanticAnnotationsView annotation_view;
        OirSemanticTypesView empty_types_view;
        OirSemanticSymbolsView empty_symbols_view;
        OirSemanticAnnotation decoded_annotation;

        assert(oir_semantic_annotations_write(annotation_bytes,
            sizeof(annotation_bytes), annotations, 2u,
            &annotation_written) == OIR_STATUS_OK);
        assert(annotation_written == sizeof(annotation_bytes));
        assert(oir_semantic_annotations_open(annotation_bytes,
            annotation_written, &annotation_view) == OIR_STATUS_OK);
        assert(annotation_view.record_count == 2u);
        assert(oir_semantic_annotations_get(&annotation_view, 0u,
            &decoded_annotation) == OIR_STATUS_OK);
        assert(decoded_annotation.node_id == 2u &&
            decoded_annotation.scope_id == 1u &&
            decoded_annotation.resolved_symbol_id == 1u &&
            decoded_annotation.resolved_type_id == 1u &&
            decoded_annotation.value_category == OIR_SEMANTIC_VALUE_LVALUE);
        assert(oir_semantic_annotations_get(&annotation_view, 1u,
            &decoded_annotation) == OIR_STATUS_OK);
        assert(decoded_annotation.node_id == 5u &&
            decoded_annotation.resolved_symbol_id == 0u &&
            decoded_annotation.value_category == OIR_SEMANTIC_VALUE_RVALUE);

        assert(oir_semantic_annotations_write(annotation_second,
            sizeof(annotation_second), annotations, 2u,
            &annotation_second_written) == OIR_STATUS_OK);
        assert(annotation_second_written == annotation_written &&
            memcmp(annotation_bytes, annotation_second, annotation_written) == 0);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        write_u32(annotation_bad + 4u, 24u);
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        write_u32(annotation_bad, UINT32_MAX);
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        write_u32(annotation_bad + OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
            OIR_SEMANTIC_ANNOTATION_RECORD_SIZE, 2u);
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        write_u32(annotation_bad + OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE + 4u, 0u);
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        annotation_bad[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE + 16u] = 0xffu;
        annotation_bad[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE + 17u] = 0u;
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(annotation_bad, annotation_bytes, annotation_written);
        annotation_bad[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE + 18u] = 1u;
        assert(oir_semantic_annotations_open(annotation_bad,
            annotation_written, &annotation_view) == OIR_STATUS_INVALID_SECTION);

        assert(oir_semantic_annotations_open(annotation_bytes,
            annotation_written - 1u, &annotation_view) ==
            OIR_STATUS_LENGTH_MISMATCH);

        memset(annotation_second, 0xa5, sizeof(annotation_second));
        assert(oir_semantic_annotations_write(annotation_second,
            sizeof(annotation_second) - 1u, annotations, 2u,
            &annotation_second_written) == OIR_STATUS_BUFFER_TOO_SMALL);
        assert(annotation_second_written == 0u && annotation_second[0] == 0xa5u);

        /* Empty symbol/type/annotation tables are valid and still have headers. */
        assert(oir_semantic_types_write(empty_types, sizeof(empty_types),
            NULL, 0u, NULL, 0u, &empty_written) == OIR_STATUS_OK);
        assert(empty_written == sizeof(empty_types));
        assert(oir_semantic_types_open(empty_types, empty_written,
            &empty_types_view) == OIR_STATUS_OK && empty_types_view.record_count == 0u);

        assert(oir_semantic_symbols_write(empty_symbols, sizeof(empty_symbols),
            NULL, 0u, NULL, 0u, &empty_written) == OIR_STATUS_OK);
        assert(empty_written == sizeof(empty_symbols));
        assert(oir_semantic_symbols_open(empty_symbols, empty_written,
            &empty_symbols_view) == OIR_STATUS_OK && empty_symbols_view.record_count == 0u);

        assert(oir_semantic_annotations_write(empty_table, sizeof(empty_table),
            NULL, 0u, &empty_written) == OIR_STATUS_OK);
        assert(empty_written == sizeof(empty_table));
        assert(oir_semantic_annotations_open(empty_table, empty_written,
            &annotation_view) == OIR_STATUS_OK &&
            annotation_view.record_count == 0u);
    }


    {
        const uint8_t relation_names[] = {
            'x', 'y', 'f', 'i', 'e', 'l', 'd'
        };
        const OirSemanticRelation relations[] = {
            { 1u, OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER, 0u,
                0u, 0u, 10u, 0u, 1u, 1u },
            { 1u, OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER, 0u,
                1u, 0u, 11u, 1u, 1u, 1u },
            { 2u, OIR_SEMANTIC_RELATION_AGGREGATE_FIELD, 0u,
                0u, 3u, 20u, 2u, 5u, 2u }
        };
        uint8_t relation_bytes[OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            3u * OIR_SEMANTIC_RELATION_RECORD_SIZE + sizeof(relation_names)];
        uint8_t relation_second[sizeof(relation_bytes)];
        uint8_t relation_bad[sizeof(relation_bytes)];
        uint8_t empty_relations[OIR_SEMANTIC_RELATIONS_HEADER_SIZE];
        size_t relation_written = 0u;
        size_t relation_second_written = 0u;
        size_t empty_relation_written = 0u;
        size_t records = OIR_SEMANTIC_RELATIONS_HEADER_SIZE;
        OirSemanticRelationsView relation_view;
        OirSemanticRelation decoded_relation;
        const uint8_t* decoded_relation_name = NULL;

        assert(oir_semantic_relations_write(relation_bytes,
            sizeof(relation_bytes), relations, 3u, relation_names,
            (uint32_t)sizeof(relation_names), &relation_written) == OIR_STATUS_OK);
        assert(relation_written == sizeof(relation_bytes));
        assert(oir_semantic_relations_open(relation_bytes, relation_written,
            &relation_view) == OIR_STATUS_OK);
        assert(relation_view.record_count == 3u);
        assert(oir_semantic_relations_get(&relation_view, 1u,
            &decoded_relation, &decoded_relation_name) == OIR_STATUS_OK);
        assert(decoded_relation.owner_symbol_id == 1u &&
            decoded_relation.kind == OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER &&
            decoded_relation.ordinal == 1u &&
            decoded_relation.related_node_id == 11u &&
            decoded_relation_name && memcmp(decoded_relation_name, "y", 1u) == 0);
        assert(oir_semantic_relations_get(&relation_view, 2u,
            &decoded_relation, &decoded_relation_name) == OIR_STATUS_OK);
        assert(decoded_relation.owner_symbol_id == 2u &&
            decoded_relation.kind == OIR_SEMANTIC_RELATION_AGGREGATE_FIELD &&
            decoded_relation.related_symbol_id == 3u &&
            decoded_relation.type_id == 2u &&
            decoded_relation_name && memcmp(decoded_relation_name, "field", 5u) == 0);

        assert(oir_semantic_relations_write(relation_second,
            sizeof(relation_second), relations, 3u, relation_names,
            (uint32_t)sizeof(relation_names), &relation_second_written) ==
            OIR_STATUS_OK);
        assert(relation_second_written == relation_written &&
            memcmp(relation_bytes, relation_second, relation_written) == 0);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + 4u, 40u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad, UINT32_MAX);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records + OIR_SEMANTIC_RELATION_RECORD_SIZE + 8u,
            0u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records + OIR_SEMANTIC_RELATION_RECORD_SIZE + 8u,
            2u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records +
            2u * OIR_SEMANTIC_RELATION_RECORD_SIZE, 1u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        relation_bad[records + 4u] = 0xffu;
        relation_bad[records + 5u] = 0u;
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        relation_bad[records + 6u] = 1u;
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records, 0u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records + 16u, 0u);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        write_u32(relation_bad + records + 20u, UINT32_MAX);
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        memcpy(relation_bad, relation_bytes, relation_written);
        relation_bad[records + 3u * OIR_SEMANTIC_RELATION_RECORD_SIZE] = 0xc0u;
        assert(oir_semantic_relations_open(relation_bad, relation_written,
            &relation_view) == OIR_STATUS_INVALID_SECTION);

        assert(oir_semantic_relations_open(relation_bytes,
            relation_written - 1u, &relation_view) ==
            OIR_STATUS_LENGTH_MISMATCH);

        memset(relation_second, 0xa5, sizeof(relation_second));
        assert(oir_semantic_relations_write(relation_second,
            sizeof(relation_second) - 1u, relations, 3u, relation_names,
            (uint32_t)sizeof(relation_names), &relation_second_written) ==
            OIR_STATUS_BUFFER_TOO_SMALL);
        assert(relation_second_written == 0u && relation_second[0] == 0xa5u);

        assert(oir_semantic_relations_write(empty_relations,
            sizeof(empty_relations), NULL, 0u, NULL, 0u,
            &empty_relation_written) == OIR_STATUS_OK);
        assert(empty_relation_written == sizeof(empty_relations));
        assert(oir_semantic_relations_open(empty_relations,
            empty_relation_written, &relation_view) == OIR_STATUS_OK &&
            relation_view.record_count == 0u);
    }

    puts("OIR SEMANTIC_AST scope/type/symbol/annotation/relation codec tests passed.");
    return 0;
}
