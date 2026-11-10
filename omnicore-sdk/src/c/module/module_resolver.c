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
    if (length > resolver->max_text_bytes ||
        resolver->text_bytes > resolver->max_text_bytes - length)
        return OCC_MODULE_RESOLVER_LIMIT_EXCEEDED;
    for (i = 0u; i < resolver->module_count; ++i) {
        node = &resolver->modules[i];
        if (strcmp(node->canonical_identity, canonical_identity) == 0) {
            if (node->content_fingerprint != content_fingerprint)
                return OCC_MODULE_RESOLVER_CONTENT_MISMATCH;
            *out_module_id = node->module_id;
            return OCC_MODULE_RESOLVER_OK;
        }
    }
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

OccModuleResolverStatus occ_module_resolver_add_dependency(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t dependency_module_id,
    const char* requested_path,
    uint32_t ast_use_node_id,
    const OccSourceSpan* use_span
) {
    OccModuleResolverNode* importer;
    const OccModuleResolverNode* dependency;
    size_t length, i;
    char* path_copy;
    OccModuleResolverStatus result = OCC_MODULE_RESOLVER_OK;
    OccModuleResolverEdge* edge;
    if (!resolver || !requested_path || !requested_path[0] ||
        ast_use_node_id == 0u || !use_span)
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
            existing->ast_use_node_id == ast_use_node_id)
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
    resolver->text_bytes += length;
    if (dependency_module_id == importer_module_id ||
        dependency->state == OCC_MODULE_STATE_RESOLVING)
        result = OCC_MODULE_RESOLVER_CYCLE;
    return result;
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
        default: return "unknown module resolver status";
    }
}
