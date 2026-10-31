#include "occ_module_api.h"

static int valid_artifact_kind(uint32_t kind) {
    return kind >= OIR_KIND_SOURCE_UNIT && kind <= OIR_KIND_LINK_INPUT;
}

#define OCC_MODULE_CAPABILITY_MASK_V1 ( \
    (uint64_t)OCC_MODULE_CAP_REENTRANT | \
    (uint64_t)OCC_MODULE_CAP_DETERMINISTIC | \
    (uint64_t)OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT | \
    (uint64_t)OCC_MODULE_CAP_STATELESS )


static int valid_descriptor_fields(const OccStageDescriptorV1* d) {
    const uint64_t known_capabilities=OCC_MODULE_CAPABILITY_MASK_V1;
    const uint64_t known_artifact_kinds=
        (OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_LINK_INPUT+1u)-UINT64_C(2));
    if(!d || d->struct_size < sizeof(*d) ||
        d->abi_major!=OCC_MODULE_ABI_MAJOR ||
        d->abi_minor>OCC_MODULE_ABI_MINOR ||
        d->module_id==OCC_MODULE_ID_UNASSIGNED ||
        d->stage_id==0u || d->module_version_major==0u ||
        d->reserved0!=0u || d->reserved1!=0u ||
        !valid_artifact_kind(d->output_artifact_kind) ||
        d->min_input_count>d->max_input_count ||
        d->accepted_input_kind_mask==0u ||
        (d->required_input_kind_mask & ~d->accepted_input_kind_mask)!=0u ||
        (d->accepted_input_kind_mask & ~known_artifact_kinds)!=0u ||
        (d->capability_flags & ~known_capabilities)!=0u ||
        !d->module_name || d->module_name[0]=='\0') {
        return 0;
    }
    return 1;
}

uint32_t occ_module_validate_descriptor_v1(const OccStageModuleV1* module) {
    if(!module || module->struct_size < sizeof(*module) ||
        module->reserved!=0u || !module->process ||
        !valid_descriptor_fields(&module->descriptor)) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    return OCC_MODULE_STATUS_OK;
}

uint32_t occ_module_validate_invocation_v1(
    const OccStageDescriptorV1* descriptor,
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    uint32_t i;
    uint64_t present_mask=0u;
    if(!valid_descriptor_fields(descriptor) || !invocation || !result ||
        invocation->struct_size < sizeof(*invocation) ||
        result->struct_size < sizeof(*result) ||
        invocation->input_count<descriptor->min_input_count ||
        invocation->input_count>descriptor->max_input_count ||
        (invocation->input_count && !invocation->inputs) ||
        (invocation->output_capacity && !invocation->output_buffer) ||
        invocation->flags!=0u) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
#if SIZE_MAX < UINT64_MAX
    if(invocation->output_capacity>(uint64_t)SIZE_MAX) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
#endif
    for(i=0;i<invocation->input_count;i++) {
        OirArtifactView view;
        const OccModuleArtifactInputV1* input=&invocation->inputs[i];
        if(input->reserved!=0u || input->flags!=0u || !input->bytes) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
#if SIZE_MAX < UINT64_MAX
        if(input->length>(uint64_t)SIZE_MAX) return OCC_MODULE_STATUS_INVALID_INVOCATION;
#endif
        if(oir_artifact_open(input->bytes,(size_t)input->length,&view)!=OIR_STATUS_OK ||
            !valid_artifact_kind(view.header.artifact_kind)) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        if((descriptor->accepted_input_kind_mask &
            OCC_MODULE_ARTIFACT_KIND_MASK(view.header.artifact_kind))==0u) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        if(view.header.validity!=OIR_VALIDITY_VALID &&
            !(view.header.validity==OIR_VALIDITY_PARTIAL &&
              (descriptor->capability_flags&OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT)!=0u)) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        if(invocation->job_id!=0u && view.header.job_id!=0u &&
            invocation->job_id!=view.header.job_id) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        present_mask |= OCC_MODULE_ARTIFACT_KIND_MASK(view.header.artifact_kind);
    }
    if((present_mask & descriptor->required_input_kind_mask)!=
        descriptor->required_input_kind_mask) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    return OCC_MODULE_STATUS_OK;
}
