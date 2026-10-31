#include "occ_module_api.h"
#include "parser.h"
#include "lexer.h"
#include "oir_payload.h"
#include "oir_token_stream.h"
#include "oir_diagnostics.h"

#include <stdlib.h>
#include <string.h>

#define LEXER_STAGE_TOKEN_LIMIT 1000000u

static const OccStageModuleV1 g_lexer_module;

static OirStatus find_payload(
    const OirArtifactView* artifact, uint16_t type, OirSectionView* out
) {
    uint32_t i;
    int found = 0;
    for (i = 0u; i < artifact->section_count; ++i) {
        OirSectionView section;
        OirStatus status = oir_artifact_get_section(artifact, i, &section);
        if (status != OIR_STATUS_OK) return status;
        if (section.type == type) {
            if (found) return OIR_STATUS_DUPLICATE_PAYLOAD;
            *out = section;
            found = 1;
        }
    }
    return found ? OIR_STATUS_OK : OIR_STATUS_MISSING_PAYLOAD;
}

static void report_diagnostics(
    const OccModuleInvocationV1* invocation,
    const OccDiagnosticCollector* collector
) {
    size_t i;
    for (i = 0u; i < occ_diagnostics_count(collector); ++i) {
        OccDiagnosticView view;
        OccModuleDiagnosticV1 diagnostic;
        if (!occ_diagnostics_get(collector, i, &view) || !view.record) return;
        memset(&diagnostic, 0, sizeof(diagnostic));
        diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
        diagnostic.code = view.record->code;
        diagnostic.message_id = view.record->message_id;
        diagnostic.severity = view.record->severity;
        diagnostic.span = view.record->span;
        diagnostic.message = view.message;
        diagnostic.message_length = view.record->message_length;
        diagnostic.hint = view.hint;
        diagnostic.hint_length = view.record->hint_length;
        if (!invocation->diagnostic_sink.report(
            invocation->diagnostic_sink.user_context, &diagnostic)) return;
    }
}

static uint32_t OCC_MODULE_CALL lexer_stage_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView source_artifact;
    OirSectionView source_section;
    OirSourceUnitView source;
    ParserContext* lexer_context = NULL;
    Token* tokens = NULL;
    OirTokenStreamInput* token_inputs = NULL;
    uint8_t *token_payload = NULL, *diagnostics_payload = NULL;
    size_t token_capacity = 0u, token_count = 0u;
    size_t token_payload_capacity = 0u, token_payload_length = 0u;
    size_t diagnostics_payload_capacity = 0u, diagnostics_payload_length = 0u;
    size_t artifact_required = 0u, artifact_written = 0u;
    char* filename = NULL;
    OccDiagnosticCollector diagnostics;
    OirArtifactHeader header;
    OirSectionInput sections[2];
    uint32_t section_count = 1u;
    OirStatus status;
    uint32_t module_status = OCC_MODULE_STATUS_FAILED;
    size_t i;

    occ_diagnostics_init(&diagnostics);
    result->bytes_written = 0u;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE;
    result->failure_code = 0u;
    result->diagnostics_count = 0u;

    if (occ_module_validate_invocation_v1(
        &g_lexer_module.descriptor, invocation, result) != OCC_MODULE_STATUS_OK ||
        invocation->input_count != 1u) {
        module_status = OCC_MODULE_STATUS_INVALID_INVOCATION;
        goto cleanup;
    }
    if (oir_artifact_open(invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length, &source_artifact) != OIR_STATUS_OK ||
        source_artifact.header.artifact_kind != OIR_KIND_SOURCE_UNIT ||
        source_artifact.header.job_id != invocation->job_id ||
        find_payload(&source_artifact, OIR_SECTION_PAYLOAD, &source_section) != OIR_STATUS_OK ||
        oir_source_unit_open(source_section.data, (size_t)source_section.length,
            &source) != OIR_STATUS_OK) {
        module_status = OCC_MODULE_STATUS_INVALID_INVOCATION;
        result->failure_code = 7101u;
        goto cleanup;
    }
