#include "oir_semantic_ast_artifact.h"
#include "diagnostics.h"

#include <string.h>

static int semantic_ast_has_group_extensions(
    const OirSemanticAstArtifactView* view
) {
    uint32_t i;
    if (!view) return 1;
    for (i = 0u; i < view->symbols.record_count; ++i) {
        OirSemanticSymbol symbol;
        if (oir_semantic_symbols_get(&view->symbols, i, &symbol, NULL) !=
                OIR_STATUS_OK || symbol.kind > OIR_SEMANTIC_SYMBOL_LABEL) {
            return 1;
        }
    }
    for (i = 0u; i < view->relations.record_count; ++i) {
        OirSemanticRelation relation;
        if (oir_semantic_relations_get(&view->relations, i, &relation, NULL) !=
                OIR_STATUS_OK ||
            relation.kind > OIR_SEMANTIC_RELATION_ENUM_PAYLOAD_FIELD) {
            return 1;
        }
    }
    return 0;
}

OirStatus oir_semantic_ast_artifact_open(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    OirSemanticAstArtifactView* out_view,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* graph_stack,
    size_t graph_stack_capacity,
    uint32_t* owner_scope_by_node,
    size_t owner_scope_capacity
) {
    OirSemanticAstArtifactView result;
    OirSectionView section;
    uint32_t i;
    int payload_seen = 0;
    int scopes_seen = 0;
    int types_seen = 0;
    int symbols_seen = 0;
    int annotations_seen = 0;
    int relations_seen = 0;
    OirStatus status;

    if (!out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    memset(&result, 0, sizeof(result));

    if (!artifact_bytes || !incoming_counts || !visited || !graph_stack ||
        !owner_scope_by_node) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    status = oir_artifact_open(
        artifact_bytes, artifact_length, &result.envelope);
    if (status != OIR_STATUS_OK) return status;

    if (result.envelope.header.artifact_kind != OIR_KIND_SEMANTIC_AST ||
        result.envelope.header.payload_schema_major !=
            OIR_SEMANTIC_AST_SCHEMA_MAJOR ||
        result.envelope.header.payload_schema_minor >
            OIR_SEMANTIC_AST_SCHEMA_MINOR ||
        result.envelope.header.producer_stage_id != OCC_STAGE_SEMANTIC ||
        result.envelope.header.validity != OIR_VALIDITY_VALID) {
        return OIR_STATUS_INVALID_HEADER;
    }

    for (i = 0u; i < result.envelope.section_count; ++i) {
        status = oir_artifact_get_section(&result.envelope, i, &section);
        if (status != OIR_STATUS_OK) return status;

        switch (section.type) {
            case OIR_SECTION_PAYLOAD:
                if (payload_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                payload_seen = 1;
                status = oir_ast_node_table_open(
                    section.data, (size_t)section.length, &result.ast);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_SCOPES:
                if (scopes_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                scopes_seen = 1;
                status = oir_semantic_scopes_open(
                    section.data, (size_t)section.length, &result.scopes);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_TYPES:
                if (types_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                types_seen = 1;
                status = oir_semantic_types_open(
                    section.data, (size_t)section.length, &result.types);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_SYMBOLS:
                if (symbols_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                symbols_seen = 1;
                status = oir_semantic_symbols_open(
                    section.data, (size_t)section.length, &result.symbols);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_ANNOTATIONS:
                if (annotations_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                annotations_seen = 1;
                status = oir_semantic_annotations_open(
                    section.data, (size_t)section.length, &result.annotations);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_RELATIONS:
                if (relations_seen || section.version != 1u ||
                    (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                relations_seen = 1;
                status = oir_semantic_relations_open(
                    section.data, (size_t)section.length, &result.relations);
                if (status != OIR_STATUS_OK) return status;
                break;

            case OIR_SECTION_SEMANTIC_CONVERSIONS:
                if (result.has_conversions || section.version != 1u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                result.has_conversions = 1u;
                status = oir_semantic_conversions_open(
                    section.data, (size_t)section.length, &result.conversions);
                if (status != OIR_STATUS_OK) return status;
                break;

            default:
                /* This stage may skip optional extensions, never required data it does not consume. */
                if ((section.flags & OIR_SECTION_FLAG_REQUIRED) != 0u) {
                    return OIR_STATUS_UNKNOWN_REQUIRED_SECTION;
                }
                break;
        }
    }

    if (!payload_seen || !scopes_seen || !types_seen || !symbols_seen ||
        !annotations_seen || !relations_seen) {
        return OIR_STATUS_INVALID_SECTION;
    }

    /* Schemas before 1.3 do not define pointer/reference type tags. */
    if (result.envelope.header.payload_schema_minor < 3u) {
        uint32_t type_index;
        for (type_index = 0u; type_index < result.types.record_count;
             ++type_index) {
            OirSemanticType type_record;
            status = oir_semantic_types_get(
                &result.types, type_index, &type_record, NULL);
            if (status != OIR_STATUS_OK) return status;
            if (type_record.kind >= OIR_SEMANTIC_TYPE_POINTER)
                return OIR_STATUS_INVALID_SECTION;
        }
    }

    /* Schema 1.0 consumers did not define Group semantic enum values. */
    if (result.envelope.header.payload_schema_minor == 0u &&
        semantic_ast_has_group_extensions(&result)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    /* Schema 1.4 introduced the distinct immutable-value symbol flag. */
    if (result.envelope.header.payload_schema_minor < 4u) {
        uint32_t symbol_index;
        for (symbol_index = 0u; symbol_index < result.symbols.record_count;
             ++symbol_index) {
            OirSemanticSymbol symbol;
            status = oir_semantic_symbols_get(
                &result.symbols, symbol_index, &symbol, NULL);
            if (status != OIR_STATUS_OK) return status;
            if ((symbol.flags & OIR_SEMANTIC_SYMBOL_FLAG_IMMUTABLE) != 0u)
                return OIR_STATUS_INVALID_SECTION;
        }
    }

    if (incoming_capacity < result.ast.node_count ||
        visited_capacity < result.ast.node_count ||
        graph_stack_capacity < result.ast.node_count ||
        owner_scope_capacity < (size_t)result.ast.node_count + 1u) {
        return OIR_STATUS_BUFFER_TOO_SMALL;
    }

    status = oir_ast_field_schema_validate(&result.ast);
    if (status != OIR_STATUS_OK) return status;

    status = oir_ast_graph_validate(
        &result.ast,
        incoming_counts, incoming_capacity,
        visited, visited_capacity,
        graph_stack, graph_stack_capacity);
    if (status != OIR_STATUS_OK) return status;

    status = oir_semantic_ast_validate_references(
        &result.ast,
        &result.scopes,
        &result.types,
        &result.symbols,
        &result.annotations,
        &result.relations,
        result.has_conversions ? &result.conversions : NULL,
        owner_scope_by_node,
        owner_scope_capacity);
    if (status != OIR_STATUS_OK) return status;

    /* Schema 1.1 reserved all annotation flags as zero. */
    if (result.envelope.header.payload_schema_minor < 2u) {
        uint32_t annotation_index;
        for (annotation_index = 0u;
            annotation_index < result.annotations.record_count;
            ++annotation_index) {
            OirSemanticAnnotation annotation;
            status = oir_semantic_annotations_get(
                &result.annotations, annotation_index, &annotation);
            if (status != OIR_STATUS_OK) return status;
            if (annotation.flags != 0u) return OIR_STATUS_INVALID_SECTION;
        }
    }

    *out_view = result;
    return OIR_STATUS_OK;
}
