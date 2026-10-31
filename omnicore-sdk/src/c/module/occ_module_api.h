#ifndef OCC_MODULE_API_H
#define OCC_MODULE_API_H

#include <stdint.h>
#include "../oir/oir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Draft v1 ABI. Do not freeze until the parser-stage adapter is exercised. */
#define OCC_MODULE_ABI_MAJOR 1u
#define OCC_MODULE_ABI_MINOR 0u
#define OCC_MODULE_API_SYMBOL "occ_get_stage_module_v1"
#define OCC_MODULE_ID_UNASSIGNED 0u

#if defined(_WIN32)
    #define OCC_MODULE_CALL __cdecl
    #if defined(OCC_BUILD_MODULE)
        #define OCC_MODULE_EXPORT __declspec(dllexport)
    #else
        #define OCC_MODULE_EXPORT __declspec(dllimport)
    #endif
#else
    #define OCC_MODULE_CALL
    #if defined(__GNUC__) || defined(__clang__)
        #define OCC_MODULE_EXPORT __attribute__((visibility("default")))
    #else
        #define OCC_MODULE_EXPORT
    #endif
#endif

/* Artifact-kind bit N corresponds to stable OIR artifact kind ID N. */
#define OCC_MODULE_ARTIFACT_KIND_MASK(kind_id) (UINT64_C(1) << (kind_id))

typedef enum OccModuleStatusV1 {
    OCC_MODULE_STATUS_OK = 0,
    OCC_MODULE_STATUS_PARTIAL = 1,
    OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER = 2,
    OCC_MODULE_STATUS_FAILED = 3,
    OCC_MODULE_STATUS_CANCELLED = 4,
    OCC_MODULE_STATUS_INVALID_INVOCATION = 5
} OccModuleStatusV1;

typedef enum OccModuleCapabilityV1 {
    OCC_MODULE_CAP_REENTRANT = UINT64_C(1) << 0,
    OCC_MODULE_CAP_DETERMINISTIC = UINT64_C(1) << 1,
    OCC_MODULE_CAP_SUPPORTS_PARTIAL_INPUT = UINT64_C(1) << 2,
    OCC_MODULE_CAP_STATELESS = UINT64_C(1) << 3
} OccModuleCapabilityV1;

typedef struct OccModuleArtifactInputV1 {
    const uint8_t* bytes;
    uint64_t length;
    uint32_t flags;
    uint32_t reserved;
} OccModuleArtifactInputV1;

typedef struct OccModuleInvocationV1 {
    uint32_t struct_size;
    uint32_t input_count;
    const OccModuleArtifactInputV1* inputs;
    uint8_t* output_buffer;
    uint64_t output_capacity;
    uint64_t job_id;
    uint64_t flags;
} OccModuleInvocationV1;

typedef struct OccModuleResultV1 {
    uint32_t struct_size;
    uint32_t diagnostics_count;
    uint64_t bytes_written;
    uint64_t required_output_capacity;
    uint32_t output_validity;
    uint32_t failure_code;
} OccModuleResultV1;

typedef struct OccStageDescriptorV1 {
    uint32_t struct_size;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t module_id;
    uint32_t stage_id;
    uint16_t module_version_major;
    uint16_t module_version_minor;
    uint16_t module_version_patch;
    uint16_t reserved0;
    uint64_t accepted_input_kind_mask;
    uint64_t required_input_kind_mask;
    uint32_t output_artifact_kind;
    uint32_t min_input_count;
    uint32_t max_input_count;
    uint32_t reserved1;
    uint64_t capability_flags;
    const char* module_name;
} OccStageDescriptorV1;

typedef uint32_t (OCC_MODULE_CALL *OccModuleProcessV1)(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
);

typedef struct OccStageModuleV1 {
    uint32_t struct_size;
    uint32_t reserved;
    OccStageDescriptorV1 descriptor;
    OccModuleProcessV1 process;
} OccStageModuleV1;

/* Each shared library exports this symbol and returns a static descriptor. */
typedef const OccStageModuleV1* (OCC_MODULE_CALL *OccGetStageModuleV1Fn)(void);
OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void);

#ifdef __cplusplus
}
#endif

#endif
