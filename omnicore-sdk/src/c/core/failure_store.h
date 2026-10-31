#ifndef OMNICORE_FAILURE_STORE_H
#define OMNICORE_FAILURE_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OCC host-side persistence policy. Creates the requested directory if absent,
 * writes a job-scoped OIR failure bundle to a temporary sibling, then renames
 * it into place. Stage modules must never call this API or choose paths.
 */
int occ_failure_store_save(
    const char* directory,
    uint64_t job_id,
    uint32_t producer_stage_id,
    const char* source_path,
    const uint8_t* source,
    size_t source_length,
    const OccDiagnosticCollector* diagnostics,
    char* out_path,
    size_t out_path_capacity
);

#ifdef __cplusplus
}
#endif

#endif
