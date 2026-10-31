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

    uint64_t expected_length;

    if (!view || !view->bytes || !out_scope) return OIR_STATUS_INVALID_ARGUMENT;
    if (view->records_offset != OIR_SEMANTIC_SCOPES_HEADER_SIZE ||
        view->record_count == 0u ||
        view->record_count > OIR_SEMANTIC_MAX_SCOPE_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_SCOPE_RECORD_SIZE;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length) {
        return OIR_STATUS_INVALID_SECTION;
    }
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


static int is_valid_utf8_name(const uint8_t* bytes, uint32_t length) {
    uint32_t i = 0u;

    if (!bytes && length != 0u) return 0;
    while (i < length) {
        uint8_t c = bytes[i];
        uint32_t remaining;
        uint32_t codepoint;

        if (c == 0u) return 0;
        if (c <= 0x7fu) {
            ++i;
            continue;
        }
        if (c >= 0xc2u && c <= 0xdfu) {
            remaining = 1u;
            codepoint = (uint32_t)(c & 0x1fu);
        } else if (c >= 0xe0u && c <= 0xefu) {
            remaining = 2u;
            codepoint = (uint32_t)(c & 0x0fu);
        } else if (c >= 0xf0u && c <= 0xf4u) {
            remaining = 3u;
            codepoint = (uint32_t)(c & 0x07u);
        } else {
            return 0;
        }

        if (remaining > length - i - 1u) return 0;
        while (remaining != 0u) {
            uint8_t next = bytes[++i];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6) | (uint32_t)(next & 0x3fu);
            --remaining;
        }

        /* Reject overlong encodings, UTF-16 surrogates and values above U+10FFFF. */
        if (codepoint < 0x80u ||
            (codepoint < 0x800u && c >= 0xe0u) ||
            (codepoint < 0x10000u && c >= 0xf0u) ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
            codepoint > 0x10ffffu) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static OirStatus validate_type_values(
    uint32_t type_id,
    uint16_t kind,
    uint16_t flags,
    uint32_t name_offset,
    uint32_t name_length,
    uint32_t referenced_symbol_id,
    uint32_t base_type_id,
    const uint8_t* string_table,
    uint32_t string_table_length
) {
    if (kind < OIR_SEMANTIC_TYPE_PRIMITIVE ||
        kind > OIR_SEMANTIC_TYPE_UNKNOWN ||
        (flags & ~(OIR_SEMANTIC_TYPE_FLAG_CONST |
            OIR_SEMANTIC_TYPE_FLAG_HARD)) != 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (name_length == 0u) {
        if (name_offset != 0u) return OIR_STATUS_INVALID_SECTION;
    } else {
        if (name_offset > string_table_length ||
            name_length > string_table_length - name_offset ||
            !is_valid_utf8_name(string_table + name_offset, name_length)) {
            return OIR_STATUS_INVALID_SECTION;
        }
    }

    if (kind == OIR_SEMANTIC_TYPE_USER_DEFINED) {
        if (referenced_symbol_id == 0u) return OIR_STATUS_INVALID_SECTION;
    } else if (referenced_symbol_id != 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }

    /* Canonical first-use ordering keeps the type-reference graph acyclic. */
    if (base_type_id != 0u && base_type_id >= type_id) {
        return OIR_STATUS_INVALID_SECTION;
    }
    return OIR_STATUS_OK;
}

static OirStatus validate_type_records(
    const uint8_t* records,
    uint32_t type_count,
    const uint8_t* string_table,
    uint32_t string_table_length
) {
    uint32_t i;

    if ((!records && type_count != 0u) ||
        type_count > OIR_SEMANTIC_MAX_TYPE_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (!string_table && string_table_length != 0u) ||
        (type_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < type_count; ++i) {
        const uint8_t* record = records + (size_t)i * OIR_SEMANTIC_TYPE_RECORD_SIZE;
        uint32_t type_id = read_u32_le(record);
        uint16_t kind = (uint16_t)((uint16_t)record[4] |
            ((uint16_t)record[5] << 8));
        uint16_t flags = (uint16_t)((uint16_t)record[6] |
            ((uint16_t)record[7] << 8));
        OirStatus status;

        if (type_id != i + 1u) return OIR_STATUS_INVALID_SECTION;
        status = validate_type_values(
            type_id, kind, flags,
            read_u32_le(record + 8),
            read_u32_le(record + 12),
            read_u32_le(record + 16),
            read_u32_le(record + 20),
            string_table, string_table_length);
        if (status != OIR_STATUS_OK) return status;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_types_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticTypesView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint32_t string_length;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SEMANTIC_TYPES_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    string_length = read_u32_le(payload + 8);
    if (record_size != OIR_SEMANTIC_TYPE_RECORD_SIZE ||
        count > OIR_SEMANTIC_MAX_TYPE_COUNT ||
        string_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_TYPES_HEADER_SIZE +
        (uint64_t)count * OIR_SEMANTIC_TYPE_RECORD_SIZE + string_length;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_type_records(
        payload + OIR_SEMANTIC_TYPES_HEADER_SIZE, count,
        payload + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            (size_t)count * OIR_SEMANTIC_TYPE_RECORD_SIZE,
        string_length);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_TYPES_HEADER_SIZE;
    out_view->strings_offset = OIR_SEMANTIC_TYPES_HEADER_SIZE +
        (size_t)count * OIR_SEMANTIC_TYPE_RECORD_SIZE;
    out_view->string_table_length = string_length;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_types_get(
    const OirSemanticTypesView* view,
    uint32_t index,
    OirSemanticType* out_type,
    const uint8_t** out_name
) {
    const uint8_t* record;
    uint64_t expected_length;

    if (!view || !view->bytes || !out_type) return OIR_STATUS_INVALID_ARGUMENT;
    if (view->records_offset != OIR_SEMANTIC_TYPES_HEADER_SIZE ||
        view->record_count > OIR_SEMANTIC_MAX_TYPE_COUNT ||
        view->string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_TYPES_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_TYPE_RECORD_SIZE +
        view->string_table_length;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length ||
        view->strings_offset != OIR_SEMANTIC_TYPES_HEADER_SIZE +
            (size_t)view->record_count * OIR_SEMANTIC_TYPE_RECORD_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_TYPE_RECORD_SIZE;
    out_type->type_id = read_u32_le(record);
    out_type->kind = (uint16_t)((uint16_t)record[4] |
        ((uint16_t)record[5] << 8));
    out_type->flags = (uint16_t)((uint16_t)record[6] |
        ((uint16_t)record[7] << 8));
    out_type->name_offset = read_u32_le(record + 8);
    out_type->name_length = read_u32_le(record + 12);
    out_type->referenced_symbol_id = read_u32_le(record + 16);
    out_type->base_type_id = read_u32_le(record + 20);
    if (out_name) {
        *out_name = out_type->name_length
            ? view->bytes + view->strings_offset + out_type->name_offset
            : NULL;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_types_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticType* types,
    uint32_t type_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;

    if (out_written) *out_written = 0u;
    if (!output || !out_written || (!types && type_count != 0u) ||
        (!string_table && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (type_count > OIR_SEMANTIC_MAX_TYPE_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (type_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0u; i < type_count; ++i) {
        const OirSemanticType* type = &types[i];
        OirStatus status;
        if (type->type_id != i + 1u) return OIR_STATUS_INVALID_SECTION;
        status = validate_type_values(
            type->type_id, type->kind, type->flags,
            type->name_offset, type->name_length,
            type->referenced_symbol_id, type->base_type_id,
            string_table, string_table_length);
        if (status != OIR_STATUS_OK) return status;
    }

    total_length = (uint64_t)OIR_SEMANTIC_TYPES_HEADER_SIZE +
        (uint64_t)type_count * OIR_SEMANTIC_TYPE_RECORD_SIZE +
        string_table_length;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, type_count);
    write_u32_le(output + 4, OIR_SEMANTIC_TYPE_RECORD_SIZE);
    write_u32_le(output + 8, string_table_length);
    for (i = 0u; i < type_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_TYPE_RECORD_SIZE;
        write_u32_le(record, types[i].type_id);
        record[4] = (uint8_t)(types[i].kind & 0xffu);
        record[5] = (uint8_t)((types[i].kind >> 8) & 0xffu);
        record[6] = (uint8_t)(types[i].flags & 0xffu);
        record[7] = (uint8_t)((types[i].flags >> 8) & 0xffu);
        write_u32_le(record + 8, types[i].name_offset);
        write_u32_le(record + 12, types[i].name_length);
        write_u32_le(record + 16, types[i].referenced_symbol_id);
        write_u32_le(record + 20, types[i].base_type_id);
    }
    if (string_table_length != 0u) {
        memcpy(output + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            (size_t)type_count * OIR_SEMANTIC_TYPE_RECORD_SIZE,
            string_table, string_table_length);
    }

    *out_written = (size_t)total_length;
    return OIR_STATUS_OK;
}


static OirStatus validate_symbol_values(
    uint32_t symbol_id,
    uint32_t scope_id,
    uint32_t declaration_node_id,
    uint16_t kind,
    uint16_t flags,
    uint32_t name_offset,
    uint32_t name_length,
    uint32_t first_relation,
    uint32_t relation_count,
    const uint8_t* string_table,
    uint32_t string_table_length
) {
    uint64_t relation_end;

    if (kind < OIR_SEMANTIC_SYMBOL_VARIABLE ||
        kind > OIR_SEMANTIC_SYMBOL_LABEL ||
        (flags & ~(OIR_SEMANTIC_SYMBOL_FLAG_CONST |
            OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC |
            OIR_SEMANTIC_SYMBOL_FLAG_EXPORTED |
            OIR_SEMANTIC_SYMBOL_FLAG_PACKED |
            OIR_SEMANTIC_SYMBOL_FLAG_HARD)) != 0u ||
        scope_id == 0u || declaration_node_id == 0u ||
        name_length == 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (name_offset > string_table_length ||
        name_length > string_table_length - name_offset ||
        !is_valid_utf8_name(string_table + name_offset, name_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (relation_count == 0u) {
        if (first_relation != 0u) return OIR_STATUS_INVALID_SECTION;
    } else {
        if (first_relation == 0u) return OIR_STATUS_INVALID_SECTION;
        relation_end = (uint64_t)first_relation + relation_count - 1u;
        if (relation_end > UINT32_MAX) return OIR_STATUS_INVALID_SECTION;
    }

    (void)symbol_id;
    return OIR_STATUS_OK;
}

static OirStatus validate_symbol_records(
    const uint8_t* records,
    uint32_t symbol_count,
    const uint8_t* string_table,
    uint32_t string_table_length
) {
    uint32_t i;

    if ((!records && symbol_count != 0u) ||
        symbol_count > OIR_SEMANTIC_MAX_SYMBOL_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (!string_table && string_table_length != 0u) ||
        (symbol_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < symbol_count; ++i) {
        const uint8_t* record = records + (size_t)i * OIR_SEMANTIC_SYMBOL_RECORD_SIZE;
        uint16_t kind = (uint16_t)((uint16_t)record[12] |
            ((uint16_t)record[13] << 8));
        uint16_t flags = (uint16_t)((uint16_t)record[14] |
            ((uint16_t)record[15] << 8));
        OirStatus status;

        if (read_u32_le(record) != i + 1u) return OIR_STATUS_INVALID_SECTION;
        status = validate_symbol_values(
            read_u32_le(record),
            read_u32_le(record + 4),
            read_u32_le(record + 8),
            kind,
            flags,
            read_u32_le(record + 16),
            read_u32_le(record + 20),
            read_u32_le(record + 28),
            read_u32_le(record + 32),
            string_table, string_table_length);
        if (status != OIR_STATUS_OK) return status;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_symbols_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticSymbolsView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint32_t string_length;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SEMANTIC_SYMBOLS_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    string_length = read_u32_le(payload + 8);
    if (record_size != OIR_SEMANTIC_SYMBOL_RECORD_SIZE ||
        count > OIR_SEMANTIC_MAX_SYMBOL_COUNT ||
        string_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        (uint64_t)count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE + string_length;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_symbol_records(
        payload + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE, count,
        payload + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            (size_t)count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE,
        string_length);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_SYMBOLS_HEADER_SIZE;
    out_view->strings_offset = OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        (size_t)count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE;
    out_view->string_table_length = string_length;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_symbols_get(
    const OirSemanticSymbolsView* view,
    uint32_t index,
    OirSemanticSymbol* out_symbol,
    const uint8_t** out_name
) {
    const uint8_t* record;
    uint64_t expected_length;

    if (!view || !view->bytes || !out_symbol) return OIR_STATUS_INVALID_ARGUMENT;
    if (view->records_offset != OIR_SEMANTIC_SYMBOLS_HEADER_SIZE ||
        view->record_count > OIR_SEMANTIC_MAX_SYMBOL_COUNT ||
        view->string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE +
        view->string_table_length;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length ||
        view->strings_offset != OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            (size_t)view->record_count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_SYMBOL_RECORD_SIZE;
    out_symbol->symbol_id = read_u32_le(record);
    out_symbol->scope_id = read_u32_le(record + 4);
    out_symbol->declaration_node_id = read_u32_le(record + 8);
    out_symbol->kind = (uint16_t)((uint16_t)record[12] |
        ((uint16_t)record[13] << 8));
    out_symbol->flags = (uint16_t)((uint16_t)record[14] |
        ((uint16_t)record[15] << 8));
    out_symbol->name_offset = read_u32_le(record + 16);
    out_symbol->name_length = read_u32_le(record + 20);
    out_symbol->type_id = read_u32_le(record + 24);
    out_symbol->first_relation = read_u32_le(record + 28);
    out_symbol->relation_count = read_u32_le(record + 32);
    if (out_name) {
        *out_name = view->bytes + view->strings_offset + out_symbol->name_offset;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_symbols_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticSymbol* symbols,
    uint32_t symbol_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;

    if (out_written) *out_written = 0u;
    if (!output || !out_written || (!symbols && symbol_count != 0u) ||
        (!string_table && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (symbol_count > OIR_SEMANTIC_MAX_SYMBOL_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (symbol_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0u; i < symbol_count; ++i) {
        const OirSemanticSymbol* symbol = &symbols[i];
        OirStatus status;
        if (symbol->symbol_id != i + 1u) return OIR_STATUS_INVALID_SECTION;
        status = validate_symbol_values(
            symbol->symbol_id, symbol->scope_id,
            symbol->declaration_node_id, symbol->kind, symbol->flags,
            symbol->name_offset, symbol->name_length,
            symbol->first_relation, symbol->relation_count,
            string_table, string_table_length);
        if (status != OIR_STATUS_OK) return status;
    }

    total_length = (uint64_t)OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
        (uint64_t)symbol_count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE +
        string_table_length;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, symbol_count);
    write_u32_le(output + 4, OIR_SEMANTIC_SYMBOL_RECORD_SIZE);
    write_u32_le(output + 8, string_table_length);
    for (i = 0u; i < symbol_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_SYMBOL_RECORD_SIZE;
        write_u32_le(record, symbols[i].symbol_id);
        write_u32_le(record + 4, symbols[i].scope_id);
        write_u32_le(record + 8, symbols[i].declaration_node_id);
        record[12] = (uint8_t)(symbols[i].kind & 0xffu);
        record[13] = (uint8_t)((symbols[i].kind >> 8) & 0xffu);
        record[14] = (uint8_t)(symbols[i].flags & 0xffu);
        record[15] = (uint8_t)((symbols[i].flags >> 8) & 0xffu);
        write_u32_le(record + 16, symbols[i].name_offset);
        write_u32_le(record + 20, symbols[i].name_length);
        write_u32_le(record + 24, symbols[i].type_id);
        write_u32_le(record + 28, symbols[i].first_relation);
        write_u32_le(record + 32, symbols[i].relation_count);
    }
    if (string_table_length != 0u) {
        memcpy(output + OIR_SEMANTIC_SYMBOLS_HEADER_SIZE +
            (size_t)symbol_count * OIR_SEMANTIC_SYMBOL_RECORD_SIZE,
            string_table, string_table_length);
    }

    *out_written = (size_t)total_length;
    return OIR_STATUS_OK;
}


static OirStatus validate_annotation_records(
    const uint8_t* records,
    uint32_t annotation_count
) {
    uint32_t i;
    uint32_t previous_node_id = 0u;

    if ((!records && annotation_count != 0u) ||
        annotation_count > OIR_SEMANTIC_MAX_ANNOTATION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < annotation_count; ++i) {
        const uint8_t* record =
            records + (size_t)i * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
        uint32_t node_id = read_u32_le(record);
        uint32_t scope_id = read_u32_le(record + 4);
        uint16_t value_category = (uint16_t)((uint16_t)record[16] |
            ((uint16_t)record[17] << 8));
        uint16_t flags = (uint16_t)((uint16_t)record[18] |
            ((uint16_t)record[19] << 8));

        if (node_id == 0u || node_id <= previous_node_id ||
            scope_id == 0u ||
            value_category > OIR_SEMANTIC_VALUE_FUNCTION_DESIGNATOR ||
            flags != 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }
        previous_node_id = node_id;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_annotations_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticAnnotationsView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    if (record_size != OIR_SEMANTIC_ANNOTATION_RECORD_SIZE ||
        count > OIR_SEMANTIC_MAX_ANNOTATION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
        (uint64_t)count * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_annotation_records(
        payload + OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE, count);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_annotations_get(
    const OirSemanticAnnotationsView* view,
    uint32_t index,
    OirSemanticAnnotation* out_annotation
) {
    const uint8_t* record;
    uint64_t expected_length;

    if (!view || !view->bytes || !out_annotation) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (view->records_offset != OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE ||
        view->record_count > OIR_SEMANTIC_MAX_ANNOTATION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
    out_annotation->node_id = read_u32_le(record);
    out_annotation->scope_id = read_u32_le(record + 4);
    out_annotation->resolved_symbol_id = read_u32_le(record + 8);
    out_annotation->resolved_type_id = read_u32_le(record + 12);
    out_annotation->value_category = (uint16_t)((uint16_t)record[16] |
        ((uint16_t)record[17] << 8));
    out_annotation->flags = (uint16_t)((uint16_t)record[18] |
        ((uint16_t)record[19] << 8));
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_annotations_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticAnnotation* annotations,
    uint32_t annotation_count,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;
    uint32_t previous_node_id = 0u;

    if (out_written) *out_written = 0u;
    if (!output || !out_written ||
        (!annotations && annotation_count != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (annotation_count > OIR_SEMANTIC_MAX_ANNOTATION_COUNT) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0u; i < annotation_count; ++i) {
        const OirSemanticAnnotation* annotation = &annotations[i];
        if (annotation->node_id == 0u ||
            annotation->node_id <= previous_node_id ||
            annotation->scope_id == 0u ||
            annotation->value_category > OIR_SEMANTIC_VALUE_FUNCTION_DESIGNATOR ||
            annotation->flags != 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }
        previous_node_id = annotation->node_id;
    }

    total_length = (uint64_t)OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
        (uint64_t)annotation_count * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, annotation_count);
    write_u32_le(output + 4, OIR_SEMANTIC_ANNOTATION_RECORD_SIZE);
    for (i = 0u; i < annotation_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_ANNOTATION_RECORD_SIZE;
        write_u32_le(record, annotations[i].node_id);
        write_u32_le(record + 4, annotations[i].scope_id);
        write_u32_le(record + 8, annotations[i].resolved_symbol_id);
        write_u32_le(record + 12, annotations[i].resolved_type_id);
        record[16] = (uint8_t)(annotations[i].value_category & 0xffu);
        record[17] = (uint8_t)((annotations[i].value_category >> 8) & 0xffu);
        record[18] = (uint8_t)(annotations[i].flags & 0xffu);
        record[19] = (uint8_t)((annotations[i].flags >> 8) & 0xffu);
    }

    *out_written = (size_t)total_length;
    return OIR_STATUS_OK;
}


static OirStatus validate_relation_records(
    const uint8_t* records,
    uint32_t relation_count,
    const uint8_t* string_table,
    uint32_t string_table_length
) {
    uint32_t i;
    uint32_t previous_owner = 0u;
    uint16_t previous_kind = 0u;
    uint32_t previous_ordinal = 0u;

    if ((!records && relation_count != 0u) ||
        relation_count > OIR_SEMANTIC_MAX_RELATION_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (!string_table && string_table_length != 0u) ||
        (relation_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < relation_count; ++i) {
        const uint8_t* record = records +
            (size_t)i * OIR_SEMANTIC_RELATION_RECORD_SIZE;
        uint32_t owner = read_u32_le(record);
        uint16_t kind = (uint16_t)((uint16_t)record[4] |
            ((uint16_t)record[5] << 8));
        uint16_t flags = (uint16_t)((uint16_t)record[6] |
            ((uint16_t)record[7] << 8));
        uint32_t ordinal = read_u32_le(record + 8);
        uint32_t related_node_id = read_u32_le(record + 16);
        uint32_t name_offset = read_u32_le(record + 20);
        uint32_t name_length = read_u32_le(record + 24);

        if (owner == 0u ||
            kind < OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER ||
            kind > OIR_SEMANTIC_RELATION_ENUM_PAYLOAD_FIELD ||
            flags != 0u || related_node_id == 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }

        if (name_length == 0u) {
            if (name_offset != 0u) return OIR_STATUS_INVALID_SECTION;
        } else if (name_offset > string_table_length ||
            name_length > string_table_length - name_offset ||
            !is_valid_utf8_name(string_table + name_offset, name_length)) {
            return OIR_STATUS_INVALID_SECTION;
        }

        if (i != 0u) {
            if (owner < previous_owner ||
                (owner == previous_owner && kind < previous_kind)) {
                return OIR_STATUS_INVALID_SECTION;
            }
            if (owner == previous_owner && kind == previous_kind) {
                if (previous_ordinal == UINT32_MAX ||
                    ordinal != previous_ordinal + 1u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
            } else if (ordinal != 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } else if (ordinal != 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }

        previous_owner = owner;
        previous_kind = kind;
        previous_ordinal = ordinal;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_relations_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticRelationsView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint32_t string_length;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SEMANTIC_RELATIONS_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    string_length = read_u32_le(payload + 8);
    if (record_size != OIR_SEMANTIC_RELATION_RECORD_SIZE ||
        count > OIR_SEMANTIC_MAX_RELATION_COUNT ||
        string_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        (uint64_t)count * OIR_SEMANTIC_RELATION_RECORD_SIZE + string_length;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_relation_records(
        payload + OIR_SEMANTIC_RELATIONS_HEADER_SIZE, count,
        payload + OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            (size_t)count * OIR_SEMANTIC_RELATION_RECORD_SIZE,
        string_length);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_RELATIONS_HEADER_SIZE;
    out_view->strings_offset = OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        (size_t)count * OIR_SEMANTIC_RELATION_RECORD_SIZE;
    out_view->string_table_length = string_length;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_relations_get(
    const OirSemanticRelationsView* view,
    uint32_t index,
    OirSemanticRelation* out_relation,
    const uint8_t** out_name
) {
    const uint8_t* record;
    uint64_t expected_length;

    if (!view || !view->bytes || !out_relation) return OIR_STATUS_INVALID_ARGUMENT;
    if (view->records_offset != OIR_SEMANTIC_RELATIONS_HEADER_SIZE ||
        view->record_count > OIR_SEMANTIC_MAX_RELATION_COUNT ||
        view->string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_RELATION_RECORD_SIZE +
        view->string_table_length;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length ||
        view->strings_offset != OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            (size_t)view->record_count * OIR_SEMANTIC_RELATION_RECORD_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_RELATION_RECORD_SIZE;
    out_relation->owner_symbol_id = read_u32_le(record);
    out_relation->kind = (uint16_t)((uint16_t)record[4] |
        ((uint16_t)record[5] << 8));
    out_relation->flags = (uint16_t)((uint16_t)record[6] |
        ((uint16_t)record[7] << 8));
    out_relation->ordinal = read_u32_le(record + 8);
    out_relation->related_symbol_id = read_u32_le(record + 12);
    out_relation->related_node_id = read_u32_le(record + 16);
    out_relation->name_offset = read_u32_le(record + 20);
    out_relation->name_length = read_u32_le(record + 24);
    out_relation->type_id = read_u32_le(record + 28);
    if (out_name) {
        *out_name = out_relation->name_length
            ? view->bytes + view->strings_offset + out_relation->name_offset
            : NULL;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_relations_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticRelation* relations,
    uint32_t relation_count,
    const uint8_t* string_table,
    uint32_t string_table_length,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;
    uint32_t previous_owner = 0u;
    uint16_t previous_kind = 0u;
    uint32_t previous_ordinal = 0u;

    if (out_written) *out_written = 0u;
    if (!output || !out_written ||
        (!relations && relation_count != 0u) ||
        (!string_table && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (relation_count > OIR_SEMANTIC_MAX_RELATION_COUNT ||
        string_table_length > OIR_SEMANTIC_MAX_STRING_TABLE_SIZE ||
        (relation_count == 0u && string_table_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0u; i < relation_count; ++i) {
        const OirSemanticRelation* relation = &relations[i];
        if (relation->owner_symbol_id == 0u ||
            relation->kind < OIR_SEMANTIC_RELATION_FUNCTION_PARAMETER ||
            relation->kind > OIR_SEMANTIC_RELATION_ENUM_PAYLOAD_FIELD ||
            relation->flags != 0u || relation->related_node_id == 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (relation->name_length == 0u) {
            if (relation->name_offset != 0u) return OIR_STATUS_INVALID_SECTION;
        } else if (relation->name_offset > string_table_length ||
            relation->name_length > string_table_length - relation->name_offset ||
            !is_valid_utf8_name(string_table + relation->name_offset,
                relation->name_length)) {
            return OIR_STATUS_INVALID_SECTION;
        }

        if (i != 0u) {
            if (relation->owner_symbol_id < previous_owner ||
                (relation->owner_symbol_id == previous_owner &&
                    relation->kind < previous_kind)) {
                return OIR_STATUS_INVALID_SECTION;
            }
            if (relation->owner_symbol_id == previous_owner &&
                relation->kind == previous_kind) {
                if (previous_ordinal == UINT32_MAX ||
                    relation->ordinal != previous_ordinal + 1u) {
                    return OIR_STATUS_INVALID_SECTION;
                }
            } else if (relation->ordinal != 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } else if (relation->ordinal != 0u) {
            return OIR_STATUS_INVALID_SECTION;
        }
        previous_owner = relation->owner_symbol_id;
        previous_kind = relation->kind;
        previous_ordinal = relation->ordinal;
    }

    total_length = (uint64_t)OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
        (uint64_t)relation_count * OIR_SEMANTIC_RELATION_RECORD_SIZE +
        string_table_length;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, relation_count);
    write_u32_le(output + 4, OIR_SEMANTIC_RELATION_RECORD_SIZE);
    write_u32_le(output + 8, string_table_length);
    for (i = 0u; i < relation_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_RELATION_RECORD_SIZE;
        write_u32_le(record, relations[i].owner_symbol_id);
        record[4] = (uint8_t)(relations[i].kind & 0xffu);
        record[5] = (uint8_t)((relations[i].kind >> 8) & 0xffu);
        record[6] = (uint8_t)(relations[i].flags & 0xffu);
        record[7] = (uint8_t)((relations[i].flags >> 8) & 0xffu);
        write_u32_le(record + 8, relations[i].ordinal);
        write_u32_le(record + 12, relations[i].related_symbol_id);
        write_u32_le(record + 16, relations[i].related_node_id);
        write_u32_le(record + 20, relations[i].name_offset);
        write_u32_le(record + 24, relations[i].name_length);
        write_u32_le(record + 28, relations[i].type_id);
    }
    if (string_table_length != 0u) {
        memcpy(output + OIR_SEMANTIC_RELATIONS_HEADER_SIZE +
            (size_t)relation_count * OIR_SEMANTIC_RELATION_RECORD_SIZE,
            string_table, string_table_length);
    }

    *out_written = (size_t)total_length;
    return OIR_STATUS_OK;
}


static OirStatus validate_conversion_records(
    const uint8_t* records,
    uint32_t conversion_count
) {
    uint32_t i;
    uint32_t previous_node_id = 0u;
    uint32_t previous_operand = 0u;

    if ((!records && conversion_count != 0u) ||
        conversion_count > OIR_SEMANTIC_MAX_CONVERSION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < conversion_count; ++i) {
        const uint8_t* record = records +
            (size_t)i * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
        uint32_t node_id = read_u32_le(record);
        uint32_t operand_ordinal = read_u32_le(record + 4);
        uint32_t source_type_id = read_u32_le(record + 8);
        uint32_t destination_type_id = read_u32_le(record + 12);
        uint16_t kind = (uint16_t)((uint16_t)record[16] |
            ((uint16_t)record[17] << 8));
        uint16_t flags = (uint16_t)((uint16_t)record[18] |
            ((uint16_t)record[19] << 8));

        if (node_id == 0u || source_type_id == 0u ||
            destination_type_id == 0u ||
            kind < OIR_SEMANTIC_CONVERSION_IDENTITY ||
            kind > OIR_SEMANTIC_CONVERSION_HARD_TYPE ||
            (flags != OIR_SEMANTIC_CONVERSION_FLAG_IMPLICIT &&
                flags != OIR_SEMANTIC_CONVERSION_FLAG_EXPLICIT)) {
            return OIR_STATUS_INVALID_SECTION;
        }

        if (i != 0u &&
            (node_id < previous_node_id ||
                (node_id == previous_node_id &&
                    operand_ordinal <= previous_operand))) {
            return OIR_STATUS_INVALID_SECTION;
        }
        previous_node_id = node_id;
        previous_operand = operand_ordinal;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_conversions_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSemanticConversionsView* out_view
) {
    uint32_t count;
    uint32_t record_size;
    uint64_t expected_length;
    OirStatus status;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE) {
        return OIR_STATUS_INVALID_SECTION;
    }

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    if (record_size != OIR_SEMANTIC_CONVERSION_RECORD_SIZE ||
        count > OIR_SEMANTIC_MAX_CONVERSION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }

    expected_length = (uint64_t)OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
        (uint64_t)count * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
    if (expected_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if (expected_length != (uint64_t)payload_length) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    status = validate_conversion_records(
        payload + OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE, count);
    if (status != OIR_STATUS_OK) return status;

    out_view->bytes = payload;
    out_view->length = payload_length;
    out_view->record_count = count;
    out_view->records_offset = OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE;
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_conversions_get(
    const OirSemanticConversionsView* view,
    uint32_t index,
    OirSemanticConversion* out_conversion
) {
    const uint8_t* record;
    uint64_t expected_length;

    if (!view || !view->bytes || !out_conversion) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (view->records_offset != OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE ||
        view->record_count > OIR_SEMANTIC_MAX_CONVERSION_COUNT) {
        return OIR_STATUS_INVALID_SECTION;
    }
    expected_length = (uint64_t)OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
        (uint64_t)view->record_count * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
    if (expected_length > (uint64_t)SIZE_MAX ||
        expected_length != (uint64_t)view->length) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    record = view->bytes + view->records_offset +
        (size_t)index * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
    out_conversion->node_id = read_u32_le(record);
    out_conversion->operand_ordinal = read_u32_le(record + 4);
    out_conversion->source_type_id = read_u32_le(record + 8);
    out_conversion->destination_type_id = read_u32_le(record + 12);
    out_conversion->kind = (uint16_t)((uint16_t)record[16] |
        ((uint16_t)record[17] << 8));
    out_conversion->flags = (uint16_t)((uint16_t)record[18] |
        ((uint16_t)record[19] << 8));
    return OIR_STATUS_OK;
}

OirStatus oir_semantic_conversions_write(
    uint8_t* output,
    size_t capacity,
    const OirSemanticConversion* conversions,
    uint32_t conversion_count,
    size_t* out_written
) {
    uint64_t total_length;
    uint32_t i;
    uint32_t previous_node_id = 0u;
    uint32_t previous_operand = 0u;

    if (out_written) *out_written = 0u;
    if (!output || !out_written ||
        (!conversions && conversion_count != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (conversion_count > OIR_SEMANTIC_MAX_CONVERSION_COUNT) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0u; i < conversion_count; ++i) {
        const OirSemanticConversion* conversion = &conversions[i];
        if (conversion->node_id == 0u ||
            conversion->source_type_id == 0u ||
            conversion->destination_type_id == 0u ||
            conversion->kind < OIR_SEMANTIC_CONVERSION_IDENTITY ||
            conversion->kind > OIR_SEMANTIC_CONVERSION_HARD_TYPE ||
            (conversion->flags != OIR_SEMANTIC_CONVERSION_FLAG_IMPLICIT &&
                conversion->flags != OIR_SEMANTIC_CONVERSION_FLAG_EXPLICIT)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (i != 0u &&
            (conversion->node_id < previous_node_id ||
                (conversion->node_id == previous_node_id &&
                    conversion->operand_ordinal <= previous_operand))) {
            return OIR_STATUS_INVALID_SECTION;
        }
        previous_node_id = conversion->node_id;
        previous_operand = conversion->operand_ordinal;
    }

    total_length = (uint64_t)OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
        (uint64_t)conversion_count * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
    if (total_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
    if ((size_t)total_length > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    write_u32_le(output, conversion_count);
    write_u32_le(output + 4, OIR_SEMANTIC_CONVERSION_RECORD_SIZE);
    for (i = 0u; i < conversion_count; ++i) {
        uint8_t* record = output + OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE +
            (size_t)i * OIR_SEMANTIC_CONVERSION_RECORD_SIZE;
        write_u32_le(record, conversions[i].node_id);
        write_u32_le(record + 4, conversions[i].operand_ordinal);
        write_u32_le(record + 8, conversions[i].source_type_id);
        write_u32_le(record + 12, conversions[i].destination_type_id);
        record[16] = (uint8_t)(conversions[i].kind & 0xffu);
        record[17] = (uint8_t)((conversions[i].kind >> 8) & 0xffu);
        record[18] = (uint8_t)(conversions[i].flags & 0xffu);
        record[19] = (uint8_t)((conversions[i].flags >> 8) & 0xffu);
    }

    *out_written = (size_t)total_length;
    return OIR_STATUS_OK;
}
