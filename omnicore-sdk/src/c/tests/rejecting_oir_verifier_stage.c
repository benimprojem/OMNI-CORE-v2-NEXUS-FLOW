# Test-only verifier replacement used to prove the host stops before Codegen.
#include "occ_module_api.h"

#include <string.h>

static uint32_t OCC_MODULE_CALL reject_oir(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
);

static const OccStageModuleV1 g_rejecting_verifier = {
    (uint32_t)sizeof(OccStageModuleV1),
    0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR,
        OCC_MODULE_ABI_MINOR,
        0x00010006u,
        OCC_STAGE_OIR_VERIFY,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OIR_KIND_MODULE,
        1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
            (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_STATELESS |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "occ-test-rejecting-oir-verifier"
    },
    reject_oir
};

static uint32_t OCC_MODULE_CALL reject_oir(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    static const char message[] = "Test verifier deliberately rejected OIR_MODULE.";
    OccModuleDiagnosticV1 diagnostic;
    uint32_t status = occ_module_validate_invocation_v1(
        &g_rejecting_verifier.descriptor, invocation, result);
    if (status != OCC_MODULE_STATUS_OK) return status;

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
    diagnostic.code = 9902u;
    diagnostic.severity = OCC_DIAG_ERROR;
    diagnostic.message = message;
    diagnostic.message_length = (uint64_t)(sizeof(message) - 1u);
    (void)invocation->diagnostic_sink.report(
        invocation->diagnostic_sink.user_context, &diagnostic);

    result->bytes_written = 0u;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE;
    result->failure_code = 9902u;
    return OCC_MODULE_STATUS_FAILED;
}

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_rejecting_verifier;
}
