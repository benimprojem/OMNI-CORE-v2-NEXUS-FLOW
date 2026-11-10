#include "module_resolver.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct FakeSourceProvider {
    OccModuleSourceProviderStatus status;
    int malformed_success;
    unsigned int resolve_calls;
    unsigned int release_calls;
} FakeSourceProvider;

static const uint8_t fake_module_source[] = "group db { }";

static OccModuleSourceProviderStatus fake_source_resolve(
    void* context,
    const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
) {
    FakeSourceProvider* fake = (FakeSourceProvider*)context;
    ++fake->resolve_calls;
    if (!request || !request->requested_path ||
        strcmp(request->requested_path, "db.mysql.sql") != 0 ||
        !request->importer_identity ||
        strcmp(request->importer_identity, "project://app/main.nxf") != 0 ||
        !request->use_span || !request->use_span->has_location)
        return OCC_MODULE_SOURCE_PROVIDER_ERROR;
    if (fake->status != OCC_MODULE_SOURCE_PROVIDER_OK)
        return fake->status;
    out_source->provider_cookie = fake;
    out_source->source_bytes = fake_module_source;
    out_source->source_length = sizeof(fake_module_source) - 1u;
    if (!fake->malformed_success)
        out_source->canonical_identity = "project://lib/db.nxf";
    return OCC_MODULE_SOURCE_PROVIDER_OK;
}

static void fake_source_release(void* context, OccModuleSourceView* source) {
    FakeSourceProvider* fake = (FakeSourceProvider*)context;
    ++fake->release_calls;
    (void)source;
}

static int test_source_provider_boundary(void) {
    OccModuleResolver resolver;
    OccModuleResolverConfig config = {4u, 8u, 512u, 128u};
    OccModuleSourceRequest request;
    OccModuleSourceView source;
    OccModuleSourceProvider provider;
    FakeSourceProvider fake;
    OccSourceSpan span;
    uint32_t module_id = 0u;
    int resolver_initialized = 0;
    int ok = 0;
#define PROVIDER_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "PROVIDER_CHECK failed line %d: %s\n", __LINE__, #condition); \
        goto done; \
    } \
} while (0)
    memset(&resolver, 0, sizeof(resolver));
    memset(&request, 0, sizeof(request));
    memset(&source, 0, sizeof(source));
    memset(&provider, 0, sizeof(provider));
    memset(&fake, 0, sizeof(fake));
    memset(&span, 0, sizeof(span));
    span.start_offset = 4u;
    span.end_offset = 20u;
    span.has_location = 1u;
    request.requested_path = "db.mysql.sql";
    request.importer_identity = "project://app/main.nxf";
    request.use_span = &span;
    provider.context = &fake;
    provider.resolve = fake_source_resolve;
    provider.release = fake_source_release;
    fake.status = OCC_MODULE_SOURCE_PROVIDER_OK;

    PROVIDER_CHECK(occ_module_resolver_init(&resolver, &config) == OCC_MODULE_RESOLVER_OK);
    resolver_initialized = 1;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_OK);
    PROVIDER_CHECK(source.canonical_identity != NULL);
    PROVIDER_CHECK(strcmp(source.canonical_identity, "project://lib/db.nxf") == 0);
    PROVIDER_CHECK(source.source_bytes == fake_module_source);
    PROVIDER_CHECK(source.source_length == sizeof(fake_module_source) - 1u);
    PROVIDER_CHECK(source.content_fingerprint ==
        occ_module_resolver_fingerprint_source(fake_module_source,
            sizeof(fake_module_source) - 1u));
    PROVIDER_CHECK(occ_module_resolver_register(&resolver, source.canonical_identity,
        source.content_fingerprint, &module_id) == OCC_MODULE_RESOLVER_OK);
    PROVIDER_CHECK(module_id == 1u);
    occ_module_resolver_release_source(&provider, &source);
    PROVIDER_CHECK(fake.release_calls == 1u);
    PROVIDER_CHECK(source.canonical_identity == NULL && source.source_bytes == NULL);

    fake.status = OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_NOT_FOUND);
    PROVIDER_CHECK(source.canonical_identity == NULL);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_DENIED;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_DENIED);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_AMBIGUOUS;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_IO_ERROR;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_IO_ERROR);
    PROVIDER_CHECK(fake.release_calls == 1u);

    fake.status = OCC_MODULE_SOURCE_PROVIDER_OK;
    fake.malformed_success = 1;
    PROVIDER_CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_PROVIDER_ERROR);
    PROVIDER_CHECK(fake.release_calls == 2u);
    PROVIDER_CHECK(source.canonical_identity == NULL && source.source_bytes == NULL);
    occ_module_resolver_destroy(&resolver);
    resolver_initialized = 0;

    /* A source exceeding the configured byte budget is released and rejected. */
    {
        OccModuleResolverConfig small = {2u, 2u, 128u, 4u};
        PROVIDER_CHECK(occ_module_resolver_init(&resolver, &small) == OCC_MODULE_RESOLVER_OK);
        resolver_initialized = 1;
        fake.malformed_success = 0;
        PROVIDER_CHECK(occ_module_resolver_resolve_source(
            &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED);
        PROVIDER_CHECK(fake.release_calls == 3u);
    }
    ok = 1;
done:
    if (resolver_initialized) occ_module_resolver_destroy(&resolver);
#undef PROVIDER_CHECK
    return ok;
}

