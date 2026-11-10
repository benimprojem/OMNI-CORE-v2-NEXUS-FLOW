#ifndef OCC_MODULE_RESOLVER_H
#define OCC_MODULE_RESOLVER_H

#include <stddef.h>
#include <stdint.h>
#include "../core/diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_MODULE_RESOLVER_DEFAULT_MAX_MODULES 65536u
#define OCC_MODULE_RESOLVER_DEFAULT_MAX_EDGES 262144u
#define OCC_MODULE_RESOLVER_DEFAULT_MAX_TEXT_BYTES (16u * 1024u * 1024u)
#define OCC_MODULE_RESOLVER_DEFAULT_MAX_SOURCE_BYTES (64u * 1024u * 1024u)

typedef enum OccModuleResolverStatus {
    OCC_MODULE_RESOLVER_OK = 0,
    OCC_MODULE_RESOLVER_INVALID_ARGUMENT = 1,
    OCC_MODULE_RESOLVER_OUT_OF_MEMORY = 2,
    OCC_MODULE_RESOLVER_LIMIT_EXCEEDED = 3,
    OCC_MODULE_RESOLVER_NOT_FOUND = 4,
    OCC_MODULE_RESOLVER_CONTENT_MISMATCH = 5,
    OCC_MODULE_RESOLVER_INVALID_STATE = 6,
    OCC_MODULE_RESOLVER_CYCLE = 7,
    OCC_MODULE_RESOLVER_DEPENDENCY_FAILED = 8,
    OCC_MODULE_RESOLVER_SOURCE_DENIED = 9,
    OCC_MODULE_RESOLVER_SOURCE_AMBIGUOUS = 10,
    OCC_MODULE_RESOLVER_SOURCE_IO_ERROR = 11,
    OCC_MODULE_RESOLVER_PROVIDER_ERROR = 12
} OccModuleResolverStatus;

typedef enum OccModuleResolverState {
    OCC_MODULE_STATE_DISCOVERED = 1,
    OCC_MODULE_STATE_RESOLVING = 2,
    OCC_MODULE_STATE_RESOLVED = 3,
    OCC_MODULE_STATE_FAILED = 4
} OccModuleResolverState;

typedef struct OccModuleResolverConfig {
    size_t max_modules;
    size_t max_edges;
    size_t max_text_bytes;
    size_t max_source_bytes;
} OccModuleResolverConfig;

typedef enum OccModuleSourceProviderStatus {
    OCC_MODULE_SOURCE_PROVIDER_OK = 0,
    OCC_MODULE_SOURCE_PROVIDER_NOT_FOUND = 1,
    OCC_MODULE_SOURCE_PROVIDER_DENIED = 2,
    OCC_MODULE_SOURCE_PROVIDER_AMBIGUOUS = 3,
    OCC_MODULE_SOURCE_PROVIDER_IO_ERROR = 4,
    OCC_MODULE_SOURCE_PROVIDER_ERROR = 5
} OccModuleSourceProviderStatus;

/* A provider request is one AST_USE item, not a filesystem path. */
typedef struct OccModuleSourceRequest {
    const char* requested_path;       /* Dotted USE spelling or implicit standard module name. */
    const char* importer_identity;    /* Canonical identity; optional for root imports. */
    const OccSourceSpan* use_span;    /* Origin span: AST_USE or program root; required. */
} OccModuleSourceRequest;

/*
 * Provider-owned view. All pointers remain valid until release() is called.
 * content_fingerprint is computed by the resolver and must be treated as output.
 */
typedef struct OccModuleSourceView {
    const char* canonical_identity;
    const uint8_t* source_bytes;
    size_t source_length;
    uint64_t content_fingerprint;
    void* provider_cookie;
} OccModuleSourceView;

/* On failure, resolve must clean up and zero out_source. On success, all
   returned views are owned by the provider until the matching release call. */
typedef OccModuleSourceProviderStatus (*OccModuleSourceResolveFn)(
    void* provider_context,
    const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
);
typedef void (*OccModuleSourceReleaseFn)(
    void* provider_context,
    OccModuleSourceView* source
);

typedef struct OccModuleSourceProvider {
    void* context;
    OccModuleSourceResolveFn resolve;
    OccModuleSourceReleaseFn release;
} OccModuleSourceProvider;

typedef struct OccModuleResolverNode {
    uint32_t module_id;             /* Stable, 1-based within this graph. */
    char* canonical_identity;       /* Owned by the graph. */
    uint64_t content_fingerprint;   /* Host/provider supplied for this job. */
    OccModuleResolverState state;
} OccModuleResolverNode;

