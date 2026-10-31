#ifndef OMNICORE_OIR_FAILURE_ARTIFACT_H
#define OMNICORE_OIR_FAILURE_ARTIFACT_H

#include "oir.h"
#include "../core/diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Write a failed compilation checkpoint containing the original SOURCE_UNIT
 * and its DIAGNOSTICS section. The caller owns the output buffer.
 * The artifact is always marked OIR_VALIDITY_INVALID and is not a stage input.
 */
OirStatus oir_failure_artifact_write(
    uint8_t* output,
    size_t capacity,
    uint64_t job_id,
    uint32_t producer_stage_id,
    const uint8_t* path,
    uint32_t path_length,
    const uint8_t* source,
    uint64_t source_length,
    const OccDiagnosticCollector* diagnostics,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
