#include "oir_failure_artifact.h"
#include "oir_diagnostics.h"
#include "oir_payload.h"
#include "oir_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", message, __LINE__); \
        free(buffer); \
        occ_diagnostics_destroy(&collector); \
        return 1; \
    } \
} while (0)

int main(void) {
    static const uint8_t path[] = "src/main.nxf";
    static const uint8_t source[] = "v:x = 1;\\nv:x = ;\\n";
    OccDiagnosticCollector collector;
    OccDiagnosticInput input = {0};
    OirArtifactView artifact;
    OirSectionView payload_section, diagnostics_section;
    OirSourceUnitView source_view;
    OirDiagnosticsPayloadView diagnostics_view;
    OirDiagnosticEntryView diagnostic;
    uint8_t* buffer = NULL;
    size_t capacity, written = 0;

    occ_diagnostics_init(&collector);
    input.code = 2001u;
    input.stage_id = OCC_STAGE_PARSER;
    input.message_id = 2001u;
    input.severity = OCC_DIAG_ERROR;
    input.span.source_id = 0u;
    input.span.start_offset = 15u;
    input.span.end_offset = 16u;
    input.span.start_line = 2u;
    input.span.start_column = 7u;
    input.span.end_line = 2u;
    input.span.end_column = 8u;
    input.span.has_location = 1u;
    input.message = "expected expression";
    input.message_length = strlen(input.message);
    CHECK(occ_diagnostics_add(&collector, &input), "add parser diagnostic");

    capacity = OIR_HEADER_SIZE + 2u * OIR_SECTION_HEADER_SIZE +
        OIR_SOURCE_UNIT_HEADER_SIZE + sizeof(path) - 1u +
        sizeof(source) - 1u + OIR_DIAGNOSTICS_HEADER_SIZE +
        OIR_DIAGNOSTICS_RECORD_SIZE + collector.text_length;
    buffer = (uint8_t*)malloc(capacity);
    CHECK(buffer != NULL, "allocate failure artifact");

    CHECK(oir_failure_artifact_write(buffer, capacity, 77u, OCC_STAGE_PARSER,
        path, (uint32_t)(sizeof(path) - 1u), source,
        sizeof(source) - 1u, &collector, &written) == OIR_STATUS_OK,
        "write failure artifact");
    CHECK(oir_artifact_open(buffer, written, &artifact) == OIR_STATUS_OK,
        "open failure artifact");
    CHECK(artifact.header.artifact_kind == OIR_KIND_SOURCE_UNIT &&
        artifact.header.validity == OIR_VALIDITY_INVALID &&
        artifact.header.job_id == 77u &&
        artifact.header.producer_stage_id == OCC_STAGE_PARSER,
        "failure artifact header should identify invalid source checkpoint");
    CHECK(artifact.section_count == 2u, "failure artifact should have source and diagnostics");
    CHECK(oir_artifact_get_section(&artifact, 0u, &payload_section) == OIR_STATUS_OK &&
        payload_section.type == OIR_SECTION_PAYLOAD, "read source payload section");
    CHECK(oir_source_unit_open(payload_section.data,
        (size_t)payload_section.length, &source_view) == OIR_STATUS_OK,
        "open preserved source unit");
    CHECK(source_view.path_length == sizeof(path) - 1u &&
        memcmp(source_view.path, path, sizeof(path) - 1u) == 0 &&
        source_view.source_length == sizeof(source) - 1u &&
        memcmp(source_view.source, source, sizeof(source) - 1u) == 0,
        "original source and path must be preserved byte-for-byte");

    CHECK(oir_artifact_get_section(&artifact, 1u, &diagnostics_section) == OIR_STATUS_OK &&
        diagnostics_section.type == OIR_SECTION_DIAGNOSTICS,
        "read diagnostics section");
    CHECK(oir_diagnostics_payload_open(diagnostics_section.data,
        (size_t)diagnostics_section.length, &diagnostics_view) == OIR_STATUS_OK &&
        diagnostics_view.record_count == 1u, "open preserved diagnostics");
    CHECK(oir_diagnostics_payload_get(&diagnostics_view, 0u, &diagnostic) == OIR_STATUS_OK &&
        diagnostic.span.start_line == 2u &&
        diagnostic.span.start_column == 7u &&
        diagnostic.span.end_line == 2u &&
        diagnostic.span.end_column == 8u,
        "diagnostic source location should survive artifact roundtrip");

    free(buffer);
    occ_diagnostics_destroy(&collector);
    puts("OIR failure artifact roundtrip tests passed.");
    return 0;
}
