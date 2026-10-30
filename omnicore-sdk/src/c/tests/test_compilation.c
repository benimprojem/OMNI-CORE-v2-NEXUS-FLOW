#include "compilation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        return 1; \
    } \
} while (0)

static void add_warning(OccCompilationContext* context) {
    OccDiagnosticInput input;
    memset(&input, 0, sizeof(input));
    input.code = 7001;
    input.stage_id = 1;
    input.severity = OCC_DIAG_WARNING;
    input.message = "recoverable warning";
    input.message_length = strlen(input.message);
    if (!occ_diagnostics_add(&context->diagnostics, &input)) {
        fprintf(stderr, "Could not add test warning.\n");
        exit(2);
    }
}

int main(void) {
    OccCompilationContext context;
    OccCompilationOptions options;
    char target[] = "x86_64_test";
    memset(&options, 0, sizeof(options));
    options.target_name = target;
    options.stop_after_stage_id = 4;
    options.optimization_level = 2;

    CHECK(occ_compilation_init(&context, 42, &options),
        "context should initialize");
    target[0] = 'X';
    CHECK(strcmp(context.target_name, "x86_64_test") == 0,
        "context must own a copy of target name");
    CHECK(context.job_id == 42 && context.stop_after_stage_id == 4 &&
        context.optimization_level == 2, "job options should be preserved");
    CHECK(context.state == OCC_JOB_CREATED,
        "new context should start in CREATED state");
    CHECK(!occ_compilation_can_publish_output(&context),
        "created job cannot publish output");
    CHECK(occ_compilation_begin(&context), "job should begin");
    CHECK(!occ_compilation_begin(&context), "job cannot begin twice");

    add_warning(&context);
    CHECK(context.state == OCC_JOB_RUNNING,
        "source warning must not stop pipeline execution");
    CHECK(!occ_compilation_can_publish_output(&context),
        "running job cannot publish final output");
    CHECK(occ_compilation_complete(&context),
        "job with recoverable diagnostics should complete its stages");
    CHECK(context.state == OCC_JOB_COMPLETED,
        "completed job state should be recorded");
    CHECK(!occ_compilation_can_publish_output(&context),
        "strict warning gate should block final output after completion");
    CHECK(!occ_compilation_fail(&context),
        "completed job cannot transition to failed");
    occ_compilation_destroy(&context);

    {
        OccCompilationContext clean;
        CHECK(occ_compilation_init(&clean, 43, NULL),
            "clean context should initialize without options");
        CHECK(occ_compilation_begin(&clean), "clean job should begin");
        CHECK(occ_compilation_complete(&clean), "clean job should complete");
        CHECK(occ_compilation_can_publish_output(&clean),
            "completed job without blocking diagnostics may publish output");
        occ_compilation_destroy(&clean);
    }

    {
        OccCompilationContext cancelled;
        CHECK(occ_compilation_init(&cancelled, 44, NULL),
            "cancelled context should initialize");
        CHECK(occ_compilation_begin(&cancelled), "cancelled job should begin");
        CHECK(occ_compilation_cancel(&cancelled), "running job should cancel");
        CHECK(cancelled.state == OCC_JOB_CANCELLED &&
            !occ_compilation_can_publish_output(&cancelled),
            "cancelled job must never publish output");
        occ_compilation_destroy(&cancelled);
    }

    {
        OccCompilationContext failed;
        CHECK(occ_compilation_init(&failed, 45, NULL),
            "failed context should initialize");
        CHECK(occ_compilation_fail(&failed),
            "job may fail before stages begin due infrastructure error");
        CHECK(failed.state == OCC_JOB_FAILED &&
            !occ_compilation_can_publish_output(&failed),
            "failed job must never publish output");
        occ_compilation_destroy(&failed);
    }

    {
        OccCompilationContext invalid;
        options.target_name = "";
        CHECK(!occ_compilation_init(&invalid, 46, &options),
            "empty target name should be rejected");
    }

    puts("Compilation context tests passed.");
    return 0;
}
