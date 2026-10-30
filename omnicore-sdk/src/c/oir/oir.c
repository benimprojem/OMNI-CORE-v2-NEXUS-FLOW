#include "oir.h"

#include <limits.h>
#include <string.h>

#define OIR_KNOWN_SECTION_FLAGS OIR_SECTION_FLAG_REQUIRED

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
    write_u32_le(p, (uint32_t)(value & 0xffffffffu));
    write_u32_le(p + 4, (uint32_t)(value >> 32));
}

static int is_known_section(uint16_t type) {
    return type >= OIR_SECTION_PAYLOAD && type <= OIR_SECTION_CHECKSUM;
}

static int is_valid_validity(uint8_t validity) {
    /* A serialized artifact must contain a payload; NONE is a pipeline result, not an artifact state. */
    return validity >= OIR_VALIDITY_VALID && validity <= OIR_VALIDITY_INVALID;
}

OirStatus oir_artifact_open(
    const uint8_t* bytes,
    size_t length,
    OirArtifactView* out_view
) {
    uint32_t section_count;
    uint32_t payload_count = 0;
    uint32_t i;
    uint32_t header_size;
    uint64_t total_length;
    size_t offset;

    if (!bytes || !out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));

    if (length < OIR_HEADER_SIZE) return OIR_STATUS_INVALID_HEADER;
    if (bytes[0] != OIR_MAGIC_0 || bytes[1] != OIR_MAGIC_1 ||
        bytes[2] != OIR_MAGIC_2 || bytes[3] != OIR_MAGIC_3) {
        return OIR_STATUS_BAD_MAGIC;
    }

    out_view->header.container_major = read_u16_le(bytes + 4);
    out_view->header.container_minor = read_u16_le(bytes + 6);
    header_size = read_u32_le(bytes + 8);
    out_view->header.artifact_kind = read_u32_le(bytes + 12);
    out_view->header.payload_schema_major = read_u16_le(bytes + 16);
    out_view->header.payload_schema_minor = read_u16_le(bytes + 18);
    out_view->header.producer_stage_id = read_u32_le(bytes + 20);
    out_view->header.validity = bytes[24];
    /* Byte 25 is the v1 header flags field; no v1 flags are currently defined. */
    section_count = read_u16_le(bytes + 26);
    total_length = read_u64_le(bytes + 28);
    out_view->header.job_id = read_u64_le(bytes + 36);

    if (out_view->header.container_major != OIR_CONTAINER_MAJOR ||
        out_view->header.container_minor > OIR_CONTAINER_MINOR) {
        return OIR_STATUS_UNSUPPORTED_VERSION;
    }
    if (bytes[25] != 0 || header_size < OIR_HEADER_SIZE ||
        (uint64_t)header_size > (uint64_t)length ||
        out_view->header.artifact_kind == 0 ||
        out_view->header.payload_schema_major == 0 ||
        !is_valid_validity(out_view->header.validity) ||
        section_count == 0) {
        return OIR_STATUS_INVALID_HEADER;
    }
    if (total_length != (uint64_t)length) return OIR_STATUS_LENGTH_MISMATCH;

    offset = (size_t)header_size;
    for (i = 0; i < section_count; ++i) {
        uint16_t type;
        uint32_t flags;
        uint64_t section_length;

        if (offset > length || length - offset < OIR_SECTION_HEADER_SIZE) {
            return OIR_STATUS_INVALID_SECTION;
        }

        type = read_u16_le(bytes + offset);
        flags = read_u32_le(bytes + offset + 4);
        section_length = read_u64_le(bytes + offset + 8);
        offset += OIR_SECTION_HEADER_SIZE;

        if (read_u16_le(bytes + offset - OIR_SECTION_HEADER_SIZE + 2) == 0 ||
            (flags & ~OIR_KNOWN_SECTION_FLAGS) != 0) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (!is_known_section(type) && (flags & OIR_SECTION_FLAG_REQUIRED) != 0) {
            return OIR_STATUS_UNKNOWN_REQUIRED_SECTION;
        }
        if (section_length > (uint64_t)(length - offset)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (type == OIR_SECTION_PAYLOAD) {
            ++payload_count;
            if (payload_count > 1) return OIR_STATUS_DUPLICATE_PAYLOAD;
        }
        offset += (size_t)section_length;
    }

    if (offset != length) return OIR_STATUS_LENGTH_MISMATCH;
    if (payload_count != 1) return OIR_STATUS_MISSING_PAYLOAD;

    out_view->bytes = bytes;
    out_view->length = length;
    out_view->section_count = section_count;
    out_view->sections_offset = (size_t)header_size;
    return OIR_STATUS_OK;
}

