#include "module_resolver.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static char* resolver_duplicate(const char* text) {
    size_t length;
    char* copy;
    if (!text) return NULL;
    length = strlen(text);
    if (length == SIZE_MAX) return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy) memcpy(copy, text, length + 1u);
    return copy;
}

static int resolver_grow(void** buffer, size_t* capacity,
    size_t required, size_t element_size, size_t maximum) {
    size_t next;
    void* grown;
    if (required > maximum || element_size == 0u ||
        required > SIZE_MAX / element_size) return 0;
    if (*capacity >= required) return 1;
    next = *capacity ? *capacity : 8u;
    while (next < required) {
        if (next > maximum / 2u) {
            next = maximum;
            break;
        }
        next *= 2u;
    }
    if (next < required || next > SIZE_MAX / element_size) return 0;
    grown = realloc(*buffer, next * element_size);
    if (!grown) return 0;
    *buffer = grown;
    *capacity = next;
    return 1;
}

static OccModuleResolverNode* resolver_find_mutable(
    OccModuleResolver* resolver, uint32_t module_id
) {
    if (!resolver || module_id == 0u || module_id > resolver->module_count)
        return NULL;
    if (resolver->modules[module_id - 1u].module_id != module_id) return NULL;
    return &resolver->modules[module_id - 1u];
}

static const OccModuleResolverNode* resolver_find(
    const OccModuleResolver* resolver, uint32_t module_id
) {
    if (!resolver || module_id == 0u || module_id > resolver->module_count)
        return NULL;
    if (resolver->modules[module_id - 1u].module_id != module_id) return NULL;
    return &resolver->modules[module_id - 1u];
}

OccModuleResolverStatus occ_module_resolver_init(
    OccModuleResolver* resolver,
    const OccModuleResolverConfig* config
) {
    if (!resolver) return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    memset(resolver, 0, sizeof(*resolver));
    resolver->max_modules = config && config->max_modules
        ? config->max_modules : OCC_MODULE_RESOLVER_DEFAULT_MAX_MODULES;
    resolver->max_edges = config && config->max_edges
        ? config->max_edges : OCC_MODULE_RESOLVER_DEFAULT_MAX_EDGES;
    resolver->max_text_bytes = config && config->max_text_bytes
        ? config->max_text_bytes : OCC_MODULE_RESOLVER_DEFAULT_MAX_TEXT_BYTES;
    resolver->max_source_bytes = config && config->max_source_bytes
        ? config->max_source_bytes : OCC_MODULE_RESOLVER_DEFAULT_MAX_SOURCE_BYTES;
    if (resolver->max_modules > UINT32_MAX ||
        resolver->max_modules > SIZE_MAX / sizeof(*resolver->modules) ||
        resolver->max_edges > SIZE_MAX / sizeof(*resolver->edges)) {
        memset(resolver, 0, sizeof(*resolver));
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    }
    return OCC_MODULE_RESOLVER_OK;
}

void occ_module_resolver_destroy(OccModuleResolver* resolver) {
    size_t i;
    if (!resolver) return;
    for (i = 0u; i < resolver->module_count; ++i)
        free(resolver->modules[i].canonical_identity);
    for (i = 0u; i < resolver->edge_count; ++i)
        free(resolver->edges[i].requested_path);
    free(resolver->modules);
    free(resolver->edges);
    memset(resolver, 0, sizeof(*resolver));
}

