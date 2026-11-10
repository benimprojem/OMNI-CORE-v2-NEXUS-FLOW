#include "filesystem_module_provider.h"

#include <stdio.h>
#include <string.h>

#ifndef OCC_MODULE_PROVIDER_FIXTURE_DIR
#error OCC_MODULE_PROVIDER_FIXTURE_DIR must identify the fixture root
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 0; } } while (0)

static int resolve_text(
    const OccModuleSourceProvider* provider,
    const char* name,
    const char* expected
) {
    OccModuleSourceRequest request;
    OccModuleSourceView view;
    OccSourceSpan span;
    OccModuleSourceProviderStatus status;
    memset(&request, 0, sizeof(request));
    memset(&view, 0, sizeof(view));
    memset(&span, 0, sizeof(span));
    span.has_location = 1u;
    span.start_line = 1u;
    span.start_column = 1u;
    span.end_line = 1u;
    span.end_column = 1u;
    request.requested_path = name;
    request.importer_identity = "fixture://root.nxf";
    request.use_span = &span;
    status = provider->resolve(provider->context, &request, &view);
    if (!expected) {
        CHECK(status == OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND);
        return 1;
    }
    CHECK(status == OCC_MODULE_SOURCE_PROVIDER_OK);
    CHECK(view.canonical_identity != NULL);
    CHECK(view.source_bytes != NULL);
    {
        size_t source_index = 0u, expected_index = 0u;
        size_t expected_length = strlen(expected);
        while (source_index < view.source_length && expected_index < expected_length) {
            if (view.source_bytes[source_index] == '\r' &&
                source_index + 1u < view.source_length &&
                view.source_bytes[source_index + 1u] == '\n') {
                ++source_index;
                continue;
            }
            if (view.source_bytes[source_index] != (uint8_t)expected[expected_index]) {
                provider->release(provider->context, &view);
                return 0;
            }
            ++source_index;
            ++expected_index;
        }
        CHECK(source_index == view.source_length);
        CHECK(expected_index == expected_length);
    }
    provider->release(provider->context, &view);
    CHECK(view.canonical_identity == NULL);
    CHECK(view.source_bytes == NULL);
    return 1;
}

static int test_single_file_name_mapping(void) {
    char filename[128];
    CHECK(occ_filesystem_module_name_to_filename(
        "db.mysql.sql", filename, sizeof(filename)) == OCC_MODULE_RESOLVER_OK);
    CHECK(strcmp(filename, "db.mysql.sql.nxf") == 0);
    CHECK(occ_filesystem_module_name_to_filename(
        "db", filename, sizeof(filename)) == OCC_MODULE_RESOLVER_OK);
    CHECK(strcmp(filename, "db.nxf") == 0);
    CHECK(occ_filesystem_module_name_to_filename(
        "../db", filename, sizeof(filename)) == OCC_MODULE_RESOLVER_SOURCE_DENIED);
    CHECK(occ_filesystem_module_name_to_filename(
        "db/sql", filename, sizeof(filename)) == OCC_MODULE_RESOLVER_SOURCE_DENIED);
    CHECK(occ_filesystem_module_name_to_filename(
        "db..sql", filename, sizeof(filename)) == OCC_MODULE_RESOLVER_INVALID_ARGUMENT);
    CHECK(occ_filesystem_module_name_to_filename(
        "db", filename, 4u) == OCC_MODULE_RESOLVER_LIMIT_EXCEEDED);
    return 1;
}

static int test_source_size_limit(void) {
    OccProjectConfig project;
    OccFilesystemModuleProvider provider_state;
    OccFilesystemModuleProviderConfig config;
    const OccModuleSourceProvider* provider;
    OccModuleSourceRequest request;
    OccModuleSourceView view;
    OccSourceSpan span;
    memset(&project, 0, sizeof(project));
    memset(&provider_state, 0, sizeof(provider_state));
    memset(&config, 0, sizeof(config));
    memset(&request, 0, sizeof(request));
    memset(&view, 0, sizeof(view));
    memset(&span, 0, sizeof(span));
    project.project_root = (char*)OCC_MODULE_PROVIDER_FIXTURE_DIR;
    config.dsplibs_root = OCC_MODULE_PROVIDER_FIXTURE_DIR "/dsplibs";
    config.libs_root = OCC_MODULE_PROVIDER_FIXTURE_DIR "/libs";
    config.project_config = &project;
    config.max_source_bytes = 4u;
    CHECK(occ_filesystem_module_provider_init(&provider_state, &config) ==
        OCC_MODULE_RESOLVER_OK);
    provider = occ_filesystem_module_provider_source(&provider_state);
    span.has_location = 1u;
    request.requested_path = "priority";
    request.importer_identity = "fixture://root.nxf";
    request.use_span = &span;
    CHECK(provider->resolve(provider->context, &request, &view) ==
        OCC_MODULE_SOURCE_PROVIDER_LIMIT_EXCEEDED);
    CHECK(view.canonical_identity == NULL && view.source_bytes == NULL);
    occ_filesystem_module_provider_destroy(&provider_state);
    return 1;
}

static int test_ordered_lookup(void) {
    OccProjectConfig project;
    OccFilesystemModuleProvider provider_state;
    OccFilesystemModuleProviderConfig config;
    const OccModuleSourceProvider* provider;
    memset(&project, 0, sizeof(project));
    memset(&provider_state, 0, sizeof(provider_state));
    memset(&config, 0, sizeof(config));
    project.project_root = (char*)OCC_MODULE_PROVIDER_FIXTURE_DIR;
    project.include_paths[0] = "custom";
    project.include_count = 1u;
    config.dsplibs_root = OCC_MODULE_PROVIDER_FIXTURE_DIR "/dsplibs";
    config.libs_root = OCC_MODULE_PROVIDER_FIXTURE_DIR "/libs";
    config.project_config = &project;
    CHECK(occ_filesystem_module_provider_init(&provider_state, &config) ==
        OCC_MODULE_RESOLVER_OK);
    provider = occ_filesystem_module_provider_source(&provider_state);
    CHECK(provider != NULL);
    CHECK(resolve_text(provider, "priority", "from-dsplibs\n"));
    CHECK(resolve_text(provider, "fallback", "from-libs-fallback\n"));
    CHECK(resolve_text(provider, "project_only", "from-project-include\n"));
    CHECK(resolve_text(provider, "configured_only", "from-configured-include\n"));
    CHECK(resolve_text(provider, "not_present", NULL));
    occ_filesystem_module_provider_destroy(&provider_state);
    CHECK(occ_filesystem_module_provider_source(&provider_state) == NULL);
    return 1;
}

int main(void) {
    if (!test_single_file_name_mapping()) return 1;
    if (!test_ordered_lookup()) return 1;
    if (!test_source_size_limit()) return 1;
    return 0;
}
