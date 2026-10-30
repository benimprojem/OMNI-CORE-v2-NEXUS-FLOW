#include "compilation.h"

#include <stdlib.h>
#include <string.h>

int occ_compilation_init(
    OccCompilationContext* context,
    uint64_t job_id,
    const OccCompilationOptions* options
) {
    size_t target_length = 0;

    if (!context) return 0;
    memset(context, 0, sizeof(*context));

    if (options && options->target_name) {
        target_length = strlen(options->target_name);
        if (target_length == 0 || target_length > OCC_COMPILATION_TARGET_MAX) {
            return 0;
        }
        context->target_name = (char*)malloc(target_length + 1u);
        if (!context->target_name) return 0;
        memcpy(context->target_name, options->target_name, target_length + 1u);
        context->stop_after_stage_id = options->stop_after_stage_id;
        context->flags = options->flags;
        context->optimization_level = options->optimization_level;
    }

    context->job_id = job_id;
    context->state = OCC_JOB_CREATED;
    occ_diagnostics_init(&context->diagnostics);
    return 1;
}

void occ_compilation_destroy(OccCompilationContext* context) {
    if (!context) return;
    occ_diagnostics_destroy(&context->diagnostics);
    free(context->target_name);
    memset(context, 0, sizeof(*context));
}

int occ_compilation_begin(OccCompilationContext* context) {
    if (!context || context->state != OCC_JOB_CREATED) return 0;
    context->state = OCC_JOB_RUNNING;
    return 1;
}

int occ_compilation_complete(OccCompilationContext* context) {
    if (!context || context->state != OCC_JOB_RUNNING) return 0;
    context->state = OCC_JOB_COMPLETED;
    return 1;
}

int occ_compilation_fail(OccCompilationContext* context) {
    if (!context ||
        (context->state != OCC_JOB_CREATED && context->state != OCC_JOB_RUNNING)) {
        return 0;
    }
    context->state = OCC_JOB_FAILED;
    return 1;
}

int occ_compilation_cancel(OccCompilationContext* context) {
    if (!context ||
        (context->state != OCC_JOB_CREATED && context->state != OCC_JOB_RUNNING)) {
        return 0;
    }
    context->state = OCC_JOB_CANCELLED;
    return 1;
}

int occ_compilation_can_publish_output(const OccCompilationContext* context) {
    if (!context || context->state != OCC_JOB_COMPLETED) return 0;
    return !occ_diagnostics_blocks_output(&context->diagnostics);
}
