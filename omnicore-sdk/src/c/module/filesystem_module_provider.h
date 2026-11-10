#ifndef OCC_FILESYSTEM_MODULE_PROVIDER_H
#define OCC_FILESYSTEM_MODULE_PROVIDER_H

#include "module_resolver.h"
#include "occ_project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_FILESYSTEM_MODULE_PATH_MAX 4096u
#define OCC_FILESYSTEM_MODULE_MAX_INCLUDE_PATHS (OCC_PROJECT_CONFIG_MAX_INCLUDE_PATHS + 1u)

typedef struct OccFilesystemModuleProviderConfig {
    /* Ordered search roots: dsplibs, then libs, then OCF [include] entries. */
    const char* dsplibs_root;
    const char* libs_root;
    const OccProjectConfig* project_config;
    size_t max_source_bytes; /* Zero selects resolver default. */
} OccFilesystemModuleProviderConfig;

typedef struct OccFilesystemModuleProvider {
    char* dsplibs_root;
    char* libs_root;
    char* project_root;
    char* include_paths[OCC_FILESYSTEM_MODULE_MAX_INCLUDE_PATHS];
    size_t include_count;
    size_t max_source_bytes;
    OccModuleSourceProvider provider;
    int initialized;
} OccFilesystemModuleProvider;

/* Roots and include paths are copied; caller may release the OCF config after init. */
OccModuleResolverStatus occ_filesystem_module_provider_init(
    OccFilesystemModuleProvider* provider,
    const OccFilesystemModuleProviderConfig* config
);
void occ_filesystem_module_provider_destroy(
    OccFilesystemModuleProvider* provider
);

/* Root requests (importer_identity == NULL) are direct source file paths.
 * Dependency requests are dotted module names mapped to one <name>.nxf file.
 * Search order: dsplibs_root, libs_root, then OCF include paths. */
const OccModuleSourceProvider* occ_filesystem_module_provider_source(
    const OccFilesystemModuleProvider* provider
);

/* Pure validation/mapping helper, useful to frontends and tests. */
OccModuleResolverStatus occ_filesystem_module_name_to_filename(
    const char* module_name,
    char* out_filename,
    size_t out_capacity
);

#ifdef __cplusplus
}
#endif

#endif
