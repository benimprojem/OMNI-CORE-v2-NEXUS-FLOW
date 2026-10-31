#include "occ_module_api.h"
#include "parser.h"
#include "lexer.h"
#include "oir_payload.h"
#include "oir_token_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define MODULE_HANDLE HMODULE
#define module_open(path) LoadLibraryA(path)
#define module_symbol(handle,name) GetProcAddress(handle,name)
#define module_close(handle) FreeLibrary(handle)
#else
#include <dlfcn.h>
#define MODULE_HANDLE void*
#define module_open(path) dlopen(path,RTLD_NOW|RTLD_LOCAL)
#define module_symbol(handle,name) dlsym(handle,name)
#define module_close(handle) dlclose(handle)
#endif

#define CHECK(x) do { if(!(x)) { \
    fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); goto cleanup; \
} } while(0)

int main(int argc,char** argv) {
    static const uint8_t source_bytes[] =
        "f:topla(a!i32, b!i32) !i32 { ret a + b; }\n"
        "f:main() { v:x = 10; v:y = 20; v:sonuc = topla(x, y); }\n";
    static const uint8_t source_path[]="parser-stage-test.nxf";
    ParserContext* lexer_context=NULL;
    Token frontend_tokens[256];
    OirTokenStreamInput token_inputs[256];
    uint8_t source_payload[1024],source_artifact[1200];
    uint8_t token_payload[OIR_TOKEN_STREAM_HEADER_SIZE+256u*OIR_TOKEN_STREAM_RECORD_SIZE];
    uint8_t token_artifact[OIR_TOKEN_STREAM_HEADER_SIZE+256u*OIR_TOKEN_STREAM_RECORD_SIZE+128u];
    uint8_t* output=NULL;
    size_t source_payload_len=0,source_artifact_len=0,token_payload_len=0,token_artifact_len=0;
    size_t token_count=0,output_capacity=0;
    OirArtifactHeader header;
    OirSectionInput section;
    OccModuleArtifactInputV1 inputs[2];
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;
    OccDiagnosticCollector collector;
    OccModuleDiagnosticHostContextV1 host_context;
    OccModuleDiagnosticSinkV1 sink;
    const OccStageModuleV1* module=NULL;
    OccGetStageModuleV1Fn get_module=NULL;
    OirArtifactView output_view;
    MODULE_HANDLE handle=NULL;
    int exit_code=1;

    if(argc!=2) return 2;
    occ_diagnostics_init(&collector);
    lexer_context=parser_init_source("parser-stage-test.nxf",
        source_bytes,sizeof(source_bytes)-1u);
    CHECK(lexer_context!=NULL);
    for(;;) {
        Token token=lexer_next_token(lexer_context);
        CHECK(token_count<256u);
        frontend_tokens[token_count]=token;
        CHECK(oir_token_stream_input_from_lexer_token(
            &frontend_tokens[token_count],&token_inputs[token_count])==OIR_STATUS_OK);
        ++token_count;
        if(token.type==TOKEN_EOF) break;
        CHECK(token.type!=TOKEN_ERROR);
    }

    CHECK(oir_source_unit_write(source_payload,sizeof(source_payload),0u,
        source_path,(uint32_t)(sizeof(source_path)-1u),
        source_bytes,sizeof(source_bytes)-1u,&source_payload_len)==OIR_STATUS_OK);
    memset(&header,0,sizeof(header));
    header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR;
    header.artifact_kind=OIR_KIND_SOURCE_UNIT;
    header.payload_schema_major=OIR_SOURCE_UNIT_SCHEMA_MAJOR;
    header.payload_schema_minor=OIR_SOURCE_UNIT_SCHEMA_MINOR;
    header.validity=OIR_VALIDITY_VALID;
    header.job_id=42u;
    section.type=OIR_SECTION_PAYLOAD;
    section.version=1u;
    section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=source_payload;
    section.length=source_payload_len;
    CHECK(oir_artifact_write(source_artifact,sizeof(source_artifact),
        &header,&section,1u,&source_artifact_len)==OIR_STATUS_OK);

    CHECK(oir_token_stream_write(token_payload,sizeof(token_payload),0u,
        sizeof(source_bytes)-1u,0u,token_inputs,(uint32_t)token_count,
        &token_payload_len)==OIR_STATUS_OK);
    header.artifact_kind=OIR_KIND_TOKEN_STREAM;
    header.payload_schema_major=OIR_TOKEN_STREAM_SCHEMA_MAJOR;
    header.payload_schema_minor=OIR_TOKEN_STREAM_SCHEMA_MINOR;
    section.data=token_payload;
    section.length=token_payload_len;
    CHECK(oir_artifact_write(token_artifact,sizeof(token_artifact),
        &header,&section,1u,&token_artifact_len)==OIR_STATUS_OK);

    handle=module_open(argv[1]);
    CHECK(handle!=NULL);
    get_module=(OccGetStageModuleV1Fn)module_symbol(handle,OCC_MODULE_API_SYMBOL);
    CHECK(get_module!=NULL);
    module=get_module();
    CHECK(occ_module_validate_descriptor_v1(module)==OCC_MODULE_STATUS_OK);
    CHECK(module->descriptor.stage_id==OCC_STAGE_PARSER);
    CHECK(module->descriptor.output_artifact_kind==OIR_KIND_AST);

    memset(inputs,0,sizeof(inputs));
    inputs[0].bytes=source_artifact; inputs[0].length=source_artifact_len;
    inputs[1].bytes=token_artifact; inputs[1].length=token_artifact_len;
    memset(&invocation,0,sizeof(invocation));
    invocation.struct_size=(uint32_t)sizeof(invocation);
    invocation.input_count=2u;
    invocation.inputs=inputs;
    invocation.job_id=42u;
    CHECK(occ_module_diagnostic_sink_init_v1(&sink,&host_context,&collector,
        OCC_STAGE_PARSER)==OCC_MODULE_STATUS_OK);
    invocation.diagnostic_sink=sink;

    memset(&result,0,sizeof(result));
    result.struct_size=(uint32_t)sizeof(result);
    CHECK(module->process(&invocation,&result)==OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER);
    CHECK(result.required_output_capacity>OIR_HEADER_SIZE);
    output_capacity=(size_t)result.required_output_capacity;
    output=(uint8_t*)malloc(output_capacity);
    CHECK(output!=NULL);
    invocation.output_buffer=output;
    invocation.output_capacity=output_capacity;
    memset(&result,0,sizeof(result));
    result.struct_size=(uint32_t)sizeof(result);
    CHECK(module->process(&invocation,&result)==OCC_MODULE_STATUS_OK);
    CHECK(result.output_validity==OIR_VALIDITY_VALID);
    CHECK(result.bytes_written<=output_capacity);
    CHECK(result.diagnostics_count==0u);
    CHECK(occ_diagnostics_count(&collector)==0u);
    CHECK(oir_artifact_open(output,(size_t)result.bytes_written,&output_view)==OIR_STATUS_OK);
    CHECK(output_view.header.artifact_kind==OIR_KIND_AST);
    CHECK(output_view.header.validity==OIR_VALIDITY_VALID);
    CHECK(output_view.header.job_id==42u);

    /* Invalid source must report exact token coordinates and publish no AST. */
    {
        static const uint8_t bad_source[]="v:x = ;\n";
        static const uint8_t bad_path[]="invalid-parser-stage-test.nxf";
        uint8_t bad_source_payload[256],bad_source_artifact[512];
        uint8_t bad_token_payload[OIR_TOKEN_STREAM_HEADER_SIZE+32u*OIR_TOKEN_STREAM_RECORD_SIZE];
        uint8_t bad_token_artifact[OIR_TOKEN_STREAM_HEADER_SIZE+32u*OIR_TOKEN_STREAM_RECORD_SIZE+128u];
        OirTokenStreamInput bad_inputs[32];
        Token bad_tokens[32];
        size_t bad_count=0,bad_source_payload_len=0,bad_source_artifact_len=0;
        size_t bad_token_payload_len=0,bad_token_artifact_len=0;
        OccDiagnosticView diagnostic_view;
        OirArtifactView rejected_view;
        if(lexer_context) { parser_destroy(lexer_context); lexer_context=NULL; }
        lexer_context=parser_init_source("invalid-parser-stage-test.nxf",
            bad_source,sizeof(bad_source)-1u);
        CHECK(lexer_context!=NULL);
        for(;;) {
            Token token=lexer_next_token(lexer_context);
            CHECK(bad_count<32u);
            bad_tokens[bad_count]=token;
            CHECK(oir_token_stream_input_from_lexer_token(
                &bad_tokens[bad_count],&bad_inputs[bad_count])==OIR_STATUS_OK);
            ++bad_count;
            if(token.type==TOKEN_EOF) break;
            CHECK(token.type!=TOKEN_ERROR);
        }
        CHECK(oir_source_unit_write(bad_source_payload,sizeof(bad_source_payload),0u,
            bad_path,(uint32_t)(sizeof(bad_path)-1u),bad_source,
            sizeof(bad_source)-1u,&bad_source_payload_len)==OIR_STATUS_OK);
        header.artifact_kind=OIR_KIND_SOURCE_UNIT;
        header.payload_schema_major=OIR_SOURCE_UNIT_SCHEMA_MAJOR;
        header.payload_schema_minor=OIR_SOURCE_UNIT_SCHEMA_MINOR;
        section.data=bad_source_payload;
        section.length=bad_source_payload_len;
        CHECK(oir_artifact_write(bad_source_artifact,sizeof(bad_source_artifact),
            &header,&section,1u,&bad_source_artifact_len)==OIR_STATUS_OK);
        CHECK(oir_token_stream_write(bad_token_payload,sizeof(bad_token_payload),
            0u,sizeof(bad_source)-1u,0u,bad_inputs,(uint32_t)bad_count,
            &bad_token_payload_len)==OIR_STATUS_OK);
        header.artifact_kind=OIR_KIND_TOKEN_STREAM;
        header.payload_schema_major=OIR_TOKEN_STREAM_SCHEMA_MAJOR;
        header.payload_schema_minor=OIR_TOKEN_STREAM_SCHEMA_MINOR;
        section.data=bad_token_payload;
        section.length=bad_token_payload_len;
        CHECK(oir_artifact_write(bad_token_artifact,sizeof(bad_token_artifact),
            &header,&section,1u,&bad_token_artifact_len)==OIR_STATUS_OK);
        inputs[0].bytes=bad_source_artifact;
        inputs[0].length=bad_source_artifact_len;
        inputs[1].bytes=bad_token_artifact;
        inputs[1].length=bad_token_artifact_len;
        invocation.output_buffer=NULL;
        invocation.output_capacity=0u;
        memset(&result,0,sizeof(result));
        result.struct_size=(uint32_t)sizeof(result);
        CHECK(module->process(&invocation,&result)==OCC_MODULE_STATUS_FAILED);
        CHECK(result.output_validity==OIR_VALIDITY_NONE);
        CHECK(result.bytes_written==0u);
        CHECK(result.diagnostics_count==1u);
        CHECK(occ_diagnostics_count(&collector)==1u);
        CHECK(occ_diagnostics_get(&collector,0u,&diagnostic_view));
        CHECK(diagnostic_view.record->stage_id==OCC_STAGE_PARSER);
        CHECK(diagnostic_view.record->span.start_offset==6u);
        CHECK(diagnostic_view.record->span.end_offset==7u);
        CHECK(diagnostic_view.record->span.start_line==1u);
        CHECK(diagnostic_view.record->span.start_column==7u);
        CHECK(diagnostic_view.record->span.end_line==1u);
        CHECK(diagnostic_view.record->span.end_column==8u);
        CHECK(oir_artifact_open(output,(size_t)result.bytes_written,&rejected_view)!=OIR_STATUS_OK);
    }
    exit_code=0;
    puts("Real parser stage shared-module test passed.");

cleanup:
    free(output);
    if(handle) module_close(handle);
    if(lexer_context) parser_destroy(lexer_context);
    occ_diagnostics_destroy(&collector);
    return exit_code;
}
