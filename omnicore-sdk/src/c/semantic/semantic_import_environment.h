#ifndef OCC_SEMANTIC_IMPORT_ENVIRONMENT_H
#define OCC_SEMANTIC_IMPORT_ENVIRONMENT_H

#include <stddef.h>
#include <stdint.h>
#include "module_import_bundle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OccSemanticImportEnvironmentStatus {
    OCC_SEMANTIC_IMPORT_ENV_OK = 0,
    OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT = 1,
    OCC_SEMANTIC_IMPORT_ENV_OUT_OF_MEMORY = 2,
    OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND = 3,
    OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS = 4,
    OCC_SEMANTIC_IMPORT_ENV_LIMIT_EXCEEDED = 5
} OccSemanticImportEnvironmentStatus;

typedef struct OccSemanticImportEnvironmentBinding {
    uint32_t use_ordinal;
    const char* requested_path;
    const char* dependency_identity;
    const char* alias;
    const char* namespace_name;
    const OccModuleInterface* module_interface;
} OccSemanticImportEnvironmentBinding;

typedef struct OccSemanticImportedSymbol {
    uint32_t source_symbol_id;
    uint32_t source_type_id;
    uint32_t source_scope_id;
    uint16_t kind;
    uint16_t flags;
    const char* name;
} OccSemanticImportedSymbol;

typedef struct OccSemanticImportedType {
    uint32_t source_type_id;
    uint32_t referenced_symbol_id;
    uint32_t base_type_id;
    uint16_t kind;
    uint16_t flags;
    const char* name;
} OccSemanticImportedType;

/* Read-only Semantic-side index over a verified import bundle.
 * All strings and interface records are borrowed from the decoded OIR views.
 * The environment owns only its binding index array and must be destroyed
 * before the source bundle/artifact views are destroyed.
 *
 * This layer intentionally does not decide PUBLIC vs EXPORTED access,
 * alias defaulting, namespace selection, or local-name collision policy.
 */
typedef struct OccSemanticImportEnvironment {
    OccSemanticImportEnvironmentBinding* bindings;
    size_t binding_count;
} OccSemanticImportEnvironment;

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_init(
    const OccModuleImportBundle* bundle,
    OccSemanticImportEnvironment* out_environment
);

/* Resolve only an explicitly written alias/namespace selector.
 * NULL, empty, and "_" selectors are intentionally not implicit bindings. */
OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_alias(
    const OccSemanticImportEnvironment* environment,
    const char* alias,
    size_t* out_binding_index
);

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_namespace(
    const OccSemanticImportEnvironment* environment,
    const char* namespace_name,
    size_t* out_binding_index
);

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_get_binding(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    OccSemanticImportEnvironmentBinding* out_binding
);

/* Lookup is scoped to one explicit USE record. Duplicate exported names in
 * that module are reported as AMBIGUOUS; no visibility bit is discarded. */
OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_symbol(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    const char* name,
    OccSemanticImportedSymbol* out_symbol
);

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_type(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    uint32_t source_type_id,
    OccSemanticImportedType* out_type
);

void occ_semantic_import_environment_destroy(
    OccSemanticImportEnvironment* environment
);

const char* occ_semantic_import_environment_status_string(
    OccSemanticImportEnvironmentStatus status
);

#ifdef __cplusplus
}
#endif
#endif
