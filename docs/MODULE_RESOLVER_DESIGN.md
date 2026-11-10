# Module Resolver Architecture (Draft)

**Status:** Architecture contract draft. The portable C dependency-graph core and callback-driven discovery/Semantic scheduler are implemented and tested with fake providers. The concrete filesystem provider, main-pipeline DLL wiring, OIR dependency interfaces, and imported-symbol Semantic validation remain pending.

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

Repository configuration review found `omnicore-sdk/occ.ocf`, `omnicore-sdk/src/c/occ.ocf`, `omnicore-sdk/myprojem/occ.ocf` and `omnicore-sdk/src/oce/occ.ocf`. They declare `[include]` and `[dependencies]`; the root config also contains package metadata, flags, default macros and default libraries. A new bounded C reader (`occ_project_config.h/.c`) now parses the `[include]` path list relative to the OCF file's project root, enforces a 1 MiB file budget, 4 KiB line/path budget and 64-entry include limit, and ignores unrelated sections. The `[dependencies]` section is deliberately left to OCM; the reader does not invent or reinterpret dependency declaration syntax. The parser returns the project root and raw include entries but performs no filesystem lookup, path canonicalization or module resolution. The portable Module Resolver still does not parse OCF or open paths.

The language contract now distinguishes two standard modules: `coremodule` contains OS-independent general-purpose functions; `osmodule` contains OS-dependent services such as file, print, echo and input. `osmodule` is automatically loaded by default. The program-level `no: osmodule;` directive suppresses only that implicit load; it is not a generic module-disable syntax, and `no: coremodule;` is invalid. The `coremodule` default-loading policy remains undecided. The Parser now stores this policy on `ASTProgram.disable_osmodule` with the directive's exact source span; AST stream v6 carries the field and continues reading v1-v5. The callback-driven host scheduler consumes this program policy before adding the implicit `osmodule` dependency. The concrete provider must still resolve the standard module under the active OCF/include policy, and `main.c` is not yet wired to invoke the scheduler. The directive's meaning is specifically to suppress automatic loading; any interaction with an explicit `use osmodule` must be resolved in the host/import policy rather than silently assumed by Parser. The current Parser's `use` grammar accepts identifier-based dotted paths (for example `use platform.io as io;`), not quoted filename strings. Therefore the provider resolves a dotted logical module request under host policy, not an arbitrary user-supplied path. Exact dotted-path-to-source mapping, extension policy, dependency package installation/cache behavior and precedence between importer-relative and `[include]` roots remain open until the project configuration contract is specified.

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

1. Define canonical module identity and source-provider policy for `use`; keep immutable `m:` value semantics in the Semantic declaration path, outside Module Resolver. [in progress: bounded OCF `[include]` reader added; provider policy remains open].
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

Nexus Flow `m:name = expression;` declares a normal value with an immutable binding. Its initializer is an expression, its type is inferred by Semantic, and the resulting symbol carries the distinct `IMMUTABLE` flag for assignment/update checks. It is not a `c:` compile-time constant and must not be encoded with the `CONST` flag. It does not inspect file existence, load source, or create a dependency edge. Source/module loading is performed by `use` and the host-owned source provider described in this document. The AST enum/payload name `AST_MUST_DECL` is retained for append-only compatibility; it is not a module/dependency declaration.


## 11. Host source-provider C contract status

`OccModuleSourceProvider` now exposes `resolve(context, request, out_view)` and `release(context, view)`. A request carries the dotted import spelling, optional importer identity and mandatory source span. A successful view carries canonical identity, source bytes/length and an opaque provider cookie; the resolver fills its content fingerprint. All view pointers are provider-owned and borrowed until the matching release call. The host must copy/register the identity and consume the bytes before releasing the view. A failed provider callback must clean up its own partial state and leave the view zeroed.

Stable resolver statuses distinguish not-found, denied, ambiguous, provider I/O and malformed-provider results. Configurable source byte limits default to 64 MiB; aggregate identity/path text remains separately bounded. The portable resolver never opens a file or parses OCF. The project-config host layer is responsible for turning OCF `[include]` and `[dependencies]` entries into the policy used by the provider.

