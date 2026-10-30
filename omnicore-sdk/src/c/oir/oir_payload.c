#include "oir_payload.h"

#include <string.h>

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
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void write_u32_le(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void write_u64_le(uint8_t* p, uint64_t value) {
    write_u32_le(p, (uint32_t)(value & UINT64_C(0xffffffff)));
    write_u32_le(p + 4, (uint32_t)(value >> 32));
}

/* Strict UTF-8 validation; source/path payloads cannot contain embedded NUL. */
static int is_valid_utf8_text(const uint8_t* data, size_t length) {
    size_t i = 0;
    while (i < length) {
        uint8_t a = data[i];
        uint32_t codepoint;
        size_t extra;

        if (a == 0) return 0;
        if (a <= 0x7fu) {
            ++i;
            continue;
        } else if (a >= 0xc2u && a <= 0xdfu) {
            codepoint = (uint32_t)(a & 0x1fu);
            extra = 1;
        } else if (a >= 0xe0u && a <= 0xefu) {
            codepoint = (uint32_t)(a & 0x0fu);
            extra = 2;
        } else if (a >= 0xf0u && a <= 0xf4u) {
            codepoint = (uint32_t)(a & 0x07u);
            extra = 3;
        } else {
            return 0;
        }

        if (extra > length - i - 1u) return 0;
        {
            size_t j;
            for (j = 1; j <= extra; ++j) {
                uint8_t b = data[i + j];
                if ((b & 0xc0u) != 0x80u) return 0;
                codepoint = (codepoint << 6) | (uint32_t)(b & 0x3fu);
            }
        }

        if ((extra == 1 && codepoint < 0x80u) ||
            (extra == 2 && codepoint < 0x800u) ||
            (extra == 3 && codepoint < 0x10000u) ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
            codepoint > 0x10ffffu) {
            return 0;
        }
        i += extra + 1u;
    }
    return 1;
}

OirStatus oir_source_unit_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSourceUnitView* out_view
) {
    uint32_t path_length;
    uint64_t source_length;
    uint64_t expected_length;

    if (!payload || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    if (payload_length < OIR_SOURCE_UNIT_HEADER_SIZE) {
        return OIR_STATUS_INVALID_HEADER;
    }

    out_view->source_id = read_u32_le(payload);
    out_view->encoding_id = (uint16_t)((uint16_t)payload[4] |
        ((uint16_t)payload[5] << 8));
    out_view->flags = (uint16_t)((uint16_t)payload[6] |
        ((uint16_t)payload[7] << 8));
    path_length = read_u32_le(payload + 8);
    if (read_u32_le(payload + 12) != 0) return OIR_STATUS_INVALID_HEADER;
    source_length = read_u64_le(payload + 16);

    if (out_view->source_id != 0 ||
        out_view->encoding_id != OIR_SOURCE_ENCODING_UTF8 ||
        out_view->flags != 0) {
        return OIR_STATUS_INVALID_HEADER;
    }

    if (path_length > payload_length - OIR_SOURCE_UNIT_HEADER_SIZE ||
        source_length != (uint64_t)(payload_length -
            OIR_SOURCE_UNIT_HEADER_SIZE - (size_t)path_length)) {
        return OIR_STATUS_LENGTH_MISMATCH;
    }

    out_view->path = payload + OIR_SOURCE_UNIT_HEADER_SIZE;
    out_view->path_length = path_length;
    out_view->source = out_view->path + path_length;
    out_view->source_length = source_length;

    if ((path_length && !is_valid_utf8_text(out_view->path, path_length)) ||
        (source_length && !is_valid_utf8_text(out_view->source, (size_t)source_length))) {
        memset(out_view, 0, sizeof(*out_view));
        return OIR_STATUS_INVALID_SECTION;
    }
    return OIR_STATUS_OK;
}

OirStatus oir_source_unit_write(
    uint8_t* output,
    size_t capacity,
    uint32_t source_id,
    const uint8_t* path,
    uint32_t path_length,
    const uint8_t* source,
    uint64_t source_length,
    size_t* out_written
) {
    size_t total;

    if (out_written) *out_written = 0;
    if (!output || !out_written || (!path && path_length != 0) ||
        (!source && source_length != 0)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (source_id != 0) return OIR_STATUS_INVALID_HEADER;
#if SIZE_MAX < UINT64_MAX
    if (source_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
#endif
    if ((size_t)path_length > SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE ||
        (size_t)source_length >
            SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE - (size_t)path_length) {
        return OIR_STATUS_OVERFLOW;
    }
    total = OIR_SOURCE_UNIT_HEADER_SIZE + (size_t)path_length + (size_t)source_length;
    if (total > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    if ((path_length && !is_valid_utf8_text(path, path_length)) ||
        (source_length && !is_valid_utf8_text(source, (size_t)source_length))) {
        return OIR_STATUS_INVALID_SECTION;
    }

    write_u32_le(output, source_id);
    write_u16_le(output + 4, OIR_SOURCE_ENCODING_UTF8);
    write_u16_le(output + 6, 0);
    write_u32_le(output + 8, path_length);
    write_u32_le(output + 12, 0);
    write_u64_le(output + 16, source_length);

    if (path_length) memmove(output + OIR_SOURCE_UNIT_HEADER_SIZE, path, path_length);
    if (source_length) {
        memmove(output + OIR_SOURCE_UNIT_HEADER_SIZE + path_length,
            source, (size_t)source_length);
    }
    *out_written = total;
    return OIR_STATUS_OK;
}
