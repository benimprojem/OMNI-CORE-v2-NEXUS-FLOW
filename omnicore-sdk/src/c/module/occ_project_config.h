#ifndef OCC_PROJECT_CONFIG_H
#define OCC_PROJECT_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_PROJECT_CONFIG_MAX_INCLUDE_PATHS 64u
#define OCC_PROJECT_CONFIG_MAX_PATH_BYTES 4096u
#define OCC_PROJECT_CONFIG_MAX_FILE_BYTES (1024u * 1024u)

typedef enum OccProjectConfigStatus {
    OCC_PROJECT_CONFIG_OK = 0,
    OCC_PROJECT_CONFIG_INVALID_ARGUMENT = 1,
    OCC_PROJECT_CONFIG_IO_ERROR = 2,
    OCC_PROJECT_CONFIG_SYNTAX_ERROR = 3,
    OCC_PROJECT_CONFIG_LIMIT_EXCEEDED = 4,
    OCC_PROJECT_CONFIG_OUT_OF_MEMORY = 5
} OccProjectConfigStatus;

/*
 * Parsed project configuration. project_root is the directory containing the
 * OCF file. Include entries are preserved as written and are relative to that
 * root unless the entry itself is absolute. Dependency declarations belong to
 * OCM and are intentionally not interpreted by this host include-path reader.
 */
typedef struct OccProjectConfig {
    char* project_root;
    char* include_paths[OCC_PROJECT_CONFIG_MAX_INCLUDE_PATHS];
    size_t include_count;
} OccProjectConfig;

/* Initialize with {0}; load leaves the output empty on failure. */
OccProjectConfigStatus occ_project_config_load(
    const char* config_path,
    OccProjectConfig* out_config
);
void occ_project_config_destroy(OccProjectConfig* config);
const char* occ_project_config_status_string(OccProjectConfigStatus status);

#ifdef __cplusplus
}
#endif

#endif
