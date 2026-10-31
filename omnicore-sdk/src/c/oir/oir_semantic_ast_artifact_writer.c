#include "oir_semantic_ast_artifact.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <string.h>

static void* allocate_array(uint32_t count, size_t element_size) {
    if (count == 0u) return NULL;
    if (element_size == 0u || (size_t)count > SIZE_MAX / element_size) return NULL;
    return malloc((size_t)count * element_size);
}

static int add_size(size_t* total, size_t amount) {
    if (amount > SIZE_MAX - *total) return 0;
    *total += amount;
    return 1;
}

static OirStatus count_string_bytes(const char* name, uint32_t* total) {
    size_t length;
    if (!name || !*name) return OIR_STATUS_OK;
    length = strlen(name);
    if (length > UINT32_MAX ||
        length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE - *total) {
        return OIR_STATUS_OVERFLOW;
    }
    *total += (uint32_t)length;
    return OIR_STATUS_OK;
}

static OirStatus append_string(
    const char* name,
    uint8_t* table,
    uint32_t capacity,
    uint32_t* used,
    uint32_t* out_offset,
    uint32_t* out_length
) {
    size_t length;

    *out_offset = 0u;
    *out_length = 0u;
    if (!name || !*name) return OIR_STATUS_OK;

    length = strlen(name);
    if (length > UINT32_MAX || *used > capacity ||
        length > (size_t)(capacity - *used)) {
        return OIR_STATUS_OVERFLOW;
    }

    *out_offset = *used;
    *out_length = (uint32_t)length;
    memcpy(table + *used, name, length);
    *used += (uint32_t)length;
    return OIR_STATUS_OK;
}

