#include "oir_diagnostics.h"

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

static int valid_string(
    const uint8_t* strings,
    uint32_t string_length,
    uint32_t offset,
    uint32_t length
) {
    if (offset == OIR_DIAGNOSTICS_NO_HINT ||
        (uint64_t)offset + (uint64_t)length >= string_length) {
        return 0;
    }
    if (strings[offset + length] != 0) return 0;
    return memchr(strings + offset, 0, length) == NULL;
}

static OirStatus read_record(
    const OirDiagnosticsPayloadView* view,
    uint32_t index,
    OirDiagnosticEntryView* out_entry
) {
    const uint8_t* p;
    const uint8_t* strings;
    uint8_t span_flags;
    uint16_t reserved;
    uint32_t message_offset, hint_offset;

    if (!view || !out_entry || !view->payload) return OIR_STATUS_INVALID_ARGUMENT;
    if (index >= view->record_count) return OIR_STATUS_INVALID_ARGUMENT;

    p = view->payload + view->records_offset +
        (size_t)index * OIR_DIAGNOSTICS_RECORD_SIZE;
    strings = view->payload + view->strings_offset;
    memset(out_entry, 0, sizeof(*out_entry));

    out_entry->code = read_u32_le(p);
    out_entry->stage_id = read_u32_le(p + 4);
    out_entry->message_id = read_u32_le(p + 8);
    out_entry->severity = p[12];
    span_flags = p[13];
    reserved = read_u16_le(p + 14);
    out_entry->span.source_id = read_u32_le(p + 16);
    out_entry->span.start_offset = read_u64_le(p + 20);
    out_entry->span.end_offset = read_u64_le(p + 28);
    out_entry->span.start_line = read_u32_le(p + 36);
    out_entry->span.start_column = read_u32_le(p + 40);
    out_entry->span.end_line = read_u32_le(p + 44);
    out_entry->span.end_column = read_u32_le(p + 48);
    message_offset = read_u32_le(p + 52);
    out_entry->message_length = read_u32_le(p + 56);
    hint_offset = read_u32_le(p + 60);
    out_entry->hint_length = read_u32_le(p + 64);

    if (out_entry->code == 0 ||
        out_entry->severity < OCC_DIAG_NOTE ||
        out_entry->severity > OCC_DIAG_FATAL ||
        span_flags > 1u || reserved != 0 ||
        out_entry->message_length == 0 ||
        !valid_string(strings, view->string_table_length,
            message_offset, out_entry->message_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    if (out_entry->hint_length == 0) {
        if (hint_offset != OIR_DIAGNOSTICS_NO_HINT) return OIR_STATUS_INVALID_SECTION;
    } else if (!valid_string(strings, view->string_table_length,
        hint_offset, out_entry->hint_length)) {
        return OIR_STATUS_INVALID_SECTION;
    }

    out_entry->span.has_location = span_flags;
    if (span_flags) {
        if (out_entry->span.end_offset < out_entry->span.start_offset ||
            out_entry->span.start_line == 0 ||
            out_entry->span.start_column == 0 ||
            out_entry->span.end_line == 0 ||
            out_entry->span.end_column == 0) {
            return OIR_STATUS_INVALID_SECTION;
        }
    } else if (out_entry->span.source_id != 0 ||
        out_entry->span.start_offset != 0 ||
        out_entry->span.end_offset != 0 ||
        out_entry->span.start_line != 0 ||
        out_entry->span.start_column != 0 ||
        out_entry->span.end_line != 0 ||
        out_entry->span.end_column != 0) {
        return OIR_STATUS_INVALID_SECTION;
    }

    out_entry->message = (const char*)(strings + message_offset);
    out_entry->hint = out_entry->hint_length
        ? (const char*)(strings + hint_offset) : NULL;
    return OIR_STATUS_OK;
}

OirStatus oir_diagnostics_payload_open(
    const uint8_t* payload,
    size_t payload_length,
    OirDiagnosticsPayloadView* out_view
) {
    uint32_t count, record_size, string_length, flags;
    size_t records_bytes, strings_offset, expected_length;
    uint32_t i;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_DIAGNOSTICS_HEADER_SIZE) return OIR_STATUS_INVALID_HEADER;

    count = read_u32_le(payload);
    record_size = read_u32_le(payload + 4);
    string_length = read_u32_le(payload + 8);
    flags = read_u32_le(payload + 12);
    if (record_size != OIR_DIAGNOSTICS_RECORD_SIZE || flags != 0) {
        return OIR_STATUS_INVALID_HEADER;
    }
    if ((size_t)count >
        (SIZE_MAX - OIR_DIAGNOSTICS_HEADER_SIZE) / OIR_DIAGNOSTICS_RECORD_SIZE) {
        return OIR_STATUS_OVERFLOW;
    }
    records_bytes = (size_t)count * OIR_DIAGNOSTICS_RECORD_SIZE;
    strings_offset = OIR_DIAGNOSTICS_HEADER_SIZE + records_bytes;
    if ((size_t)string_length > SIZE_MAX - strings_offset) return OIR_STATUS_OVERFLOW;
    expected_length = strings_offset + (size_t)string_length;
    if (expected_length != payload_length) return OIR_STATUS_LENGTH_MISMATCH;

    out_view->payload = payload;
    out_view->payload_length = payload_length;
    out_view->record_count = count;
    out_view->string_table_length = string_length;
    out_view->records_offset = OIR_DIAGNOSTICS_HEADER_SIZE;
    out_view->strings_offset = strings_offset;

    for (i = 0; i < count; ++i) {
        OirDiagnosticEntryView entry;
        OirStatus status = read_record(out_view, i, &entry);
        if (status != OIR_STATUS_OK) {
            memset(out_view, 0, sizeof(*out_view));
            return status;
        }
    }
    return OIR_STATUS_OK;
}

OirStatus oir_diagnostics_payload_get(
    const OirDiagnosticsPayloadView* view,
    uint32_t index,
    OirDiagnosticEntryView* out_entry
) {
    return read_record(view, index, out_entry);
}

OirStatus oir_diagnostics_payload_write(
    uint8_t* output,
    size_t capacity,
    const OccDiagnosticCollector* collector,
    size_t* out_written
) {
    size_t records_bytes, strings_offset, total;
    size_t i;

    if (out_written) *out_written = 0;
    if (!output || !collector || !out_written) return OIR_STATUS_INVALID_ARGUMENT;
    if (collector->allocation_failed ||
        (collector->count && !collector->records) ||
        collector->count > UINT32_MAX ||
        collector->text_length > UINT32_MAX ||
        (collector->text_length && !collector->text_arena)) {
        return OIR_STATUS_INVALID_SECTION;
    }
    if (collector->count >
        (SIZE_MAX - OIR_DIAGNOSTICS_HEADER_SIZE) / OIR_DIAGNOSTICS_RECORD_SIZE) {
        return OIR_STATUS_OVERFLOW;
    }
    records_bytes = collector->count * OIR_DIAGNOSTICS_RECORD_SIZE;
    strings_offset = OIR_DIAGNOSTICS_HEADER_SIZE + records_bytes;
    if (collector->text_length > SIZE_MAX - strings_offset) return OIR_STATUS_OVERFLOW;
    total = strings_offset + collector->text_length;
    if (total > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    for (i = 0; i < collector->count; ++i) {
        const OccDiagnosticRecord* r = &collector->records[i];
        if (r->code == 0 ||
            r->severity < OCC_DIAG_NOTE || r->severity > OCC_DIAG_FATAL ||
            r->message_length == 0 ||
            !valid_string((const uint8_t*)collector->text_arena,
                (uint32_t)collector->text_length,
                r->message_offset, r->message_length)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (r->hint_length == 0) {
            /* The in-memory collector stores an unused offset for empty hints. */
        } else if (!valid_string((const uint8_t*)collector->text_arena,
            (uint32_t)collector->text_length,
            r->hint_offset, r->hint_length)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (r->span.has_location > 1u) return OIR_STATUS_INVALID_SECTION;
        if (r->span.has_location) {
            if (r->span.end_offset < r->span.start_offset ||
                r->span.start_line == 0 || r->span.start_column == 0 ||
                r->span.end_line == 0 || r->span.end_column == 0) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } else if (r->span.source_id != 0 || r->span.start_offset != 0 ||
            r->span.end_offset != 0 || r->span.start_line != 0 ||
            r->span.start_column != 0 || r->span.end_line != 0 ||
            r->span.end_column != 0) {
            return OIR_STATUS_INVALID_SECTION;
        }
    }

    write_u32_le(output, (uint32_t)collector->count);
    write_u32_le(output + 4, OIR_DIAGNOSTICS_RECORD_SIZE);
    write_u32_le(output + 8, (uint32_t)collector->text_length);
    write_u32_le(output + 12, 0);

    for (i = 0; i < collector->count; ++i) {
        const OccDiagnosticRecord* r = &collector->records[i];
        uint8_t* p = output + OIR_DIAGNOSTICS_HEADER_SIZE +
            i * OIR_DIAGNOSTICS_RECORD_SIZE;
        write_u32_le(p, r->code);
        write_u32_le(p + 4, r->stage_id);
        write_u32_le(p + 8, r->message_id);
        p[12] = r->severity;
        p[13] = r->span.has_location ? 1u : 0u;
        write_u16_le(p + 14, 0);
        write_u32_le(p + 16, r->span.has_location ? r->span.source_id : 0u);
        write_u64_le(p + 20, r->span.has_location ? r->span.start_offset : 0u);
        write_u64_le(p + 28, r->span.has_location ? r->span.end_offset : 0u);
        write_u32_le(p + 36, r->span.has_location ? r->span.start_line : 0u);
        write_u32_le(p + 40, r->span.has_location ? r->span.start_column : 0u);
        write_u32_le(p + 44, r->span.has_location ? r->span.end_line : 0u);
        write_u32_le(p + 48, r->span.has_location ? r->span.end_column : 0u);
        write_u32_le(p + 52, r->message_offset);
        write_u32_le(p + 56, r->message_length);
        write_u32_le(p + 60, r->hint_length ? r->hint_offset : OIR_DIAGNOSTICS_NO_HINT);
        write_u32_le(p + 64, r->hint_length);
    }

    if (collector->text_length) {
        memmove(output + strings_offset, collector->text_arena, collector->text_length);
    }
    *out_written = total;
    return OIR_STATUS_OK;
}

