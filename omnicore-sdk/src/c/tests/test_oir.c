#include "oir.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    uint8_t buffer[256];
    uint8_t payload[] = { 0x10, 0x20, 0x30, 0x40 };
    uint8_t metadata[] = { 't', 'e', 's', 't' };
    OirArtifactHeader header;
    OirSectionInput sections[2];
    OirArtifactView view;
    OirSectionView section;
    size_t written = 0;

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_TOKEN_STREAM;
    header.payload_schema_major = 1;
    header.payload_schema_minor = 0;
    header.producer_stage_id = 1;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = UINT64_C(0x0102030405060708);

    sections[0].type = OIR_SECTION_PAYLOAD;
    sections[0].version = 1;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = payload;
    sections[0].length = sizeof(payload);
    sections[1].type = OIR_SECTION_METADATA;
    sections[1].version = 1;
    sections[1].flags = 0;
    sections[1].data = metadata;
    sections[1].length = sizeof(metadata);

    CHECK(oir_artifact_write(buffer, sizeof(buffer), &header, sections, 2, &written)
        == OIR_STATUS_OK, "writer should serialize valid artifact");
    CHECK(written == OIR_HEADER_SIZE + 2 * OIR_SECTION_HEADER_SIZE +
        sizeof(payload) + sizeof(metadata), "serialized size should match");
    CHECK(buffer[0] == 'O' && buffer[1] == 'I' &&
        buffer[2] == 'R' && buffer[3] == '1', "magic should be OIR1");
    CHECK(buffer[28] == (uint8_t)(written & 0xffu), "total length is little-endian");

    CHECK(oir_artifact_open(buffer, written, &view) == OIR_STATUS_OK,
        "reader should accept writer output");
    CHECK(view.header.artifact_kind == OIR_KIND_TOKEN_STREAM,
        "artifact kind should round-trip");
    CHECK(view.header.job_id == header.job_id, "job id should round-trip");
    CHECK(view.section_count == 2, "section count should round-trip");

    CHECK(oir_artifact_get_section(&view, 0, &section) == OIR_STATUS_OK,
        "first section should be readable");
    CHECK(section.type == OIR_SECTION_PAYLOAD &&
        section.length == sizeof(payload) &&
        memcmp(section.data, payload, sizeof(payload)) == 0,
        "payload bytes should round-trip");

    CHECK(oir_artifact_get_section(&view, 1, &section) == OIR_STATUS_OK,
        "second section should be readable");
    CHECK(section.type == OIR_SECTION_METADATA &&
        section.length == sizeof(metadata) &&
        memcmp(section.data, metadata, sizeof(metadata)) == 0,
        "metadata bytes should round-trip");

    CHECK(oir_artifact_open(buffer, written - 1, &view) != OIR_STATUS_OK,
        "truncated container should be rejected");

    {
        uint8_t damaged[256];
        memcpy(damaged, buffer, written);
        damaged[0] = 'X';
        CHECK(oir_artifact_open(damaged, written, &view) == OIR_STATUS_BAD_MAGIC,
            "bad magic should be rejected");
    }

    {
        uint8_t damaged[256];
        memcpy(damaged, buffer, written);
        damaged[28] = (uint8_t)((written + 1u) & 0xffu);
        CHECK(oir_artifact_open(damaged, written, &view) ==
            OIR_STATUS_LENGTH_MISMATCH, "incorrect total length should be rejected");
    }

    {
        uint8_t damaged[256];
        memcpy(damaged, buffer, written);
        /* First section payload length begins at header + section-header + 8. */
        memset(damaged + OIR_HEADER_SIZE + 8, 0xff, 8);
        CHECK(oir_artifact_open(damaged, written, &view) ==
            OIR_STATUS_INVALID_SECTION, "oversized section length should be rejected");
    }

    {
        uint8_t damaged[256];
        memcpy(damaged, buffer, written);
        /* Second section begins after the first section header and payload. */
        damaged[OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + sizeof(payload)] = 99;
        damaged[OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + sizeof(payload) + 1] = 0;
        damaged[OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + sizeof(payload) + 4] =
            (uint8_t)OIR_SECTION_FLAG_REQUIRED;
        CHECK(oir_artifact_open(damaged, written, &view) ==
            OIR_STATUS_UNKNOWN_REQUIRED_SECTION,
            "unknown required section should be rejected");
    }

    {
        uint8_t damaged[256];
        memcpy(damaged, buffer, written);
        damaged[OIR_HEADER_SIZE] = 99;
        damaged[OIR_HEADER_SIZE + 1] = 0;
        damaged[OIR_HEADER_SIZE + 4] = 0;
        CHECK(oir_artifact_open(damaged, written, &view) ==
            OIR_STATUS_MISSING_PAYLOAD, "missing primary payload should be rejected");
    }

    {
        OirSectionInput only_metadata = sections[1];
        CHECK(oir_artifact_write(buffer, sizeof(buffer), &header,
            &only_metadata, 1, &written) == OIR_STATUS_MISSING_PAYLOAD,
            "writer should require exactly one payload");
    }

    {
        OirSectionInput unknown = sections[1];
        unknown.type = 99;
        unknown.flags = OIR_SECTION_FLAG_REQUIRED;
        CHECK(oir_artifact_write(buffer, sizeof(buffer), &header,
            &unknown, 1, &written) == OIR_STATUS_UNKNOWN_REQUIRED_SECTION,
            "writer should reject unknown required section");
    }

    CHECK(oir_artifact_write(buffer, 8, &header, sections, 2, &written)
        == OIR_STATUS_BUFFER_TOO_SMALL, "writer should detect small output buffer");

    puts("OIR container tests passed.");
    return 0;
}
