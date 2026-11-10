#include "module_scheduler.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct SchedulerFrame {
    uint32_t module_id;
    size_t next_use;
} SchedulerFrame;

typedef struct UseCollector {
    OccModuleSchedulerEntry* entry;
    int failed;
} UseCollector;

static char* scheduler_strdup(const char* value) {
    size_t length;
    char* copy;
    if (!value) return NULL;
    length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}

static int scheduler_grow(void** buffer, size_t* capacity,
    size_t required, size_t element_size) {
    size_t next;
    void* grown;
    if (element_size == 0u || required > SIZE_MAX / element_size) return 0;
    if (*capacity >= required) return 1;
    next = *capacity ? *capacity : 4u;
    while (next < required) {
        if (next > SIZE_MAX / 2u) { next = required; break; }
        next *= 2u;
    }
    if (next > SIZE_MAX / element_size) return 0;
    grown = realloc(*buffer, next * element_size);
    if (!grown) return 0;
    *buffer = grown;
    *capacity = next;
    return 1;
}

static OccModuleSchedulerEntry* scheduler_entry(
    OccModuleScheduler* scheduler, uint32_t module_id
) {
    if (!scheduler || module_id == 0u ||
        (size_t)module_id > scheduler->entry_capacity) return NULL;
    return &scheduler->entries[module_id - 1u];
}

static int scheduler_collect_use(
    const ASTUse* use, uint32_t ordinal, void* context
) {
    UseCollector* collector = (UseCollector*)context;
    OccModuleSchedulerEntry* entry;
    OccModuleSchedulerUse* item;
    if (!collector || !use || !use->module_name || !use->module_name[0]) {
        if (collector) collector->failed = 1;
        return 0;
    }
    entry = collector->entry;
    if (!scheduler_grow((void**)&entry->uses, &entry->use_capacity,
        entry->use_count + 1u, sizeof(*entry->uses))) {
        collector->failed = 1;
        return 0;
    }
    item = &entry->uses[entry->use_count];
    memset(item, 0, sizeof(*item));
    item->requested_path = scheduler_strdup(use->module_name);
    if (!item->requested_path) {
        collector->failed = 1;
        return 0;
    }
    item->span = use->base.source_span;
    item->ordinal = ordinal;
    ++entry->use_count;
    return 1;
}

static OccModuleSchedulerStatus scheduler_map_resolver(
    OccModuleResolverStatus status
) {
    if (status == OCC_MODULE_RESOLVER_OK) return OCC_MODULE_SCHEDULER_OK;
    if (status == OCC_MODULE_RESOLVER_OUT_OF_MEMORY)
        return OCC_MODULE_SCHEDULER_OUT_OF_MEMORY;
    if (status == OCC_MODULE_RESOLVER_CYCLE)
        return OCC_MODULE_SCHEDULER_CYCLE;
    if (status == OCC_MODULE_RESOLVER_NOT_FOUND)
        return OCC_MODULE_SCHEDULER_SOURCE_NOT_FOUND;
    if (status == OCC_MODULE_RESOLVER_SOURCE_DENIED)
        return OCC_MODULE_SCHEDULER_SOURCE_DENIED;
    if (status == OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS)
        return OCC_MODULE_SCHEDULER_SOURCE_AMBIGUOUS;
    if (status == OCC_MODULE_RESOLVER_SOURCE_IO_ERROR)
        return OCC_MODULE_SCHEDULER_SOURCE_IO_ERROR;
    if (status == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED)
        return OCC_MODULE_SCHEDULER_RESOURCE_LIMIT;
    if (status == OCC_MODULE_RESOLVER_PROVIDER_ERROR)
        return OCC_MODULE_SCHEDULER_PROVIDER_ERROR;
    return OCC_MODULE_SCHEDULER_RESOLVER_ERROR;
}

