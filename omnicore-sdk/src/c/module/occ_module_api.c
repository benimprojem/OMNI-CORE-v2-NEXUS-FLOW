#include "occ_module_api.h"

#include <string.h>

static int valid_artifact_kind(uint32_t kind) {
    return kind > 0u && kind < 64u;
}

uint32_t occ_module_validate_descriptor_v1(const OccStageModuleV1* module) {
    const OccStageDescriptorV1* d;
    if(!module || module->struct_size < sizeof(*module) || module->reserved!=0u ||
        !module->process) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    d=&module->descriptor;
    if(d->struct_size < sizeof(*d) ||
        d->abi_major!=OCC_MODULE_ABI_MAJOR ||
        d->abi_minor>OCC_MODULE_ABI_MINOR ||
        d->module_id==OCC_MODULE_ID_UNASSIGNED ||
        d->stage_id==0u || d->reserved0!=0u || d->reserved1!=0u ||
        !valid_artifact_kind(d->output_artifact_kind) ||
        d->min_input_count>d->max_input_count ||
        (d->required_input_kind_mask & ~d->accepted_input_kind_mask)!=0u ||
        (d->accepted_input_kind_mask & UINT64_C(1))!=0u ||
        !d->module_name || d->module_name[0]=='\\0') {
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
    if(!descriptor || !invocation || !result ||
        descriptor->struct_size < sizeof(*descriptor) ||
        invocation->struct_size < sizeof(*invocation) ||
        result->struct_size < sizeof(*result) ||
        invocation->input_count<descriptor->min_input_count ||
        invocation->input_count>descriptor->max_input_count ||
        (invocation->input_count && !invocation->inputs) ||
        (invocation->output_capacity && !invocation->output_buffer) ||
        invocation->flags!=0u) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    for(i=0;i<invocation->input_count;i++) {
        OirArtifactView view;
        const OccModuleArtifactInputV1* input=&invocation->inputs[i];
        if(input->reserved!=0u || input->flags!=0u ||
            (input->length && !input->bytes) ||
            input->length>(uint64_t)SIZE_MAX ||
            oir_artifact_open(input->bytes,(size_t)input->length,&view)!=OIR_STATUS_OK ||
            !valid_artifact_kind(view.header.artifact_kind)) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        if((descriptor->accepted_input_kind_mask &
            OCC_MODULE_ARTIFACT_KIND_MASK(view.header.artifact_kind))==0u) {
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
