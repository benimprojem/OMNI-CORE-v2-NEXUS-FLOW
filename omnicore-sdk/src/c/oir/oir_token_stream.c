#include "oir_token_stream.h"
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

static uint64_t read_u64_le(const uint8_t* p) {
    return (uint64_t)read_u32_le(p) |
        ((uint64_t)read_u32_le(p + 4) << 32);
}

static void write_u16_le(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_u64_le(uint8_t* p, uint64_t value) {
    write_u32_le(p, (uint32_t)value);
    write_u32_le(p + 4, (uint32_t)(value >> 32));
}

static OirStatus validate_record(
    uint16_t token_id,
    uint16_t flags,
    uint32_t diagnostic_id,
    uint64_t source_offset,
    uint32_t lexeme_length,
    uint32_t line,
    uint32_t column,
    uint64_t source_length,
    uint32_t diagnostic_count,
    uint32_t index,
    uint32_t token_count,
    uint64_t* previous_source_end
) {
    OmniTokenType frontend_token;

    if (!oir_token_id_to_omni(token_id, &frontend_token) ||
        flags != 0 || line == 0 || column == 0 || !previous_source_end) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (token_id == 1u) { /* TOKEN_EOF */
        if (index + 1u != token_count || lexeme_length != 0 ||
            source_offset != source_length || diagnostic_id != 0 ||
            source_offset < *previous_source_end) {
            return OIR_STATUS_INVALID_SECTION;
        }
        *previous_source_end = source_offset;
        return OIR_STATUS_OK;
    }

    if (index + 1u == token_count) return OIR_STATUS_MISSING_PAYLOAD;

    if (token_id == 2u) { /* TOKEN_ERROR */
        if (diagnostic_id == 0 || diagnostic_id > diagnostic_count) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (source_offset == OIR_TOKEN_STREAM_NO_SOURCE_OFFSET) {
            if (lexeme_length != 0) return OIR_STATUS_INVALID_SECTION;
            return OIR_STATUS_OK;
        }
    } else if (diagnostic_id != 0 ||
        source_offset == OIR_TOKEN_STREAM_NO_SOURCE_OFFSET ||
        lexeme_length == 0) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (source_offset > source_length ||
        (uint64_t)lexeme_length > source_length - source_offset ||
        source_offset < *previous_source_end) {
        return OIR_STATUS_INVALID_SECTION;
    }
    *previous_source_end = source_offset + lexeme_length;
    return OIR_STATUS_OK;
}

static OirStatus decode_record(
    const uint8_t* p,
    OirTokenStreamRecordView* out_record
) {
    if (!p || !out_record) return OIR_STATUS_INVALID_ARGUMENT;
    out_record->token_id = read_u16_le(p);
    out_record->flags = read_u16_le(p + 2);
    out_record->diagnostic_id = read_u32_le(p + 4);
    out_record->source_offset = read_u64_le(p + 8);
    out_record->lexeme_length = read_u32_le(p + 16);
    out_record->line = read_u32_le(p + 20);
    out_record->column = read_u32_le(p + 24);
    if (read_u32_le(p + 28) != 0) return OIR_STATUS_INVALID_SECTION;
    return OIR_STATUS_OK;
}

OirStatus oir_token_stream_open(
    const uint8_t* payload,
    size_t payload_length,
    uint32_t expected_source_id,
    uint64_t source_length,
    uint32_t diagnostic_count,
    OirTokenStreamPayloadView* out_view
) {
    uint32_t count, source_id, record_size, flags, i;
    uint64_t records_bytes;
    size_t expected_length;
    uint64_t previous_source_end = 0;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_TOKEN_STREAM_HEADER_SIZE) return OIR_STATUS_INVALID_HEADER;

    count = read_u32_le(payload);
    source_id = read_u32_le(payload + 4);
    record_size = read_u32_le(payload + 8);
    flags = read_u32_le(payload + 12);

    if (count == 0 || record_size != OIR_TOKEN_STREAM_RECORD_SIZE || flags != 0) {
        return OIR_STATUS_INVALID_HEADER;
    }
    if (source_id != expected_source_id) return OIR_STATUS_INVALID_SECTION;

    records_bytes = (uint64_t)count * OIR_TOKEN_STREAM_RECORD_SIZE;
    if (records_bytes > (uint64_t)(SIZE_MAX - OIR_TOKEN_STREAM_HEADER_SIZE)) {
        return OIR_STATUS_OVERFLOW;
    }
    expected_length = OIR_TOKEN_STREAM_HEADER_SIZE + (size_t)records_bytes;
    if (expected_length != payload_length) return OIR_STATUS_LENGTH_MISMATCH;

    out_view->payload = payload;
    out_view->payload_length = payload_length;
    out_view->token_count = count;
    out_view->source_id = source_id;
    out_view->source_length = source_length;
    out_view->diagnostic_count = diagnostic_count;
    out_view->records_offset = OIR_TOKEN_STREAM_HEADER_SIZE;

    for (i = 0; i < count; ++i) {
        OirTokenStreamRecordView record;
        const uint8_t* p = payload + OIR_TOKEN_STREAM_HEADER_SIZE +
            (size_t)i * OIR_TOKEN_STREAM_RECORD_SIZE;
        OirStatus status = decode_record(p, &record);
        if (status == OIR_STATUS_OK) {
            status = validate_record(record.token_id, record.flags,
                record.diagnostic_id, record.source_offset,
                record.lexeme_length, record.line, record.column,
                source_length, diagnostic_count, i, count,
                &previous_source_end);
        }
        if (status != OIR_STATUS_OK) {
            memset(out_view, 0, sizeof(*out_view));
            return status;
        }
    }
    return OIR_STATUS_OK;
}