static OccModuleSchedulerStatus scheduler_parse_source(
    OccModuleScheduler* scheduler,
    const OccModuleSourceView* source,
    uint32_t module_id
) {
    OccModuleSchedulerEntry* entry;
    ASTValidationError validation_error;
    UseCollector collector;
    ASTUseVisitStatus visit_status;
    ASTNode* program = NULL;
    if (!scheduler || !source || !source->canonical_identity ||
        !scheduler->callbacks.parse) return OCC_MODULE_SCHEDULER_INVALID_ARGUMENT;
    entry = scheduler_entry(scheduler, module_id);
    if (!entry) return OCC_MODULE_SCHEDULER_OUT_OF_MEMORY;
    if (!scheduler->callbacks.parse(scheduler->callbacks.context, source, &program) ||
        !program) {
        if (program) ast_destroy(program);
        return OCC_MODULE_SCHEDULER_PARSE_ERROR;
    }
    if (program->type != AST_PROGRAM || !ast_validate(program, &validation_error)) {
        ast_destroy(program);
        return OCC_MODULE_SCHEDULER_AST_ERROR;
    }
    entry->program = program;
    memset(&collector, 0, sizeof(collector));
    collector.entry = entry;
    visit_status = ast_visit_top_level_uses(program, scheduler_collect_use,
        &collector, NULL);
    if (collector.failed) return OCC_MODULE_SCHEDULER_OUT_OF_MEMORY;
    if (visit_status != AST_USE_VISIT_OK)
        return OCC_MODULE_SCHEDULER_AST_ERROR;
    return OCC_MODULE_SCHEDULER_OK;
}

static OccModuleSchedulerStatus scheduler_resolve_dependency(
    OccModuleScheduler* scheduler,
    uint32_t importer_id,
    const char* requested_path,
    uint32_t ordinal,
    const OccSourceSpan* span,
    int implicit,
    uint32_t* out_dependency_id,
    int* out_is_new
) {
    const OccModuleResolverNode* importer;
    OccModuleSourceRequest request;
    OccModuleSourceView source;
    OccModuleResolverStatus resolver_status;
    OccModuleSchedulerStatus status;
    uint32_t dependency_id = 0u;
    const OccModuleResolverNode* dependency;
    int is_new;
    memset(&request, 0, sizeof(request));
    memset(&source, 0, sizeof(source));
    importer = occ_module_resolver_get_module(&scheduler->resolver, importer_id);
    if (!importer) return OCC_MODULE_SCHEDULER_RESOLVER_ERROR;
    request.requested_path = requested_path;
    request.importer_identity = importer->canonical_identity;
    request.use_span = span;
    resolver_status = occ_module_resolver_resolve_source(
        &scheduler->resolver, &scheduler->provider, &request, &source);
    if (resolver_status != OCC_MODULE_RESOLVER_OK)
        return scheduler_map_resolver(resolver_status);
    resolver_status = occ_module_resolver_register(&scheduler->resolver,
        source.canonical_identity, source.content_fingerprint, &dependency_id);
    if (resolver_status != OCC_MODULE_RESOLVER_OK) {
        occ_module_resolver_release_source(&scheduler->provider, &source);
        return scheduler_map_resolver(resolver_status);
    }
    is_new = occ_module_resolver_get_module(&scheduler->resolver,
        dependency_id)->state == OCC_MODULE_STATE_DISCOVERED;
    resolver_status = implicit
        ? occ_module_resolver_add_implicit_osmodule(&scheduler->resolver,
            importer_id, dependency_id, span)
        : occ_module_resolver_add_dependency(&scheduler->resolver,
            importer_id, dependency_id, requested_path, ordinal, span);
    if (resolver_status != OCC_MODULE_RESOLVER_OK) {
        occ_module_resolver_release_source(&scheduler->provider, &source);
        return scheduler_map_resolver(resolver_status);
    }
    dependency = occ_module_resolver_get_module(&scheduler->resolver, dependency_id);
    if (is_new) {
        resolver_status = occ_module_resolver_begin(&scheduler->resolver, dependency_id);
        if (resolver_status != OCC_MODULE_RESOLVER_OK) {
            occ_module_resolver_release_source(&scheduler->provider, &source);
            return scheduler_map_resolver(resolver_status);
        }
        status = scheduler_parse_source(scheduler, &source, dependency_id);
        occ_module_resolver_release_source(&scheduler->provider, &source);
        if (status != OCC_MODULE_SCHEDULER_OK) {
            (void)occ_module_resolver_fail(&scheduler->resolver, dependency_id);
            return status;
        }
    } else {
        occ_module_resolver_release_source(&scheduler->provider, &source);
        if (dependency && dependency->state == OCC_MODULE_STATE_RESOLVING)
            return OCC_MODULE_SCHEDULER_CYCLE;
        if (dependency && dependency->state == OCC_MODULE_STATE_FAILED)
            return OCC_MODULE_SCHEDULER_RESOLVER_ERROR;
    }
    *out_dependency_id = dependency_id;
    *out_is_new = is_new;
    return OCC_MODULE_SCHEDULER_OK;
}

