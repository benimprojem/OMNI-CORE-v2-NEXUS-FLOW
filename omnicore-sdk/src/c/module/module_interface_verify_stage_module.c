#include "occ_module_api.h"
#include "oir_module_interface_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OIR_MI_VERIFY_DIAG_INVALID_ARTIFACT = 8701u,
       OIR_MI_VERIFY_DIAG_SECTION_COPY = 8702u,
       OIR_MI_VERIFY_DIAG_OUTPUT_WRITE = 8703u };

static const OccStageModuleV1 g_module_interface_verify_module;

static uint32_t report_failure(const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result, uint32_t code, const char* message) {
    OccModuleDiagnosticV1 diagnostic;
    if (!invocation || !result) return OCC_MODULE_STATUS_FAILED;
    result->failure_code = code;
    if (!invocation->diagnostic_sink.report) return OCC_MODULE_STATUS_FAILED;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
    diagnostic.code = code;
    diagnostic.message_id = code;
    diagnostic.severity = OCC_DIAG_ERROR;
    diagnostic.message = message;
    diagnostic.message_length = strlen(message);
    if (invocation->diagnostic_sink.report(
            invocation->diagnostic_sink.user_context, &diagnostic))
        result->diagnostics_count++;
    return OCC_MODULE_STATUS_FAILED;
}

static uint32_t OCC_MODULE_CALL module_interface_verify_process(
    const OccModuleInvocationV1* invocation, OccModuleResultV1* result) {
    OirModuleInterfaceArtifactView view;
    OirSectionInput* sections = NULL;
    OirArtifactHeader output_header;
    OirStatus status;
    size_t required, written = 0u;
    uint32_t i;
    uint32_t rc = OCC_MODULE_STATUS_FAILED;

    if (!result) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    result->bytes_written = 0u;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE;
    result->failure_code = 0u;
    result->diagnostics_count = 0u;
    memset(&view, 0, sizeof(view));

    if (occ_module_validate_invocation_v1(
            &g_module_interface_verify_module.descriptor,
            invocation, result) != OCC_MODULE_STATUS_OK ||
        invocation->input_count != 1u || !invocation->inputs ||
        !invocation->inputs[0].bytes ||
        invocation->inputs[0].length > (uint64_t)SIZE_MAX ||
        invocation->output_capacity > (uint64_t)SIZE_MAX)
        return OCC_MODULE_STATUS_INVALID_INVOCATION;

    status = oir_module_interface_artifact_open(
        invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length, &view);
    if (status != OIR_STATUS_OK ||
        view.envelope.header.producer_stage_id != OCC_STAGE_SEMANTIC ||
        view.envelope.header.job_id != invocation->job_id) {
        oir_module_interface_artifact_destroy(&view);
        return report_failure(invocation, result,
            OIR_MI_VERIFY_DIAG_INVALID_ARTIFACT,
            "Module Interface Verifier rejected the artifact envelope or payload.");
    }

    required = (size_t)invocation->inputs[0].length;
    if (required > (size_t)invocation->output_capacity) {
        result->required_output_capacity = (uint64_t)required;
        oir_module_interface_artifact_destroy(&view);
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }

    if (view.envelope.section_count) {
        sections = (OirSectionInput*)calloc(
            view.envelope.section_count, sizeof(OirSectionInput));
        if (!sections) {
            oir_module_interface_artifact_destroy(&view);
            return report_failure(invocation, result,
                OIR_MI_VERIFY_DIAG_SECTION_COPY,
                "Module Interface Verifier could not allocate section workspace.");
        }
    }
    for (i = 0u; i < view.envelope.section_count; ++i) {
        OirSectionView section;
        status = oir_artifact_get_section(&view.envelope, i, &section);
        if (status != OIR_STATUS_OK) {
            free(sections);
            oir_module_interface_artifact_destroy(&view);
            return report_failure(invocation, result,
                OIR_MI_VERIFY_DIAG_SECTION_COPY,
                "Module Interface Verifier could not read a validated section.");
        }
        sections[i].type = section.type;
        sections[i].version = section.version;
        sections[i].flags = section.flags;
        sections[i].data = section.data;
        sections[i].length = section.length;
    }

    output_header = view.envelope.header;
    output_header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    status = oir_artifact_write(invocation->output_buffer,
        (size_t)invocation->output_capacity, &output_header,
        sections, view.envelope.section_count, &written);
    free(sections);
    oir_module_interface_artifact_destroy(&view);
    if (status != OIR_STATUS_OK || written != required)
        return report_failure(invocation, result,
            OIR_MI_VERIFY_DIAG_OUTPUT_WRITE,
            "Module Interface Verifier could not publish the validated artifact.");

    result->bytes_written = (uint64_t)written;
    result->output_validity = OIR_VALIDITY_VALID;
    rc = OCC_MODULE_STATUS_OK;
    return rc;
}

static const OccStageModuleV1 g_module_interface_verify_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR, OCC_MODULE_ABI_MINOR,
        0x00010007u, OCC_STAGE_OIR_VERIFY,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE_INTERFACE),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE_INTERFACE),
        OIR_KIND_MODULE_INTERFACE, 1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
            (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_STATELESS |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "occ-module-interface-verifier"
    },
    module_interface_verify_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_module_interface_verify_module;
}
