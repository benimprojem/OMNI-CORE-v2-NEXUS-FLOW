#ifndef OMNICORE_COMPILATION_H
#define OMNICORE_COMPILATION_H

#include <stdint.h>
#include "diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_COMPILATION_TARGET_MAX 255u

typedef enum OccCompilationState {
    OCC_JOB_CREATED = 0,
    OCC_JOB_RUNNING = 1,
    OCC_JOB_COMPLETED = 2,
    OCC_JOB_FAILED = 3,
    OCC_JOB_CANCELLED = 4
} OccCompilationState;

typedef struct OccCompilationOptions {
    const char* target_name;
    uint32_t stop_after_stage_id;
    uint32_t flags;
    uint8_t optimization_level;
} OccCompilationOptions;

/*
 * One context belongs to exactly one compilation job.
 * Mutable state and diagnostics must not be shared between jobs.
 */
typedef struct OccCompilationContext {
    uint64_t job_id;
    OccCompilationState state;
    char* target_name;
    uint32_t stop_after_stage_id;
    uint32_t flags;
    uint8_t optimization_level;
    OccDiagnosticCollector diagnostics;
} OccCompilationContext;

int occ_compilation_init(
    OccCompilationContext* context,
    uint64_t job_id,
    const OccCompilationOptions* options
);
void occ_compilation_destroy(OccCompilationContext* context);

int occ_compilation_begin(OccCompilationContext* context);
int occ_compilation_complete(OccCompilationContext* context);
int occ_compilation_fail(OccCompilationContext* context);
int occ_compilation_cancel(OccCompilationContext* context);

/* True only after every required stage completed and the strict diagnostic gate passes. */
int occ_compilation_can_publish_output(const OccCompilationContext* context);

#ifdef __cplusplus
}
#endif

#endif