uint64_t occ_module_resolver_fingerprint_source(
    const uint8_t* source_bytes,
    size_t source_length
) {
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;
    if (source_length != 0u && !source_bytes) return 0u;
    for (i = 0u; i < source_length; ++i) {
        hash ^= (uint64_t)source_bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static OccModuleResolverStatus resolver_map_provider_status(
    OccModuleSourceProviderStatus status
) {
    switch (status) {
        case OCC_MODULE_SOURCE_PROVIDER_OK:
            return OCC_MODULE_RESOLVER_OK;
        case OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND:
            return OCC_MODULE_RESOLVER_NOT_FOUND;
        case OCC_MODULE_SOURCE_PROVIDER_DENIED:
            return OCC_MODULE_RESOLVER_SOURCE_DENIED;
        case OCC_MODULE_SOURCE_PROVIDER_AMBIGUOUS:
            return OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS;
        case OCC_MODULE_SOURCE_PROVIDER_IO_ERROR:
            return OCC_MODULE_RESOLVER_SOURCE_IO_ERROR;
        case OCC_MODULE_SOURCE_PROVIDER_LIMIT_EXCEEDED:
            return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
        case OCC_MODULE_SOURCE_PROVIDER_ERROR:
        default:
            return OCC_MODULE_RESOLVER_PROVIDER_ERROR;
    }
}

void occ_module_resolver_release_source(
    const OccModuleSourceProvider* provider,
    OccModuleSourceView* source
) {
    if (!source) return;
    if (provider && provider->release &&
        (source->canonical_identity || source->source_bytes ||
         source->provider_cookie || source->source_length != 0u)) {
        provider->release(provider->context, source);
    }
    memset(source, 0, sizeof(*source));
}

OccModuleResolverStatus occ_module_resolver_resolve_source(
    const OccModuleResolver* resolver,
    const OccModuleSourceProvider* provider,
    const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
) {
    OccModuleSourceProviderStatus provider_status;
    OccModuleResolverStatus status;
    size_t path_length;
    size_t identity_length;
    if (out_source) memset(out_source, 0, sizeof(*out_source));
    if (!resolver || !provider || !provider->resolve || !provider->release ||
        !request || !request->requested_path || !request->requested_path[0] ||
        !request->use_span || !out_source) {
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    }
    if (request->use_span->has_location &&
        request->use_span->end_offset < request->use_span->start_offset) {
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    }
    path_length = strlen(request->requested_path);
    if (path_length > resolver->max_text_bytes ||
        (request->importer_identity &&
         strlen(request->importer_identity) > resolver->max_text_bytes)) {
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    }

    provider_status = provider->resolve(provider->context, request, out_source);
    status = resolver_map_provider_status(provider_status);
    if (status != OCC_MODULE_RESOLVER_OK) {
        /* Providers should normally return an empty view on failure. Release
           defensively if a callback left a partial allocation behind. */
        occ_module_resolver_release_source(provider, out_source);
        return status;
    }

    if (!out_source->canonical_identity ||
        !out_source->canonical_identity[0] ||
        (out_source->source_length != 0u && !out_source->source_bytes)) {
        occ_module_resolver_release_source(provider, out_source);
        return OCC_MODULE_RESOLVER_PROVIDER_ERROR;
    }
    identity_length = strlen(out_source->canonical_identity);
    if (identity_length > resolver->max_text_bytes ||
        out_source->source_length > resolver->max_source_bytes) {
        occ_module_resolver_release_source(provider, out_source);
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    }
    out_source->content_fingerprint = occ_module_resolver_fingerprint_source(
        out_source->source_bytes, out_source->source_length);
    return OCC_MODULE_RESOLVER_OK;
}

OccModuleResolverStatus occ_module_resolver_register(
    OccModuleResolver* resolver,
    const char* canonical_identity,
    uint64_t content_fingerprint,
    uint32_t* out_module_id
) {
    size_t i, length;
    char* copy;
    OccModuleResolverNode* node;
    if (out_module_id) *out_module_id = 0u;
    if (!resolver || !canonical_identity || !canonical_identity[0] ||
        !out_module_id) return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    length = strlen(canonical_identity);
    for (i = 0u; i < resolver->module_count; ++i) {
        node = &resolver->modules[i];
        if (strcmp(node->canonical_identity, canonical_identity) == 0) {
            if (node->content_fingerprint != content_fingerprint)
                return OCC_MODULE_RESOLVER_CONTENT_MISMATCH;
            *out_module_id = node->module_id;
            return OCC_MODULE_RESOLVER_OK;
        }
    }
    if (length > resolver->max_text_bytes ||
        resolver->text_bytes > resolver->max_text_bytes - length)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    if (resolver->module_count >= resolver->max_modules ||
        resolver->module_count >= UINT32_MAX)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    copy = resolver_duplicate(canonical_identity);
    if (!copy) return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
    if (!resolver_grow((void**)&resolver->modules, &resolver->module_capacity,
        resolver->module_count + 1u, sizeof(*resolver->modules),
        resolver->max_modules)) {
        free(copy);
        return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
    }
    node = &resolver->modules[resolver->module_count];
    memset(node, 0, sizeof(*node));
    node->module_id = (uint32_t)(resolver->module_count + 1u);
    node->canonical_identity = copy;
    node->content_fingerprint = content_fingerprint;
    node->state = OCC_MODULE_STATE_DISCOVERED;
    ++resolver->module_count;
    resolver->text_bytes += length;
    *out_module_id = node->module_id;
    return OCC_MODULE_RESOLVER_OK;
}

static OccModuleResolverStatus resolver_transition(
    OccModuleResolver* resolver, uint32_t module_id,
    OccModuleResolverState next
) {
    OccModuleResolverNode* node = resolver_find_mutable(resolver, module_id);
    if (!node) return OCC_MODULE_RESOLVER_NOT_FOUND;
    if (node->state != OCC_MODULE_STATE_RESOLVING &&
        !(node->state == OCC_MODULE_STATE_DISCOVERED &&
          (next == OCC_MODULE_STATE_RESOLVING ||
           next == OCC_MODULE_STATE_FAILED)))
        return OCC_MODULE_RESOLVER_INVALID_STATE;
    if (node->state == OCC_MODULE_STATE_RESOLVING &&
        next != OCC_MODULE_STATE_RESOLVED &&
        next != OCC_MODULE_STATE_FAILED)
        return OCC_MODULE_RESOLVER_INVALID_STATE;
    node->state = next;
    return OCC_MODULE_RESOLVER_OK;
}

OccModuleResolverStatus occ_module_resolver_begin(
    OccModuleResolver* resolver, uint32_t module_id
) {
    return resolver_transition(resolver, module_id, OCC_MODULE_STATE_RESOLVING);
}

OccModuleResolverStatus occ_module_resolver_complete(
    OccModuleResolver* resolver, uint32_t module_id
) {
    return resolver_transition(resolver, module_id, OCC_MODULE_STATE_RESOLVED);
}

OccModuleResolverStatus occ_module_resolver_fail(
    OccModuleResolver* resolver, uint32_t module_id
) {
    return resolver_transition(resolver, module_id, OCC_MODULE_STATE_FAILED);
}

static OccModuleResolverStatus resolver_add_dependency(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t dependency_module_id,
    const char* requested_path,
    uint32_t ast_use_node_id,
    const OccSourceSpan* use_span,
    OccModuleResolverEdgeKind kind
) {
    OccModuleResolverNode* importer;
    const OccModuleResolverNode* dependency;
    size_t length, i;
    char* path_copy;
    OccModuleResolverStatus result = OCC_MODULE_RESOLVER_OK;
    OccModuleResolverEdge* edge;
    if (!resolver || !requested_path || !requested_path[0] || !use_span ||
        (kind == OCC_MODULE_EDGE_EXPLICIT_USE && ast_use_node_id == 0u) ||
        (kind == OCC_MODULE_EDGE_IMPLICIT_OSMODULE && ast_use_node_id != 0u) ||
        (kind != OCC_MODULE_EDGE_EXPLICIT_USE &&
         kind != OCC_MODULE_EDGE_IMPLICIT_OSMODULE))
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    importer = resolver_find_mutable(resolver, importer_module_id);
    dependency = resolver_find(resolver, dependency_module_id);
    if (!importer || !dependency) return OCC_MODULE_RESOLVER_NOT_FOUND;
    if (importer->state != OCC_MODULE_STATE_RESOLVING)
        return OCC_MODULE_RESOLVER_INVALID_STATE;
    if (dependency->state == OCC_MODULE_STATE_FAILED)
        return OCC_MODULE_RESOLVER_DEPENDENCY_FAILED;
    if (use_span->has_location &&
        use_span->end_offset < use_span->start_offset)
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    for (i = 0u; i < resolver->edge_count; ++i) {
        const OccModuleResolverEdge* existing = &resolver->edges[i];
        if (existing->importer_module_id == importer_module_id &&
            ((kind == OCC_MODULE_EDGE_EXPLICIT_USE &&
              existing->kind == OCC_MODULE_EDGE_EXPLICIT_USE &&
              existing->ast_use_node_id == ast_use_node_id) ||
             (kind == OCC_MODULE_EDGE_IMPLICIT_OSMODULE &&
              existing->kind == OCC_MODULE_EDGE_IMPLICIT_OSMODULE &&
              strcmp(existing->requested_path, requested_path) == 0)))
            return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    }
    length = strlen(requested_path);
    if (length > resolver->max_text_bytes ||
        resolver->text_bytes > resolver->max_text_bytes - length)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    if (resolver->edge_count >= resolver->max_edges)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    path_copy = resolver_duplicate(requested_path);
    if (!path_copy) return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
    if (!resolver_grow((void**)&resolver->edges, &resolver->edge_capacity,
        resolver->edge_count + 1u, sizeof(*resolver->edges),
        resolver->max_edges)) {
        free(path_copy);
        return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
    }
    edge = &resolver->edges[resolver->edge_count++];
    memset(edge, 0, sizeof(*edge));
    edge->importer_module_id = importer_module_id;
    edge->dependency_module_id = dependency_module_id;
    edge->ast_use_node_id = ast_use_node_id;
    edge->use_span = *use_span;
    edge->requested_path = path_copy;
    edge->kind = kind;
    resolver->text_bytes += length;
    if (dependency_module_id == importer_module_id ||
        dependency->state == OCC_MODULE_STATE_RESOLVING)
        result = OCC_MODULE_RESOLVER_CYCLE;
    return result;
}

OccModuleResolverStatus occ_module_resolver_add_dependency(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t dependency_module_id,
    const char* requested_path,
    uint32_t ast_use_node_id,
    const OccSourceSpan* use_span
) {
    return resolver_add_dependency(resolver, importer_module_id,
        dependency_module_id, requested_path, ast_use_node_id, use_span,
        OCC_MODULE_EDGE_EXPLICIT_USE);
}

OccModuleResolverStatus occ_module_resolver_add_implicit_osmodule(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t osmodule_module_id,
    const OccSourceSpan* program_span
) {
    return resolver_add_dependency(resolver, importer_module_id,
        osmodule_module_id, "osmodule", 0u, program_span,
        OCC_MODULE_EDGE_IMPLICIT_OSMODULE);
}

const OccModuleResolverNode* occ_module_resolver_get_module(
    const OccModuleResolver* resolver, uint32_t module_id
) {
    return resolver_find(resolver, module_id);
}

const OccModuleResolverEdge* occ_module_resolver_get_edge(
    const OccModuleResolver* resolver, size_t edge_index
) {
    if (!resolver || edge_index >= resolver->edge_count) return NULL;
    return &resolver->edges[edge_index];
}

const char* occ_module_resolver_status_string(OccModuleResolverStatus status) {
    switch (status) {
        case OCC_MODULE_RESOLVER_OK: return "ok";
        case OCC_MODULE_RESOLVER_INVALID_ARGUMENT: return "invalid argument";
        case OCC_MODULE_RESOLVER_OUT_OF_MEMORY: return "out of memory";
        case OCC_MODULE_RESOLVER_LIMIT_EXCEEDED: return "limit exceeded";
        case OCC_MODULE_RESOLVER_NOT_FOUND: return "module not found";
        case OCC_MODULE_RESOLVER_CONTENT_MISMATCH: return "canonical identity content mismatch";
        case OCC_MODULE_RESOLVER_INVALID_STATE: return "invalid module state transition";
        case OCC_MODULE_RESOLVER_CYCLE: return "dependency cycle";
        case OCC_MODULE_RESOLVER_DEPENDENCY_FAILED: return "dependency already failed";
        case OCC_MODULE_RESOLVER_SOURCE_DENIED: return "source access denied by provider policy";
        case OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS: return "source identity is ambiguous";
        case OCC_MODULE_RESOLVER_SOURCE_IO_ERROR: return "source provider I/O error";
        case OCC_MODULE_RESOLVER_PROVIDER_ERROR: return "invalid source provider response";
        default: return "unknown module resolver status";
    }
}

static int resolver_order_visit(
    const OccModuleResolver* resolver, uint32_t module_id,
    uint8_t* marks, uint32_t* output, size_t* count
) {
    size_t i;
    uint8_t* mark;
    if (!resolver || module_id == 0u || module_id > resolver->module_count)
        return 0;
    mark = &marks[module_id - 1u];
    if (*mark == 2u) return 1;
    if (*mark == 1u) return 0;
    *mark = 1u;
    for (i = 0u; i < resolver->edge_count; ++i) {
        const OccModuleResolverEdge* edge = &resolver->edges[i];
        if (edge->importer_module_id == module_id &&
            !resolver_order_visit(resolver, edge->dependency_module_id,
                marks, output, count)) return 0;
    }
    *mark = 2u;
    output[(*count)++] = module_id;
    return 1;
}

OccModuleResolverStatus occ_module_resolver_dependency_order(
    const OccModuleResolver* resolver,
    uint32_t* out_module_ids,
    size_t out_capacity,
    size_t* out_count
) {
    uint8_t* marks;
    size_t i, count = 0u;
    if (out_count) *out_count = 0u;
    if (!resolver || !out_count ||
        (resolver->module_count && (!out_module_ids ||
         out_capacity < resolver->module_count)))
        return OCC_MODULE_RESOLVER_INVALID_ARGUMENT;
    if (!resolver->module_count) return OCC_MODULE_RESOLVER_OK;
    marks = (uint8_t*)calloc(resolver->module_count, sizeof(*marks));
    if (!marks) return OCC_MODULE_RESOLVER_OUT_OF_MEMORY;
    for (i = 0u; i < resolver->module_count; ++i) {
        if (!resolver_order_visit(resolver, (uint32_t)(i + 1u),
            marks, out_module_ids, &count)) {
            free(marks);
            return OCC_MODULE_RESOLVER_CYCLE;
        }
    }
    free(marks);
    *out_count = count;
    return OCC_MODULE_RESOLVER_OK;
}

