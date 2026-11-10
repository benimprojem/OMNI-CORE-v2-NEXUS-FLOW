#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_module_imports_artifact.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    OccModuleImportBinding bindings[2];
    OirModuleImports imports;
    OirModuleImportsArtifactView decoded;
    uint8_t bytes[4096];
    size_t written=0u;

    memset(bindings,0,sizeof(bindings));
    memset(&imports,0,sizeof(imports));
    memset(&decoded,0,sizeof(decoded));

    bindings[0].importer_module_id=1u;
    bindings[0].dependency_module_id=2u;
    bindings[0].dependency_identity="file:///libs/math.nxf";
    bindings[0].use_ordinal=1u;
    bindings[0].requested_path="math";
    bindings[0].alias="m";
    bindings[0].namespace_name="core";
    bindings[0].span.source_id=7u;
    bindings[0].span.start_offset=12u;
    bindings[0].span.end_offset=38u;
    bindings[0].span.start_line=2u;
    bindings[0].span.start_column=1u;
    bindings[0].span.end_line=2u;
    bindings[0].span.end_column=27u;
    bindings[0].span.has_location=1u;

    bindings[1].importer_module_id=1u;
    bindings[1].dependency_module_id=3u;
    bindings[1].dependency_identity="file:///libs/legacy.nxf";
    bindings[1].use_ordinal=2u;
    bindings[1].requested_path="legacy";
    bindings[1].alias="_";
    bindings[1].namespace_name=NULL;

    assert(oir_module_imports_build("file:///project/main.nxf",
        bindings,2u,&imports)==OIR_STATUS_OK);
    assert(imports.record_count==2u);
    assert(oir_module_imports_artifact_write(&imports,12345u,bytes,
        sizeof(bytes),&written)==OIR_STATUS_OK);
    assert(written>OIR_HEADER_SIZE);
    assert(oir_module_imports_artifact_open(bytes,written,&decoded)==OIR_STATUS_OK);
    assert(decoded.envelope.header.job_id==12345u);
    assert(decoded.envelope.header.artifact_kind==OIR_KIND_MODULE_IMPORTS);
    assert(strcmp(decoded.imports.importer_identity,"file:///project/main.nxf")==0);
    assert(decoded.imports.record_count==2u);
    assert(strcmp(decoded.imports.records[0].dependency_identity,
        "file:///libs/math.nxf")==0);
    assert(strcmp(decoded.imports.records[0].alias,"m")==0);
    assert(strcmp(decoded.imports.records[0].namespace_name,"core")==0);
    assert(decoded.imports.records[0].span.start_offset==12u);
    assert(strcmp(decoded.imports.records[1].alias,"_")==0);
    assert(decoded.imports.records[1].namespace_name==NULL);

    oir_module_imports_artifact_destroy(&decoded);
    oir_module_imports_destroy(&imports);

    /* Duplicate USE ordinals are structurally invalid, even if strings parse. */
    {
        size_t second_ordinal = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE +
            16u + strlen("file:///project/main.nxf") + 60u +
            strlen("math") + strlen("m") + strlen("core") +
            strlen("file:///libs/math.nxf");
        bytes[second_ordinal] = 1u;
    }
    assert(oir_module_imports_artifact_open(bytes,written,&decoded)==OIR_STATUS_INVALID_SECTION);
    return 0;
}
