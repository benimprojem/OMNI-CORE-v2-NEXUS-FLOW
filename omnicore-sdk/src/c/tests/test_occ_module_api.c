#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t OCC_MODULE_CALL dummy_process(
    const OccModuleInvocationV1* invocation, OccModuleResultV1* result
) {
    (void)invocation; (void)result;
    return OCC_MODULE_STATUS_OK;
}

int main(void) {
    static const uint8_t payload[]={0x41u,0x53u,0x54u};
    uint8_t artifact[128];
    size_t artifact_length=0u;
    OirArtifactHeader header;
    OirSectionInput section;
    OccStageModuleV1 module;
    OccModuleArtifactInputV1 input;
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;

    memset(&header,0,sizeof(header));
    header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR;
    header.artifact_kind=OIR_KIND_AST;
    header.payload_schema_major=1u;
    header.producer_stage_id=1u;
    header.validity=OIR_VALIDITY_VALID;
    header.job_id=19u;
    memset(&section,0,sizeof(section));
    section.type=OIR_SECTION_PAYLOAD;
    section.version=1u;
    section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=payload;
    section.length=sizeof(payload);
    assert(oir_artifact_write(artifact,sizeof(artifact),&header,&section,1u,
        &artifact_length)==OIR_STATUS_OK);

    memset(&module,0,sizeof(module));
    module.struct_size=sizeof(module);
    module.descriptor.struct_size=sizeof(module.descriptor);
    module.descriptor.abi_major=OCC_MODULE_ABI_MAJOR;
    module.descriptor.abi_minor=OCC_MODULE_ABI_MINOR;
    module.descriptor.module_id=1u;
    module.descriptor.stage_id=1u;
    module.descriptor.module_version_major=1u;
    module.descriptor.accepted_input_kind_mask=OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST);
    module.descriptor.required_input_kind_mask=OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST);
    module.descriptor.output_artifact_kind=OIR_KIND_SEMANTIC_AST;
    module.descriptor.min_input_count=1u;
    module.descriptor.max_input_count=1u;
    module.descriptor.module_name="semantic";
    module.process=dummy_process;
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_OK);

    input.bytes=artifact;
    input.length=artifact_length;
    input.flags=0u;
    input.reserved=0u;
    memset(&invocation,0,sizeof(invocation));
    invocation.struct_size=sizeof(invocation);
    invocation.input_count=1u;
    invocation.inputs=&input;
    memset(&result,0,sizeof(result));
    result.struct_size=sizeof(result);
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_OK);

    module.descriptor.abi_major++;
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.abi_major=OCC_MODULE_ABI_MAJOR;
    module.descriptor.required_input_kind_mask=OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM);
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.required_input_kind_mask=OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST);
    module.descriptor.capability_flags=UINT64_C(1)<<20;
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.capability_flags=0u;
    module.descriptor.output_artifact_kind=63u;
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.output_artifact_kind=OIR_KIND_SEMANTIC_AST;
    input.bytes=NULL;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    input.bytes=artifact;
    input.flags=1u;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    input.flags=0u;
    invocation.input_count=0u;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    assert(OCC_MODULE_ARTIFACT_KIND_MASK(0u)==0u);
    assert(OCC_MODULE_ARTIFACT_KIND_MASK(64u)==0u);

    puts("OCC stage ABI validation tests passed.");
    return 0;
}
