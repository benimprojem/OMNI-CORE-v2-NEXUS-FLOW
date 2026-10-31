#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_api.h"
#include "../core/diagnostics.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static uint32_t OCC_MODULE_CALL mock_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    if(!invocation || !result ||
        invocation->struct_size<sizeof(OccModuleInvocationV1) ||
        result->struct_size<sizeof(OccModuleResultV1) ||
        (invocation->input_count && !invocation->inputs) ||
        !invocation->output_buffer) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    if(invocation->output_capacity<64u) {
        result->bytes_written=0u;
        result->required_output_capacity=64u;
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }
    memset(invocation->output_buffer,0x5a,64u);
    result->bytes_written=64u;
    result->required_output_capacity=0u;
    result->output_validity=OIR_VALIDITY_VALID;
    return OCC_MODULE_STATUS_OK;
}

int main(void) {
    OccStageModuleV1 module;
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;
    OccModuleArtifactInputV1 inputs[2];
    uint8_t output[64];

    memset(&module,0,sizeof(module));
    module.struct_size=(uint32_t)sizeof(module);
    module.descriptor.struct_size=(uint32_t)sizeof(module.descriptor);
    module.descriptor.abi_major=OCC_MODULE_ABI_MAJOR;
    module.descriptor.abi_minor=OCC_MODULE_ABI_MINOR;
    module.descriptor.module_id=OCC_MODULE_ID_UNASSIGNED;
    module.descriptor.stage_id=OCC_STAGE_PARSER;
    module.descriptor.accepted_input_kind_mask=
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM);
    module.descriptor.required_input_kind_mask=
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM);
    module.descriptor.output_artifact_kind=OIR_KIND_AST;
    module.descriptor.min_input_count=2u;
    module.descriptor.max_input_count=3u;
    module.descriptor.capability_flags=
        OCC_MODULE_CAP_REENTRANT | OCC_MODULE_CAP_DETERMINISTIC;
    module.descriptor.module_name="parser-stage-draft";
    module.process=mock_process;

    assert(module.struct_size>=sizeof(OccStageModuleV1));
    assert(module.descriptor.struct_size>=sizeof(OccStageDescriptorV1));
    assert(module.descriptor.abi_major==1u && module.descriptor.abi_minor==0u);
    assert(module.descriptor.stage_id==OCC_STAGE_PARSER);
    assert((module.descriptor.required_input_kind_mask &
        ~module.descriptor.accepted_input_kind_mask)==0u);
    assert(module.descriptor.output_artifact_kind==OIR_KIND_AST);
    assert(sizeof(((OccModuleInvocationV1*)0)->job_id)==8u);
    assert(sizeof(((OccModuleArtifactInputV1*)0)->length)==8u);
    assert(offsetof(OccModuleInvocationV1,output_capacity)>
        offsetof(OccModuleInvocationV1,output_buffer));

    memset(inputs,0,sizeof(inputs));
    memset(&invocation,0,sizeof(invocation));
    memset(&result,0,sizeof(result));
    invocation.struct_size=(uint32_t)sizeof(invocation);
    invocation.input_count=2u;
    invocation.inputs=inputs;
    invocation.output_buffer=output;
    invocation.output_capacity=32u;
    invocation.job_id=17u;
    result.struct_size=(uint32_t)sizeof(result);

    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER);
    assert(result.bytes_written==0u && result.required_output_capacity==64u);

    invocation.output_capacity=sizeof(output);
    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_OK);
    assert(result.bytes_written==64u);
    assert(result.output_validity==OIR_VALIDITY_VALID);
    assert(output[0]==0x5au && output[63]==0x5au);

    invocation.struct_size=0u;
    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_INVALID_INVOCATION);

    return 0;
}
