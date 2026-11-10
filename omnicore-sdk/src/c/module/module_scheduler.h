#ifndef OCC_MODULE_SCHEDULER_H
#define OCC_MODULE_SCHEDULER_H

#include "module_resolver.h"
#include "../parser/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OccModuleSchedulerStatus {
    OCC_MODULE_SCHEDULER_OK = 0,
    OCC_MODULE_SCHEDULER_INVALID_ARGUMENT = 1,
    OCC_MODULE_SCHEDULER_OUT_OF_MEMORY = 2,
    OCC_MODULE_SCHEDULER_RESOLVER_ERROR = 3,
    OCC_MODULE_SCHEDULER_PARSE_ERROR = 4,
    OCC_MODULE_SCHEDULER_AST_ERROR = 5,
    OCC_MODULE_SCHEDULER_CYCLE = 6,
    OCC_MODULE_SCHEDULER_SEMANTIC_ERROR = 7,
    OCC_MODULE_SCHEDULER_SOURCE_NOT_FOUND = 8,
    OCC_MODULE_SCHEDULER_SOURCE_DENIED = 9,
    OCC_MODULE_SCHEDULER_SOURCE_AMBIGUOUS = 10,
    OCC_MODULE_SCHEDULER_SOURCE_IO_ERROR = 11,
    OCC_MODULE_SCHEDULER_RESOURCE_LIMIT = 12,
    OCC_MODULE_SCHEDULER_PROVIDER_ERROR = 13
} OccModuleSchedulerStatus;

/* Parser callback returns an owned AST_PROGRAM root on success. The scheduler
 * retains it until destroy; it never mutates the AST. */
typedef int (*OccModuleSchedulerParseFn)(
    void* context,
    const OccModuleSourceView* source,
    ASTNode** out_program
);

/* Called once per module in dependency-first order. A nonzero result means
 * Semantic succeeded for this module; zero aborts the semantic pass. */
typedef int (*OccModuleSchedulerSemanticFn)(
    void* context,
    uint32_t module_id,
    const char* canonical_identity,
    const ASTNode* program
);

typedef struct OccModuleSchedulerCallbacks {
    void* context;
    OccModuleSchedulerParseFn parse;
    OccModuleSchedulerSemanticFn semantic;
} OccModuleSchedulerCallbacks;

typedef struct OccModuleSchedulerUse {
    char* requested_path;
    char* alias;
    char* namespace_name;
    OccSourceSpan span;
    uint32_t ordinal;
} OccModuleSchedulerUse;

typedef struct OccModuleImportBinding {
    uint32_t importer_module_id;
    uint32_t dependency_module_id;
    uint32_t use_ordinal;
    const char* requested_path;   /* Borrowed from scheduler. */
    const char* alias;            /* Borrowed; NULL when no `as` binding. */
    const char* namespace_name;  /* Borrowed; NULL when no `ns` selector. */
    OccSourceSpan span;
} OccModuleImportBinding;

typedef struct OccModuleSchedulerEntry {
    ASTNode* program;
    OccModuleSchedulerUse* uses;
    size_t use_count;
    size_t use_capacity;
    uint8_t implicit_osmodule_processed;
} OccModuleSchedulerEntry;

typedef struct OccModuleScheduler {
    OccModuleResolver resolver;
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSchedulerEntry* entries;
    size_t entry_capacity;
    int initialized;
} OccModuleScheduler;

/* Provider and callbacks are borrowed and must outlive the scheduler. */
OccModuleSchedulerStatus occ_module_scheduler_init(
    OccModuleScheduler* scheduler,
    const OccModuleResolverConfig* resolver_config,
    const OccModuleSourceProvider* provider,
    const OccModuleSchedulerCallbacks* callbacks
);

/* Resolve/parse the root and all transitive top-level USE dependencies, then
 * invoke Semantic exactly once per module in dependency-first order.
 * The root request's requested_path identifies the root source; importer may
 * be NULL. use_span must identify the root source location. */
OccModuleSchedulerStatus occ_module_scheduler_run(
    OccModuleScheduler* scheduler,
    const OccModuleSourceRequest* root_request
);

/* Retrieve the exact importer-side binding for one top-level USE ordinal. */
OccModuleSchedulerStatus occ_module_scheduler_get_import_binding(
    const OccModuleScheduler* scheduler,
    uint32_t importer_module_id,
    uint32_t use_ordinal,
    OccModuleImportBinding* out_binding
);

void occ_module_scheduler_destroy(OccModuleScheduler* scheduler);
const char* occ_module_scheduler_status_string(OccModuleSchedulerStatus status);

#ifdef __cplusplus
}
#endif

#endif
