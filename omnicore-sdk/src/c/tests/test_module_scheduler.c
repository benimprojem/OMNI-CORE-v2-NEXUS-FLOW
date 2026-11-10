#include "module_scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 0; } } while (0)

typedef struct FakeProvider {
    int release_count;
    int cycle_mode;
} FakeProvider;

typedef struct FakePipeline {
    uint32_t semantic_order[8];
    size_t semantic_count;
    int cycle_mode;
    int long_import_mode;
    int duplicate_import_mode;
    int implicit_osmodule_mode;
    int binding_mode;
    int parse_root_count;
    int parse_a_count;
    int parse_b_count;
    int parse_osmodule_count;
} FakePipeline;

static const char* fake_source_for(const char* path, int cycle_mode) {
    if (strcmp(path, "root") == 0) return "root";
    if (strcmp(path, "a") == 0) return "a";
    if (strcmp(path, "b") == 0) return "b";
    if (strcmp(path, "osmodule") == 0) return "osmodule";
    (void)cycle_mode;
    return NULL;
}

static OccModuleSourceProviderStatus fake_resolve(
    void* context, const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
) {
    FakeProvider* provider = (FakeProvider*)context;
    const char* bytes;
    const char* identity;
    if (!request || !request->requested_path || !out_source)
        return OCC_MODULE_SOURCE_PROVIDER_ERROR;
    bytes = fake_source_for(request->requested_path, provider->cycle_mode);
    if (!bytes) return OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    if (strcmp(request->requested_path, "root") == 0)
        identity = "test://root";
    else if (strcmp(request->requested_path, "a") == 0)
        identity = "test://a";
    else if (strcmp(request->requested_path, "b") == 0)
        identity = "test://b";
    else if (strcmp(request->requested_path, "osmodule") == 0)
        identity = "test://osmodule";
    else return OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    out_source->canonical_identity = identity;
    out_source->source_bytes = (const uint8_t*)bytes;
    out_source->source_length = strlen(bytes);
    out_source->provider_cookie = provider;
    return OCC_MODULE_SOURCE_PROVIDER_OK;
}

static void fake_release(void* context, OccModuleSourceView* source) {
    FakeProvider* provider = (FakeProvider*)context;
    (void)source;
    ++provider->release_count;
}

static int append_use_ex(ASTNode* program, const char* name,
    const char* alias, const char* namespace_name) {
    ASTProgram* root = (ASTProgram*)program;
    ASTUse* use = (ASTUse*)ast_create(AST_USE);
    ASTNode* tail;
    size_t length;
    if (!use) return 0;
    length = strlen(name);
    use->module_name = (char*)malloc(length + 1u);
    if (!use->module_name) { ast_destroy((ASTNode*)use); return 0; }
    memcpy(use->module_name, name, length + 1u);
    if (alias) {
        size_t n = strlen(alias);
        use->alias = (char*)malloc(n + 1u);
        if (!use->alias) { ast_destroy((ASTNode*)use); return 0; }
        memcpy(use->alias, alias, n + 1u);
    }
    if (namespace_name) {
        size_t n = strlen(namespace_name);
        use->namespace_name = (char*)malloc(n + 1u);
        if (!use->namespace_name) { ast_destroy((ASTNode*)use); return 0; }
        memcpy(use->namespace_name, namespace_name, n + 1u);
    }
    use->base.line = 1;
    use->base.col = 1;
    use->base.source_span.has_location = 1u;
    use->base.source_span.start_line = 1u;
    use->base.source_span.start_column = 1u;
    use->base.source_span.end_line = 1u;
    use->base.source_span.end_column = (uint32_t)(length + 1u);
    if (!root->declarations) root->declarations = (ASTNode*)use;
    else {
        tail = root->declarations;
        while (tail->next) tail = tail->next;
        tail->next = (ASTNode*)use;
    }
    return 1;
}

static int append_use(ASTNode* program, const char* name) {
    return append_use_ex(program, name, NULL, NULL);
}

