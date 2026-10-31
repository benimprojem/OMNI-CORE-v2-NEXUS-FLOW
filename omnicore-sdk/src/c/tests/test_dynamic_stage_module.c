#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_api.h"
#include "diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

static size_t make_ast_artifact(uint8_t* output,size_t capacity) {
    static const uint8_t payload[]={0x4fu,0x49u,0x52u,0x2du,0x41u,0x53u,0x54u};
    OirArtifactHeader header;
    OirSectionInput section;
    size_t written=0u;
    memset(&header,0,sizeof(header));
    header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR;
    header.artifact_kind=OIR_KIND_AST;
    header.payload_schema_major=1u;
    header.payload_schema_minor=0u;
    header.producer_stage_id=OCC_STAGE_PARSER;
    header.validity=OIR_VALIDITY_VALID;
    header.job_id=99u;
    memset(&section,0,sizeof(section));
    section.type=OIR_SECTION_PAYLOAD;
    section.version=1u;
    section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=payload;
    section.length=sizeof(payload);
    assert(oir_artifact_write(output,capacity,&header,&section,1u,&written)==OIR_STATUS_OK);
    return written;
}

int main(int argc,char** argv) {
    uint8_t input_bytes[128],output_bytes[128];
    size_t input_length;
    OccModuleArtifactInputV1 input;
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;
    OccGetStageModuleV1Fn get_api=NULL;
    const OccStageModuleV1* module;
    OirArtifactView output_view;
    OccDiagnosticCollector collector;
    OccModuleDiagnosticHostContextV1 host_diag;
    OccDiagnosticView diagnostic_view;

    if(argc!=2) return 2;
    input_length=make_ast_artifact(input_bytes,sizeof(input_bytes));
    memset(output_bytes,0xa5,sizeof(output_bytes));
    occ_diagnostics_init(&collector);

#if defined(_WIN32)
    {
        HMODULE handle=LoadLibraryA(argv[1]);
        FARPROC symbol;
        if(!handle) return 3;
        symbol=GetProcAddress(handle,OCC_MODULE_API_SYMBOL);
        if(!symbol) { FreeLibrary(handle); return 4; }
        assert(sizeof(symbol)==sizeof(get_api));
        memcpy(&get_api,&symbol,sizeof(get_api));
        module=get_api();
        assert(module!=NULL);
        assert(occ_module_validate_descriptor_v1(module)==OCC_MODULE_STATUS_OK);
        memset(&input,0,sizeof(input));
        input.bytes=input_bytes; input.length=input_length;
        memset(&invocation,0,sizeof(invocation));
        invocation.struct_size=(uint32_t)sizeof(invocation);
        invocation.input_count=1u; invocation.inputs=&input;
        invocation.output_buffer=output_bytes;
        invocation.output_capacity=input_length-1u;
        invocation.job_id=99u;
        assert(occ_module_diagnostic_sink_init_v1(
            &invocation.diagnostic_sink,&host_diag,&collector,module->descriptor.stage_id)==
            OCC_MODULE_STATUS_OK);
        memset(&result,0,sizeof(result)); result.struct_size=(uint32_t)sizeof(result);
        assert(module->process(&invocation,&result)==OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER);
        assert(result.bytes_written==0u && result.required_output_capacity==input_length);
        assert(output_bytes[0]==0xa5u);
        invocation.output_capacity=sizeof(output_bytes);
        assert(module->process(&invocation,&result)==OCC_MODULE_STATUS_OK);
        assert(result.bytes_written==input_length);
        assert(result.diagnostics_count==1u);
        assert(occ_diagnostics_count(&collector)==1u);
        assert(occ_diagnostics_get(&collector,0u,&diagnostic_view));
        assert(diagnostic_view.record->stage_id==OCC_STAGE_PARSER);
        assert(diagnostic_view.record->code==9901u);
        assert(diagnostic_view.record->severity==OCC_DIAG_NOTE);
        assert(strcmp(diagnostic_view.message,"passthrough stage completed")==0);
        assert(memcmp(input_bytes,output_bytes,input_length)==0);
        assert(oir_artifact_open(output_bytes,(size_t)result.bytes_written,&output_view)==OIR_STATUS_OK);
        assert(output_view.header.artifact_kind==OIR_KIND_AST);
        occ_diagnostics_destroy(&collector);
        assert(FreeLibrary(handle));
    }
#else
    {
        void* handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        void* symbol;
        if(!handle) { fprintf(stderr,"%s\n",dlerror()); return 3; }
        symbol=dlsym(handle,OCC_MODULE_API_SYMBOL);
        if(!symbol) { dlclose(handle); return 4; }
        assert(sizeof(symbol)==sizeof(get_api));
        memcpy(&get_api,&symbol,sizeof(get_api));
        module=get_api();
        assert(module!=NULL);
        assert(occ_module_validate_descriptor_v1(module)==OCC_MODULE_STATUS_OK);
        memset(&input,0,sizeof(input));
        input.bytes=input_bytes; input.length=input_length;
        memset(&invocation,0,sizeof(invocation));
        invocation.struct_size=(uint32_t)sizeof(invocation);
        invocation.input_count=1u; invocation.inputs=&input;
        invocation.output_buffer=output_bytes;
        invocation.output_capacity=input_length-1u;
        invocation.job_id=99u;
        memset(&result,0,sizeof(result)); result.struct_size=(uint32_t)sizeof(result);
        assert(module->process(&invocation,&result)==OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER);
        assert(result.bytes_written==0u && result.required_output_capacity==input_length);
        assert(output_bytes[0]==0xa5u);
        invocation.output_capacity=sizeof(output_bytes);
        assert(module->process(&invocation,&result)==OCC_MODULE_STATUS_OK);
        assert(result.bytes_written==input_length);
        assert(memcmp(input_bytes,output_bytes,input_length)==0);
        assert(oir_artifact_open(output_bytes,(size_t)result.bytes_written,&output_view)==OIR_STATUS_OK);
        assert(output_view.header.artifact_kind==OIR_KIND_AST);
        occ_diagnostics_destroy(&collector);
        assert(dlclose(handle)==0);
    }
#endif
    puts("Dynamic stage module load/process/unload passed.");
    return 0;
}