static OirStatus build_type_strings(
    const SemanticResult* result,
    OirSemanticType* records,
    uint8_t** out_table,
    uint32_t* out_length
) {
    uint32_t i, length = 0u, used = 0u;
    OirStatus status;

    for (i = 0u; i < result->type_count; ++i) {
        status = count_string_bytes(result->types[i].name, &length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_table = length ? (uint8_t*)malloc(length) : NULL;
    if (length && !*out_table) return OIR_STATUS_OUT_OF_MEMORY;

    for (i = 0u; i < result->type_count; ++i) {
        const SemanticResultType* source = &result->types[i];
        records[i].type_id = source->type_id;
        records[i].kind = source->kind;
        records[i].flags = source->flags;
        records[i].referenced_symbol_id = source->referenced_symbol_id;
        records[i].base_type_id = source->base_type_id;
        status = append_string(source->name, *out_table, length, &used,
            &records[i].name_offset, &records[i].name_length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_length = length;
    return used == length ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}

static OirStatus build_symbol_strings(
    const SemanticResult* result,
    OirSemanticSymbol* records,
    uint8_t** out_table,
    uint32_t* out_length
) {
    uint32_t i, length = 0u, used = 0u;
    OirStatus status;

    for (i = 0u; i < result->symbol_count; ++i) {
        status = count_string_bytes(result->symbols[i].name, &length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_table = length ? (uint8_t*)malloc(length) : NULL;
    if (length && !*out_table) return OIR_STATUS_OUT_OF_MEMORY;

    for (i = 0u; i < result->symbol_count; ++i) {
        const SemanticResultSymbol* source = &result->symbols[i];
        records[i].symbol_id = source->symbol_id;
        records[i].scope_id = source->scope_id;
        records[i].declaration_node_id = source->declaration_node_id;
        records[i].kind = source->kind;
        records[i].flags = source->flags;
        records[i].type_id = source->type_id;
        records[i].first_relation = source->first_relation;
        records[i].relation_count = source->relation_count;
        status = append_string(source->name, *out_table, length, &used,
            &records[i].name_offset, &records[i].name_length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_length = length;
    return used == length ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}

static OirStatus build_relation_strings(
    const SemanticResult* result,
    OirSemanticRelation* records,
    uint8_t** out_table,
    uint32_t* out_length
) {
    uint32_t i, length = 0u, used = 0u;
    OirStatus status;

    for (i = 0u; i < result->relation_count; ++i) {
        status = count_string_bytes(result->relations[i].name, &length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_table = length ? (uint8_t*)malloc(length) : NULL;
    if (length && !*out_table) return OIR_STATUS_OUT_OF_MEMORY;

    for (i = 0u; i < result->relation_count; ++i) {
        const SemanticResultRelation* source = &result->relations[i];
        records[i].owner_symbol_id = source->owner_symbol_id;
        records[i].kind = source->kind;
        records[i].flags = source->flags;
        records[i].ordinal = source->ordinal;
        records[i].related_symbol_id = source->related_symbol_id;
        records[i].related_node_id = source->related_node_id;
        records[i].type_id = source->type_id;
        status = append_string(source->name, *out_table, length, &used,
            &records[i].name_offset, &records[i].name_length);
        if (status != OIR_STATUS_OK) return status;
    }
    *out_length = length;
    return used == length ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}

OirStatus oir_semantic_ast_artifact_write(
    const uint8_t* ast_payload,
    size_t ast_payload_length,
    const SemanticResult* semantic_result,
    uint64_t job_id,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
) {
    OirStatus status = OIR_STATUS_INVALID_ARGUMENT;
    OirArtifactHeader header;
    OirSectionInput sections[7];
    OirSemanticScope* scope_records = NULL;
    OirSemanticType* type_records = NULL;
    OirSemanticSymbol* symbol_records = NULL;
    OirSemanticAnnotation* annotation_records = NULL;
    OirSemanticRelation* relation_records = NULL;
    OirSemanticConversion* conversion_records = NULL;
    uint8_t* type_strings = NULL;
    uint8_t* symbol_strings = NULL;
    uint8_t* relation_strings = NULL;
    uint8_t* scope_section = NULL;
    uint8_t* type_section = NULL;
    uint8_t* symbol_section = NULL;
    uint8_t* annotation_section = NULL;
    uint8_t* relation_section = NULL;
    uint8_t* conversion_section = NULL;
    uint8_t* artifact_scratch = NULL;
    uint32_t type_string_length = 0u;
    uint32_t symbol_string_length = 0u;
    uint32_t relation_string_length = 0u;
    size_t scope_section_size, type_section_size, symbol_section_size;
    size_t annotation_section_size, relation_section_size, conversion_section_size = 0u;
    size_t total_size = OIR_HEADER_SIZE;
    size_t written = 0u;
    uint32_t section_count;
    uint32_t i;
    OirAstNodeTableView ast_view;
    OirSemanticAstArtifactView artifact_view;
    uint32_t* incoming_counts = NULL;
    uint8_t* visited = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    uint32_t* owner_scope_by_node = NULL;

    if (out_written) *out_written = 0u;
    if (!ast_payload || !semantic_result || !output || !out_written) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (!semantic_result->finalized ||
        semantic_result->scope_count == 0u ||
        semantic_result->scope_count > OIR_SEMANTIC_MAX_SCOPE_COUNT ||
        semantic_result->type_count > OIR_SEMANTIC_MAX_TYPE_COUNT ||
        semantic_result->symbol_count > OIR_SEMANTIC_MAX_SYMBOL_COUNT ||
        semantic_result->annotation_count > OIR_SEMANTIC_MAX_ANNOTATION_COUNT ||
        semantic_result->relation_count > OIR_SEMANTIC_MAX_RELATION_COUNT ||
        semantic_result->conversion_count > OIR_SEMANTIC_MAX_CONVERSION_COUNT ||
        !semantic_result->scopes ||
        (semantic_result->type_count && !semantic_result->types) ||
        (semantic_result->symbol_count && !semantic_result->symbols) ||
        (semantic_result->annotation_count && !semantic_result->annotations) ||
        (semantic_result->relation_count && !semantic_result->relations) ||
        (semantic_result->conversion_count && !semantic_result->conversions)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    status = oir_ast_node_table_open(ast_payload, ast_payload_length, &ast_view);
    if (status != OIR_STATUS_OK) goto cleanup;
    if (ast_view.node_count == 0u) {
        status = OIR_STATUS_INVALID_SECTION;
        goto cleanup;
    }

    scope_records = (OirSemanticScope*)allocate_array(
        semantic_result->scope_count, sizeof(*scope_records));
    type_records = (OirSemanticType*)allocate_array(
        semantic_result->type_count, sizeof(*type_records));
    symbol_records = (OirSemanticSymbol*)allocate_array(
        semantic_result->symbol_count, sizeof(*symbol_records));
    annotation_records = (OirSemanticAnnotation*)allocate_array(
        semantic_result->annotation_count, sizeof(*annotation_records));
    relation_records = (OirSemanticRelation*)allocate_array(
        semantic_result->relation_count, sizeof(*relation_records));
    conversion_records = (OirSemanticConversion*)allocate_array(
        semantic_result->conversion_count, sizeof(*conversion_records));

    if (!scope_records ||
        (semantic_result->type_count && !type_records) ||
        (semantic_result->symbol_count && !symbol_records) ||
        (semantic_result->annotation_count && !annotation_records) ||
        (semantic_result->relation_count && !relation_records) ||
        (semantic_result->conversion_count && !conversion_records)) {
        status = OIR_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }

    for (i = 0u; i < semantic_result->scope_count; ++i) {
        scope_records[i].scope_id = semantic_result->scopes[i].scope_id;
        scope_records[i].parent_scope_id = semantic_result->scopes[i].parent_scope_id;
        scope_records[i].depth = semantic_result->scopes[i].depth;
        scope_records[i].flags = semantic_result->scopes[i].flags;
        scope_records[i].owner_node_id = semantic_result->scopes[i].owner_node_id;
    }
    status = build_type_strings(
        semantic_result, type_records, &type_strings, &type_string_length);
    if (status != OIR_STATUS_OK) goto cleanup;
    status = build_symbol_strings(
        semantic_result, symbol_records, &symbol_strings, &symbol_string_length);
    if (status != OIR_STATUS_OK) goto cleanup;
    status = build_relation_strings(
        semantic_result, relation_records, &relation_strings, &relation_string_length);
    if (status != OIR_STATUS_OK) goto cleanup;

    for (i = 0u; i < semantic_result->annotation_count; ++i) {
        const SemanticResultAnnotation* source = &semantic_result->annotations[i];
        annotation_records[i].node_id = source->node_id;
        annotation_records[i].scope_id = source->scope_id;
        annotation_records[i].resolved_symbol_id = source->resolved_symbol_id;
        annotation_records[i].resolved_type_id = source->resolved_type_id;
        annotation_records[i].value_category = source->value_category;
        annotation_records[i].flags = source->flags;
    }
    for (i = 0u; i < semantic_result->conversion_count; ++i) {
        const SemanticResultConversion* source = &semantic_result->conversions[i];
        conversion_records[i].node_id = source->node_id;
        conversion_records[i].operand_ordinal = source->operand_ordinal;
        conversion_records[i].source_type_id = source->source_type_id;
        conversion_records[i].destination_type_id = source->destination_type_id;
        conversion_records[i].kind = source->kind;
        conversion_records[i].flags = source->flags;
    }

    scope_section_size = OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        (size_t)semantic_result->scope_count * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
    type_section_size = OIR_SEMANTIC_TYPES_HEADER_SIZE +
        (size_t)semantic_result->type_count * OIR_SEMANTIC_TYPE_RECORD_SIZE +
        type_string_length;
    symbol_section_size = OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        (size_t)semantic_result->symbol_count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE +
        symbol_string_length;
    annotation_section_size = OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
        (size_t)semantic_result->annotation_count * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
    relation_section_size = OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        (size_t)semantic_result->relation_count * OIR_SEMANTIC_RELATION_RECORD_SIZE +
        relation_string_length;
    if (semantic_result->conversion_count != 0u) {
        conversion_section_size = OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
            (size_t)semantic_result->conversion_count *
                OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
    }

    scope_section = (uint8_t*)malloc(scope_section_size);
    type_section = (uint8_t*)malloc(type_section_size);
    symbol_section = (uint8_t*)malloc(symbol_section_size);
    annotation_section = (uint8_t*)malloc(annotation_section_size);
    relation_section = (uint8_t*)malloc(relation_section_size);
    if (conversion_section_size) conversion_section = (uint8_t*)malloc(conversion_section_size);
    if (!scope_section || !type_section || !symbol_section ||
        !annotation_section || !relation_section ||
        (conversion_section_size && !conversion_section)) {
        status = OIR_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }

    status = oir_semantic_scopes_write(
        scope_section, scope_section_size, scope_records,
        semantic_result->scope_count, &written);
    if (status != OIR_STATUS_OK || written != scope_section_size) goto cleanup;
    status = oir_semantic_types_write(
        type_section, type_section_size, type_records,
        semantic_result->type_count, type_strings, type_string_length, &written);
    if (status != OIR_STATUS_OK || written != type_section_size) goto cleanup;
    status = oir_semantic_symbols_write(
        symbol_section, symbol_section_size, symbol_records,
        semantic_result->symbol_count, symbol_strings, symbol_string_length, &written);
    if (status != OIR_STATUS_OK || written != symbol_section_size) goto cleanup;
    status = oir_semantic_annotations_write(
        annotation_section, annotation_section_size, annotation_records,
        semantic_result->annotation_count, &written);
    if (status != OIR_STATUS_OK || written != annotation_section_size) goto cleanup;
    status = oir_semantic_relations_write(
        relation_section, relation_section_size, relation_records,
        semantic_result->relation_count, relation_strings, relation_string_length,
        &written);
    if (status != OIR_STATUS_OK || written != relation_section_size) goto cleanup;
    if (conversion_section_size) {
        status = oir_semantic_conversions_write(
            conversion_section, conversion_section_size, conversion_records,
            semantic_result->conversion_count, &written);
        if (status != OIR_STATUS_OK || written != conversion_section_size) goto cleanup;
    }

    section_count = conversion_section_size ? 7u : 6u;
    memset(sections, 0, sizeof(sections));
    sections[0].type = OIR_SECTION_PAYLOAD;
    sections[0].version = 1u;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = ast_payload;
    sections[0].length = ast_payload_length;

    sections[1].type = OIR_SECTION_SEMANTIC_SCOPES;
    sections[1].version = 1u;
    sections[1].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[1].data = scope_section;
    sections[1].length = scope_section_size;

    sections[2].type = OIR_SECTION_SEMANTIC_TYPES;
    sections[2].version = 1u;
    sections[2].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[2].data = type_section;
    sections[2].length = type_section_size;

    sections[3].type = OIR_SECTION_SEMANTIC_SYMBOLS;
    sections[3].version = 1u;
    sections[3].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[3].data = symbol_section;
    sections[3].length = symbol_section_size;

    sections[4].type = OIR_SECTION_SEMANTIC_ANNOTATIONS;
    sections[4].version = 1u;
    sections[4].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[4].data = annotation_section;
    sections[4].length = annotation_section_size;

    sections[5].type = OIR_SECTION_SEMANTIC_RELATIONS;
    sections[5].version = 1u;
    sections[5].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[5].data = relation_section;
    sections[5].length = relation_section_size;

    if (conversion_section_size) {
        sections[6].type = OIR_SECTION_SEMANTIC_CONVERSIONS;
        sections[6].version = 1u;
        sections[6].flags = 0u;
        sections[6].data = conversion_section;
        sections[6].length = conversion_section_size;
    }

    if (!add_size(&total_size, (size_t)section_count * OIR_SECTION_HEADER_SIZE) ||
        !add_size(&total_size, ast_payload_length) ||
        !add_size(&total_size, scope_section_size) ||
        !add_size(&total_size, type_section_size) ||
        !add_size(&total_size, symbol_section_size) ||
        !add_size(&total_size, annotation_section_size) ||
        !add_size(&total_size, relation_section_size) ||
        (conversion_section_size && !add_size(&total_size, conversion_section_size))) {
        status = OIR_STATUS_OVERFLOW;
        goto cleanup;
    }
    if (total_size > output_capacity) {
        status = OIR_STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_SEMANTIC_AST;
    header.payload_schema_major = OIR_SEMANTIC_AST_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_SEMANTIC_AST_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_SEMANTIC;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = job_id;

    artifact_scratch = (uint8_t*)malloc(total_size);
    if (!artifact_scratch) {
        status = OIR_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }
    status = oir_artifact_write(
        artifact_scratch, total_size, &header, sections, section_count, &written);
    if (status != OIR_STATUS_OK || written != total_size) goto cleanup;

    incoming_counts = (uint32_t*)calloc(ast_view.node_count, sizeof(*incoming_counts));
    visited = (uint8_t*)calloc(ast_view.node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)allocate_array(
        ast_view.node_count, sizeof(*graph_stack));
    owner_scope_by_node = (uint32_t*)calloc(
        (size_t)ast_view.node_count + 1u, sizeof(*owner_scope_by_node));
    if (!incoming_counts || !visited || !graph_stack || !owner_scope_by_node) {
        status = OIR_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }

    status = oir_semantic_ast_artifact_open(
        artifact_scratch, written, &artifact_view,
        incoming_counts, ast_view.node_count,
        visited, ast_view.node_count,
        graph_stack, ast_view.node_count,
        owner_scope_by_node, (size_t)ast_view.node_count + 1u);
    if (status != OIR_STATUS_OK) goto cleanup;

    memmove(output, artifact_scratch, written);
    *out_written = written;
    status = OIR_STATUS_OK;

cleanup:
    free(scope_records);
    free(type_records);
    free(symbol_records);
    free(annotation_records);
    free(relation_records);
    free(conversion_records);
    free(type_strings);
    free(symbol_strings);
    free(relation_strings);
    free(scope_section);
    free(type_section);
    free(symbol_section);
    free(annotation_section);
    free(relation_section);
    free(conversion_section);
    free(artifact_scratch);
    free(incoming_counts);
    free(visited);
    free(graph_stack);
    free(owner_scope_by_node);
    return status;
}