`test_module_resolver.c` now covers provider success + content fingerprint, graph registration from the returned identity, exactly-once release, not-found/denied/ambiguous/I/O status mapping, malformed successful response cleanup, and source-size limit cleanup. This remains a callback contract test using a fake provider; no real filesystem/package provider or recursive Lexer/Parser scheduler is implemented.


## 12. Default module policy: `coremodule` / `osmodule`

The default-module policy is a program-level decision, not a Parser filesystem operation:

- The host pipeline normally schedules `osmodule` as an implicit dependency.
- If the root `ASTProgram.disable_osmodule` flag is set by `no: osmodule;`, the host omits that implicit dependency.
- The AST field is accompanied by `disable_osmodule_span` so host diagnostics can point to the exact directive.
- This directive does not create an `AST_USE` node and does not represent a user-written dependency edge.
- The graph API now distinguishes `OCC_MODULE_EDGE_EXPLICIT_USE` from `OCC_MODULE_EDGE_IMPLICIT_OSMODULE`. `occ_module_resolver_add_implicit_osmodule()` records the standard dependency with `ast_use_node_id == 0` and the root program span; it shares the same graph ownership, limits, state checks and cycle detection as explicit edges. The host must call it only when `ASTProgram.disable_osmodule == 0`. Host scheduling, provider resolution of the standard module, and the actual ASTProgram-to-resolver pipeline connection are still required before automatic loading is operational.
- `coremodule` is OS-independent; its automatic/manual loading policy remains an open language decision.

Do not rename the compiler's internal `src/c/core` subsystem: these names refer to Nexus Flow standard-library modules, not the OCC compiler's own core implementation directory.


## 13. Predefined library source layout

Predefined Nexus Flow standard modules live under `omnicore-sdk/occ/libs/`:

- `osmodule.nxf`: OS-dependent standard module source; implicitly scheduled unless the root program contains `no: osmodule;`.
- `coremodule.nxf`: OS-independent general-purpose module source; its automatic/default loading policy remains undecided.
- `.exf` files are reserved for external function declarations and external object/library linkage metadata; they are not the primary source format for these standard modules.

The two `.nxf` files currently contain only starter comments. The previous mixed `stdlib/osmodule.exf` draft was moved out of the active lookup path to `temp/legacy-stdlib/osmodule.exf` for later review; none of its API is treated as implemented.

The package-root `omnicore-sdk/occ.ocf` uses `./occ/libs`. The runtime copy `omnicore-sdk/src/c/occ.ocf` is placed beside the `bin/occ` executable and uses `../occ/libs`; CMake stages the source `occ/` directory at the build root, avoiding a collision between the executable file `bin/occ` and a same-named directory. CTest `occ_predefined_standard_libraries_staged_for_runtime` verifies the staged files and runtime config path.

**Implementation update:** a concrete C filesystem source provider now consumes the bounded OCF `[include]` paths. For dependency requests, it maps a dotted module name to one filename by appending `.nxf` (for example, `db.mysql.sql` → `db.mysql.sql.nxf`) and searches in this strict order: host-supplied `dsplibs/` root, host-supplied `libs/` root, project-root `include/`, then additional OCF `[include]` paths in declaration order. It does not split dotted names into directory components. The host still must supply the actual package roots and load the correct project OCF; current `main.c` only reads the SDK runtime OCF language setting, so end-to-end CLI resolution remains pending.


## 14. Dependency loading tree and host scheduler (implementation progress)

The intended loading tree is a recursive graph walk, not a flat list of source files. Each node represents one canonical module identity; each edge represents one explicit `use` or the root's implicit `osmodule` dependency. Alias (`as`) and namespace (`ns`) are edge-local Semantic bindings and do not create additional module nodes.

### 14.1 Loading tree

```text
Compilation Job
└─ Root Module (main.nxf)
   ├─ implicit osmodule [unless no: osmodule;]
   ├─ use db.mysql as db
   │  └─ db.mysql
   │     ├─ use db.types
   │     │  └─ db.types
   │     └─ use db.driver
   │        └─ db.driver
   └─ use math.vector ns vec
      └─ math.vector
```