OccModuleSchedulerStatus occ_module_scheduler_init(
    OccModuleScheduler* scheduler,
    const OccModuleResolverConfig* resolver_config,
    const OccModuleSourceProvider* provider,
    const OccModuleSchedulerCallbacks* callbacks
) {
    OccModuleResolverStatus status;
    if (!scheduler || !provider || !provider->resolve || !provider->release ||
        !callbacks || !callbacks->parse || !callbacks->semantic)
        return OCC_MODULE_SCHEDULER_INVALID_ARGUMENT;
    memset(scheduler, 0, sizeof(*scheduler));
    status = occ_module_resolver_init(&scheduler->resolver, resolver_config);
    if (status != OCC_MODULE_RESOLVER_OK) return scheduler_map_resolver(status);
    scheduler->provider = *provider;
    scheduler->callbacks = *callbacks;
    scheduler->initialized = 1;
    return OCC_MODULE_SCHEDULER_OK;
}

OccModuleSchedulerStatus occ_module_scheduler_run(
    OccModuleScheduler* scheduler,
    const OccModuleSourceRequest* root_request
) {
    OccModuleSourceView root_source;
    OccModuleResolverStatus resolver_status;
    OccModuleSchedulerStatus status;
    uint32_t root_id = 0u;
    uint32_t* order = NULL;
    size_t order_count = 0u, i;
    SchedulerFrame* stack = NULL;
    size_t stack_count = 0u, stack_capacity = 0u;
    if (!scheduler || !scheduler->initialized || !root_request ||
        !root_request->requested_path || !root_request->use_span ||
        scheduler->resolver.module_count != 0u)
        return OCC_MODULE_SCHEDULER_INVALID_ARGUMENT;
    memset(&root_source, 0, sizeof(root_source));
    resolver_status = occ_module_resolver_resolve_source(
        &scheduler->resolver, &scheduler->provider, root_request, &root_source);
    if (resolver_status != OCC_MODULE_RESOLVER_OK)
        return scheduler_map_resolver(resolver_status);
    resolver_status = occ_module_resolver_register(&scheduler->resolver,
        root_source.canonical_identity, root_source.content_fingerprint, &root_id);
    if (resolver_status != OCC_MODULE_RESOLVER_OK) {
        occ_module_resolver_release_source(&scheduler->provider, &root_source);
        return scheduler_map_resolver(resolver_status);
    }
    if (!scheduler_grow((void**)&scheduler->entries, &scheduler->entry_capacity,
        scheduler->resolver.max_modules, sizeof(*scheduler->entries))) {
        occ_module_resolver_release_source(&scheduler->provider, &root_source);
        return OCC_MODULE_SCHEDULER_OUT_OF_MEMORY;
    }
    memset(scheduler->entries, 0,
        scheduler->entry_capacity * sizeof(*scheduler->entries));
    resolver_status = occ_module_resolver_begin(&scheduler->resolver, root_id);
    if (resolver_status != OCC_MODULE_RESOLVER_OK) {
        occ_module_resolver_release_source(&scheduler->provider, &root_source);
        return scheduler_map_resolver(resolver_status);
    }
    status = scheduler_parse_source(scheduler, &root_source, root_id);
    occ_module_resolver_release_source(&scheduler->provider, &root_source);
    if (status != OCC_MODULE_SCHEDULER_OK) {
        (void)occ_module_resolver_fail(&scheduler->resolver, root_id);
        return status;
    }
    if (!scheduler_grow((void**)&stack, &stack_capacity, 1u, sizeof(*stack)))
        return OCC_MODULE_SCHEDULER_OUT_OF_MEMORY;
    stack[stack_count++] = (SchedulerFrame){root_id, 0u};

    while (stack_count != 0u) {
        SchedulerFrame* frame = &stack[stack_count - 1u];
        OccModuleSchedulerEntry* entry = scheduler_entry(scheduler, frame->module_id);
        const ASTProgram* program;
        if (!entry || !entry->program) { status = OCC_MODULE_SCHEDULER_AST_ERROR; goto done; }
        program = (const ASTProgram*)entry->program;
        if (!entry->implicit_osmodule_processed) {
            entry->implicit_osmodule_processed = 1u;
            if (!program->disable_osmodule) {
                uint32_t dep_id = 0u;
                int is_new = 0;
                OccSourceSpan span = program->base.source_span;
                status = scheduler_resolve_dependency(scheduler, frame->module_id,
                    "osmodule", 0u, &span, 1, &dep_id, &is_new);
                if (status != OCC_MODULE_SCHEDULER_OK) goto done;
                if (is_new) {
                    if (!scheduler_grow((void**)&stack, &stack_capacity,
                        stack_count + 1u, sizeof(*stack))) {
                        status = OCC_MODULE_SCHEDULER_OUT_OF_MEMORY; goto done;
                    }
                    stack[stack_count++] = (SchedulerFrame){dep_id, 0u};
                    continue;
                }
            }
        }
        if (frame->next_use < entry->use_count) {
            OccModuleSchedulerUse* use = &entry->uses[frame->next_use++];
            uint32_t dep_id = 0u;
            int is_new = 0;
            status = scheduler_resolve_dependency(scheduler, frame->module_id,
                use->requested_path, use->ordinal, &use->span, 0,
                &dep_id, &is_new);
            if (status != OCC_MODULE_SCHEDULER_OK) goto done;
            if (is_new) {
                if (!scheduler_grow((void**)&stack, &stack_capacity,
                    stack_count + 1u, sizeof(*stack))) {
                    status = OCC_MODULE_SCHEDULER_OUT_OF_MEMORY; goto done;
                }
                stack[stack_count++] = (SchedulerFrame){dep_id, 0u};
            }
            continue;
        }
        resolver_status = occ_module_resolver_complete(&scheduler->resolver,
            frame->module_id);
        if (resolver_status != OCC_MODULE_RESOLVER_OK) {
            status = scheduler_map_resolver(resolver_status); goto done;
        }
        --stack_count;
    }

    order = (uint32_t*)malloc(scheduler->resolver.module_count * sizeof(*order));
    if (!order) { status = OCC_MODULE_SCHEDULER_OUT_OF_MEMORY; goto done; }
    resolver_status = occ_module_resolver_dependency_order(&scheduler->resolver,
        order, scheduler->resolver.module_count, &order_count);
    if (resolver_status != OCC_MODULE_RESOLVER_OK) {
        status = scheduler_map_resolver(resolver_status); goto done;
    }
    for (i = 0u; i < order_count; ++i) {
        const OccModuleResolverNode* node =
            occ_module_resolver_get_module(&scheduler->resolver, order[i]);
        const OccModuleSchedulerEntry* item = scheduler_entry(scheduler, order[i]);
        if (!node || !item || !item->program ||
            !scheduler->callbacks.semantic(scheduler->callbacks.context,
                order[i], node->canonical_identity, item->program)) {
            status = OCC_MODULE_SCHEDULER_SEMANTIC_ERROR;
            goto done;
        }
    }
    status = OCC_MODULE_SCHEDULER_OK;
done:
    free(order);
    free(stack);
    return status;
}

