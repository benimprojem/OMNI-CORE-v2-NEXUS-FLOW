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

static size_t write_test_artifact(
    uint32_t kind,uint8_t validity,uint64_t job_id,uint8_t* output,size_t capacity
) {
    static const uint8_t payload_byte=0x7bu;
    OirArtifactHeader header;
    OirSectionInput section;
    size_t written=0u;
    memset(&header,0,sizeof(header));
    header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR;
    header.artifact_kind=kind;
    header.payload_schema_major=1u;
    header.payload_schema_minor=0u;
    header.producer_stage_id=OCC_STAGE_LEXER;
    header.validity=validity;
    header.job_id=job_id;
    memset(&section,0,sizeof(section));
    section.type=OIR_SECTION_PAYLOAD;
    section.version=1u;
    section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=&payload_byte;
    section.length=1u;
    assert(oir_artifact_write(output,capacity,&header,&section,1u,&written)==OIR_STATUS_OK);
    return written;
}

int main(void) {
    OccStageModuleV1 module;
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;
    OccModuleArtifactInputV1 inputs[2];
    uint8_t source_artifact[64],token_artifact[64],wrong_artifact[64];
    uint8_t partial_artifact[64],output[64];
    size_t source_length,token_length,wrong_length,partial_length;
    OccDiagnosticCollector collector;
    OccModuleDiagnosticHostContextV1 host_diag;
    OccModuleDiagnosticV1 diagnostic;
    OccDiagnosticView diagnostic_view;

    memset(&module,0,sizeof(module));
    module.struct_size=(uint32_t)sizeof(module);
    module.descriptor.struct_size=(uint32_t)sizeof(module.descriptor);
    module.descriptor.abi_major=OCC_MODULE_ABI_MAJOR;
    module.descriptor.abi_minor=OCC_MODULE_ABI_MINOR;
    module.descriptor.module_id=1u;
    module.descriptor.stage_id=OCC_STAGE_PARSER;
    module.descriptor.module_version_major=1u;
    module.descriptor.accepted_input_kind_mask=
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM);
    module.descriptor.required_input_kind_mask=
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM);
    module.descriptor.output_artifact_kind=OIR_KIND_AST;
    module.descriptor.min_input_count=2u;
    module.descriptor.max_input_count=2u;
    module.descriptor.capability_flags=
        OCC_MODULE_CAP_REENTRANT | OCC_MODULE_CAP_DETERMINISTIC |
        OCC_MODULE_CAP_DIAGNOSTIC_REPORTING;
    module.descriptor.module_name="parser-stage-draft";
    module.process=mock_process;

    assert(module.struct_size>=sizeof(OccStageModuleV1));
    assert(module.descriptor.struct_size>=sizeof(OccStageDescriptorV1));
    assert(module.descriptor.abi_major==OCC_MODULE_ABI_MAJOR);
    assert(module.descriptor.stage_id==OCC_STAGE_PARSER);
    assert((module.descriptor.required_input_kind_mask &
        ~module.descriptor.accepted_input_kind_mask)==0u);
    assert(module.descriptor.output_artifact_kind==OIR_KIND_AST);
    assert(sizeof(((OccModuleInvocationV1*)0)->job_id)==8u);
    assert(sizeof(((OccModuleArtifactInputV1*)0)->length)==8u);
    assert(offsetof(OccModuleInvocationV1,output_capacity)>
        offsetof(OccModuleInvocationV1,output_buffer));
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_OK);

    source_length=write_test_artifact(OIR_KIND_SOURCE_UNIT,OIR_VALIDITY_VALID,17u,
        source_artifact,sizeof(source_artifact));
    token_length=write_test_artifact(OIR_KIND_TOKEN_STREAM,OIR_VALIDITY_VALID,17u,
        token_artifact,sizeof(token_artifact));
    wrong_length=write_test_artifact(OIR_KIND_AST,OIR_VALIDITY_VALID,17u,
        wrong_artifact,sizeof(wrong_artifact));
    partial_length=write_test_artifact(OIR_KIND_SOURCE_UNIT,OIR_VALIDITY_PARTIAL,17u,
        partial_artifact,sizeof(partial_artifact));

    memset(inputs,0,sizeof(inputs));
    inputs[0].bytes=source_artifact; inputs[0].length=source_length;
    inputs[1].bytes=token_artifact; inputs[1].length=token_length;
    memset(&invocation,0,sizeof(invocation));
    memset(&result,0,sizeof(result));
    invocation.struct_size=(uint32_t)sizeof(invocation);
    invocation.input_count=2u;
    invocation.inputs=inputs;
    invocation.output_buffer=output;
    invocation.output_capacity=32u;
    invocation.job_id=17u;
    result.struct_size=(uint32_t)sizeof(result);
    occ_diagnostics_init(&collector);
    assert(occ_module_diagnostic_sink_init_v1(
        &invocation.diagnostic_sink,&host_diag,&collector,OCC_STAGE_PARSER)==
        OCC_MODULE_STATUS_OK);
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_OK);
    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER);
    assert(result.bytes_written==0u && result.required_output_capacity==64u);

    invocation.output_capacity=sizeof(output);
    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_OK);
    assert(result.bytes_written==64u && result.output_validity==OIR_VALIDITY_VALID);
    assert(output[0]==0x5au && output[63]==0x5au);

    /* DLL diagnostics are copied into OCC storage with host-assigned stage ID. */
    memset(&diagnostic,0,sizeof(diagnostic));
    diagnostic.struct_size=(uint32_t)sizeof(diagnostic);
    diagnostic.code=2301u;
    diagnostic.message_id=91u;
    diagnostic.severity=OCC_DIAG_ERROR;
    diagnostic.span.source_id=7u;
    diagnostic.span.start_offset=15u;
    diagnostic.span.end_offset=16u;
    diagnostic.span.start_line=2u;
    diagnostic.span.start_column=7u;
    diagnostic.span.end_line=2u;
    diagnostic.span.end_column=8u;
    diagnostic.span.has_location=1u;
    diagnostic.message="unexpected token";
    diagnostic.message_length=(uint64_t)strlen(diagnostic.message);
    diagnostic.hint="expected expression";
    diagnostic.hint_length=(uint64_t)strlen(diagnostic.hint);
    assert(invocation.diagnostic_sink.report(
        invocation.diagnostic_sink.user_context,&diagnostic)==1u);
    assert(occ_diagnostics_count(&collector)==1u);
    assert(occ_diagnostics_get(&collector,0u,&diagnostic_view));
    assert(diagnostic_view.record->code==2301u);
    assert(diagnostic_view.record->stage_id==OCC_STAGE_PARSER);
    assert(diagnostic_view.record->message_id==91u);
    assert(diagnostic_view.record->span.start_offset==15u);
    assert(diagnostic_view.record->span.end_offset==16u);
    assert(diagnostic_view.record->span.start_line==2u);
    assert(diagnostic_view.record->span.start_column==7u);
    assert(diagnostic_view.record->span.end_line==2u);
    assert(diagnostic_view.record->span.end_column==8u);
    assert(strcmp(diagnostic_view.message,"unexpected token")==0);
    assert(strcmp(diagnostic_view.hint,"expected expression")==0);

    diagnostic.reserved[1]=1u;
    assert(invocation.diagnostic_sink.report(
        invocation.diagnostic_sink.user_context,&diagnostic)==0u);
    diagnostic.reserved[1]=0u;
    invocation.diagnostic_sink.flags=1u;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    invocation.diagnostic_sink.flags=0u;

    /* Artifact kind, required input presence and job identity are host-checked. */
    inputs[1].bytes=wrong_artifact; inputs[1].length=wrong_length;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    inputs[1].bytes=token_artifact; inputs[1].length=token_length;
    invocation.input_count=1u;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    invocation.input_count=2u;
    inputs[1].bytes=wrong_artifact; inputs[1].length=wrong_length;
    inputs[1].bytes=token_artifact; inputs[1].length=token_length;

    token_length=write_test_artifact(OIR_KIND_TOKEN_STREAM,OIR_VALIDITY_VALID,18u,
        token_artifact,sizeof(token_artifact));
    inputs[1].length=token_length;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    token_length=write_test_artifact(OIR_KIND_TOKEN_STREAM,OIR_VALIDITY_VALID,17u,
        token_artifact,sizeof(token_artifact));
    inputs[1].length=token_length;

    inputs[0].bytes=partial_artifact; inputs[0].length=partial_length;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.capability_flags|=OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_OK);
    inputs[0].bytes=source_artifact; inputs[0].length=source_length;

    module.descriptor.module_id=OCC_MODULE_ID_UNASSIGNED;
    assert(occ_module_validate_descriptor_v1(&module)==OCC_MODULE_STATUS_INVALID_INVOCATION);
    module.descriptor.module_id=1u;
    invocation.flags=1u;
    assert(occ_module_validate_invocation_v1(&module.descriptor,&invocation,&result)==
        OCC_MODULE_STATUS_INVALID_INVOCATION);
    invocation.flags=0u;
    invocation.struct_size=0u;
    assert(module.process(&invocation,&result)==OCC_MODULE_STATUS_INVALID_INVOCATION);

    occ_diagnostics_destroy(&collector);
    return 0;
}
