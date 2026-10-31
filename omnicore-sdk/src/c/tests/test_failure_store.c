#include "failure_store.h"

#include "../oir/oir.h"
#include "../oir/oir_payload.h"
#include "../oir/oir_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define TEST_RMDIR(path) rmdir(path)
#endif

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        free(bytes); \
        occ_diagnostics_destroy(&collector); \
        return 1; \
    } \
} while (0)

int main(void) {
    static const char directory[] = "occ-failure-store-test";
    static const char source_path[] = "samples/repeated.nxf";
    static const uint8_t source[] = "v:x = 1;\nv:x = ;\n";
    OccDiagnosticCollector collector;
    OccDiagnosticInput input = {0};
    OirArtifactView artifact;
    OirSectionView payload;
    OirSectionView diagnostic_section;
    OirSourceUnitView source_view;
    OirDiagnosticsPayloadView diagnostics_view;
    OirDiagnosticEntryView diagnostic;
    char output_path[4096];
    FILE* file = NULL;
    uint8_t* bytes = NULL;
    long file_size;

    (void)remove("occ-failure-store-test/job-987654321.oir");
    (void)remove("occ-failure-store-test/job-987654321.oir.tmp");
    (void)TEST_RMDIR(directory);

    occ_diagnostics_init(&collector);
    input.code = 2001u;
    input.stage_id = OCC_STAGE_PARSER;
    input.message_id = 2001u;
    input.severity = OCC_DIAG_ERROR;
    input.span.start_offset = 15u;
    input.span.end_offset = 16u;
    input.span.start_line = 2u;
    input.span.start_column = 7u;
    input.span.end_line = 2u;
    input.span.end_column = 8u;
    input.span.has_location = 1u;
    input.message = "expected expression";
    input.message_length = strlen(input.message);
    CHECK(occ_diagnostics_add(&collector, &input), "add diagnostic");

    CHECK(occ_failure_store_save(directory, 987654321u, OCC_STAGE_PARSER,
        source_path, source, sizeof(source) - 1u, &collector,
        output_path, sizeof(output_path)), "save OIR failure artifact");
    CHECK(strstr(output_path, "job-987654321.oir") != NULL,
        "output path should use job ID");
    CHECK(!occ_failure_store_save(directory, 987654321u, OCC_STAGE_PARSER,
        source_path, source, sizeof(source) - 1u, &collector,
        output_path, sizeof(output_path)),
        "existing job artifact must not be overwritten");

    file = fopen("occ-failure-store-test/job-987654321.oir", "rb");
    CHECK(file != NULL, "open persisted artifact");
    CHECK(fseek(file, 0, SEEK_END) == 0, "seek artifact end");
    file_size = ftell(file);
    CHECK(file_size > 0 && fseek(file, 0, SEEK_SET) == 0, "measure artifact");
    bytes = (uint8_t*)malloc((size_t)file_size);
    CHECK(bytes != NULL, "allocate artifact bytes");
    CHECK(fread(bytes, 1u, (size_t)file_size, file) == (size_t)file_size,
        "read artifact bytes");
    fclose(file);
    file = NULL;

    CHECK(oir_artifact_open(bytes, (size_t)file_size, &artifact) == OIR_STATUS_OK,
        "validate persisted artifact");
    CHECK(artifact.header.validity == OIR_VALIDITY_INVALID &&
        artifact.header.job_id == 987654321u, "validate failure metadata");
    CHECK(oir_artifact_get_section(&artifact, 0u, &payload) == OIR_STATUS_OK,
        "get source section");
    CHECK(oir_source_unit_open(payload.data, (size_t)payload.length,
        &source_view) == OIR_STATUS_OK, "open source payload");
    CHECK(source_view.path_length == sizeof(source_path) - 1u &&
        memcmp(source_view.path, source_path, sizeof(source_path) - 1u) == 0 &&
        source_view.source_length == sizeof(source) - 1u &&
        memcmp(source_view.source, source, sizeof(source) - 1u) == 0,
        "persisted source must be byte-identical");

    CHECK(oir_artifact_get_section(&artifact, 1u, &diagnostic_section) == OIR_STATUS_OK,
        "get diagnostics section");
    CHECK(oir_diagnostics_payload_open(diagnostic_section.data,
        (size_t)diagnostic_section.length, &diagnostics_view) == OIR_STATUS_OK,
        "open diagnostics payload");
    CHECK(oir_diagnostics_payload_get(&diagnostics_view, 0u, &diagnostic) == OIR_STATUS_OK &&
        diagnostic.span.start_line == 2u &&
        diagnostic.span.start_column == 7u &&
        diagnostic.span.end_line == 2u &&
        diagnostic.span.end_column == 8u,
        "persisted diagnostic must retain exact source location");

    free(bytes);
    bytes = NULL;
    remove(output_path);
    TEST_RMDIR(directory);
    occ_diagnostics_destroy(&collector);
    puts("OCC failure store persistence tests passed.");
    return 0;
}
