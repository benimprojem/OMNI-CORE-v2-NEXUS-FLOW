#include "module_resolver.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
    OccModuleResolver resolver;
    OccModuleResolverConfig config = {8u, 16u, 1024u};
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
    puts("Module resolver graph tests passed.");
    return 0;
}
