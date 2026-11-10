#define _POSIX_C_SOURCE 200809L
#include "filesystem_module_provider.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* fs_duplicate(const char* value) {
    size_t n;
    char* copy;
    if (!value) return NULL;
    n = strlen(value);
    if (n >= OCC_FILESYSTEM_MODULE_PATH_MAX) return NULL;
    copy = (char*)malloc(n + 1u);
    if (copy) memcpy(copy, value, n + 1u);
    return copy;
}

static int fs_path_join(char* out, size_t cap, const char* root,
    const char* leaf) {
    size_t a, b;
    int separator;
    if (!out || !cap || !root || !root[0] || !leaf || !leaf[0]) return 0;
    a = strlen(root);
    b = strlen(leaf);
    separator = root[a - 1u] != '/' && root[a - 1u] != '\\';
    if (a + (size_t)separator + b + 1u > cap) return 0;
    memcpy(out, root, a);
    if (separator) out[a++] = '/';
    memcpy(out + a, leaf, b + 1u);
    return 1;
}

static int fs_is_absolute(const char* path) {
    if (!path || !path[0]) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    return path[0] && path[1] == ':';
}

OccModuleResolverStatus occ_filesystem_module_name_to_filename(
    const char* module_name, char* out_filename, size_t out_capacity
) {
    size_t i, n;
    int segment_start = 1;
    if (!module_name || !module_name[0] || !out_filename || out_capacity == 0u)
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    n = strlen(module_name);
    if (out_capacity < 5u || n > out_capacity - 5u)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    if (fs_is_absolute(module_name)) return OCC_MODULE_RESOLVER_SOURCE_DENIED;
    for (i = 0u; i < n; ++i) {
        unsigned char ch = (unsigned char)module_name[i];
        if (ch == '.') {
            if (segment_start) return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
            segment_start = 1;
        } else if ((ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch >= '0' && ch <= '9') || ch == '_') {
            segment_start = 0;
        } else {
            return OCC_MODULE_RESOLVER_SOURCE_DENIED;
        }
    }
    if (segment_start) return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    memcpy(out_filename, module_name, n);
    memcpy(out_filename + n, ".nxf", 5u);
    return OCC_MODULE_RESOLVER_OK;
}

static int fs_read_file(const char* path, size_t max_bytes,
    uint8_t** out_bytes, size_t* out_length) {
    FILE* file;
    long file_size;
    uint8_t* bytes;
    size_t read_size;
    *out_bytes = NULL;
    *out_length = 0u;
    file = fopen(path, "rb");
    if (!file) return errno == ENOENT || errno == ENOTDIR ? 0 : -1;
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        (uintmax_t)file_size > (uintmax_t)max_bytes ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    bytes = (uint8_t*)malloc((size_t)file_size + 1u);
    if (!bytes) { fclose(file); return -2; }
    read_size = fread(bytes, 1u, (size_t)file_size, file);
    {
        int read_failed = read_size != (size_t)file_size || ferror(file);
        int close_failed = fclose(file) != 0;
        if (read_failed || close_failed) {
            free(bytes);
            return -1;
        }
    }
    bytes[read_size] = 0u;
    *out_bytes = bytes;
    *out_length = read_size;
    return 1;
}

static OccModuleSourceProviderStatus fs_try_candidate(
    const char* candidate, size_t max_bytes, OccModuleSourceView* out_source
) {
    uint8_t* bytes = NULL;
    size_t length = 0u;
    char* identity;
    int result = fs_read_file(candidate, max_bytes, &bytes, &length);
    if (result == 0) return OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    if (result == -2) return OCC_MODULE_SOURCE_PROVIDER_ERROR;
    if (result < 0) return OCC_MODULE_SOURCE_PROVIDER_IO_ERROR;
#if defined(_WIN32)
    identity = _fullpath(NULL, candidate, 0);
    if (!identity) identity = fs_duplicate(candidate);
#else
    identity = realpath(candidate, NULL);
    if (!identity) identity = fs_duplicate(candidate);
#endif
    if (!identity) { free(bytes); return OCC_MODULE_SOURCE_PROVIDER_ERROR; }
    out_source->canonical_identity = identity;
    out_source->source_bytes = bytes;
    out_source->source_length = length;
    out_source->provider_cookie = NULL;
    return OCC_MODULE_SOURCE_PROVIDER_OK;
}

static OccModuleSourceProviderStatus fs_search_root(
    const char* root, const char* filename, size_t max_bytes,
    OccModuleSourceView* out_source
) {
    char candidate[OCC_FILESYSTEM_MODULE_PATH_MAX];
    if (!root || !root[0]) return OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
    if (!fs_path_join(candidate, sizeof(candidate), root, filename))
        return OCC_MODULE_SOURCE_PROVIDER_IO_ERROR;
    return fs_try_candidate(candidate, max_bytes, out_source);
}

