#include "occ_module_api.h"
#include "diagnostics.h"

#include <string.h>

static const OccStageModuleV1 g_module;

static uint32_t OCC_MODULE_CALL passthrough_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView view;
    OccModuleDiagnosticV1 diagnostic;
    static const char diagnostic_message[]="passthrough stage completed";
    uint32_t status=occ_module_validate_invocation_v1(
        &g_module.descriptor,invocation,result);
    const OccModuleArtifactInputV1* input;
    if(status!=OCC_MODULE_STATUS_OK) return status;
    if(invocation->input_count!=1u) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    input=&invocation->inputs[0];
    if(invocation->output_capacity<input->length) {
        result->bytes_written=0u;
        result->required_output_capacity=input->length;
        result->output_validity=OIR_VALIDITY_NONE;
        result->failure_code=0u;
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }
    if(!invocation->output_buffer) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    status=oir_artifact_open(input->bytes,(size_t)input->length,&view);
    if(status!=OIR_STATUS_OK) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    memset(&diagnostic,0,sizeof(diagnostic));
    diagnostic.struct_size=(uint32_t)sizeof(diagnostic);
    diagnostic.code=9901u;
    diagnostic.severity=OCC_DIAG_NOTE;
    diagnostic.message=diagnostic_message;
    diagnostic.message_length=(uint64_t)(sizeof(diagnostic_message)-1u);
    if(!invocation->diagnostic_sink.report(
        invocation->diagnostic_sink.user_context,&diagnostic)) {
        result->bytes_written=0u;
        result->output_validity=OIR_VALIDITY_NONE;
        result->failure_code=9901u;
        return OCC_MODULE_STATUS_FAILED;
    }
    memcpy(invocation->output_buffer,input->bytes,(size_t)input->length);
    result->bytes_written=input->length;
    result->required_output_capacity=0u;
    result->diagnostics_count=1u;
    result->output_validity=view.header.validity;
    result->failure_code=0u;
    return view.header.validity==OIR_VALIDITY_PARTIAL ?
        OCC_MODULE_STATUS_PARTIAL : OCC_MODULE_STATUS_OK;
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
        (uint64_t)OCC_MODULE_CAP_STATELESS |
        (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "test-ast-passthrough"
    },
    passthrough_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_module;
}
