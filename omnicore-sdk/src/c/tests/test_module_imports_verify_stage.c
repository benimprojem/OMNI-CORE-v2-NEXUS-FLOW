#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_loader.h"
#include "oir_module_imports_artifact.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char** argv) {
    OccModuleImportBinding binding;
    OirModuleImports imports;
    OirModuleImportsArtifactView verified;
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    uint8_t input_bytes[2048];
    uint8_t* output_bytes=NULL;
    size_t input_length=0u,output_length=0u;
    if(argc!=2) return 2;
    memset(&binding,0,sizeof(binding));
    memset(&imports,0,sizeof(imports));
    memset(&verified,0,sizeof(verified));
    memset(&loaded,0,sizeof(loaded));
    binding.use_ordinal=1u; binding.requested_path="dep";
    binding.dependency_identity="file:///dep.nxf";
    binding.alias="d"; binding.namespace_name="ns";
    assert(oir_module_imports_build("file:///root.nxf",&binding,1u,&imports)==OIR_STATUS_OK);
    assert(oir_module_imports_artifact_write(&imports,7788u,input_bytes,
        sizeof(input_bytes),&input_length)==OIR_STATUS_OK);
    occ_diagnostics_init(&diagnostics);
    assert(occ_module_loader_open(&loaded,argv[1],NULL,0u)==OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.output_artifact_kind==OIR_KIND_MODULE_IMPORTS);
    {
        OccModuleArtifactInputV1 input;
        memset(&input,0,sizeof(input)); input.bytes=input_bytes; input.length=input_length;
        assert(occ_module_loader_invoke(&loaded,&input,1u,7788u,&diagnostics,
            16u,4096u,&output_bytes,&output_length)==OCC_MODULE_LOADER_OK);
    }
    assert(output_bytes && output_length==input_length);
    assert(oir_module_imports_artifact_open(output_bytes,output_length,&verified)==OIR_STATUS_OK);
    assert(verified.envelope.header.producer_stage_id==OCC_STAGE_OIR_VERIFY);
    assert(verified.envelope.header.job_id==7788u);
    assert(occ_diagnostics_count(&diagnostics)==0u);
    oir_module_imports_artifact_destroy(&verified);
    free(output_bytes); output_bytes=NULL;
    occ_module_loader_close(&loaded);
    occ_diagnostics_destroy(&diagnostics);
    oir_module_imports_destroy(&imports);
    return 0;
}
