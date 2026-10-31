#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_semantic_ast.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static void write_u32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

int main(void) {
    const OirSemanticScope scopes[] = {
        { 1u, 0u, 0u, 0u, 0u },
        { 2u, 1u, 1u, 0u, 2u },
        { 3u, 1u, 1u, 0u, 5u }
    };
    const uint8_t type_names[] = { 'i', '3', '2', 'M', 'o', 'd', 'e' };
    const OirSemanticType types[] = {
        { 1u, OIR_SEMANTIC_TYPE_PRIMITIVE, 0u, 0u, 3u, 0u, 0u },
        { 2u, OIR_SEMANTIC_TYPE_USER_DEFINED, 0u, 3u, 4u, 3u, 1u }
    };
    const uint8_t symbol_names[] = {
        'c','o','u','n','t','e','r','r','u','n','M','o','d','e','o','t','h','e','r'
    };
    const OirSemanticSymbol symbols[] = {
        { 1u, 2u, 3u, OIR_SEMANTIC_SYMBOL_VARIABLE,
            OIR_SEMANTIC_SYMBOL_FLAG_CONST, 0u, 7u, 1u, 0u, 0u },
        { 2u, 1u, 4u, OIR_SEMANTIC_SYMBOL_FUNCTION,
            OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC, 7u, 3u, 1u, 1u, 2u },
        { 3u, 1u, 7u, OIR_SEMANTIC_SYMBOL_ENUM,
            OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC, 10u, 4u, 2u, 3u, 2u },
        { 4u, 3u, 6u, OIR_SEMANTIC_SYMBOL_VARIABLE,
            0u, 14u, 5u, 1u, 0u, 0u }
    };
    const OirSemanticAnnotation annotations[] = {
        { 3u, 2u, 1u, 1u, OIR_SEMANTIC_VALUE_LVALUE, 0u },
        { 9u, 2u, 2u, 1u, OIR_SEMANTIC_VALUE_FUNCTION_DESIGNATOR, 0u }
    };
    const uint8_t relation_names[] = {
        'a', 'b', 'O', 'n', 'p', 'a', 'y', 'l', 'o', 'a', 'd'
    };
    const OirSemanticRelation relations[] = {
        { 2u, OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER, 0u,
            0u, 0u, 9u, 0u, 1u, 1u },
        { 2u, OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER, 0u,
            1u, 0u, 10u, 1u, 1u, 1u },
        { 3u, OIR_SEMANTIC_RELATION_ENUM_VARIANT, 0u,
            0u, 0u, 8u, 2u, 2u, 2u },
        { 3u, OIR_SEMANTIC_RELATION_ENUM_PAYLOAD_FIELD, 0u,
            0u, 0u, 10u, 4u, 7u, 1u }
    };
    const OirSemanticConversion conversions[] = {
        { 9u, 0u, 1u, 2u, OIR_SEMANTIC_CONVERSION_EXPLICIT_CAST,
            OIR_SEMANTIC_CONVERSION_FLAG_EXPLICIT }
    };
    OirAstNodeRecord ast_records[10];
    uint8_t ast_payload[OIR_AST_PAYLOAD_HEADER_SIZE +
        10u * OIR_AST_NODE_RECORD_SIZE];
    uint8_t scope_bytes[OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        3u * OIR_SEMANTIC_SCOPE_RECORD_SIZE];
    uint8_t type_bytes[OIR_SEMANTIC_TYPES_HEADER_SIZE +
        2u * OIR_SEMANTIC_TYPE_RECORD_SIZE + sizeof(type_names)];
    uint8_t symbol_bytes[OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        4u * OIR_SEMANTIC_SYMBOL_RECORD_SIZE + sizeof(symbol_names)];
    uint8_t annotation_bytes[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
        2u * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE];
    uint8_t relation_bytes[OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        4u * OIR_SEMANTIC_RELATION_RECORD_SIZE + sizeof(relation_names)];
    uint8_t conversion_bytes[OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
        OIR_SEMANTIC_CONVERSION_RECORD_SIZE];
    uint32_t owner_scope_by_node[11];
    size_t ast_written = 0u, scope_written = 0u, type_written = 0u;
    size_t symbol_written = 0u, annotation_written = 0u;
    size_t relation_written = 0u, conversion_written = 0u;
    OirAstNodeTableView ast;
    OirSemanticScopesView scopes_view;
    OirSemanticTypesView types_view;
    OirSemanticSymbolsView symbols_view;
    OirSemanticAnnotationsView annotations_view;
    OirSemanticRelationsView relations_view;
    OirSemanticConversionsView conversions_view;

    memset(ast_records, 0, sizeof(ast_records));
    {
        const uint16_t kinds[10] = {
            1u, 2u, 3u, 4u, 2u, 3u, 6u, 9u, 3u, 3u
        };
        uint32_t i;
        for (i = 0u; i < 10u; ++i) {
            ast_records[i].node_id = i + 1u;
            ast_records[i].node_kind_id = kinds[i];
        }
    }

    CHECK(oir_ast_node_table_write(ast_payload, sizeof(ast_payload),
        ast_records, 10u, 1u, NULL, 0u, NULL, 0u, &ast_written) ==
        OIR_STATUS_OK);
    CHECK(oir_ast_node_table_open(ast_payload, ast_written, &ast) ==
        OIR_STATUS_OK);

    CHECK(oir_semantic_scopes_write(scope_bytes, sizeof(scope_bytes),
        scopes, 3u, &scope_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_scopes_open(scope_bytes, scope_written, &scopes_view) ==
        OIR_STATUS_OK);

    CHECK(oir_semantic_types_write(type_bytes, sizeof(type_bytes), types, 2u,
        type_names, (uint32_t)sizeof(type_names), &type_written) ==
        OIR_STATUS_OK);
    CHECK(oir_semantic_types_open(type_bytes, type_written, &types_view) ==
        OIR_STATUS_OK);

    CHECK(oir_semantic_symbols_write(symbol_bytes, sizeof(symbol_bytes),
        symbols, 4u, symbol_names, (uint32_t)sizeof(symbol_names),
        &symbol_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_symbols_open(symbol_bytes, symbol_written, &symbols_view) ==
        OIR_STATUS_OK);

    CHECK(oir_semantic_annotations_write(annotation_bytes,
        sizeof(annotation_bytes), annotations, 2u, &annotation_written) ==
        OIR_STATUS_OK);
    CHECK(oir_semantic_annotations_open(annotation_bytes, annotation_written,
        &annotations_view) == OIR_STATUS_OK);

    CHECK(oir_semantic_relations_write(relation_bytes, sizeof(relation_bytes),
        relations, 4u, relation_names, (uint32_t)sizeof(relation_names),
        &relation_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_relations_open(relation_bytes, relation_written,
        &relations_view) == OIR_STATUS_OK);

    CHECK(oir_semantic_conversions_write(conversion_bytes,
        sizeof(conversion_bytes), conversions, 1u, &conversion_written) ==
        OIR_STATUS_OK);
    CHECK(oir_semantic_conversions_open(conversion_bytes, conversion_written,
        &conversions_view) == OIR_STATUS_OK);

    CHECK(oir_semantic_ast_validate_references(
        &ast, &scopes_view, &types_view, &symbols_view,
        &annotations_view, &relations_view, &conversions_view,
        owner_scope_by_node, 11u) == OIR_STATUS_OK);

    /* The optional conversion section may be absent. */
    CHECK(oir_semantic_ast_validate_references(
        &ast, &scopes_view, &types_view, &symbols_view,
        &annotations_view, &relations_view, NULL,
        owner_scope_by_node, 11u) == OIR_STATUS_OK);

    /* Explicit implicit-symbol flag permits the registered identifier declaration node. */
    {
        uint8_t implicit_ast_payload[sizeof(ast_payload)];
        uint8_t implicit_symbol_bytes[sizeof(symbol_bytes)];
        OirAstNodeTableView implicit_ast;
        OirSemanticSymbolsView implicit_symbols_view;
        memcpy(implicit_ast_payload, ast_payload, ast_written);
        write_u32(implicit_ast_payload + OIR_AST_PAYLOAD_HEADER_SIZE +
            2u * OIR_AST_NODE_RECORD_SIZE + 4u, 14u);
        CHECK(oir_ast_node_table_open(
            implicit_ast_payload, ast_written, &implicit_ast) == OIR_STATUS_OK);

        memcpy(implicit_symbol_bytes, symbol_bytes, symbol_written);
        write_u16(implicit_symbol_bytes + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE + 6u,
            OIR_SEMANTIC_SYMBOL_FLAG_IMPLICIT);
        CHECK(oir_semantic_symbols_open(implicit_symbol_bytes, symbol_written,
            &implicit_symbols_view) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &implicit_ast, &scopes_view, &types_view, &implicit_symbols_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_OK);
    }

    /* Duplicate scope owner nodes are rejected. */
    {
        uint8_t bad[sizeof(scope_bytes)];
        OirSemanticScopesView bad_view;
        memcpy(bad, scope_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
            2u * OIR_SEMANTIC_SCOPE_RECORD_SIZE + 16u, 2u);
        CHECK(oir_semantic_scopes_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &bad_view, &types_view, &symbols_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    /* A symbol cannot refer to a missing scope or a declaration of the wrong AST kind. */
    {
        uint8_t bad[sizeof(symbol_bytes)];
        OirSemanticSymbolsView bad_view;
        memcpy(bad, symbol_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE + 4u, 99u);
        CHECK(oir_semantic_symbols_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &bad_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);

        memcpy(bad, symbol_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE + 8u, 4u);
        CHECK(oir_semantic_symbols_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &bad_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    /* User-defined types must reference an existing aggregate declaration. */
    {
        uint8_t bad[sizeof(type_bytes)];
        OirSemanticTypesView bad_view;
        memcpy(bad, type_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE + 16u, 99u);
        CHECK(oir_semantic_types_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &bad_view, &symbols_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    /* A resolved symbol from a sibling scope is not visible here. */
    {
        uint8_t bad[sizeof(annotation_bytes)];
        OirSemanticAnnotationsView bad_view;
        memcpy(bad, annotation_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE + 8u, 4u);
        CHECK(oir_semantic_annotations_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &symbols_view,
            &bad_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    /* Relation slices and AST node kinds are cross-checked against their owner. */
    {
        uint8_t bad_symbols[sizeof(symbol_bytes)];
        uint8_t bad_relations[sizeof(relation_bytes)];
        OirSemanticSymbolsView bad_symbols_view;
        OirSemanticRelationsView bad_relations_view;

        memcpy(bad_symbols, symbol_bytes, sizeof(bad_symbols));
        write_u32(bad_symbols + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            OIR_SEMANTIC_SYMBOL_RECORD_SIZE + 32u, 1u);
        CHECK(oir_semantic_symbols_open(bad_symbols, sizeof(bad_symbols),
            &bad_symbols_view) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &bad_symbols_view,
            &annotations_view, &relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);

        memcpy(bad_relations, relation_bytes, sizeof(bad_relations));
        write_u32(bad_relations + OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            2u * OIR_SEMANTIC_RELATION_RECORD_SIZE + 16u, 4u);
        CHECK(oir_semantic_relations_open(bad_relations, sizeof(bad_relations),
            &bad_relations_view) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &symbols_view,
            &annotations_view, &bad_relations_view, &conversions_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    /* Conversion records must reference AST nodes and existing types. */
    {
        uint8_t bad[sizeof(conversion_bytes)];
        OirSemanticConversionsView bad_view;
        memcpy(bad, conversion_bytes, sizeof(bad));
        write_u32(bad + OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE + 12u, 99u);
        CHECK(oir_semantic_conversions_open(bad, sizeof(bad), &bad_view) ==
            OIR_STATUS_OK);
        CHECK(oir_semantic_ast_validate_references(
            &ast, &scopes_view, &types_view, &symbols_view,
            &annotations_view, &relations_view, &bad_view,
            owner_scope_by_node, 11u) == OIR_STATUS_INVALID_SECTION);
    }

    CHECK(oir_semantic_ast_validate_references(
        &ast, &scopes_view, &types_view, &symbols_view,
        &annotations_view, &relations_view, &conversions_view,
        owner_scope_by_node, 10u) == OIR_STATUS_INVALID_ARGUMENT);

    puts("OIR SEMANTIC_AST cross-table reference validation tests passed.");
    return 0;
}