The diagram is illustrative: actual edges and module names come from the source-provider's canonical identity and the AST_USE records. A repeated canonical identity is one graph node even when reached through multiple paths or imported with different aliases/namespaces.

### 14.2 Recursive scheduler algorithm

1. Register the root using its canonical identity and content fingerprint; mark it `RESOLVING`.
2. Run Lexer and Parser for that module and retain its AST artifact for the job.
3. Walk the module AST's top-level `AST_USE` declarations. Preserve each node ID, dotted request, alias, namespace and source span.
4. For each USE, call the host source provider. The provider applies project-root, `[include]`, standard-library and package policy; it returns canonical identity and borrowed source bytes. The portable resolver itself never opens paths.
5. Register/reuse the canonical graph node by identity + fingerprint. Add the importer→dependency edge with the originating USE node ID/span.
6. If the dependency is `RESOLVED`, reuse its retained AST/interface. If `RESOLVING`, report a cycle at the current USE span. If `DISCOVERED`, recursively Lexer/Parser it and scan its USE declarations.
7. After all outgoing dependencies have been discovered successfully, mark that module `RESOLVED` in the discovery graph. A parse/provider failure marks it `FAILED`; it cannot publish a valid interface.
8. Once discovery is complete, obtain a dependency-first schedule and run Semantic on dependencies before importers. Publish an interface only after that module's Semantic result is valid.
9. Give each importer its validated dependency interfaces plus its own AST to Semantic. The root's valid result is the compilation's Semantic output.

### 14.3 Dependency-first schedule API

The portable graph now exposes:

```c
OccModuleResolverStatus occ_module_resolver_dependency_order(
    const OccModuleResolver* resolver,
    uint32_t* out_module_ids,
    size_t out_capacity,
    size_t* out_count
);
```

The API performs deterministic DFS ordering: dependencies precede importers and each node appears once. It rejects insufficient output capacity and cycles; on failure `out_count` is zero, so callers must not consume a partial order. The host scheduler still must ensure all nodes have completed source/Parser discovery before requesting the final order.

### 14.4 Discovery versus Semantic execution

Discovery and Semantic are separate passes:

- **Discovery pass:** provider lookup → Lexer/Parser → AST_USE scan → graph edges → cycle/failure detection.
- **Semantic pass:** dependency-first schedule → per-module Semantic → validated exported interface → importer Semantic.

This prevents a module from being marked semantically usable merely because its source parsed. It also keeps imported symbol visibility, Group ownership, alias/namespace collision and type checking in Semantic—not in the Resolver.

### 14.6 AST_USE traversal API

The Parser/AST library now exposes a Resolver-independent visitor:

```c
ASTUseVisitStatus ast_visit_top_level_uses(
    const ASTNode* program,
    ASTUseVisitorFn visitor,
    void* context,
    size_t* out_count
);
```

The visitor walks only the direct `ASTProgram.declarations` sibling list, selects `AST_USE` nodes in source order, and passes each borrowed `ASTUse` plus a 1-based per-program ordinal to the callback. It does not descend into function or block bodies, allocate memory, mutate the AST, or resolve module paths. The callback may stop traversal; the result distinguishes normal completion, callback stop, invalid input/root, and ordinal overflow.

The ordinal is not a persistent AST node identity. The host may use it as the Resolver edge's `ast_use_node_id` correlation value for the lifetime of the current parsed module. `module_name`, `alias`, and `namespace_name` remain separate AST fields; path lookup and alias/namespace binding are outside this visitor.

The visitor and its regression test are implemented. A callback-driven host scheduler now consumes the visitor output, resolves provider requests, recursively parses newly discovered modules, retains their AST roots for the job, and invokes the Semantic callback in dependency-first order. The scheduler is deliberately independent of filesystem policy and stage-DLL loading.

### 14.5 Current implementation boundary

