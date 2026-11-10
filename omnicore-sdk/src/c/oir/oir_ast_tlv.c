#include "oir_ast_tlv.h"
#include "oir_token_map.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static void write_u16_le(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)(value >> 24);
}

static int value_length_is_valid(uint8_t type, const uint8_t* value, uint32_t length) {
    switch (type) {
        case OIR_AST_VALUE_NULL: return length == 0u;
        case OIR_AST_VALUE_BOOL: return length == 1u && value && value[0] <= 1u;
        case OIR_AST_VALUE_U32:
        case OIR_AST_VALUE_I32:
        case OIR_AST_VALUE_NODE_REF: return length == 4u;
        case OIR_AST_VALUE_U64:
        case OIR_AST_VALUE_I64:
        case OIR_AST_VALUE_F64: return length == 8u;
        case OIR_AST_VALUE_TOKEN_ID: {
            OmniTokenType token;
            return length == 2u && value &&
                read_u16_le(value) >= 1u &&
                read_u16_le(value) <= OIR_AST_TOKEN_ID_MAX &&
                oir_token_id_to_omni(read_u16_le(value), &token);
        }
        case OIR_AST_VALUE_STRING_REF: return length == 8u;
        default: return 0;
    }
}

void oir_ast_tlv_writer_init(OirAstTlvWriter* writer, uint8_t* buffer, size_t capacity) {
    if (!writer) return;
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->used = 0;
    writer->last_field_id = 0;
}

OirStatus oir_ast_tlv_write(
    OirAstTlvWriter* writer,
    uint16_t field_id,
    uint8_t value_type,
    uint8_t flags,
    const uint8_t* value,
    uint32_t value_length
) {
    size_t required;
    uint8_t* p;

    if (!writer || !writer->buffer || field_id == 0u ||
        (flags & (uint8_t)~OIR_AST_TLV_KNOWN_FLAGS) != 0u ||
        (!value && value_length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (field_id <= writer->last_field_id) return OIR_STATUS_INVALID_SECTION;
    if (!value_length_is_valid(value_type, value, value_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if ((size_t)value_length > SIZE_MAX - OIR_AST_TLV_HEADER_SIZE) {
        return OIR_STATUS_OVERFLOW;
    }
    required = OIR_AST_TLV_HEADER_SIZE + (size_t)value_length;
    if (writer->used > writer->capacity || required > writer->capacity - writer->used) {
        return OIR_STATUS_BUFFER_TOO_SMALL;
    }

    p = writer->buffer + writer->used;
    write_u16_le(p, field_id);
    p[2] = value_type;
    p[3] = flags;
    write_u32_le(p + 4, value_length);
    if (value_length) memmove(p + OIR_AST_TLV_HEADER_SIZE, value, value_length);

    writer->used += required;
    writer->last_field_id = field_id;
    return OIR_STATUS_OK;
}

void oir_ast_tlv_reader_init(OirAstTlvReader* reader, const uint8_t* buffer, size_t length) {
    if (!reader) return;
    reader->buffer = buffer;
    reader->length = length;
    reader->offset = 0;
    reader->last_field_id = 0;
}

OirStatus oir_ast_tlv_reader_next(
    OirAstTlvReader* reader,
    OirAstTlvField* out_field,
    int* out_has_field
) {
    const uint8_t* p;
    uint16_t field_id;
    uint8_t type;
    uint8_t flags;
    uint32_t value_length;
    size_t remaining;

    if (!reader || !out_field || !out_has_field ||
        (!reader->buffer && reader->length != 0u)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    memset(out_field, 0, sizeof(*out_field));
    *out_has_field = 0;
    if (reader->offset == reader->length) return OIR_STATUS_OK;
    if (reader->offset > reader->length) return OIR_STATUS_INVALID_SECTION;

    remaining = reader->length - reader->offset;
    if (remaining < OIR_AST_TLV_HEADER_SIZE) return OIR_STATUS_INVALID_SECTION;

    p = reader->buffer + reader->offset;
    field_id = read_u16_le(p);
    type = p[2];
    flags = p[3];
    value_length = read_u32_le(p + 4);
    if (field_id == 0u || field_id <= reader->last_field_id ||
        (flags & (uint8_t)~OIR_AST_TLV_KNOWN_FLAGS) != 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if ((size_t)value_length > remaining - OIR_AST_TLV_HEADER_SIZE) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }
    if (!value_length_is_valid(type, p + OIR_AST_TLV_HEADER_SIZE, value_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    out_field->field_id = field_id;
    out_field->value_type = type;
    out_field->flags = flags;
    out_field->value = p + OIR_AST_TLV_HEADER_SIZE;
    out_field->value_length = value_length;

    reader->offset += OIR_AST_TLV_HEADER_SIZE + (size_t)value_length;
    reader->last_field_id = field_id;
    *out_has_field = 1;
    return OIR_STATUS_OK;
}

OirStatus oir_ast_tlv_validate(const uint8_t* buffer, size_t length) {
    OirAstTlvReader reader;
    OirAstTlvField field;
    int has_field = 0;
    OirStatus status;

    if (!buffer && length != 0u) return OIR_STATUS_INVALID_ARGUMENT;
    oir_ast_tlv_reader_init(&reader, buffer, length);
    do {
        status = oir_ast_tlv_reader_next(&reader, &field, &has_field);
        if (status != OIR_STATUS_OK) return status;
    } while (has_field);
    return OIR_STATUS_OK;
}
