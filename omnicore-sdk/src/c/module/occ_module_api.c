#include "occ_module_api.h"

#include <limits.h>
#include <string.h>

static int valid_artifact_kind(uint32_t kind) {
    return kind >= OIR_KIND_SOURCE_UNIT && kind <= OIR_KIND_MODULE_INTERFACE;
}

#define OCC_MODULE_CAPABILITY_MASK_V1 ( \
    (uint64_t)OCC_MODULE_CAP_REENTRANT | \
    (uint64_t)OCC_MODULE_CAP_DETERMINISTIC | \
    (uint64_t)OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT | \
    (uint64_t)OCC_MODULE_CAP_STATELESS | \
    (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING )


static int valid_descriptor_fields(const OccStageDescriptorV1* d) {
    const uint64_t known_capabilities=OCC_MODULE_CAPABILITY_MASK_V1;
    const uint64_t known_artifact_kinds=
        (OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE_INTERFACE+1u)-UINT64_C(2));
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

static uint32_t OCC_MODULE_CALL host_report_diagnostic(
    void* user_context,
    const OccModuleDiagnosticV1* diagnostic
) {
    OccModuleDiagnosticHostContextV1* context =
        (OccModuleDiagnosticHostContextV1*)user_context;
    OccDiagnosticInput input;
    size_t message_length, hint_length;

    if(!context || context->struct_size < sizeof(*context) ||
        !context->collector || context->stage_id == 0u ||
        !diagnostic || diagnostic->struct_size < sizeof(*diagnostic) ||
        diagnostic->flags != 0u ||
        diagnostic->reserved[0] != 0u ||
        diagnostic->reserved[1] != 0u ||
        diagnostic->reserved[2] != 0u ||
#if SIZE_MAX < UINT64_MAX
        diagnostic->message_length > (uint64_t)SIZE_MAX ||
        diagnostic->hint_length > (uint64_t)SIZE_MAX ||
#endif
        (!diagnostic->message && diagnostic->message_length != 0u) ||
        (!diagnostic->hint && diagnostic->hint_length != 0u)) {
        return 0u;
    }
    message_length = (size_t)diagnostic->message_length;
    hint_length = (size_t)diagnostic->hint_length;
    memset(&input, 0, sizeof(input));
    input.code = diagnostic->code;
    /* The host supplies stage identity; a module cannot spoof another stage. */
    input.stage_id = context->stage_id;
    input.message_id = diagnostic->message_id;
    if(diagnostic->severity > UINT8_MAX) return 0u;
    input.severity = (uint8_t)diagnostic->severity;
    input.span = diagnostic->span;
    input.message = diagnostic->message;
    input.message_length = message_length;
    input.hint = diagnostic->hint;
    input.hint_length = hint_length;
    return occ_diagnostics_add(context->collector, &input) ? 1u : 0u;
}

uint32_t occ_module_diagnostic_sink_init_v1(
    OccModuleDiagnosticSinkV1* sink,
    OccModuleDiagnosticHostContextV1* context,
    OccDiagnosticCollector* collector,
    uint32_t stage_id
) {
    if(!sink || !context || !collector || stage_id == 0u) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    memset(context, 0, sizeof(*context));
    context->struct_size = (uint32_t)sizeof(*context);
    context->stage_id = stage_id;
    context->collector = collector;
    memset(sink, 0, sizeof(*sink));
    sink->struct_size = (uint32_t)sizeof(*sink);
    sink->user_context = context;
    sink->report = host_report_diagnostic;
    return OCC_MODULE_STATUS_OK;
}

uint32_t occ_module_validate_descriptor_v1(const OccStageModuleV1* module) {
    if(!module || module->struct_size < sizeof(*module) ||
        module->reserved!=0u || !module->process ||
        !valid_descriptor_fields(&module->descriptor)) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    return OCC_MODULE_STATUS_OK;
}


static int valid_diagnostic_sink(
    const OccStageDescriptorV1* descriptor,
    const OccModuleDiagnosticSinkV1* sink
) {
    uint32_t i;
    if(!descriptor || !sink) return 0;
    if((descriptor->capability_flags & OCC_MODULE_CAP_DIAGNOSTIC_REPORTING) == 0u) {
        if(sink->struct_size != 0u || sink->flags != 0u ||
            sink->user_context != NULL || sink->report != NULL) return 0;
        for(i=0u;i<4u;i++) if(sink->reserved[i] != 0u) return 0;
        return 1;
    }
    if(sink->struct_size < sizeof(*sink) || sink->flags != 0u ||
        !sink->user_context || !sink->report) return 0;
    for(i=0u;i<4u;i++) if(sink->reserved[i] != 0u) return 0;
    return 1;
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
        invocation->flags!=0u ||
        !valid_diagnostic_sink(descriptor, &invocation->diagnostic_sink)) {
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
