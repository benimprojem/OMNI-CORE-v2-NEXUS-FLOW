#include "oir_ast_node_table.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static int valid_utf8(const uint8_t* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint8_t a = s[i];
        uint32_t cp;
        size_t extra;
        if (a == 0) return 0;
        if (a <= 0x7fu) { ++i; continue; }
        if (a >= 0xc2u && a <= 0xdfu) { cp = a & 0x1fu; extra = 1; }
        else if (a >= 0xe0u && a <= 0xefu) { cp = a & 0x0fu; extra = 2; }
        else if (a >= 0xf0u && a <= 0xf4u) { cp = a & 0x07u; extra = 3; }
        else return 0;
        if (extra > n - i - 1u) return 0;
        for (size_t j = 1; j <= extra; ++j) {
            uint8_t b = s[i + j];
            if ((b & 0xc0u) != 0x80u) return 0;
            cp = (cp << 6) | (uint32_t)(b & 0x3fu);
        }
        if ((extra == 1u && cp < 0x80u) ||
            (extra == 2u && cp < 0x800u) ||
            (extra == 3u && cp < 0x10000u) ||
            (cp >= 0xd800u && cp <= 0xdfffu) || cp > 0x10ffffu) return 0;
        i += extra + 1u;
    }
    return 1;
}

static OirStatus validate_node_fields(
    const uint8_t* fields, uint32_t fields_length,
    const uint8_t* strings, uint32_t strings_length,
    uint32_t node_count
) {
    OirAstTlvReader reader;
    OirAstTlvField field;
    int has_field = 0;
    OirStatus status;
    oir_ast_tlv_reader_init(&reader, fields, fields_length);
    do {
        status = oir_ast_tlv_reader_next(&reader, &field, &has_field);
        if (status != OIR_STATUS_OK) return status;
        if (!has_field) break;
        if (field.value_type == OIR_AST_VALUE_NODE_REF) {
            uint32_t id = read_u32_le(field.value);
            if (id > node_count) return OIR_STATUS_INVALID_SECTION;
        } else if (field.value_type == OIR_AST_VALUE_STRING_REF) {
            uint32_t offset = read_u32_le(field.value);
            uint32_t length = read_u32_le(field.value + 4);
            if (length > OIR_AST_STRING_MAX ||
                offset > strings_length || length > strings_length - offset) {
                return OIR_STATUS_INVALID_SECTION;
            }
            if (length && !valid_utf8(strings + offset, length)) {
                return OIR_STATUS_INVALID_SECTION;
            }
        }
    } while (has_field);
    return OIR_STATUS_OK;
}

