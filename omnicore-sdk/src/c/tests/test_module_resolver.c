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

    CHECK(occ_module_resolver_init(&resolver, &config) == OCC_MODULE_RESOLVER_OK);
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_OK);
    CHECK(source.canonical_identity != NULL);
    CHECK(strcmp(source.canonical_identity, "project://lib/db.nxf") == 0);
    CHECK(source.source_bytes == fake_module_source);
    CHECK(source.source_length == sizeof(fake_module_source) - 1u);
    CHECK(source.content_fingerprint ==
        occ_module_resolver_fingerprint_source(fake_module_source,
            sizeof(fake_module_source) - 1u));
    CHECK(occ_module_resolver_register(&resolver, source.canonical_identity,
        source.content_fingerprint, &module_id) == OCC_MODULE_RESOLVER_OK);
    CHECK(module_id == 1u);
    occ_module_resolver_release_source(&provider, &source);
    CHECK(fake.release_calls == 1u);
    CHECK(source.canonical_identity == NULL && source.source_bytes == NULL);

    fake.status = OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_NOT_FOUND);
    CHECK(source.canonical_identity == NULL);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_DENIED;
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_DENIED);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_AMBIGUOUS;
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS);
    fake.status = OCC_MODULE_SOURCE_PROVIDER_IO_ERROR;
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_SOURCE_IO_ERROR);
    CHECK(fake.release_calls == 1u);

    fake.status = OCC_MODULE_SOURCE_PROVIDER_OK;
    fake.malformed_success = 1;
    CHECK(occ_module_resolver_resolve_source(
        &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_PROVIDER_ERROR);
    CHECK(fake.release_calls == 2u);
    CHECK(source.canonical_identity == NULL && source.source_bytes == NULL);
    occ_module_resolver_destroy(&resolver);

    /* A source exceeding the configured byte budget is released and rejected. */
    {
        OccModuleResolverConfig small = {2u, 2u, 128u, 4u};
        CHECK(occ_module_resolver_init(&resolver, &small) == OCC_MODULE_RESOLVER_OK);
        fake.malformed_success = 0;
        CHECK(occ_module_resolver_resolve_source(
            &resolver, &provider, &request, &source) == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED);
        CHECK(fake.release_calls == 3u);
        occ_module_resolver_destroy(&resolver);
    }
    return 1;
}

int main(void) {
    CHECK(test_source_provider_boundary());
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
