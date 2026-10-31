#ifndef OCC_MODULE_LOADER_H
#define OCC_MODULE_LOADER_H

#include "occ_module_api.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OccModuleLoaderStatus {
    OCC_MODULE_LOADER_OK = 0,
    OCC_MODULE_LOADER_INVALID_ARGUMENT = 1,
    OCC_MODULE_LOADER_OPEN_FAILED = 2,
    OCC_MODULE_LOADER_SYMBOL_MISSING = 3,
    OCC_MODULE_LOADER_INVALID_MODULE = 4,
    OCC_MODULE_LOADER_INVOKE_FAILED = 5,
    OCC_MODULE_LOADER_INVALID_OUTPUT = 6,
    OCC_MODULE_LOADER_OUT_OF_MEMORY = 7,
    OCC_MODULE_LOADER_RETRY_LIMIT = 8
} OccModuleLoaderStatus;

/* Host-owned handle. The module descriptor remains valid until close(). */
typedef struct OccLoadedStageModule {
    void* library_handle;
    const OccStageModuleV1* module;
} OccLoadedStageModule;

/*
 * Loads one exact shared-library path and validates its exported v1 descriptor.
 * error_text is optional; when supplied it is always NUL-terminated.
 */
uint32_t occ_module_loader_open(
    OccLoadedStageModule* loaded,
    const char* library_path,
    char* error_text,
    size_t error_text_capacity
);

/* Unloads the library and clears the host-owned handle. */
void occ_module_loader_close(OccLoadedStageModule* loaded);

/*
 * Invokes a loaded stage. Inputs remain caller-owned and immutable.
 * The helper retries NEED_OUTPUT_BUFFER using the module's exact requested size.
 * On success, *out_artifact is malloc-owned by the caller and must be freed.
 * max_output_capacity bounds module-controlled allocation requests.
 */
uint32_t occ_module_loader_invoke(
    const OccLoadedStageModule* loaded,
    const OccModuleArtifactInputV1* inputs,
    uint32_t input_count,
    uint64_t job_id,
    OccDiagnosticCollector* diagnostics,
    size_t initial_output_capacity,
    size_t max_output_capacity,
    uint8_t** out_artifact,
    size_t* out_artifact_length
);

#ifdef __cplusplus
}
#endif

#endif
