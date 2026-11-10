#include "module_import_bundle.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define OCC_MODULE_IMPORT_BUNDLE_MAX_INTERFACES OIR_MODULE_IMPORTS_MAX_RECORDS

static uint64_t identity_hash(const char* text) {
    uint64_t h=UINT64_C(1469598103934665603);
    const unsigned char* p=(const unsigned char*)text;
    while(*p) { h^=(uint64_t)*p++; h*=UINT64_C(1099511628211); }
    return h;
}
static int next_capacity(size_t count,size_t* out) {
    size_t need=8u, target;
    if(count>SIZE_MAX/2u) return 0;
    target=count*2u;
    while(need<target) {
        if(need>SIZE_MAX/2u) return 0;
        need*=2u;
    }
    *out=need; return 1;
}
static int valid_identity(const char* s) { return s && s[0]!='\0'; }

void occ_module_import_bundle_destroy(OccModuleImportBundle* bundle) {
    if(!bundle) return;
    free(bundle->bindings);
    memset(bundle,0,sizeof(*bundle));
}

OccModuleImportBundleStatus occ_module_import_bundle_bind(
    const char* expected_importer_identity,
    const OirModuleImportsArtifactView* imports_artifact,
    const OirModuleInterfaceArtifactView* interfaces,
    size_t interface_count,
    OccModuleImportBundle* out_bundle
) {
    size_t capacity=0u,i;
    size_t* slots=NULL;
    uint8_t* matched=NULL;
    const OirModuleImports* imports;
    if(!out_bundle || !valid_identity(expected_importer_identity) ||
       !imports_artifact || !valid_identity(imports_artifact->imports.importer_identity) ||
       (interface_count && !interfaces) ||
       interface_count>OCC_MODULE_IMPORT_BUNDLE_MAX_INTERFACES)
        return OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT;
    memset(out_bundle,0,sizeof(*out_bundle));
    imports=&imports_artifact->imports;
    if(imports->record_count>OIR_MODULE_IMPORTS_MAX_RECORDS ||
       (imports->record_count && !imports->records))
        return OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT;
    if(imports_artifact->envelope.header.artifact_kind!=OIR_KIND_MODULE_IMPORTS ||
       imports_artifact->envelope.header.producer_stage_id!=OCC_STAGE_OIR_VERIFY ||
       imports_artifact->envelope.header.validity!=OIR_VALIDITY_VALID)
        return OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT;
    if(strcmp(expected_importer_identity,imports->importer_identity)!=0)
        return OCC_MODULE_IMPORT_BUNDLE_IMPORTER_MISMATCH;
    if(interface_count>imports->record_count)
        return OCC_MODULE_IMPORT_BUNDLE_UNUSED_INTERFACE;
    if(interface_count && !next_capacity(interface_count,&capacity))
        return OCC_MODULE_IMPORT_BUNDLE_LIMIT_EXCEEDED;
    if(capacity) {
        slots=(size_t*)malloc(capacity*sizeof(*slots));
        matched=(uint8_t*)calloc(interface_count,1u);
        if(!slots || !matched) {
            free(slots); free(matched); return OCC_MODULE_IMPORT_BUNDLE_OUT_OF_MEMORY;
        }
        for(i=0;i<capacity;i++) slots[i]=SIZE_MAX;
    }
    for(i=0;i<interface_count;i++) {
        const OccModuleInterface* mi=&interfaces[i].module_interface;
        size_t slot;
        if(!valid_identity(mi->canonical_identity) ||
           mi->schema_major!=OCC_MODULE_INTERFACE_SCHEMA_MAJOR ||
           mi->schema_minor>OCC_MODULE_INTERFACE_SCHEMA_MINOR ||
           interfaces[i].envelope.header.artifact_kind!=OIR_KIND_MODULE_INTERFACE ||
           interfaces[i].envelope.header.producer_stage_id!=OCC_STAGE_OIR_VERIFY ||
           interfaces[i].envelope.header.validity!=OIR_VALIDITY_VALID ||
           interfaces[i].envelope.header.job_id!=imports_artifact->envelope.header.job_id) {
            free(slots); free(matched); return OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT;
        }
        slot=(size_t)identity_hash(mi->canonical_identity)&(capacity-1u);
        while(slots[slot]!=SIZE_MAX) {
            size_t prior=slots[slot];
            if(strcmp(interfaces[prior].module_interface.canonical_identity,
                mi->canonical_identity)==0) {
                free(slots); free(matched); return OCC_MODULE_IMPORT_BUNDLE_DUPLICATE_INTERFACE;
            }
            slot=(slot+1u)&(capacity-1u);
        }
        slots[slot]=i;
    }
    if(imports->record_count) {
        if(imports->record_count>SIZE_MAX/sizeof(*out_bundle->bindings)) {
            free(slots); free(matched); return OCC_MODULE_IMPORT_BUNDLE_LIMIT_EXCEEDED;
        }
        out_bundle->bindings=(OccModuleImportBindingView*)calloc(
            imports->record_count,sizeof(*out_bundle->bindings));
        if(!out_bundle->bindings) {
            free(slots); free(matched); return OCC_MODULE_IMPORT_BUNDLE_OUT_OF_MEMORY;
        }
    }
    for(i=0;i<imports->record_count;i++) {
        const OirModuleImportRecord* record=&imports->records[i];
        size_t slot,index;
        if(!valid_identity(record->dependency_identity)) {
            free(slots); free(matched); occ_module_import_bundle_destroy(out_bundle);
            return OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT;
        }
        if(!capacity) {
            free(slots); free(matched); occ_module_import_bundle_destroy(out_bundle);
            return OCC_MODULE_IMPORT_BUNDLE_INTERFACE_MISSING;
        }
        slot=(size_t)identity_hash(record->dependency_identity)&(capacity-1u);
        while(slots[slot]!=SIZE_MAX &&
            strcmp(interfaces[slots[slot]].module_interface.canonical_identity,
                record->dependency_identity)!=0)
            slot=(slot+1u)&(capacity-1u);
        if(slots[slot]==SIZE_MAX) {
            free(slots); free(matched); occ_module_import_bundle_destroy(out_bundle);
            return OCC_MODULE_IMPORT_BUNDLE_INTERFACE_MISSING;
        }
        index=slots[slot]; matched[index]=1u;
        out_bundle->bindings[i].binding=record;
        out_bundle->bindings[i].module_interface=&interfaces[index].module_interface;
        out_bundle->binding_count=i+1u;
    }
    for(i=0;i<interface_count;i++) if(!matched[i]) {
        free(slots); free(matched); occ_module_import_bundle_destroy(out_bundle);
        return OCC_MODULE_IMPORT_BUNDLE_UNUSED_INTERFACE;
    }
    free(slots); free(matched);
    out_bundle->imports=imports;
    return OCC_MODULE_IMPORT_BUNDLE_OK;
}

const char* occ_module_import_bundle_status_string(OccModuleImportBundleStatus status) {
    switch(status) {
        case OCC_MODULE_IMPORT_BUNDLE_OK: return "ok";
        case OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT: return "invalid argument";
        case OCC_MODULE_IMPORT_BUNDLE_IMPORTER_MISMATCH: return "importer identity mismatch";
        case OCC_MODULE_IMPORT_BUNDLE_INTERFACE_MISSING: return "module interface missing";
        case OCC_MODULE_IMPORT_BUNDLE_DUPLICATE_INTERFACE: return "duplicate module interface";
        case OCC_MODULE_IMPORT_BUNDLE_UNUSED_INTERFACE: return "unused module interface";
        case OCC_MODULE_IMPORT_BUNDLE_OUT_OF_MEMORY: return "out of memory";
        case OCC_MODULE_IMPORT_BUNDLE_LIMIT_EXCEEDED: return "import bundle limit exceeded";
        default: return "unknown import bundle status";
    }
}