OirStatus oir_token_stream_get(
    const OirTokenStreamPayloadView* view,
    uint32_t index,
    OirTokenStreamRecordView* out_record
) {
    const uint8_t* p;
    OirStatus status;
    if (!view || !view->payload || !out_record) return OIR_STATUS_INVALID_ARGUMENT;
    if (index >= view->token_count) return OIR_STATUS_INVALID_ARGUMENT;
    p = view->payload + view->records_offset +
        (size_t)index * OIR_TOKEN_STREAM_RECORD_SIZE;
    status = decode_record(p, out_record);
    return status;
}

OirStatus oir_token_stream_write(
    uint8_t* output,
    size_t capacity,
    uint32_t source_id,
    uint64_t source_length,
    uint32_t diagnostic_count,
    const OirTokenStreamInput* tokens,
    uint32_t token_count,
    size_t* out_written
) {
    uint64_t records_bytes;
    size_t total;
    uint64_t previous_source_end = 0;
    uint32_t i;

    if (out_written) *out_written = 0;
    if (!output || !out_written || !tokens) return OIR_STATUS_INVALID_ARGUMENT;
    if (token_count == 0) return OIR_STATUS_MISSING_PAYLOAD;

    records_bytes = (uint64_t)token_count * OIR_TOKEN_STREAM_RECORD_SIZE;
    if (records_bytes > (uint64_t)(SIZE_MAX - OIR_TOKEN_STREAM_HEADER_SIZE)) {
        return OIR_STATUS_OVERFLOW;
    }
    total = OIR_TOKEN_STREAM_HEADER_SIZE + (size_t)records_bytes;
    if (total > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    for (i = 0; i < token_count; ++i) {
        const OirTokenStreamInput* token = &tokens[i];
        OirStatus status = validate_record(token->token_id, token->flags,
            token->diagnostic_id, token->source_offset,
            token->lexeme_length, token->line, token->column,
            source_length, diagnostic_count, i, token_count,
            &previous_source_end);
        if (status != OIR_STATUS_OK) return status;
    }

    write_u32_le(output, token_count);
    write_u32_le(output + 4, source_id);
    write_u32_le(output + 8, OIR_TOKEN_STREAM_RECORD_SIZE);
    write_u32_le(output + 12, 0);

    for (i = 0; i < token_count; ++i) {
        const OirTokenStreamInput* token = &tokens[i];
        uint8_t* p = output + OIR_TOKEN_STREAM_HEADER_SIZE +
            (size_t)i * OIR_TOKEN_STREAM_RECORD_SIZE;
        write_u16_le(p, token->token_id);
        write_u16_le(p + 2, token->flags);
        write_u32_le(p + 4, token->diagnostic_id);
        write_u64_le(p + 8, token->source_offset);
        write_u32_le(p + 16, token->lexeme_length);
        write_u32_le(p + 20, token->line);
        write_u32_le(p + 24, token->column);
        write_u32_le(p + 28, 0);
    }
    *out_written = total;
    return OIR_STATUS_OK;
}