typedef enum OccModuleResolverEdgeKind {
    OCC_MODULE_EDGE_EXPLICIT_USE = 1,
    OCC_MODULE_EDGE_IMPLICIT_OSMODULE = 2
} OccModuleResolverEdgeKind;

typedef struct OccModuleResolverEdge {
    uint32_t importer_module_id;
    uint32_t dependency_module_id;
    uint32_t ast_use_node_id;       /* 0 only for implicit standard-module edges. */
    OccSourceSpan use_span;         /* AST_USE span or root-program span. */
    char* requested_path;           /* Dotted spelling / standard module name; graph-owned. */
    OccModuleResolverEdgeKind kind; /* Appended to preserve the existing field prefix. */
} OccModuleResolverEdge;

typedef struct OccModuleResolver {
    OccModuleResolverNode* modules;
    size_t module_count;
    size_t module_capacity;
    OccModuleResolverEdge* edges;
    size_t edge_count;
    size_t edge_capacity;
    size_t max_modules;
    size_t max_edges;
    size_t max_text_bytes;
    size_t max_source_bytes;
    size_t text_bytes;
} OccModuleResolver;

/* Zero limits select documented defaults. No filesystem access is performed. */
OccModuleResolverStatus occ_module_resolver_init(
    OccModuleResolver* resolver,
    const OccModuleResolverConfig* config
);
void occ_module_resolver_destroy(OccModuleResolver* resolver);

/*
 * Resolve one dotted USE request through a host-owned provider. This core
 * performs no filesystem access and does not interpret OCF paths. The provider
 * enforces project/include/dependency policy and returns a canonical identity
 * plus source bytes. The returned view must be released exactly once.
 */
OccModuleResolverStatus occ_module_resolver_resolve_source(
    const OccModuleResolver* resolver,
    const OccModuleSourceProvider* provider,
    const OccModuleSourceRequest* request,
    OccModuleSourceView* out_source
);
void occ_module_resolver_release_source(
    const OccModuleSourceProvider* provider,
    OccModuleSourceView* source
);

/* Stable non-cryptographic content fingerprint used only for cache consistency. */
uint64_t occ_module_resolver_fingerprint_source(
    const uint8_t* source_bytes,
    size_t source_length
);

/*
 * Register a provider-resolved identity. Re-registering the same canonical
 * identity with the same fingerprint reuses its ID. A different fingerprint
 * for one identity in a single job is rejected as inconsistent provider data.
 */
OccModuleResolverStatus occ_module_resolver_register(
    OccModuleResolver* resolver,
    const char* canonical_identity,
    uint64_t content_fingerprint,
    uint32_t* out_module_id
);

/* Legal transitions: DISCOVERED -> RESOLVING -> RESOLVED or FAILED. */
OccModuleResolverStatus occ_module_resolver_begin(
    OccModuleResolver* resolver,
    uint32_t module_id
);
OccModuleResolverStatus occ_module_resolver_complete(
    OccModuleResolver* resolver,
    uint32_t module_id
);
OccModuleResolverStatus occ_module_resolver_fail(
    OccModuleResolver* resolver,
    uint32_t module_id
);

/*
 * Record one AST_USE dependency edge. The importer must be RESOLVING.
 * The edge is retained even when CYCLE is returned, so the caller can emit
 * the diagnostic at the exact AST_USE span. The scheduler marks a module
 * RESOLVING before walking its dependencies; an edge to another RESOLVING
 * module is therefore a deterministic DFS back-edge/cycle.
 */
OccModuleResolverStatus occ_module_resolver_add_dependency(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t dependency_module_id,
    const char* requested_path,
    uint32_t ast_use_node_id,
    const OccSourceSpan* use_span
);

/*
 * Record the implicit default osmodule dependency. The host calls this only
 * when ASTProgram.disable_osmodule is false, using the root program span.
 * No AST_USE node exists for this edge, so ast_use_node_id is stored as zero.
 */
OccModuleResolverStatus occ_module_resolver_add_implicit_osmodule(
    OccModuleResolver* resolver,
    uint32_t importer_module_id,
    uint32_t osmodule_module_id,
    const OccSourceSpan* program_span
);

/* Returned records are borrowed and valid until graph mutation/destruction. */
const OccModuleResolverNode* occ_module_resolver_get_module(
    const OccModuleResolver* resolver,
    uint32_t module_id
);
const OccModuleResolverEdge* occ_module_resolver_get_edge(
    const OccModuleResolver* resolver,
    size_t edge_index
);
const char* occ_module_resolver_status_string(OccModuleResolverStatus status);

#ifdef __cplusplus
}
#endif

#endif
