#ifdef NDEBUG
#undef NDEBUG
#endif
#include "module_import_bundle.h"

#include <assert.h>
#include <string.h>

int main(void) {
    OccModuleImportBinding bindings[3];
    OirModuleImports imports;
    OirModuleImportsArtifactView imports_artifact;
    OirModuleInterfaceArtifactView interfaces[2];
    OccModuleImportBundle bundle;
    memset(bindings,0,sizeof(bindings));
    memset(&imports,0,sizeof(imports));
    memset(&imports_artifact,0,sizeof(imports_artifact));
    memset(interfaces,0,sizeof(interfaces));
    memset(&bundle,0,sizeof(bundle));

    bindings[0].use_ordinal=1u; bindings[0].requested_path="math";
    bindings[0].dependency_identity="file:///lib/math.nxf";
    bindings[0].alias="m";
    bindings[1].use_ordinal=2u; bindings[1].requested_path="math";
    bindings[1].dependency_identity="file:///lib/math.nxf";
    bindings[1].namespace_name="core";
    bindings[2].use_ordinal=3u; bindings[2].requested_path="io";
    bindings[2].dependency_identity="file:///lib/io.nxf";

    assert(oir_module_imports_build("file:///project/main.nxf",
        bindings,3u,&imports)==OIR_STATUS_OK);

    imports_artifact.imports=imports;
    imports_artifact.envelope.header.artifact_kind=OIR_KIND_MODULE_IMPORTS;
    imports_artifact.envelope.header.producer_stage_id=OCC_STAGE_OIR_VERIFY;
    imports_artifact.envelope.header.validity=OIR_VALIDITY_VALID;
    imports_artifact.envelope.header.job_id=501u;

    interfaces[0].module_interface.canonical_identity="file:///lib/math.nxf";
    interfaces[0].module_interface.schema_major=OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    interfaces[0].envelope.header.artifact_kind=OIR_KIND_MODULE_INTERFACE;
    interfaces[0].envelope.header.producer_stage_id=OCC_STAGE_OIR_VERIFY;
    interfaces[0].envelope.header.validity=OIR_VALIDITY_VALID;
    interfaces[0].envelope.header.job_id=501u;

    interfaces[1].module_interface.canonical_identity="file:///lib/io.nxf";
    interfaces[1].module_interface.schema_major=OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    interfaces[1].envelope.header.artifact_kind=OIR_KIND_MODULE_INTERFACE;
    interfaces[1].envelope.header.producer_stage_id=OCC_STAGE_OIR_VERIFY;
    interfaces[1].envelope.header.validity=OIR_VALIDITY_VALID;
    interfaces[1].envelope.header.job_id=501u;

    assert(occ_module_import_bundle_bind("file:///project/main.nxf",
        &imports_artifact,interfaces,2u,&bundle)==OCC_MODULE_IMPORT_BUNDLE_OK);
    assert(bundle.binding_count==3u);
    assert(bundle.bindings[0].module_interface==
        &interfaces[0].module_interface);
    assert(bundle.bindings[1].module_interface==
        &interfaces[0].module_interface);
    assert(bundle.bindings[2].module_interface==
        &interfaces[1].module_interface);
    assert(strcmp(bundle.bindings[0].binding->alias,"m")==0);
    assert(strcmp(bundle.bindings[1].binding->namespace_name,"core")==0);
    occ_module_import_bundle_destroy(&bundle);

    assert(occ_module_import_bundle_bind("file:///wrong/main.nxf",
        &imports_artifact,interfaces,2u,&bundle)==
        OCC_MODULE_IMPORT_BUNDLE_IMPORTER_MISMATCH);
    assert(occ_module_import_bundle_bind("file:///project/main.nxf",
        &imports_artifact,interfaces,1u,&bundle)==
        OCC_MODULE_IMPORT_BUNDLE_INTERFACE_MISSING);

    interfaces[1].module_interface.canonical_identity="file:///lib/math.nxf";
    assert(occ_module_import_bundle_bind("file:///project/main.nxf",
        &imports_artifact,interfaces,2u,&bundle)==
        OCC_MODULE_IMPORT_BUNDLE_DUPLICATE_INTERFACE);

    oir_module_imports_destroy(&imports);
    return 0;
}