#if SIZE_MAX < UINT64_MAX
    if (source.source_length > (uint64_t)SIZE_MAX) {
        module_status = OCC_MODULE_STATUS_INVALID_INVOCATION;
        result->failure_code = 7102u;
        goto cleanup;
    }
#endif
    if ((uint64_t)source.path_length >= (uint64_t)SIZE_MAX) {
        module_status = OCC_MODULE_STATUS_INVALID_INVOCATION;
        result->failure_code = 7102u;
        goto cleanup;
    }

    filename = (char*)malloc((size_t)source.path_length + 1u);
    if (!filename) goto allocation_failure;
    if (source.path_length) memcpy(filename, source.path, source.path_length);
    filename[source.path_length] = '\0';

    lexer_context = parser_init_source(filename, source.source,
        (size_t)source.source_length);
    if (!lexer_context) goto allocation_failure;
    parser_attach_diagnostics(lexer_context, &diagnostics, OCC_STAGE_LEXER);

    token_capacity = 256u;
    tokens = (Token*)malloc(token_capacity * sizeof(*tokens));
    token_inputs = (OirTokenStreamInput*)malloc(token_capacity * sizeof(*token_inputs));
    if (!tokens || !token_inputs) goto allocation_failure;

    for (;;) {
        Token token = lexer_next_token(lexer_context);
        if (token_count == token_capacity) {
            size_t next_capacity;
            Token* next_tokens;
            OirTokenStreamInput* next_inputs;
            if (token_capacity >= LEXER_STAGE_TOKEN_LIMIT) {
                result->failure_code = 7103u;
                goto cleanup;
            }
            next_capacity = token_capacity * 2u;
            if (next_capacity > LEXER_STAGE_TOKEN_LIMIT)
                next_capacity = LEXER_STAGE_TOKEN_LIMIT;
            if (next_capacity > SIZE_MAX / sizeof(*tokens) ||
                next_capacity > SIZE_MAX / sizeof(*token_inputs)) goto allocation_failure;
            next_tokens = (Token*)realloc(tokens, next_capacity * sizeof(*tokens));
            if (!next_tokens) goto allocation_failure;
            tokens = next_tokens;
            next_inputs = (OirTokenStreamInput*)realloc(token_inputs,
                next_capacity * sizeof(*token_inputs));
            if (!next_inputs) goto allocation_failure;
            token_inputs = next_inputs;
            token_capacity = next_capacity;
        }
        tokens[token_count] = token;
        status = oir_token_stream_input_from_lexer_token(
            &tokens[token_count], &token_inputs[token_count]);
        if (status != OIR_STATUS_OK) {
            result->failure_code = 7104u;
            goto cleanup;
        }
        ++token_count;
        if (token.type == TOKEN_EOF) break;
        if (token_count >= LEXER_STAGE_TOKEN_LIMIT) {
            result->failure_code = 7103u;
            goto cleanup;
        }
    }
    if (token_count == 0u || token_count > UINT32_MAX ||
        occ_diagnostics_count(&diagnostics) > UINT32_MAX ||
        diagnostics.allocation_failed) {
        result->failure_code = 7105u;
        goto cleanup;
    }

    if (token_count > (SIZE_MAX - OIR_TOKEN_STREAM_HEADER_SIZE) /
        OIR_TOKEN_STREAM_RECORD_SIZE) goto allocation_failure;
    token_payload_capacity = OIR_TOKEN_STREAM_HEADER_SIZE +
        token_count * OIR_TOKEN_STREAM_RECORD_SIZE;
    token_payload = (uint8_t*)malloc(token_payload_capacity);
    if (!token_payload) goto allocation_failure;
    status = oir_token_stream_write(token_payload, token_payload_capacity,
        source.source_id, source.source_length,
        (uint32_t)occ_diagnostics_count(&diagnostics),
        token_inputs, (uint32_t)token_count, &token_payload_length);
    if (status != OIR_STATUS_OK) {
        result->failure_code = 7106u;
        goto cleanup;
    }

    artifact_required = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE +
        token_payload_length;
    if (diagnostics.count != 0u) {
        if (diagnostics.count >
            (SIZE_MAX - OIR_DIAGNOSTICS_HEADER_SIZE) / OIR_DIAGNOSTICS_RECORD_SIZE)
            goto allocation_failure;
        diagnostics_payload_capacity = OIR_DIAGNOSTICS_HEADER_SIZE +
            diagnostics.count * OIR_DIAGNOSTICS_RECORD_SIZE;
        if (diagnostics.text_length > SIZE_MAX - diagnostics_payload_capacity)
            goto allocation_failure;
        diagnostics_payload_capacity += diagnostics.text_length;
        diagnostics_payload = (uint8_t*)malloc(diagnostics_payload_capacity);
        if (!diagnostics_payload) goto allocation_failure;
        status = oir_diagnostics_payload_write(diagnostics_payload,
            diagnostics_payload_capacity, &diagnostics, &diagnostics_payload_length);
        if (status != OIR_STATUS_OK) {
            result->failure_code = 7107u;
            goto cleanup;
        }
        if (diagnostics_payload_length >
            SIZE_MAX - artifact_required - OIR_SECTION_HEADER_SIZE)
            goto allocation_failure;
        artifact_required += OIR_SECTION_HEADER_SIZE + diagnostics_payload_length;
        section_count = 2u;
    }

    result->required_output_capacity = (uint64_t)artifact_required;
    result->diagnostics_count = (uint32_t)diagnostics.count;
    if (invocation->output_capacity < (uint64_t)artifact_required) {
        module_status = OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
        goto cleanup;
    }

    /* Report only after capacity is sufficient, so a retry cannot duplicate diagnostics. */
    report_diagnostics(invocation, &diagnostics);
    if (diagnostics.count != 0u &&
        invocation->diagnostic_sink.report == NULL) {
        result->failure_code = 7108u;
        goto cleanup;
    }

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_TOKEN_STREAM;
    header.payload_schema_major = OIR_TOKEN_STREAM_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_TOKEN_STREAM_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_LEXER;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = invocation->job_id;

    memset(sections, 0, sizeof(sections));
    sections[0].type = OIR_SECTION_PAYLOAD;
    sections[0].version = 1u;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = token_payload;
    sections[0].length = token_payload_length;
    if (diagnostics_payload) {
        sections[1].type = OIR_SECTION_DIAGNOSTICS;
        sections[1].version = 1u;
        sections[1].flags = 0u;
        sections[1].data = diagnostics_payload;
        sections[1].length = diagnostics_payload_length;
    }
    status = oir_artifact_write(invocation->output_buffer,
        (size_t)invocation->output_capacity, &header, sections,
        section_count, &artifact_written);
    if (status != OIR_STATUS_OK) {
        result->failure_code = 7109u;
        goto cleanup;
    }
    result->bytes_written = (uint64_t)artifact_written;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_VALID;
    result->failure_code = 0u;
    module_status = OCC_MODULE_STATUS_OK;
    goto cleanup;

allocation_failure:
    result->failure_code = 7110u;
    module_status = OCC_MODULE_STATUS_FAILED;
cleanup:
    if (lexer_context) parser_destroy(lexer_context);
    free(filename);
    free(tokens);
    free(token_inputs);
    free(token_payload);
    free(diagnostics_payload);
    occ_diagnostics_destroy(&diagnostics);
    return module_status;
}

static const OccStageModuleV1 g_lexer_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR, OCC_MODULE_ABI_MINOR,
        0x00010002u, OCC_STAGE_LEXER,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT),
        OIR_KIND_TOKEN_STREAM, 1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "nexsus-lexer"
    },
    lexer_stage_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_lexer_module;
}
