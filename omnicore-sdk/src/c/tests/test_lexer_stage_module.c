#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_loader.h"
#include "oir_payload.h"
#include "oir_token_stream.h"
#include "oir_diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    static const uint8_t source[] = "v:x = \"unterminated";
    static const uint8_t path[] = "lexer-stage-invalid.nxf";
    uint8_t source_payload[256];
    uint8_t source_artifact[512];
    uint8_t* output = NULL;
    size_t source_payload_length = 0u, source_artifact_length = 0u;
    size_t output_length = 0u;
    OirArtifactHeader header;
    OirSectionInput section;
    OccModuleArtifactInputV1 input;
    OccLoadedStageModule loaded;
    OccDiagnosticCollector collector;
    OirArtifactView artifact;
    OirSectionView payload_section, diagnostics_section;
    OirTokenStreamPayloadView token_view;
    OirTokenStreamRecordView token_record;
    OirDiagnosticsPayloadView diagnostics_view;
    OirDiagnosticEntryView diagnostic_entry;
    char error_text[512];

    if (argc != 2) return 2;
    memset(&loaded, 0, sizeof(loaded));
    occ_diagnostics_init(&collector);

    assert(oir_source_unit_write(source_payload, sizeof(source_payload), 0u,
        path, (uint32_t)(sizeof(path) - 1u), source,
        (uint64_t)(sizeof(source) - 1u), &source_payload_length) == OIR_STATUS_OK);
    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_SOURCE_UNIT;
    header.payload_schema_major = OIR_SOURCE_UNIT_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_SOURCE_UNIT_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OCC_HOST;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 77u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = source_payload;
    section.length = source_payload_length;
    assert(oir_artifact_write(source_artifact, sizeof(source_artifact),
        &header, &section, 1u, &source_artifact_length) == OIR_STATUS_OK);

    assert(occ_module_loader_open(&loaded, argv[1], error_text,
        sizeof(error_text)) == OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.stage_id == OCC_STAGE_LEXER);
    assert(loaded.module->descriptor.output_artifact_kind == OIR_KIND_TOKEN_STREAM);

    memset(&input, 0, sizeof(input));
    input.bytes = source_artifact;
    input.length = source_artifact_length;
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 77u,
        &collector, 64u, 4096u, &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(output != NULL && output_length > OIR_HEADER_SIZE);
    assert(occ_diagnostics_count(&collector) == 1u);

    assert(oir_artifact_open(output, output_length, &artifact) == OIR_STATUS_OK);
    assert(artifact.header.artifact_kind == OIR_KIND_TOKEN_STREAM);
    assert(artifact.header.producer_stage_id == OCC_STAGE_LEXER);
    assert(artifact.header.validity == OIR_VALIDITY_VALID);
    assert(artifact.header.job_id == 77u);

    assert(oir_artifact_get_section(&artifact, 0u, &payload_section) == OIR_STATUS_OK);
    assert(oir_artifact_get_section(&artifact, 1u, &diagnostics_section) == OIR_STATUS_OK);
    assert(diagnostics_section.type == OIR_SECTION_DIAGNOSTICS);
    assert(oir_diagnostics_payload_open(diagnostics_section.data,
        (size_t)diagnostics_section.length, &diagnostics_view) == OIR_STATUS_OK);
    assert(diagnostics_view.record_count == 1u);
    assert(oir_diagnostics_payload_get(&diagnostics_view, 0u,
        &diagnostic_entry) == OIR_STATUS_OK);
    assert(diagnostic_entry.stage_id == OCC_STAGE_LEXER);
    assert(diagnostic_entry.span.start_offset == 6u);
    assert(diagnostic_entry.span.start_line == 1u);
    assert(diagnostic_entry.span.start_column == 7u);
    assert(strcmp(diagnostic_entry.message, "Unterminated string literal") == 0);

    assert(oir_token_stream_open(payload_section.data,
        (size_t)payload_section.length, 0u, (uint64_t)(sizeof(source) - 1u),
        diagnostics_view.record_count, &token_view) == OIR_STATUS_OK);
    assert(token_view.token_count == 2u);
    assert(oir_token_stream_get(&token_view, 0u, &token_record) == OIR_STATUS_OK);
    assert(token_record.token_id == 2u);
    assert(token_record.diagnostic_id == 1u);
    assert(oir_token_stream_get(&token_view, 1u, &token_record) == OIR_STATUS_OK);
    assert(token_record.token_id == 1u);

    {
        OccDiagnosticView view;
        assert(occ_diagnostics_get(&collector, 0u, &view));
        assert(view.record->stage_id == OCC_STAGE_LEXER);
        assert(view.record->span.start_offset == 6u);
    }

    free(output);
    occ_module_loader_close(&loaded);
    occ_diagnostics_destroy(&collector);
    puts("Real lexer stage shared-module test passed.");
    return 0;
}
