# Module Resolver Architecture (Draft)

**Status:** Architecture contract draft. A portable C dependency-graph core is now implemented; host source lookup, recursive dependency scheduling, OIR dependency interfaces, and imported-symbol Semantic validation are not yet implemented.

## 1. Responsibility boundary

The Module Resolver is a distinct C component between Parser/AST production and Semantic analysis. It owns module identity, dependency discovery, source-provider coordination, canonical module lookup, dependency graph construction, cache identity, and cycle diagnostics.

It does not parse Nexus Flow source, duplicate Parser grammar, decide type correctness, bypass Group ownership/visibility, lower AST, or perform unrestricted filesystem access based on source-controlled strings.

Parser emits AST_USE nodes with the dotted path, optional `as` alias, and optional `ns` selector. Semantic validates imported names, Group membership, visibility, local collisions, and use-site type rules.

## 2. Pipeline placement

```text
SOURCE_UNIT -> Lexer DLL -> TOKEN_STREAM -> Parser DLL -> AST
  -> Module Resolver + host dependency scheduler
       -> recursively discover AST_USE dependencies
       -> resolve canonical module identities and build dependency graph
       -> reject unresolved paths and dependency cycles
       -> schedule dependency modules before their importers
  -> Semantic DLL
       -> consume root AST + validated imported-module interface data
       -> validate alias/namespace bindings, exports, visibility and symbol use
  -> SEMANTIC_AST -> OIR lowering
```

The current Semantic DLL accepts one AST artifact and has no project/module context. Imported-symbol validation must not be faked by silently treating imports as local declarations. Its invocation contract will be extended only after the dependency/interface artifact is specified.

## 3. Host source-provider boundary

The resolver must not concatenate a dotted module path into an arbitrary filesystem path and open it directly. A host-owned source provider supplies module content under the active project policy.

The C provider boundary is now declared in `module_resolver.h`:
- the request carries the original dotted USE spelling, optional canonical importer identity, and exact AST_USE source span;
- the provider returns a canonical module identity and a borrowed source-byte view, or a structured not-found/denied/ambiguous/I/O result;
- the provider owns source lookup, project-root/search-path policy, extension selection and platform file I/O;
- the returned view remains valid until the host calls the matching provider release callback; the resolver copies no source bytes and does not retain borrowed pointers;
- the resolver computes a stable 64-bit FNV-1a content fingerprint for per-job cache consistency. This is not a security hash and must not be used for trust/authenticity decisions.

The resolver core validates provider responses and configured source/text budgets, maps provider errors to stable resolver statuses, and guarantees that malformed successful responses are released before returning. The host must register the canonical identity + fingerprint in the graph while the view is alive, pass the source bytes through Lexer/Parser, then release the view.

Repository configuration review found `omnicore-sdk/occ.ocf` and `omnicore-sdk/src/c/occ.ocf` declare `[include]` (currently `./lib`) and `[dependencies]` sections (currently empty). No C OCF project-config loader was found in the active source tree. Therefore the project-config/host layer must parse these settings and construct the provider's search policy; the portable resolver must not parse OCF or interpret `./lib` itself. Exact dotted-path-to-source mapping, extension policy, dependency package installation/cache behavior and precedence between importer-relative and `[include]` roots remain open until the project configuration contract is specified.

This keeps Windows/Linux/macOS path behavior outside the portable resolver core and prevents source text from granting arbitrary path access. `m:` is unrelated to this mechanism: it declares an immutable value initialized by an expression, while `use` is the only source/module import mechanism described here.

## 4. Module identity and graph

A resolved module is identified by its provider-issued canonical identity, not by an import alias or raw path spelling. The graph stores:
- canonical module identity and source identity/content fingerprint;
- importer-to-dependency edges;
- originating AST_USE node identity and exact source span for each edge;
- resolution state (unseen, resolving, resolved, failed);
- selected Group path, when the import selects a Group within a module.