static OirStatus validate_host_records(
    const OirAstNodeRecord* records, uint32_t count, uint32_t root,
    const uint8_t* fields, uint32_t fields_length,
    const uint8_t* strings, uint32_t strings_length
) {
    uint32_t expected_offset = 0;
    uint32_t i;
    if (!records || count == 0u || count > OIR_AST_NODE_COUNT_MAX ||
        root != 1u || (!fields && fields_length) || (!strings && strings_length)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (records[0].node_kind_id != 1u) return OIR_STATUS_INVALID_SECTION;
    for (i = 0; i < count; ++i) {
        const OirAstNodeRecord* r = &records[i];
        if (r->node_id != i + 1u || r->node_kind_id == 0u ||
            r->node_kind_id > OIR_AST_NODE_KIND_MAX || r->flags != 0u ||
            ((r->start_line == 0u) != (r->start_column == 0u)) ||
            r->next_node_id > count || r->field_data_offset != expected_offset ||
            r->field_data_length > fields_length - expected_offset) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (validate_node_fields(fields ? fields + expected_offset : NULL,
                r->field_data_length, strings, strings_length, count) != OIR_STATUS_OK) {
            return OIR_STATUS_INVALID_SECTION;
        }
        expected_offset += r->field_data_length;
    }
    if (expected_offset != fields_length) return OIR_STATUS_LENGTH_MISMATCH;
    if (strings_length && !valid_utf8(strings, strings_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_ast_node_table_open(
    const uint8_t* payload, size_t payload_length, OirAstNodeTableView* out_view
) {
    uint32_t count, root, record_size, records_bytes, fields_bytes, strings_bytes;
    size_t expected, records_offset, fields_offset, strings_offset;
    uint64_t expected64;
    uint32_t i, expected_field_offset = 0;
    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_AST_PAYLOAD_HEADER_SIZE) return OIR_STATUS_INVALID_HEADER;
    count = read_u32_le(payload);
    root = read_u32_le(payload + 4);
    record_size = read_u32_le(payload + 8);
    records_bytes = read_u32_le(payload + 12);
    fields_bytes = read_u32_le(payload + 16);
    strings_bytes = read_u32_le(payload + 20);
    if (read_u32_le(payload + 24) != 0u || read_u32_le(payload + 28) != 0u ||
        count == 0u || count > OIR_AST_NODE_COUNT_MAX || root != 1u ||
        record_size != OIR_AST_NODE_RECORD_SIZE ||
        (uint64_t)count * OIR_AST_NODE_RECORD_SIZE != records_bytes) {
        return OIR_STATUS_INVALID_HEADER;
    }
    expected64 = (uint64_t)OIR_AST_PAYLOAD_HEADER_SIZE + records_bytes +
        fields_bytes + strings_bytes;
#if SIZE_MAX < UINT64_MAX
    if (expected64 > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
#endif
    expected = (size_t)expected64;
    if (expected != payload_length) return OIR_STATUS_LENGTH_MISMATCH;
    records_offset = OIR_AST_PAYLOAD_HEADER_SIZE;
    fields_offset = records_offset + records_bytes;
    strings_offset = fields_offset + fields_bytes;

    if (read_u16_le(payload + records_offset + 4) != 1u) {
        return OIR_STATUS_INVALID_SECTION;
    }
    for (i = 0; i < count; ++i) {
        const uint8_t* p = payload + records_offset + (size_t)i * OIR_AST_NODE_RECORD_SIZE;
        uint32_t id = read_u32_le(p);
        uint16_t kind = read_u16_le(p + 4);
        uint16_t flags = read_u16_le(p + 6);
        uint32_t line = read_u32_le(p + 8);
        uint32_t column = read_u32_le(p + 12);
        uint32_t next = read_u32_le(p + 16);
        uint32_t field_offset = read_u32_le(p + 20);
        uint32_t field_length = read_u32_le(p + 24);
        uint32_t reserved = read_u32_le(p + 28);
        if (id != i + 1u || kind == 0u || kind > OIR_AST_NODE_KIND_MAX ||
            flags != 0u || reserved != 0u ||
            ((line == 0u) != (column == 0u)) || next > count ||
            field_offset != expected_field_offset ||
            field_length > fields_bytes - expected_field_offset) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (validate_node_fields(payload + fields_offset + field_offset,
                field_length, payload + strings_offset, strings_bytes, count) != OIR_STATUS_OK) {
            return OIR_STATUS_INVALID_SECTION;
        }
        expected_field_offset += field_length;
    }
    if (expected_field_offset != fields_bytes) return OIR_STATUS_LENGTH_MISMATCH;
    if (strings_bytes && !valid_utf8(payload + strings_offset, strings_bytes)) {
        return OIR_STATUS_INVALID_SECTION;
    }
    out_view->node_count = count;
    out_view->root_node_id = root;
    out_view->node_records = payload + records_offset;
    out_view->field_data = payload + fields_offset;
    out_view->field_data_length = fields_bytes;
    out_view->string_table = payload + strings_offset;
    out_view->string_table_length = strings_bytes;
    return OIR_STATUS_OK;
}

OirStatus oir_ast_node_table_get(
    const OirAstNodeTableView* view, uint32_t index, OirAstNodeRecord* out_record
) {
    const uint8_t* p;
    if (!view || !out_record || !view->node_records || index >= view->node_count) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    p = view->node_records + (size_t)index * OIR_AST_NODE_RECORD_SIZE;
    out_record->node_id = read_u32_le(p);
    out_record->node_kind_id = read_u16_le(p + 4);
    out_record->flags = read_u16_le(p + 6);
    out_record->start_line = read_u32_le(p + 8);
    out_record->start_column = read_u32_le(p + 12);
    out_record->next_node_id = read_u32_le(p + 16);
    out_record->field_data_offset = read_u32_le(p + 20);
    out_record->field_data_length = read_u32_le(p + 24);
    return OIR_STATUS_OK;
}

OirStatus oir_ast_node_table_write(
    uint8_t* output, size_t capacity, const OirAstNodeRecord* records,
    uint32_t node_count, uint32_t root_node_id,
    const uint8_t* field_data, uint32_t field_data_length,
    const uint8_t* string_table, uint32_t string_table_length,
    size_t* out_written
) {
    size_t total, records_bytes;
    uint64_t total64;
    uint32_t i;
    OirStatus status;
    if (out_written) *out_written = 0;
    if (!output || !out_written || !records ||
        (!field_data && field_data_length) || (!string_table && string_table_length)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (node_count == 0u || node_count > OIR_AST_NODE_COUNT_MAX) return OIR_STATUS_INVALID_ARGUMENT;
    records_bytes = (size_t)node_count * OIR_AST_NODE_RECORD_SIZE;
    total64 = (uint64_t)OIR_AST_PAYLOAD_HEADER_SIZE + records_bytes +
        field_data_length + string_table_length;
#if SIZE_MAX < UINT64_MAX
    if (total64 > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
#endif
    total = (size_t)total64;
    if (total > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;
    status = validate_host_records(records, node_count, root_node_id,
        field_data, field_data_length, string_table, string_table_length);
    if (status != OIR_STATUS_OK) return status;

    write_u32_le(output, node_count);
    write_u32_le(output + 4, root_node_id);
    write_u32_le(output + 8, OIR_AST_NODE_RECORD_SIZE);
    write_u32_le(output + 12, (uint32_t)records_bytes);
    write_u32_le(output + 16, field_data_length);
    write_u32_le(output + 20, string_table_length);
    write_u32_le(output + 24, 0u);
    write_u32_le(output + 28, 0u);
    for (i = 0; i < node_count; ++i) {
        uint8_t* p = output + OIR_AST_PAYLOAD_HEADER_SIZE +
            (size_t)i * OIR_AST_NODE_RECORD_SIZE;
        const OirAstNodeRecord* r = &records[i];
        write_u32_le(p, r->node_id);
        write_u16_le(p + 4, r->node_kind_id);
        write_u16_le(p + 6, r->flags);
        write_u32_le(p + 8, r->start_line);
        write_u32_le(p + 12, r->start_column);
        write_u32_le(p + 16, r->next_node_id);
        write_u32_le(p + 20, r->field_data_offset);
        write_u32_le(p + 24, r->field_data_length);
        write_u32_le(p + 28, 0u);
    }
    if (field_data_length) memmove(output + OIR_AST_PAYLOAD_HEADER_SIZE + records_bytes,
        field_data, field_data_length);
    if (string_table_length) memmove(output + OIR_AST_PAYLOAD_HEADER_SIZE +
        records_bytes + field_data_length, string_table, string_table_length);
    *out_written = total;
    return OIR_STATUS_OK;
}
