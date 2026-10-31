#include "oir_semantic_ast.h"

#include <string.h>

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

static OirStatus validate_scope_records(
    const uint8_t* records,
    uint32_t scope_count
) {
    uint32_t i;

    if (!records || scope_count == 0u ||
        scope_count > OIR_SEMANTIC_MAX_SCOPE_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0; i < scope_count; ++i) {
        const uint8_t* record = records + (size_t)i * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
        uint32_t scope_id = read_u32_le(record);
        uint32_t parent_id = read_u32_le(record + 4);
        uint32_t depth = read_u32_le(record + 8);
        uint32_t flags = read_u32_le(record + 12);
        uint32_t owner_node_id = read_u32_le(record + 16);

        if (scope_id != i + 1u || flags != 0u ||
            depth > OIR_SEMANTIC_MAX_SCOPE_DEPTH) {
            return OIR_STATUS_INVALID_SECTION;
        }

        if (i == 0u) {
            if (parent_id != 0u || depth != 0u || owner_node_id != 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } else {
            uint32_t parent_depth;
            const uint8_t* parent_record;

            /* Parent-before-child IDs make cycles impossible and traversal canonical. */
            if (parent_id == 0u || parent_id >= scope_id || depth == 0u ||
                owner_node_id == 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }

            parent_record = records +
                (size_t)(parent_id - 1u) * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
            parent_depth = read_u32_le(parent_record + 8);
            if (parent_depth + 1u != depth) {
                return OIR_STATUS_INVALID_SECTION;
            }
        }
    }

    return OIR_STATUS_OK;
}

OirStatus oir_semantic_scopes_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticScopesView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));

    if (payload_length < OIR_SEMANTIC_SCOPES_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    if (record_size != OIR_SEMANTIC_SCOPE_RECORD_SIZE ||
        count == 0u || count > OIR_SEMANTIC_MAX_SCOPE_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        (uint64_t)count * (uint64_t)record_size;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_scope_records(
        payload + OIR_SEMANTIC_SCOPES_HEADER_SIZE, count);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_SCOPES_HEADER_SIZE;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_scopes_get(
    const OirSemanticScopesView* view,
    uint32_t index,
    OirSemanticScope* out_scope
) {
    const uint8_t* record;

    if (!view || !view->bytes || !out_scope) return OIR_STATUS_INVALID_ARGUMENT;
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
    out_scope->scope_id = read_u32_le(record);
    out_scope->parent_scope_id = read_u32_le(record + 4);
    out_scope->depth = read_u32_le(record + 8);
    out_scope->flags = read_u32_le(record + 12);
    out_scope->owner_node_id = read_u32_le(record + 16);
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_scopes_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticScope* scopes,
    uint32_t scope_count,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;
    OirStatus status;

    if (out_written) *out_written = 0u;
    if (!output || !scopes || !out_written) return OIR_STATUS_INVALID_ARGUMENT;
    if (scope_count == 0u || scope_count > OIR_SEMANTIC_MAX_SCOPE_COUNT) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    /* Validate logical records before touching caller-owned output. */
    for (i = 0; i < scope_count; ++i) {
        const OirSemanticScope* scope = &scopes[i];
        if (scope->scope_id != i + 1u || scope->flags != 0u ||
            scope->depth > OIR_SEMANTIC_MAX_SCOPE_DEPTH) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (i == 0u) {
            if (scope->parent_scope_id != 0u || scope->depth != 0u ||
                scope->owner_node_id != 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } else {
            if (scope->parent_scope_id == 0u ||
                scope->parent_scope_id >= scope->scope_id ||
                scope->depth == 0u || scope->owner_node_id == 0u ||
                scopes[scope->parent_scope_id - 1u].depth + 1u != scope->depth) {
                return OIR_STATUS_INVALID_SECTION;
            }
        }
    }

    total_length = (uint64_t)OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        (uint64_t)scope_count * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, scope_count);
    write_u32_le(output + 4, OIR_SEMANTIC_SCOPE_RECORD_SIZE);
    for (i = 0; i < scope_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
        write_u32_le(record, scopes[i].scope_id);
        write_u32_le(record + 4, scopes[i].parent_scope_id);
        write_u32_le(record + 8, scopes[i].depth);
        write_u32_le(record + 12, scopes[i].flags);
        write_u32_le(record + 16, scopes[i].owner_node_id);
    }

    *out_written = (size_t)total_length;
    status = OIR_STATUS_OK;
    return status;
}