static int fake_parse(void* context, const OccModuleSourceView* source,
    ASTNode** out_program) {
    ASTNode* program;
    const char* name;
    if (!source || !source->source_bytes || !out_program) return 0;
    *out_program = NULL;
    name = (const char*)source->source_bytes;
    program = ast_create(AST_PROGRAM);
    if (!program) return 0;
    ((ASTProgram*)program)->disable_osmodule =
        (strcmp(name, "root") == 0 && ((FakePipeline*)context)->implicit_osmodule_mode)
            ? 0u : 1u;
    if (((ASTProgram*)program)->disable_osmodule) {
        ((ASTProgram*)program)->disable_osmodule_span.has_location = 1u;
        ((ASTProgram*)program)->disable_osmodule_span.start_line = 1u;
        ((ASTProgram*)program)->disable_osmodule_span.start_column = 1u;
        ((ASTProgram*)program)->disable_osmodule_span.end_line = 1u;
        ((ASTProgram*)program)->disable_osmodule_span.end_column = 1u;
    }
    program->line = 1;
    program->col = 1;
    program->source_span.has_location = 1u;
    program->source_span.start_line = 1u;
    program->source_span.start_column = 1u;
    program->source_span.end_line = 1u;
    program->source_span.end_column = 1u;
    if (strcmp(name, "root") == 0) {
        FakePipeline* pipeline = (FakePipeline*)context;
        ++pipeline->parse_root_count;
        if (pipeline->binding_mode) {
            if (!append_use_ex(program, "a", "math", "core")) {
                ast_destroy(program); return 0;
            }
        } else if (pipeline->implicit_osmodule_mode) {
            /* Root has no explicit imports in this focused policy test. */
        } else if (pipeline->long_import_mode) {
            if (!append_use(program, "module_name_that_exceeds_limit")) {
                ast_destroy(program); return 0;
            }
        } else if (!append_use(program, "a") ||
            (pipeline->duplicate_import_mode && !append_use(program, "a")) ||
            (!pipeline->cycle_mode && !append_use(program, "b"))) {
            ast_destroy(program); return 0;
        }
    } else if (strcmp(name, "a") == 0) {
        ++((FakePipeline*)context)->parse_a_count;
        if (!append_use(program, ((FakePipeline*)context)->cycle_mode ? "root" : "b")) {
            ast_destroy(program); return 0;
        }
    } else if (strcmp(name, "b") == 0) {
        ++((FakePipeline*)context)->parse_b_count;
        /* leaf */
    } else if (strcmp(name, "osmodule") == 0) {
        ++((FakePipeline*)context)->parse_osmodule_count;
        /* Standard module opts out of its own implicit dependency. */
    } else {
        ast_destroy(program); return 0;
    }
    *out_program = program;
    return 1;
}

static int fake_semantic(void* context, uint32_t module_id,
    const char* identity, const ASTNode* program) {
    FakePipeline* pipeline = (FakePipeline*)context;
    (void)program;
    if (!pipeline || !identity || pipeline->semantic_count >= 8u) return 0;
    if ((strcmp(identity, "test://b") == 0 && module_id != 3u) ||
        (strcmp(identity, "test://a") == 0 && module_id != 2u) ||
        (strcmp(identity, "test://root") == 0 && module_id != 1u) ||
        (strcmp(identity, "test://osmodule") == 0 && module_id != 2u))
        return 0;
    pipeline->semantic_order[pipeline->semantic_count++] = module_id;
    return 1;
}

static int test_dependency_first_semantic_schedule(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {8u, 16u, 1024u, 4096u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    OccModuleSchedulerStatus status;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_span.start_line = 1u;
    root_span.start_column = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;

    CHECK(occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) == OCC_MODULE_SCHEDULER_OK);
    status = occ_module_scheduler_run(&scheduler, &root_request);
    if (status != OCC_MODULE_SCHEDULER_OK) {
        fprintf(stderr, "scheduler returned %s (%d)\\n",
            occ_module_scheduler_status_string(status), (int)status);
        occ_module_scheduler_destroy(&scheduler);
        return 0;
    }
    if (pipeline.semantic_count != 3u ||
        pipeline.semantic_order[0] != 3u ||
        pipeline.semantic_order[1] != 2u ||
        pipeline.semantic_order[2] != 1u ||
        pipeline.parse_a_count != 1 || pipeline.parse_b_count != 1 ||
        fake_provider.release_count != 4) {
        occ_module_scheduler_destroy(&scheduler);
        return 0;
    }
    occ_module_scheduler_destroy(&scheduler);
    return 1;
}

static int test_use_collection_resource_limit(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {4u, 8u, 16u, 1024u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    OccModuleSchedulerStatus status;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.long_import_mode = 1;
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_span.start_line = 1u;
    root_span.start_column = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;
    if (occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) != OCC_MODULE_SCHEDULER_OK) return 0;
    status = occ_module_scheduler_run(&scheduler, &root_request);
    if (status != OCC_MODULE_SCHEDULER_RESOURCE_LIMIT ||
        pipeline.semantic_count != 0u) {
        fprintf(stderr, "limit test returned %s (%d)\\n",
            occ_module_scheduler_status_string(status), (int)status);
        occ_module_scheduler_destroy(&scheduler);
        return 0;
    }
    occ_module_scheduler_destroy(&scheduler);
    return 1;
}

static int test_dependency_cycle_is_reported(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {4u, 8u, 512u, 1024u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.cycle_mode = 1;
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;
    CHECK(occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) == OCC_MODULE_SCHEDULER_OK);
    {
        OccModuleSchedulerStatus status =
            occ_module_scheduler_run(&scheduler, &root_request);
        if (status != OCC_MODULE_SCHEDULER_CYCLE ||
            pipeline.semantic_count != 0u) {
            fprintf(stderr, "cycle test returned %s (%d)\\n",
                occ_module_scheduler_status_string(status), (int)status);
            occ_module_scheduler_destroy(&scheduler);
            return 0;
        }
    }
    occ_module_scheduler_destroy(&scheduler);
    return 1;
}


