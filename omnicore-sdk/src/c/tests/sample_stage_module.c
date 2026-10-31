#include "occ_module_api.h"
#include "diagnostics.h"

#include <string.h>

static uint32_t OCC_MODULE_CALL passthrough_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView view;
    uint32_t status=occ_module_validate_invocation_v1(
        &((const OccStageModuleV1*)0)->descriptor,invocation,result);
    (void)status;
    return OCC_MODULE_STATUS_FAILED;
}

static const OccStageModuleV1 g_module = {
    (uint32_t)sizeof(OccStageModuleV1),
    0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR,
        OCC_MODULE_ABI_MINOR,
        0x7fff0001u,
        OCC_STAGE_PARSER,
        1u,0u,0u,0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST),
        OIR_KIND_AST,
        1u,1u,0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
        (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
        (uint64_t)OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT |
        (uint64_t)OCC_MODULE_CAP_STATELESS,
        "test-ast-passthrough"
    },
    passthrough_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_module;
}
