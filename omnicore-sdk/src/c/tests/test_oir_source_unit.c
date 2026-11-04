#include "oir_payload.h"

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
    const uint8_t path[] = "src/main.nxf";
    const uint8_t source[] = "v:x = 42;\nret x;";
    size_t written = 0;
    OirSourceUnitView view;

    CHECK(oir_source_unit_write(buffer, sizeof(buffer), 0,
        path, (uint32_t)(sizeof(path) - 1u),
        source, (uint64_t)(sizeof(source) - 1u), &written) == OIR_STATUS_OK,
        "SOURCE_UNIT writer should accept valid UTF-8 input");
    CHECK(written == OIR_SOURCE_UNIT_HEADER_SIZE +
        sizeof(path) - 1u + sizeof(source) - 1u,
        "SOURCE_UNIT size should match its fields");

    CHECK(oir_source_unit_open(buffer, written, &view) == OIR_STATUS_OK,
        "SOURCE_UNIT reader should accept writer output");
    CHECK(view.source_id == 0 && view.encoding_id == OIR_SOURCE_ENCODING_UTF8,
        "SOURCE_UNIT metadata should round-trip");
    CHECK(view.path_length == sizeof(path) - 1u &&
        memcmp(view.path, path, sizeof(path) - 1u) == 0,
        "SOURCE_UNIT path should round-trip");
    CHECK(view.source_length == sizeof(source) - 1u &&
        memcmp(view.source, source, sizeof(source) - 1u) == 0,
        "SOURCE_UNIT content should round-trip");

    CHECK(oir_source_unit_open(buffer, written - 1u, &view) ==
        OIR_STATUS_LENGTH_MISMATCH, "truncated SOURCE_UNIT should be rejected");

    {
        uint8_t invalid_utf8[] = { 0xc0u, 0x80u };
        CHECK(oir_source_unit_write(buffer, sizeof(buffer), 0,
            NULL, 0, invalid_utf8, sizeof(invalid_utf8), &written) ==
            OIR_STATUS_INVALID_SECTION, "overlong UTF-8 should be rejected");
    }

    {
        uint8_t embedded_nul[] = { 'a', 0, 'b' };
        CHECK(oir_source_unit_write(buffer, sizeof(buffer), 0,
            NULL, 0, embedded_nul, sizeof(embedded_nul), &written) ==
            OIR_STATUS_INVALID_SECTION, "embedded NUL should be rejected");
    }

    CHECK(oir_source_unit_write(buffer, 8, 0,
        path, (uint32_t)(sizeof(path) - 1u),
        source, (uint64_t)(sizeof(source) - 1u), &written) ==
        OIR_STATUS_BUFFER_TOO_SMALL, "small SOURCE_UNIT buffer should be rejected");

    CHECK(oir_source_unit_write(buffer, sizeof(buffer), 42,
        path, (uint32_t)(sizeof(path) - 1u),
        source, (uint64_t)(sizeof(source) - 1u), &written) == OIR_STATUS_OK,
        "nonzero source IDs should be accepted for multi-file jobs");
    CHECK(oir_source_unit_open(buffer, written, &view) == OIR_STATUS_OK &&
        view.source_id == 42u,
        "nonzero source ID should round-trip");

    puts("OIR SOURCE_UNIT payload tests passed.");
    return 0;
}
