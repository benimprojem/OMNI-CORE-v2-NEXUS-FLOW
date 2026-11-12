#include "diagnostics.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    OccDiagnosticCollector collector;
    OccDiagnosticInput input;
    OccDiagnosticView view;
    char mutable_message[] = "undefined symbol";
    char mutable_hint[] = "declare the symbol first";

    /* Multi-file source IDs resolve to stable owned paths. */
    {
        OccSourceFileTable files;
        uint32_t first_id, second_id, duplicate_id;
        occ_source_files_init(&files);
        CHECK(occ_source_files_register(&files, "src/main.nxf", &first_id),
            "first source path should register");
        CHECK(occ_source_files_register(&files, "lib/math.nxf", &second_id),
            "second source path should register");
        CHECK(first_id == 0u && second_id == 1u,
            "source IDs should follow insertion order");
        CHECK(occ_source_files_register(&files, "src/main.nxf", &duplicate_id) &&
            duplicate_id == first_id,
            "duplicate path should reuse its source ID");
        CHECK(strcmp(occ_source_files_get(&files, first_id), "src/main.nxf") == 0 &&
            strcmp(occ_source_files_get(&files, second_id), "lib/math.nxf") == 0,
            "source IDs should resolve their file paths");
        CHECK(occ_source_files_get(&files, 99u) == NULL,
            "unknown source ID should not resolve");
        occ_source_files_destroy(&files);
    }

    occ_diagnostics_init(&collector);
    memset(&input, 0, sizeof(input));
    input.code = 3001;
    input.stage_id = 3;
    input.message_id = 3001;
    input.severity = OCC_DIAG_ERROR;
    input.span.source_id = 0;
    input.span.start_offset = 12;
    input.span.end_offset = 17;
    input.span.start_line = 2;
    input.span.start_column = 4;
    input.span.end_line = 2;
    input.span.end_column = 9;
    input.span.has_location = 1;
    input.message = mutable_message;
    input.message_length = strlen(mutable_message);
    input.hint = mutable_hint;
    input.hint_length = strlen(mutable_hint);

    CHECK(occ_diagnostics_add(&collector, &input),
        "collector should accept valid diagnostic");
    mutable_message[0] = 'X';
    mutable_hint[0] = 'Y';

    CHECK(occ_diagnostics_count(&collector) == 1,
        "diagnostic count should increment");
    CHECK(occ_diagnostics_count_severity(&collector, OCC_DIAG_ERROR) == 1,
        "severity count should increment");
    CHECK(occ_diagnostics_blocks_output(&collector),
        "an error should block final output");
    CHECK(occ_diagnostics_get(&collector, 0, &view),
        "stored diagnostic should be readable");
    CHECK(view.record->code == 3001 && view.record->stage_id == 3,
        "diagnostic identity should be preserved");
    CHECK(strcmp(view.message, "undefined symbol") == 0 &&
        strcmp(view.hint, "declare the symbol first") == 0,
        "collector should own copies of message and hint");
    CHECK(view.record->span.start_offset == 12 &&
        view.record->span.end_offset == 17 &&
        view.record->span.start_line == 2,
        "source span should be preserved");

    memset(&input, 0, sizeof(input));
    input.code = 9001;
    input.stage_id = 1;
    input.severity = OCC_DIAG_NOTE;
    input.message = "analysis completed with note";
    input.message_length = strlen(input.message);
    CHECK(occ_diagnostics_add(&collector, &input),
        "collector should accept a location-free note");
    CHECK(occ_diagnostics_blocks_output(&collector),
        "existing error should continue blocking output");
    CHECK(occ_diagnostics_count_severity(&collector, OCC_DIAG_NOTE) == 1,
        "note count should increment");

    {
        OccDiagnosticCollector notes_only;
        occ_diagnostics_init(&notes_only);
        CHECK(occ_diagnostics_add(&notes_only, &input),
            "notes-only collector should accept note");
        CHECK(!occ_diagnostics_blocks_output(&notes_only),
            "notes alone should not block output");
        occ_diagnostics_destroy(&notes_only);
    }

    input.code = 9002;
    input.severity = OCC_DIAG_WARNING;
    input.message = "warning";
    input.message_length = strlen(input.message);
    CHECK(occ_diagnostics_add(&collector, &input),
        "collector should accept warning");
    CHECK(occ_diagnostics_blocks_output(&collector),
        "warning should block output under strict policy");

    input.code = 0;
    CHECK(!occ_diagnostics_add(&collector, &input),
        "zero diagnostic code should be rejected");
    input.code = 9003;
    input.severity = 99;
    CHECK(!occ_diagnostics_add(&collector, &input),
        "unknown severity should be rejected");

    CHECK(!occ_diagnostics_get(&collector, 999, &view),
        "out-of-range diagnostic index should be rejected");

    {
        char invalid_text[] = { 'a', '\0', 'b' };
        input.code = 9004;
        input.severity = OCC_DIAG_ERROR;
        input.message = invalid_text;
        input.message_length = sizeof(invalid_text);
        CHECK(!occ_diagnostics_add(&collector, &input),
            "embedded NUL in diagnostic text should be rejected");
    }

    {
        OccDiagnosticInput invalid_span = input;
        invalid_span.code = 9005;
        invalid_span.severity = OCC_DIAG_ERROR;
        invalid_span.message = "backwards span";
        invalid_span.message_length = strlen(invalid_span.message);
        invalid_span.span.has_location = 1u;
        invalid_span.span.start_offset = 8u;
        invalid_span.span.end_offset = 9u;
        invalid_span.span.start_line = 3u;
        invalid_span.span.start_column = 4u;
        invalid_span.span.end_line = 2u;
        invalid_span.span.end_column = 9u;
        CHECK(!occ_diagnostics_add(&collector, &invalid_span),
            "backwards diagnostic line range should be rejected");
        invalid_span.span.end_line = 3u;
        invalid_span.span.end_column = 3u;
        CHECK(!occ_diagnostics_add(&collector, &invalid_span),
            "backwards diagnostic column range should be rejected");
    }

    {
        OccDiagnosticCollector failed;
        occ_diagnostics_init(&failed);
        failed.allocation_failed = 1;
        CHECK(occ_diagnostics_blocks_output(&failed),
            "collector allocation failure must block final output");
        occ_diagnostics_destroy(&failed);
    }
    CHECK(occ_diagnostics_blocks_output(NULL),
        "missing diagnostic collector must fail closed");


    {
        OccDiagnosticCollector bounded;
        size_t i;
        occ_diagnostics_init(&bounded);
        memset(&input, 0, sizeof(input));
        input.code = 9006;
        input.stage_id = OCC_STAGE_PARSER;
        input.severity = OCC_DIAG_ERROR;
        input.message = "x";
        input.message_length = 1u;
        for (i = 0u; i < OCC_DIAGNOSTICS_MAX_COUNT; ++i) {
            CHECK(occ_diagnostics_add(&bounded, &input),
                "collector should accept exactly the diagnostic count limit");
        }
        CHECK(occ_diagnostics_count(&bounded) == OCC_DIAGNOSTICS_MAX_COUNT,
            "exact diagnostic count limit should be retained");
        CHECK(!occ_diagnostics_add(&bounded, &input) &&
            occ_diagnostics_limit_exceeded(&bounded),
            "one diagnostic beyond the count limit should report limit exhaustion");
        CHECK(occ_diagnostics_blocks_output(&bounded),
            "diagnostic count exhaustion must block final output");
        occ_diagnostics_destroy(&bounded);
    }

    {
        OccDiagnosticCollector bounded;
        char* message = (char*)malloc((size_t)OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES + 2u);
        CHECK(message != NULL, "message-boundary test allocation should succeed");
        memset(message, 'm', (size_t)OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES + 1u);
        message[OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES + 1u] = '\0';
        occ_diagnostics_init(&bounded);
        memset(&input, 0, sizeof(input));
        input.code = 9007;
        input.stage_id = OCC_STAGE_PARSER;
        input.severity = OCC_DIAG_ERROR;
        input.message = message;
        input.message_length = OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES;
        CHECK(occ_diagnostics_add(&bounded, &input),
            "message exactly at the per-message byte limit should be accepted");
        occ_diagnostics_destroy(&bounded);

        occ_diagnostics_init(&bounded);
        input.message_length = (size_t)OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES + 1u;
        CHECK(!occ_diagnostics_add(&bounded, &input) &&
            occ_diagnostics_limit_exceeded(&bounded),
            "message one byte beyond the per-message limit should report limit exhaustion");
        occ_diagnostics_destroy(&bounded);
        free(message);
    }

    {
        OccDiagnosticCollector bounded;
        const size_t message_length = (size_t)OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES - 1u;
        char* message = (char*)malloc(message_length + 1u);
        size_t i;
        CHECK(message != NULL, "total-text-budget test allocation should succeed");
        memset(message, 't', message_length);
        message[message_length] = '\0';
        occ_diagnostics_init(&bounded);
        memset(&input, 0, sizeof(input));
        input.code = 9008;
        input.stage_id = OCC_STAGE_PARSER;
        input.severity = OCC_DIAG_ERROR;
        input.message = message;
        input.message_length = message_length;
        for (i = 0u; i < 16u; ++i) {
            CHECK(occ_diagnostics_add(&bounded, &input),
                "collector should accept text up to the exact aggregate byte limit");
        }
        CHECK(bounded.text_length == OCC_DIAGNOSTICS_MAX_TEXT_BYTES,
            "aggregate diagnostic text should equal the exact 16 MiB budget");
        input.message = "x";
        input.message_length = 1u;
        CHECK(!occ_diagnostics_add(&bounded, &input) &&
            occ_diagnostics_limit_exceeded(&bounded),
            "one more diagnostic byte should exceed the aggregate text budget");
        occ_diagnostics_destroy(&bounded);
        free(message);
    }

    occ_diagnostics_destroy(&collector);
    CHECK(occ_diagnostics_count(&collector) == 0,
        "destroy should reset collector");
    puts("Diagnostic collector tests passed.");
    return 0;
}
