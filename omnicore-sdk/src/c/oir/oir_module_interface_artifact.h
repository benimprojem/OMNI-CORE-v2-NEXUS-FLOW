#ifndef OCC_OIR_MODULE_INTERFACE_ARTIFACT_H
#define OCC_OIR_MODULE_INTERFACE_ARTIFACT_H

#include "oir.h"
#include "module_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MAJOR 1u
#define OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MINOR 0u

typedef struct OirModuleInterfaceArtifactView {
    OirArtifactView envelope;
    OccModuleInterface module_interface;
} OirModuleInterfaceArtifactView;

/* Serialize an owned module-interface snapshot into a complete OIR artifact.
 * The output buffer remains caller-owned. No partial output is published on error.
 */
OirStatus oir_module_interface_artifact_write(
    const OccModuleInterface* module_interface,
    uint64_t job_id,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
);

/* Validate the OIR envelope and decode the required module-interface payload.
 * Decoded strings/tables are owned by out_view and released with destroy().
 */
OirStatus oir_module_interface_artifact_open(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    OirModuleInterfaceArtifactView* out_view
);

void oir_module_interface_artifact_destroy(
    OirModuleInterfaceArtifactView* view
);

#ifdef __cplusplus
}
#endif

#endif