static int test_implicit_osmodule_dependency_edge(void) {
    OccModuleResolver resolver;
    OccModuleResolverConfig config = {4u, 8u, 128u, 1024u};
    OccSourceSpan program_span;
    uint32_t root = 0u, osmodule = 0u;
    const OccModuleResolverEdge* edge;
    int ok = 0;
#define IMPLICIT_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "IMPLICIT_CHECK failed line %d: %s\n", __LINE__, #condition); \
        goto done; \
    } \
} while (0)

    memset(&resolver, 0, sizeof(resolver));
    memset(&program_span, 0, sizeof(program_span));
    program_span.source_id = 2u;
    program_span.start_offset = 0u;
    program_span.end_offset = 64u;
    program_span.start_line = 1u;
    program_span.start_column = 1u;
    program_span.end_line = 4u;
    program_span.end_column = 1u;
    program_span.has_location = 1u;

    IMPLICIT_CHECK(occ_module_resolver_init(&resolver, &config) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(occ_module_resolver_register(&resolver, "project://app/main.nxf",
        UINT64_C(0x1010), &root) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(occ_module_resolver_register(&resolver, "stdlib://osmodule",
        UINT64_C(0x2020), &osmodule) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(occ_module_resolver_begin(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(occ_module_resolver_add_implicit_osmodule(
        &resolver, root, osmodule, &program_span) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(resolver.edge_count == 1u);
    edge = occ_module_resolver_get_edge(&resolver, 0u);
    IMPLICIT_CHECK(edge != NULL);
    IMPLICIT_CHECK(edge->kind == OCC_MODULE_EDGE_IMPLICIT_OSMODULE);
    IMPLICIT_CHECK(edge->ast_use_node_id == 0u);
    IMPLICIT_CHECK(edge->importer_module_id == root);
    IMPLICIT_CHECK(edge->dependency_module_id == osmodule);
    IMPLICIT_CHECK(strcmp(edge->requested_path, "osmodule") == 0);
    IMPLICIT_CHECK(edge->use_span.source_id == program_span.source_id);
    IMPLICIT_CHECK(edge->use_span.start_offset == program_span.start_offset);
    IMPLICIT_CHECK(edge->use_span.end_offset == program_span.end_offset);
    IMPLICIT_CHECK(occ_module_resolver_add_implicit_osmodule(
        &resolver, root, osmodule, &program_span) ==
        OCC_MODULE_RESOLVER_INVALID_ARGUMENT);
    IMPLICIT_CHECK(occ_module_resolver_add_dependency(&resolver, root, osmodule,
        "osmodule", 42u, &program_span) == OCC_MODULE_RESOLVER_OK);
    IMPLICIT_CHECK(resolver.edge_count == 2u);
    edge = occ_module_resolver_get_edge(&resolver, 1u);
    IMPLICIT_CHECK(edge != NULL && edge->kind == OCC_MODULE_EDGE_EXPLICIT_USE);
    IMPLICIT_CHECK(edge->ast_use_node_id == 42u);
    IMPLICIT_CHECK(occ_module_resolver_complete(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    ok = 1;
done:
    occ_module_resolver_destroy(&resolver);
#undef IMPLICIT_CHECK
    return ok;
}

int main(void) {\n    if (!test_dependency_first_schedule()) return 1;\n    if (!test_dependency_first_schedule()) return 1;
    if (!test_source_provider_boundary()) return 1;\n    if (!test_dependency_first_schedule()) return 1;
    if (!test_implicit_osmodule_dependency_edge()) return 1;
    OccModuleResolver resolver;
    OccModuleResolverConfig config = {8u, 16u, 1024u, 4096u};
    OccSourceSpan span;
    uint32_t root = 0u, dep = 0u, repeated = 0u;
    const OccModuleResolverNode* node;
    const OccModuleResolverEdge* edge;

    memset(&resolver, 0, sizeof(resolver));
    memset(&span, 0, sizeof(span));
    span.source_id = 3u;
    span.start_offset = 12u;
    span.end_offset = 31u;
    span.start_line = 2u;
    span.start_column = 1u;
    span.end_line = 2u;
    span.end_column = 20u;
    span.has_location = 1u;

    CHECK(occ_module_resolver_init(&resolver, &config) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_register(&resolver, "project://app/main.nxf",
        UINT64_C(0x1234), &root) == OCC_MODULE_RESOLVER_OK);
    CHECK(root == 1u);
    CHECK(occ_module_resolver_register(&resolver, "project://lib/db.nxf",
        UINT64_C(0x5678), &dep) == OCC_MODULE_RESOLVER_OK);
    CHECK(dep == 2u);
    CHECK(occ_module_resolver_register(&resolver, "project://lib/db.nxf",
        UINT64_C(0x5678), &repeated) == OCC_MODULE_RESOLVER_OK);
    CHECK(repeated == dep);
    CHECK(occ_module_resolver_register(&resolver, "project://lib/db.nxf",
        UINT64_C(0x9999), &repeated) == OCC_MODULE_RESOLVER_CONTENT_MISMATCH);
    CHECK(repeated == 0u);

    CHECK(occ_module_resolver_begin(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_add_dependency(&resolver, root, dep,
        "db.mysql.sql", 42u, &span) == OCC_MODULE_RESOLVER_OK);
    CHECK(resolver.edge_count == 1u);
    edge = occ_module_resolver_get_edge(&resolver, 0u);
    CHECK(edge != NULL);
    CHECK(edge->importer_module_id == root);
    CHECK(edge->dependency_module_id == dep);
    CHECK(edge->ast_use_node_id == 42u);
    CHECK(strcmp(edge->requested_path, "db.mysql.sql") == 0);
    CHECK(edge->use_span.start_offset == 12u && edge->use_span.end_offset == 31u);
    CHECK(occ_module_resolver_add_dependency(&resolver, root, dep,
        "db.mysql", 42u, &span) == OCC_MODULE_RESOLVER_INVALID_ARGUMENT);

    CHECK(occ_module_resolver_begin(&resolver, dep) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_add_dependency(&resolver, dep, root,
        "app.main", 77u, &span) == OCC_MODULE_RESOLVER_CYCLE);
    CHECK(resolver.edge_count == 2u);
    edge = occ_module_resolver_get_edge(&resolver, 1u);
    CHECK(edge != NULL && edge->ast_use_node_id == 77u);
    CHECK(edge->use_span.source_id == 3u);

    CHECK(occ_module_resolver_complete(&resolver, dep) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_complete(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_begin(&resolver, root) == OCC_MODULE_RESOLVER_INVALID_STATE);
    node = occ_module_resolver_get_module(&resolver, dep);
    CHECK(node != NULL && node->state == OCC_MODULE_STATE_RESOLVED);
    CHECK(occ_module_resolver_get_module(&resolver, 0u) == NULL);
    CHECK(occ_module_resolver_get_edge(&resolver, 99u) == NULL);

    occ_module_resolver_destroy(&resolver);
    CHECK(resolver.modules == NULL && resolver.edges == NULL);

    /* A failed dependency cannot be attached to a new importer. */
    CHECK(occ_module_resolver_init(&resolver, &config) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_register(&resolver, "root", 1u, &root) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_register(&resolver, "bad", 2u, &dep) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_fail(&resolver, dep) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_begin(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_add_dependency(&resolver, root, dep,
        "bad.module", 91u, &span) == OCC_MODULE_RESOLVER_DEPENDENCY_FAILED);
    CHECK(resolver.edge_count == 0u);
    CHECK(occ_module_resolver_complete(&resolver, root) == OCC_MODULE_RESOLVER_OK);
    occ_module_resolver_destroy(&resolver);

    /* Explicit resource limits reject growth without publishing partial records. */
    {
        OccModuleResolverConfig limited = {1u, 1u, 8u, 4u};
        CHECK(occ_module_resolver_init(&resolver, &limited) == OCC_MODULE_RESOLVER_OK);
        CHECK(occ_module_resolver_register(&resolver, "root", 1u, &root) == OCC_MODULE_RESOLVER_OK);
        CHECK(occ_module_resolver_register(&resolver, "dependency", 2u, &dep) == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED);
        CHECK(dep == 0u && resolver.module_count == 1u);
        CHECK(occ_module_resolver_begin(&resolver, root) == OCC_MODULE_RESOLVER_OK);
        CHECK(occ_module_resolver_add_dependency(&resolver, root, root,
            "self", 92u, &span) == OCC_MODULE_RESOLVER_CYCLE);
        CHECK(resolver.edge_count == 1u);
        CHECK(occ_module_resolver_add_dependency(&resolver, root, root,
            "self2", 93u, &span) == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED);
        CHECK(resolver.edge_count == 1u);
        occ_module_resolver_destroy(&resolver);
    }
    puts("Module resolver graph tests passed.");
    return 0;
}