Implemented:
- canonical module registration/cache identity and edge storage;
- provider callback contract and source budget checks;
- implicit `osmodule` edge representation;
- dependency-first ordering API and cycle rejection;
- top-level AST_USE collection with source-order ordinals and copied source spans;
- callback-driven recursive discovery with a bounded graph, retained per-module AST roots, provider-view release discipline, and precise source/provider/resource error statuses;
- dependency-first Semantic callback pass, with no Semantic invocation when discovery fails.

Still not implemented:
- concrete filesystem/project/include/package source provider;
- adapter callbacks that invoke the actual Lexer/Parser/Semantic DLLs from the host;
- wiring the scheduler into `main.c` in place of the current single-module stage sequence;
- dependency OIR interface artifact and validator;
- Semantic DLL imported-interface input, alias/namespace binding and exported symbol publication;
- end-to-end multi-file integration tests using real source files and stage DLLs.

The scheduler is an implemented coordinator core, not yet an operational end-to-end module loader. In particular, the existing `main.c` pipeline still invokes Semantic immediately after parsing the root module and does not yet use this scheduler.

### 14.7 Callback-driven host scheduler API

The host coordinator is exposed in `module/module_scheduler.h` and implemented in `module/module_scheduler.c`:

- `occ_module_scheduler_init()` borrows a source provider and Parser/Semantic callbacks; the provider and callback context must outlive the scheduler.
- `occ_module_scheduler_run()` resolves the root source, parses and validates each AST_PROGRAM, copies top-level USE path/span/ordinal records under the configured edge/text budgets, then performs iterative depth-first dependency discovery. A dependency is parsed only on first discovery; repeated canonical identities reuse the existing graph node and retained AST.
- The implicit `osmodule` edge is resolved before explicit USE edges when `ASTProgram.disable_osmodule == 0`. The provider determines where that standard module source comes from.
- The scheduler retains each parsed AST until `occ_module_scheduler_destroy()`. Provider source views are released exactly once after the Parser callback has consumed their bytes.
- After discovery completes, the Resolver's dependency-first order drives the Semantic callback. A Semantic callback failure aborts the pass; imported-interface data is not synthesized by the scheduler.
- The run is single-shot for one scheduler instance. Destroy it after success or failure; partial discovery state is not reusable.
- Source-not-found, denied, ambiguous, I/O, provider, resource-limit, parse, AST, cycle and Semantic failures remain distinguishable.

The scheduler has isolated C tests for shared dependencies, deterministic dependency-first Semantic order, provider-view release counts and cycle detection. These tests exercise callback contracts with fake sources; they do not claim that the filesystem provider or actual stage DLL integration is complete.


### 14.8 Filesystem module provider and single-file policy

The filesystem provider is implemented in `module/filesystem_module_provider.h/.c` and is deliberately separate from the graph Resolver.

- `use db;` maps to the single file `db.nxf`.
- `use db.mysql.sql;` maps to `db.mysql.sql.nxf`; dotted segments are not converted to directories.
- Dependency search precedence is strict: (1) host-provided `dsplibs` root, (2) host-provided `libs` root, (3) the project's fixed `include/` directory, and (4) additional directories declared by the project's OCF `[include]` section in declaration order.
- The first existing candidate wins. A later directory is not consulted after a successful earlier match.
- Module names accept identifier-like dot-separated segments only. Absolute paths, slashes, backslashes, colon, and traversal spellings are rejected for dependency requests.
- Root compilation input is different from a dependency request: when `importer_identity == NULL`, the requested path is treated as the root source path and is opened directly.
- Files are read in binary mode under a configurable source-size bound. Canonical identity is the resolved filesystem path (`realpath` on POSIX and `_fullpath` on Windows); source bytes and identity remain provider-owned until the matching release callback.
- Oversized files produce a distinct provider resource-limit status, which Resolver maps to `OCC_MODULE_RESOLVER_LIMIT_EXCEEDED`.

The provider copies its configured roots and OCF include entries at initialization, so the caller may destroy the parsed OCF object afterward. It does not itself parse OCF or decide where the global `dsplibs/` and `libs/` roots live; the host supplies those roots. The SDK's current CLI has not yet wired this provider or selected a per-project OCF.