Repeated requests for the same canonical identity reuse one graph node within a compilation job. Aliases and namespace selectors are edge-local bindings and never alter module cache identity.

A deterministic depth-first traversal may use `UNSEEN -> RESOLVING -> RESOLVED`. Encountering a `RESOLVING` node through a new dependency edge is a cycle and produces a diagnostic at the importing AST_USE span. Failed dependencies are never marked resolved or published as valid interfaces.

The initial implementation rejects dependency cycles deterministically. Strongly connected module initialization or mutually recursive module interfaces are not implied and require a separate language-level design.

## 5. USE binding semantics

Each AST_USE item preserves three independent values:
- `module_name`: dotted module/selection path written by the user;
- `alias`: optional local alias introduced by `as`;
- `namespace_name`: optional namespace root introduced by `ns`.

Rules:
- `as name` names the imported item locally; it does not rewrite the namespace root.
- `ns name` selects the namespace root used to expose members of the selected Group.
- `ns _` requests unqualified exposure in the importing scope.
- `as _` remains an ordinary alias spelling.
- Comma-separated imports are independent bindings; each retains its own source span and options.
- Alias/namespace collisions and duplicate imported symbols are checked against the actual importing scope, not by Parser.
- A selected Group must exist in the resolved module and be eligible for import.
- Only exported/public members allowed by the Group specification enter the imported interface. Group-owned private nominal types remain private even when a namespace prefix is removed.

The resolver establishes which module and Group were selected. Semantic validates the resulting symbol interface and all use-site references. Namespace flattening must never weaken visibility or Group-owned type isolation.

## 6. Imported interface and Semantic handoff

The dependency graph and imported symbol interface are separate concepts. The graph answers which source module depends on which other module. The Semantic interface answers which declarations are importable and how each import binds them.

Before Semantic import support is implemented, a versioned OIR representation must define:
- stable module and declaration identities;
- exported declaration kind, name, signature/type reference, visibility and owning Group identity;
- selected Group identity and eligible exported members;
- import-edge identity and the AST_USE node/source span that introduced it;
- alias and namespace binding values;
- references that prevent downstream name-only re-resolution.

The interface is independently validated before Semantic consumes it. SemanticResult/OIR annotations retain resolved symbol identities; downstream OIR never reopens source paths or repeats name lookup.

Dependency modules are parsed and semantically checked under their own module scopes before validated export interfaces are consumed by importers. The host scheduler may parallelize independent graph branches later, but deterministic diagnostics and collector synchronization remain required.

## 7. Diagnostics and failure behavior

Diagnostics identify the importing AST_USE node and preserve its exact source span. Resolution distinguishes at least:
- module not found;
- module denied by project/source-provider policy;
- ambiguous module identity;
- selected Group not found;
- dependency cycle;
- duplicate/colliding alias or namespace binding;
- invalid or non-exported imported member;
- dependency source read/parse failure.

A failed dependency prevents the importing module from receiving a valid Semantic interface. Diagnostics accumulate for the compilation job, but no valid SEMANTIC_AST or downstream OIR_MODULE is published while blocking errors remain.

## 8. Implementation gates

1. Define canonical module identity and source-provider policy for `use`; keep immutable `m:` value semantics in the Semantic declaration path, outside Module Resolver.
2. [x] Add the portable C graph API and host source-provider callback boundary: canonical-identity registration/cache reuse, computed content fingerprint, provider status mapping, borrowed source-view ownership/release, exact USE spans, source/text budgets, lifecycle transitions, and DFS back-edge cycle reporting. Actual host provider implementation and deterministic allocation-failure injection remain open.
3. Test cache reuse, aliases not affecting cache keys, missing/denied/ambiguous sources, nested dependencies, cycles, exact USE spans, and OOM cleanup.
4. Define a versioned OIR dependency/interface artifact and independent validator.
5. Integrate the host dependency scheduler with existing lexer/parser DLLs; do not add file I/O to Parser or Semantic.
6. Extend Semantic invocation to consume validated imported interfaces; test `as`, `ns`, `ns _`, nested Groups, visibility, collisions, and private-type isolation.
7. Verify the graph core on Windows, macOS, Ubuntu, ASan/UBSan and Valgrind; then continue with provider integration and Semantic handoff. The graph core is not the complete Module Resolver stage.