static OccModuleSourceProviderStatus fs_resolve(
    void* context, const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
) {
    OccFilesystemModuleProvider* provider = (OccFilesystemModuleProvider*)context;
    char filename[OCC_FILESYSTEM_MODULE_PATH_MAX];
    size_t i;
    OccModuleSourceProviderStatus status;
    if (out_source) memset(out_source, 0, sizeof(*out_source));
    if (!provider || !provider->initialized || !request ||
        !request->requested_path || !request->use_span || !out_source)
        return OCC_MODULE_SOURCE_PROVIDER_ERROR;

    if (!request->importer_identity) {
        if (!fs_is_absolute(request->requested_path)) {
            /* Root source paths are interpreted relative to the process CWD. */
            return fs_try_candidate(request->requested_path,
                provider->max_source_bytes, out_source);
        }
        return fs_try_candidate(request->requested_path,
            provider->max_source_bytes, out_source);
    }

    if (occ_filesystem_module_name_to_filename(request->requested_path,
        filename, sizeof(filename)) != OCC_MODULE_RESOLVER_OK)
        return OCC_MODULE_SOURCE_PROVIDER_DENIED;

    status = fs_search_root(provider->dsplibs_root, filename,
        provider->max_source_bytes, out_source);
    if (status != OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND) return status;
    status = fs_search_root(provider->libs_root, filename,
        provider->max_source_bytes, out_source);
    if (status != OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND) return status;
    for (i = 0u; i < provider->include_count; ++i) {
        status = fs_search_root(provider->include_paths[i], filename,
            provider->max_source_bytes, out_source);
        if (status != OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND) return status;
    }
    return OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND;
}

static void fs_release(void* context, OccModuleSourceView* source) {
    (void)context;
    if (!source) return;
    free((void*)source->canonical_identity);
    free((void*)source->source_bytes);
    memset(source, 0, sizeof(*source));
}

OccModuleResolverStatus occ_filesystem_module_provider_init(
    OccFilesystemModuleProvider* provider,
    const OccFilesystemModuleProviderConfig* config
) {
    size_t i;
    if (!provider || !config || !config->project_config)
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    memset(provider, 0, sizeof(*provider));
    if (config->dsplibs_root && config->dsplibs_root[0]) {
        provider->dsplibs_root = fs_duplicate(config->dsplibs_root);
        if (!provider->dsplibs_root) goto oom;
    }
    if (config->libs_root && config->libs_root[0]) {
        provider->libs_root = fs_duplicate(config->libs_root);
        if (!provider->libs_root) goto oom;
    }
    if (config->project_config->project_root) {
        provider->project_root = fs_duplicate(config->project_config->project_root);
        if (!provider->project_root) goto oom;
    }
    if (config->project_config->include_count > OCC_PROJECT_CONFIG_MAX_INCLUDE_PATHS)
        goto invalid;
    for (i = 0u; i < config->project_config->include_count; ++i) {
        const char* include_path = config->project_config->include_paths[i];
        char resolved[OCC_FILESYSTEM_MODULE_PATH_MAX];
        if (!include_path || !include_path[0]) goto invalid;
        if (fs_is_absolute(include_path)) {
            provider->include_paths[i] = fs_duplicate(include_path);
        } else if (provider->project_root &&
            fs_path_join(resolved, sizeof(resolved), provider->project_root,
                include_path)) {
            provider->include_paths[i] = fs_duplicate(resolved);
        } else {
            goto invalid;
        }
        if (!provider->include_paths[i]) goto oom;
        ++provider->include_count;
    }
    provider->max_source_bytes = config->max_source_bytes
        ? config->max_source_bytes : OCC_MODULE_RESOLVER_DEFAULT_MAX_SOURCE_BYTES;
    provider->provider.context = provider;
    provider->provider.resolve = fs_resolve;
    provider->provider.release = fs_release;
    provider->initialized = 1;
    return OCC_MODULE_RESOLVER_OK;
invalid:
    occ_filesystem_module_provider_destroy(provider);
    return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
oom:
    occ_filesystem_module_provider_destroy(provider);
    return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
}

void occ_filesystem_module_provider_destroy(OccFilesystemModuleProvider* provider) {
    size_t i;
    if (!provider) return;
    free(provider->dsplibs_root);
    free(provider->libs_root);
    free(provider->project_root);
    for (i = 0u; i < provider->include_count; ++i)
        free(provider->include_paths[i]);
    memset(provider, 0, sizeof(*provider));
}

const OccModuleSourceProvider* occ_filesystem_module_provider_source(
    const OccFilesystemModuleProvider* provider
) {
    return provider && provider->initialized ? &provider->provider : NULL;
}