Isolated C tests cover single-file dotted-name mapping, path traversal rejection, dsplibs-over-libs precedence, libs fallback, project `include/` fallback, additional OCF include fallback, missing-module behavior, and source-size limits. These tests do not yet represent an end-to-end compilation through the stage DLLs.

### 14.9 Semantic module interface snapshot

The first C interface-snapshot layer is implemented in `module/module_interface.h/.c`. It consumes a finalized `SemanticResult` and creates an independently owned, immutable-by-convention snapshot for later importer Semantic work.

- The snapshot retains canonical module identity and a versioned schema marker (1.0).
- A declaration is included as a publication candidate when its SemanticResult symbol flags contain either `PUBLIC` or `EXPORTED`. Both bits are preserved verbatim; the snapshot does not collapse their meaning or decide whether a particular importer may access the declaration.
- Symbol source IDs, lexical scope IDs, declaration AST node IDs, kind, flags, name and type ID are preserved. IDs are source-module-local and must never be interpreted as importer-local IDs.
- Only type records referenced by published symbols or published relations, plus their base-type chain, are copied. Type source IDs and referenced symbol IDs remain source-module-local for future visibility/type-reference validation.
- Relations are copied only when their owner is published; relations with a nonzero related-symbol ID are retained only when that related symbol is also published. This preserves visible Group membership and method/type relation metadata without leaking private related declarations.
- The snapshot owns every copied string and table and is released with `occ_module_interface_destroy()`. It requires a finalized and structurally populated SemanticResult.

This is a data-transfer foundation, not yet the final serialized OIR interface contract. Alias and namespace bindings remain importer-side data sourced from each AST_USE (`module_name`, `alias`, and `namespace_name` are distinct). The importing Semantic stage must apply the language's exact PUBLIC/EXPORTED rules, detect local and alias collisions, resolve namespace selection, and validate Group/type access. No imported symbol is injected into the local AST or silently treated as a local declaration.

Tests currently cover exported Group + public member publication, preservation of source IDs and type/relation metadata, omission of an unmarked private symbol, and rejection of non-finalized semantic input. The snapshot is not yet consumed by the Semantic DLL and is not yet encoded as an OIR artifact.

### 14.11 Serialized Module Interface OIR artifact

The in-memory snapshot now has a dedicated OIR transport artifact. The generic OIR artifact-kind registry adds `OIR_KIND_MODULE_INTERFACE = 10`; earlier numeric artifact kinds and the SEMANTIC_AST section registry are unchanged.

The artifact uses one required OIR payload section (version 1) containing the `OMI1` payload schema 1.0. Its fixed-width little-endian records encode canonical module identity, publication-candidate symbols, the referenced type closure, and retained relations. Strings are length-prefixed and are not serialized as host pointers or C structs.

The writer accepts only a structurally consistent `OccModuleInterface`. The reader validates the outer OIR envelope, requires Semantic producer stage + valid artifact state, validates payload schema and record limits, rejects embedded NUL bytes and duplicate source IDs, and checks that symbol/type/relation references that must be part of the published snapshot are present. Type referenced-symbol IDs remain source-module-local and may point to non-published declarations; importer Semantic must enforce visibility before exposing such references.

The reader materializes an owned `OccModuleInterface` and the caller releases it through `oir_module_interface_artifact_destroy()`. The codec is a standalone C library and is tested by a snapshot → OIR → decoded snapshot round-trip. This does not yet connect the artifact to Semantic DLL output or the scheduler's imported-interface input, and does not replace independent Verifier-stage validation.

### 14.10 Import binding preservation

The scheduler now retains each top-level USE declaration's three independent source-level values: dotted `module_name`, optional `alias`, and optional `namespace_name`. It exposes `occ_module_scheduler_get_import_binding()`, keyed by importer module ID and the visitor's 1-based USE ordinal. The returned view includes the resolved dependency module ID and original source span; string fields are borrowed from the scheduler and remain valid until scheduler destruction.

This accessor is the bridge between dependency graph resolution and later importer Semantic binding. In particular, `as _` and `ns _` remain distinguishable because the original alias and namespace fields are not merged or normalized. A C regression test checks the independent values and dependency identity. This does not yet implement the language's visibility or namespace lookup rules.