OirStatus oir_artifact_get_section(
    const OirArtifactView* view,
    uint32_t index,
    OirSectionView* out_section
) {
    size_t offset;
    uint32_t i;

    if (!view || !view->bytes || !out_section) return OIR_STATUS_INVALID_ARGUMENT;
    if (index >= view->section_count) return OIR_STATUS_INVALID_ARGUMENT;

    offset = view->sections_offset;
    for (i = 0; i <= index; ++i) {
        uint64_t section_length;
        if (offset > view->length ||
            view->length - offset < OIR_SECTION_HEADER_SIZE) {
            return OIR_STATUS_INVALID_SECTION;
        }

        out_section->type = read_u16_le(view->bytes + offset);
        out_section->version = read_u16_le(view->bytes + offset + 2);
        out_section->flags = read_u32_le(view->bytes + offset + 4);
        section_length = read_u64_le(view->bytes + offset + 8);
        offset += OIR_SECTION_HEADER_SIZE;

        if (section_length > (uint64_t)(view->length - offset)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        out_section->length = section_length;
        out_section->data = view->bytes + offset;
        if (i == index) return OIR_STATUS_OK;
        offset += (size_t)section_length;
    }
    return OIR_STATUS_INVALID_ARGUMENT;
}

OirStatus oir_artifact_write(
    uint8_t* output,
    size_t capacity,
    const OirArtifactHeader* header,
    const OirSectionInput* sections,
    uint32_t section_count,
    size_t* out_written
) {
    size_t total = OIR_HEADER_SIZE;
    uint32_t payload_count = 0;
    uint32_t i;
    size_t offset;

    if (out_written) *out_written = 0;
    if (!output || !header || !sections || !out_written || section_count == 0) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (header->container_major != OIR_CONTAINER_MAJOR ||
        header->container_minor > OIR_CONTAINER_MINOR) {
        return OIR_STATUS_UNSUPPORTED_VERSION;
    }
    if (header->artifact_kind == 0 || header->payload_schema_major == 0 ||
        !is_valid_validity(header->validity) || section_count > UINT16_MAX) {
        return OIR_STATUS_INVALID_HEADER;
    }

    for (i = 0; i < section_count; ++i) {
        if (sections[i].version == 0 ||
            (sections[i].flags & ~OIR_KNOWN_SECTION_FLAGS) != 0 ||
            (!sections[i].data && sections[i].length != 0)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (sections[i].type == OIR_SECTION_PAYLOAD) {
            ++payload_count;
        } else if (!is_known_section(sections[i].type) &&
                   (sections[i].flags & OIR_SECTION_FLAG_REQUIRED) != 0) {
            return OIR_STATUS_UNKNOWN_REQUIRED_SECTION;
        }

        if (total > SIZE_MAX - OIR_SECTION_HEADER_SIZE ||
            sections[i].length > (uint64_t)SIZE_MAX ||
            (size_t)sections[i].length >
                SIZE_MAX - total - OIR_SECTION_HEADER_SIZE) {
            return OIR_STATUS_OVERFLOW;
        }
        total += OIR_SECTION_HEADER_SIZE + (size_t)sections[i].length;
    }

    if (payload_count == 0) return OIR_STATUS_MISSING_PAYLOAD;
    if (payload_count > 1) return OIR_STATUS_DUPLICATE_PAYLOAD;
    if (total > capacity) return OIR_STATUS_BUFFER_TOO_SMALL;

    output[0] = OIR_MAGIC_0;
    output[1] = OIR_MAGIC_1;
    output[2] = OIR_MAGIC_2;
    output[3] = OIR_MAGIC_3;
    write_u16_le(output + 4, header->container_major);
    write_u16_le(output + 6, header->container_minor);
    write_u32_le(output + 8, OIR_HEADER_SIZE);
    write_u32_le(output + 12, header->artifact_kind);
    write_u16_le(output + 16, header->payload_schema_major);
    write_u16_le(output + 18, header->payload_schema_minor);
    write_u32_le(output + 20, header->producer_stage_id);
    output[24] = header->validity;
    output[25] = 0;
    write_u16_le(output + 26, (uint16_t)section_count);
    write_u64_le(output + 28, (uint64_t)total);
    write_u64_le(output + 36, header->job_id);

    offset = OIR_HEADER_SIZE;
    for (i = 0; i < section_count; ++i) {
        write_u16_le(output + offset, sections[i].type);
        write_u16_le(output + offset + 2, sections[i].version);
        write_u32_le(output + offset + 4, sections[i].flags);
        write_u64_le(output + offset + 8, sections[i].length);
        offset += OIR_SECTION_HEADER_SIZE;
        if (sections[i].length != 0) {
            memmove(output + offset, sections[i].data, (size_t)sections[i].length);
        }
        offset += (size_t)sections[i].length;
    }

    *out_written = total;
    return OIR_STATUS_OK;
}

const char* oir_status_string(OirStatus status) {
    switch (status) {
        case OIR_STATUS_OK: return "ok";
        case OIR_STATUS_INVALID_ARGUMENT: return "invalid argument";
        case OIR_STATUS_BUFFER_TOO_SMALL: return "buffer too small";
        case OIR_STATUS_BAD_MAGIC: return "bad OIR magic";
        case OIR_STATUS_UNSUPPORTED_VERSION: return "unsupported OIR version";
        case OIR_STATUS_INVALID_HEADER: return "invalid OIR header";
        case OIR_STATUS_INVALID_SECTION: return "invalid OIR section";
        case OIR_STATUS_MISSING_PAYLOAD: return "missing payload section";
        case OIR_STATUS_DUPLICATE_PAYLOAD: return "duplicate payload section";
        case OIR_STATUS_UNKNOWN_REQUIRED_SECTION: return "unknown required section";
        case OIR_STATUS_LENGTH_MISMATCH: return "OIR length mismatch";
        case OIR_STATUS_OVERFLOW: return "OIR size overflow";
        default: return "unknown OIR status";
    }
}
