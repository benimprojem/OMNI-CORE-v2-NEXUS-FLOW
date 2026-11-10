#ifndef OCC_OIR_MODULE_IMPORTS_ARTIFACT_H
#define OCC_OIR_MODULE_IMPORTS_ARTIFACT_H

#include "oir.h"
#include "../core/diagnostics.h"
#include "../module/module_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_MODULE_IMPORTS_SCHEMA_MAJOR 1u
#define OIR_MODULE_IMPORTS_SCHEMA_MINOR 0u
#define OIR_MODULE_IMPORTS_MAX_RECORDS 1000000u
#define OIR_MODULE_IMPORTS_MAX_STRING 1048576u

typedef struct OirModuleImportRecord {
    uint32_t use_ordinal;
    OccSourceSpan span;
    char* requested_path;
    char* alias;
    char* namespace_name;
    char* dependency_identity;
} OirModuleImportRecord;

typedef struct OirModuleImports {
    char* importer_identity;
    uint32_t schema_major;
    uint32_t schema_minor;
    OirModuleImportRecord* records;
    size_t record_count;
} OirModuleImports;

typedef struct OirModuleImportsArtifactView {
    OirArtifactView envelope;
    OirModuleImports imports;
} OirModuleImportsArtifactView;

/* Build an owned manifest from scheduler bindings. The importer identity and
 * each resolved dependency identity are canonical identities, not paths to
 * be re-resolved by Semantic. */
OirStatus oir_module_imports_build_from_scheduler(
    const OccModuleScheduler* scheduler,
    uint32_t importer_module_id,
    const char* importer_identity,
    OirModuleImports* out_imports
);

OirStatus oir_module_imports_build(
    const char* importer_identity,
    const OccModuleImportBinding* bindings,
    size_t binding_count,
    OirModuleImports* out_imports
);

void oir_module_imports_destroy(OirModuleImports* imports);

OirStatus oir_module_imports_artifact_write(
    const OirModuleImports* imports,
    uint64_t job_id,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
);

OirStatus oir_module_imports_artifact_open(
    const uint8_t* bytes,
    size_t length,
    OirModuleImportsArtifactView* out_view
);

void oir_module_imports_artifact_destroy(OirModuleImportsArtifactView* view);

#ifdef __cplusplus
}
#endif
#endif