No Codegen work is authorized by this plan. AST/Parser gates and Semantic import validation remain prerequisites.


## 9. C graph-core implementation status

The first C implementation is in `omnicore-sdk/src/c/module/module_resolver.h/.c` and is built as the independent static target `occ_module_resolver`. It intentionally has no filesystem dependency and does not depend on Parser internals. This is the portable graph/provider-contract core, not yet a pipeline DLL: a separate resolver-stage DLL wrapper and its input/output artifact ABI must wait for the versioned dependency/interface artifact and host scheduler contract.

- The provider supplies a canonical identity and borrowed source bytes; the resolver computes a stable 64-bit FNV-1a content fingerprint. Identical identity+fingerprint reuses the stable 1-based module ID, while identity with different content is rejected. The fingerprint is not cryptographic.
- The graph owns canonical identity and original dotted import-path strings. Each dependency edge retains importer/dependency IDs, AST_USE node ID, and the complete `OccSourceSpan`.
- Module state transitions are constrained to DISCOVERED → RESOLVING → RESOLVED/FAILED. The scheduler must mark a module RESOLVING before walking its imports; edges to a RESOLVING node are retained and return CYCLE so the caller can report the exact USE location.
- Module, edge and aggregate text limits are configurable. Failed dependencies are not added as successful edges. Returned node/edge views are borrowed and invalidated by graph mutation/destruction.
- `test_module_resolver.c` covers identity cache reuse, inconsistent fingerprints, edge payload/span preservation, duplicate AST_USE edge rejection, cycle reporting, state transitions, failed dependency rejection, and configured limits.

This graph core does not yet call a source provider, parse dependency modules, compute fingerprints, schedule recursive pipeline invocations, serialize dependency interfaces, or populate Semantic imported symbols. Those are explicit next gates.


## 10. Explicit separation from `m:`

Nexus Flow `m:name = expression;` declares an immutable value. Its initializer is an expression, its type is inferred by Semantic, and the resulting symbol is constant for assignment/update checks. It does not inspect file existence, load source, or create a dependency edge. Source/module loading is performed by `use` and the host-owned source provider described in this document. The AST enum/payload name `AST_MUST_DECL` is retained for append-only compatibility; it is not a module/dependency declaration.


## 11. Host source-provider C contract status

`OccModuleSourceProvider` now exposes `resolve(context, request, out_view)` and `release(context, view)`. A request carries the dotted import spelling, optional importer identity and mandatory source span. A successful view carries canonical identity, source bytes/length and an opaque provider cookie; the resolver fills its content fingerprint. All view pointers are provider-owned and borrowed until the matching release call. The host must copy/register the identity and consume the bytes before releasing the view. A failed provider callback must clean up its own partial state and leave the view zeroed.

Stable resolver statuses distinguish not-found, denied, ambiguous, provider I/O and malformed-provider results. Configurable source byte limits default to 64 MiB; aggregate identity/path text remains separately bounded. The portable resolver never opens a file or parses OCF. The project-config host layer is responsible for turning OCF `[include]` and `[dependencies]` entries into the policy used by the provider.

`test_module_resolver.c` now covers provider success + content fingerprint, graph registration from the returned identity, exactly-once release, not-found/denied/ambiguous/I/O status mapping, malformed successful response cleanup, and source-size limit cleanup. This remains a callback contract test using a fake provider; no real filesystem/package provider or recursive Lexer/Parser scheduler is implemented.
