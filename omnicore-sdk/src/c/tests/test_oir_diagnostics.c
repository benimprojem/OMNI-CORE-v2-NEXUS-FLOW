#include "oir_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        free(buffer); \
        occ_diagnostics_destroy(&collector); \
        return 1; \
    } \
} while (0)

int main(void) {
    OccDiagnosticCollector collector;
    OccDiagnosticInput input;
    OirDiagnosticsPayloadView payload_view;
    OirDiagnosticEntryView entry;
    uint8_t* buffer = NULL;
    size_t capacity, written = 0;
    OirStatus status;
    OccDiagnosticCollector warning_only;
    OccDiagnosticInput warning_input;

    occ_diagnostics_init(&collector);
    occ_diagnostics_init(&warning_only);
    memset(&warning_input, 0, sizeof(warning_input));
    warning_input.code = 9002u;
    warning_input.stage_id = OCC_STAGE_OIR_VERIFY;
    warning_input.message_id = 9002u;
    warning_input.severity = OCC_DIAG_WARNING;
    warning_input.message = "unused value";
    warning_input.message_length = strlen(warning_input.message);
    CHECK(occ_diagnostics_add(&warning_only, &warning_input),
        "add warning-only diagnostic");
    CHECK(occ_diagnostics_blocks_output(&warning_only),
        "the current compilation policy applies a strict warning gate");
    warning_input.severity = OCC_DIAG_ERROR;
    warning_input.code = 9003u;
    warning_input.message_id = 9003u;
    warning_input.message = "invalid OIR";
    warning_input.message_length = strlen(warning_input.message);
    CHECK(occ_diagnostics_add(&warning_only, &warning_input),
        "add blocking error diagnostic");
    CHECK(occ_diagnostics_blocks_output(&warning_only),
        "errors must block output");
    occ_diagnostics_destroy(&warning_only);
    memset(&input, 0, sizeof(input));
    input.code = 3001;
    input.stage_id = OCC_STAGE_SEMANTIC;
    input.message_id = 3001;
    input.severity = OCC_DIAG_ERROR;
    input.span.source_id = 0;
    input.span.start_offset = 18;
    input.span.end_offset = 24;
    input.span.start_line = 3;
    input.span.start_column = 5;
    input.span.end_line = 3;
    input.span.end_column = 11;
    input.span.has_location = 1;
    input.message = "undefined variable";
    input.message_length = strlen(input.message);
    input.hint = "declare the variable";
    input.hint_length = strlen(input.hint);
    CHECK(occ_diagnostics_add(&collector, &input), "add located diagnostic");

    memset(&input, 0, sizeof(input));
    input.code = 9001;
    input.stage_id = OCC_STAGE_PARSER;
    input.message_id = 9001;
    input.severity = OCC_DIAG_WARNING;
    input.message = "recoverable parser warning";
    input.message_length = strlen(input.message);
    CHECK(occ_diagnostics_add(&collector, &input), "add location-free diagnostic");

    capacity = OIR_DIAGNOSTICS_HEADER_SIZE +
        collector.count * OIR_DIAGNOSTICS_RECORD_SIZE + collector.text_length;
    buffer = (uint8_t*)malloc(capacity);
    CHECK(buffer != NULL, "allocate output buffer");

    status = oir_diagnostics_payload_write(buffer, capacity, &collector, &written);
    CHECK(status == OIR_STATUS_OK && written == capacity, "write diagnostics payload");
    CHECK(oir_diagnostics_payload_open(buffer, written, &payload_view) == OIR_STATUS_OK,
        "open valid diagnostics payload");
    CHECK(payload_view.record_count == 2, "record count should round-trip");

    CHECK(oir_diagnostics_payload_get(&payload_view, 0, &entry) == OIR_STATUS_OK,
        "read first diagnostic");
    CHECK(entry.code == 3001 && entry.stage_id == OCC_STAGE_SEMANTIC &&
        entry.severity == OCC_DIAG_ERROR, "first diagnostic identity should round-trip");
    CHECK(entry.span.has_location && entry.span.start_offset == 18 &&
        entry.span.end_offset == 24 && entry.span.start_line == 3,
        "first diagnostic source span should round-trip");
    CHECK(entry.message_length == strlen("undefined variable") &&
        strcmp(entry.message, "undefined variable") == 0 &&
        entry.hint && strcmp(entry.hint, "declare the variable") == 0,
        "first diagnostic text should round-trip");

    CHECK(oir_diagnostics_payload_get(&payload_view, 1, &entry) == OIR_STATUS_OK,
        "read second diagnostic");
    CHECK(entry.severity == OCC_DIAG_WARNING && !entry.span.has_location &&
        entry.hint == NULL && strcmp(entry.message, "recoverable parser warning") == 0,
        "location-free diagnostic and absent hint should round-trip");

    {
        uint32_t saved_end_line = collector.records[0].span.end_line;
        collector.records[0].span.end_line = 2u;
        CHECK(oir_diagnostics_payload_write(buffer, capacity, &collector, &written) ==
            OIR_STATUS_INVALID_SECTION,
            "writer must reject a diagnostic span whose end line moves backwards");
        collector.records[0].span.end_line = saved_end_line;
    }
    {
        size_t end_line_offset = OIR_DIAGNOSTICS_HEADER_SIZE + 44u;
        uint8_t saved[4];
        memcpy(saved, buffer + end_line_offset, sizeof(saved));
        buffer[end_line_offset] = 2u;
        buffer[end_line_offset + 1u] = 0u;
        buffer[end_line_offset + 2u] = 0u;
        buffer[end_line_offset + 3u] = 0u;
        CHECK(oir_diagnostics_payload_open(buffer, capacity, &payload_view) ==
            OIR_STATUS_INVALID_SECTION,
            "reader must reject a serialized diagnostic span whose end line moves backwards");
        memcpy(buffer + end_line_offset, saved, sizeof(saved));
    }

    CHECK(oir_diagnostics_payload_write(buffer, capacity - 1u, &collector, &written) ==
        OIR_STATUS_BUFFER_TOO_SMALL, "reject undersized output buffer");
    CHECK(oir_diagnostics_payload_open(buffer, capacity - 1u, &payload_view) ==
        OIR_STATUS_LENGTH_MISMATCH, "reject truncated payload");

    {
        uint8_t saved = buffer[4];
        buffer[4] = 0;
        CHECK(oir_diagnostics_payload_open(buffer, capacity, &payload_view) ==
            OIR_STATUS_INVALID_HEADER, "reject wrong record size");
        buffer[4] = saved;
    }
    {
        uint8_t saved = buffer[OIR_DIAGNOSTICS_HEADER_SIZE + 12];
        buffer[OIR_DIAGNOSTICS_HEADER_SIZE + 12] = 99;
        CHECK(oir_diagnostics_payload_open(buffer, capacity, &payload_view) ==
            OIR_STATUS_INVALID_SECTION, "reject invalid severity");
        buffer[OIR_DIAGNOSTICS_HEADER_SIZE + 12] = saved;
    }
    {
        uint8_t saved[4];
        memcpy(saved, buffer + OIR_DIAGNOSTICS_HEADER_SIZE + 52, sizeof(saved));
        memset(buffer + OIR_DIAGNOSTICS_HEADER_SIZE + 52, 0xff, sizeof(saved));
        CHECK(oir_diagnostics_payload_open(buffer, capacity, &payload_view) ==
            OIR_STATUS_INVALID_SECTION, "reject out-of-range message offset");
        memcpy(buffer + OIR_DIAGNOSTICS_HEADER_SIZE + 52, saved, sizeof(saved));
    }

    {
        size_t strings_offset = capacity - collector.text_length;
        uint8_t saved = buffer[strings_offset];
        buffer[strings_offset] = 0xff;
        CHECK(oir_diagnostics_payload_open(buffer, capacity, &payload_view) ==
            OIR_STATUS_INVALID_SECTION, "reject invalid UTF-8 diagnostic text");
        buffer[strings_offset] = saved;
    }

    free(buffer);
    occ_diagnostics_destroy(&collector);
    puts("OIR diagnostics payload tests passed.");
    return 0;
}