static int test_duplicate_import_reuses_one_module(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {8u, 16u, 1024u, 4096u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    OccModuleSchedulerStatus status;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.duplicate_import_mode = 1;
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_span.start_line = 1u;
    root_span.start_column = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;
    CHECK(occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) == OCC_MODULE_SCHEDULER_OK);
    status = occ_module_scheduler_run(&scheduler, &root_request);
    if (status != OCC_MODULE_SCHEDULER_OK ||
        pipeline.semantic_count != 3u ||
        pipeline.parse_a_count != 1 || pipeline.parse_b_count != 1 ||
        fake_provider.release_count != 5) {
        fprintf(stderr, "duplicate import test returned %s (%d)\n",
            occ_module_scheduler_status_string(status), (int)status);
        occ_module_scheduler_destroy(&scheduler);
        return 0;
    }
    occ_module_scheduler_destroy(&scheduler);
    return 1;
}

static int test_implicit_osmodule_is_dependency_first(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {8u, 16u, 1024u, 4096u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    OccModuleSchedulerStatus status;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.implicit_osmodule_mode = 1;
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_span.start_line = 1u;
    root_span.start_column = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;
    CHECK(occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) == OCC_MODULE_SCHEDULER_OK);
    status = occ_module_scheduler_run(&scheduler, &root_request);
    if (status != OCC_MODULE_SCHEDULER_OK ||
        pipeline.semantic_count != 2u ||
        pipeline.semantic_order[0] != 2u ||
        pipeline.semantic_order[1] != 1u ||
        pipeline.parse_osmodule_count != 1 ||
        fake_provider.release_count != 2) {
        fprintf(stderr, "implicit osmodule test returned %s (%d)\n",
            occ_module_scheduler_status_string(status), (int)status);
        occ_module_scheduler_destroy(&scheduler);
        return 0;
    }
    occ_module_scheduler_destroy(&scheduler);
    return 1;
}


static int test_use_alias_and_namespace_binding_preserved(void) {
    OccModuleScheduler scheduler;
    OccModuleResolverConfig config = {8u, 16u, 1024u, 4096u};
    OccModuleSourceProvider provider;
    OccModuleSchedulerCallbacks callbacks;
    OccModuleSourceRequest root_request;
    OccSourceSpan root_span;
    FakeProvider fake_provider;
    FakePipeline pipeline;
    OccModuleImportBinding binding;
    OccModuleSchedulerStatus status;
    memset(&scheduler, 0, sizeof(scheduler));
    memset(&provider, 0, sizeof(provider));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&root_request, 0, sizeof(root_request));
    memset(&root_span, 0, sizeof(root_span));
    memset(&fake_provider, 0, sizeof(fake_provider));
    memset(&pipeline, 0, sizeof(pipeline));
    memset(&binding, 0, sizeof(binding));
    pipeline.binding_mode = 1;
    provider.context = &fake_provider;
    provider.resolve = fake_resolve;
    provider.release = fake_release;
    callbacks.context = &pipeline;
    callbacks.parse = fake_parse;
    callbacks.semantic = fake_semantic;
    root_span.has_location = 1u;
    root_span.start_line = 1u;
    root_span.start_column = 1u;
    root_request.requested_path = "root";
    root_request.use_span = &root_span;
    CHECK(occ_module_scheduler_init(&scheduler, &config, &provider,
        &callbacks) == OCC_MODULE_SCHEDULER_OK);
    status = occ_module_scheduler_run(&scheduler, &root_request);
    CHECK(status == OCC_MODULE_SCHEDULER_OK);
    CHECK(occ_module_scheduler_get_import_binding(&scheduler, 1u, 1u,
        &binding) == OCC_MODULE_SCHEDULER_OK);
    CHECK(binding.importer_module_id == 1u);
    CHECK(binding.dependency_module_id == 2u);
    CHECK(binding.dependency_identity != NULL);
    CHECK(strcmp(binding.dependency_identity, "test://a") == 0);
    CHECK(strcmp(binding.requested_path, "a") == 0);
    CHECK(binding.alias && strcmp(binding.alias, "math") == 0);
    CHECK(binding.namespace_name &&
        strcmp(binding.namespace_name, "core") == 0);
    CHECK(binding.span.start_line == 1u);
    occ_module_scheduler_destroy(&scheduler);
    return 1;
fail:
    occ_module_scheduler_destroy(&scheduler);
    return 0;
}

int main(void) {
    if (!test_use_alias_and_namespace_binding_preserved()) return 1;
    if (!test_dependency_first_semantic_schedule()) return 1;
    if (!test_duplicate_import_reuses_one_module()) return 1;
    if (!test_implicit_osmodule_is_dependency_first()) return 1;
    if (!test_dependency_cycle_is_reported()) return 1;
    if (!test_use_collection_resource_limit()) return 1;
    return 0;
}