void occ_module_scheduler_destroy(OccModuleScheduler* scheduler) {
    size_t i, j;
    if (!scheduler) return;
    if (scheduler->entries) {
        for (i = 0u; i < scheduler->entry_capacity; ++i) {
            if (scheduler->entries[i].program)
                ast_destroy(scheduler->entries[i].program);
            for (j = 0u; j < scheduler->entries[i].use_count; ++j)
                free(scheduler->entries[i].uses[j].requested_path);
            free(scheduler->entries[i].uses);
        }
    }
    free(scheduler->entries);
    if (scheduler->initialized) occ_module_resolver_destroy(&scheduler->resolver);
    memset(scheduler, 0, sizeof(*scheduler));
}

const char* occ_module_scheduler_status_string(OccModuleSchedulerStatus status) {
    switch (status) {
        case OCC_MODULE_SCHEDULER_OK: return "ok";
        case OCC_MODULE_SCHEDULER_INVALID_ARGUMENT: return "invalid argument";
        case OCC_MODULE_SCHEDULER_OUT_OF_MEMORY: return "out of memory";
        case OCC_MODULE_SCHEDULER_RESOLVER_ERROR: return "resolver error";
        case OCC_MODULE_SCHEDULER_PARSE_ERROR: return "parse error";
        case OCC_MODULE_SCHEDULER_AST_ERROR: return "invalid AST";
        case OCC_MODULE_SCHEDULER_CYCLE: return "dependency cycle";
        case OCC_MODULE_SCHEDULER_SEMANTIC_ERROR: return "semantic error";
        case OCC_MODULE_SCHEDULER_SOURCE_NOT_FOUND: return "module source not found";
        case OCC_MODULE_SCHEDULER_SOURCE_DENIED: return "module source denied";
        case OCC_MODULE_SCHEDULER_SOURCE_AMBIGUOUS: return "ambiguous module source";
        case OCC_MODULE_SCHEDULER_SOURCE_IO_ERROR: return "module source I/O error";
        case OCC_MODULE_SCHEDULER_RESOURCE_LIMIT: return "resource limit exceeded";
        case OCC_MODULE_SCHEDULER_PROVIDER_ERROR: return "source provider error";
        default: return "unknown scheduler status";
    }
}
