# OmniCore Compiler Pipeline

**Status:** Accepted architecture direction; staged migration in progress  
**Main implementation language:** C  
**Module model:** One independently loadable shared library per compiler stage

## 1. Goals

OmniCore is a staged compiler, not a monolithic executable. Each compiler stage is an independent module with a documented input/output contract. The OCC executable is the pipeline orchestrator: it loads modules, schedules work, passes artifacts between stages, collects diagnostics, and controls checkpoints.

A module is built as a platform-native shared library:
- Windows: `.dll`
- Linux: `.so`
- macOS: `.dylib`

The extension differs by host platform; the module contract does not.

## 2. Initial stage graph

The first planned pipeline is:

```text
Source
  |
  v
Lexer module
  | Token stream
  v
Parser module
  | AST
  v
Semantic analysis module
  | Annotated/validated AST
  v
IR lowering module
  | Canonical OIR
  v
OIR validation module
  |
  v
Optimizer module(s)  ---> zero or more independently selectable passes
  |
  v
Target resolution / capability validation module
  |
  v
Backend / code generation module
  | Object or assembly artifact
  v
Assembler module
  |
  v
Linker / packaging module
  |
  v
Final output
```

This is the target architecture, not a claim that all listed modules currently exist. The current C implementation has independently loadable lexer, parser and semantic stage modules plus the new independent `occ_oir_lowering_stage` module. OCC invokes Lexer → Parser → Semantic and validates the `SEMANTIC_AST` artifact. The lowering DLL accepts that artifact and emits a validated `OIR_MODULE` container; its dynamic-module tests pass on Windows, Ubuntu and macOS, including Ubuntu ASan/UBSan and Valgrind. The lowerer is not yet called by the OCC CLI. The current legacy Codegen path still consumes the parser AST through the native-layout AST serializer.

A stage can be skipped only when the selected build mode explicitly permits it and the next stage accepts the supplied artifact format. A stage must not silently reinterpret an unrelated artifact as its input.

## 3. Module contract

Every module exposes the versioned C ABI documented in [OCC_MODULE_ABI.md](OCC_MODULE_ABI.md). The ABI remains Draft v1 and is not frozen.

The contract must provide, at minimum:

- Module identity and semantic version.
- Pipeline ABI major/minor version.
- Stage identity and supported input/output artifact kinds and versions.
- Capability flags and required/optional features.
- A process entry point receiving an invocation context.
- A result describing stage status and produced artifact.
- A diagnostic reporting interface.
- Explicit ownership and release rules for every allocated object and buffer.
- Optional initialize/shutdown hooks for module-level resources.

Do not pass compiler-internal C structs directly across module boundaries unless they are explicitly part of the versioned ABI. Prefer versioned serialized artifacts or opaque handles with module-owned release functions. This prevents compiler modules built with different compiler options or runtime settings from silently disagreeing about structure layout or memory ownership.

The orchestrator may load a module once and reuse it for multiple compilation jobs. Per-job mutable state belongs to a compilation context, never to module-global mutable variables.

## 4. Diagnostics and continuation policy

Errors and warnings are diagnostics, not immediate process termination commands.

- A stage should report every independent issue it can safely discover.
- A recoverable lexical, syntax or semantic issue must not automatically terminate the whole pipeline.
- The stage should produce a partial artifact when it can do so safely and mark its validity/completeness explicitly.
- Later stages may run diagnostics on a partial artifact only when their input contract explicitly allows that artifact state.
- A stage must not treat an invalid or incomplete artifact as fully valid.
- All collected errors and warnings are presented together in the final diagnostic report, grouped or sortable by stage and source location.

**Output gate:** The default policy is strict: if the compilation has one or more errors or warnings, final executable/object output is not committed. Diagnostics and requested intermediate artifacts may still be written. A future explicit warning policy may allow warnings without weakening error handling; it must be a user-visible build option, not an implicit module decision.

Infrastructure failures are different from source diagnostics. Examples include a missing module, incompatible ABI, module crash, unreadable required input, or inability to allocate required state. If a stage cannot run safely, dependent stages are skipped, independent jobs continue where possible, and the infrastructure failure is included in the final report.

## 5. Artifact validity

Every stage result must distinguish at least:

- **Valid:** complete artifact; safe for normal downstream processing.
- **Partial:** artifact exists but contains recoverable omissions or unresolved regions; only explicitly partial-aware stages may consume it.
- **Invalid:** artifact must not be consumed by downstream semantic/code-generation stages.
- **No artifact:** stage could not produce an output.

Diagnostics and artifact validity are separate. A stage can produce a partial artifact and diagnostics in the same invocation.

The pipeline must never generate or publish a final output from an artifact marked partial or invalid, or when the output gate rejects the accumulated diagnostics.

## 6. Checkpoints and intermediate outputs

The user can stop after any completed stage and request that stage's intermediate artifact. The pipeline runner must support a stop-after-stage selection (eventual CLI example: `--stop-after parser`) and an explicit intermediate-output location.

Checkpoint behavior:
- Stop only after the selected stage has completed its reporting and artifact finalization.
- Preserve the artifact together with its format/version metadata and a diagnostic summary.
- Mark partial or invalid status in the checkpoint metadata.
- Do not run later stages after the requested checkpoint.
- Checkpoint output is not a final program and must never be mistaken for one.

The same checkpoint mechanism should support debugging, test fixtures, IDE inspection and reproducible compiler bug reports.

## 7. Multiple compilation jobs and pipeline execution

A module instance may be reused across jobs, but each invocation receives an independent job context, diagnostic collection and artifact ownership scope.

The orchestrator should support:
- Sequential execution for simple builds and debugging.
- Multiple independent compilation jobs in parallel.
- Bounded queues between stages for streaming/batch workloads.
- Back-pressure when a downstream stage is slower than its producer.
- Cancellation of one job without corrupting other jobs.
- Deterministic per-job output paths and diagnostics.

Parallelism is an orchestration concern. A module must declare whether it is reentrant/thread-safe; the runner must serialize access to modules that do not declare that capability. No module may use a shared global error list or shared mutable parser state for concurrent jobs.

## 8. Failure and output transaction

Final outputs are published atomically only after all required stages succeed and the output gate passes. A failed build must not leave a stale or half-written final artifact that looks successful.

Intermediate checkpoints are separate, intentionally retained outputs. A failed stage may leave a diagnostic/checkpoint artifact when requested, but not a final deliverable.

## 9. Implementation sequence

1. Define the diagnostic record and per-compilation context.
2. Define artifact identity, version, validity and ownership rules.
3. Define the versioned module ABI and loader abstraction for Windows/Linux/macOS.
4. Convert the current codegen shared library to the common module contract.
5. Split lexer, parser and semantic analysis into separate shared modules.
6. Add the orchestrator's stage graph, checkpoint and stop-after controls.
7. Add partial-artifact recovery and strict final-output gate.
8. Add multi-job scheduling, bounded queues and concurrency tests.
9. Split remaining IR, optimizer, target, assembler and linker stages into modules.

Each step requires unit tests, module load/unload tests, ABI compatibility tests and multi-job isolation tests before being marked complete.

## 10. Current implementation boundary

Current implementation boundary (incremental migration):
- OCC currently performs orchestration in `main.c`.
- Lexer, parser and semantic analysis are built as independent shared stage modules and invoked through the OCC module loader. The live chain is `SOURCE_UNIT → TOKEN_STREAM → AST → SEMANTIC_AST`.
- OCC independently validates the semantic output. The current CLI then materializes the parser AST and writes the legacy native-layout AST serialization via `parser_save_ir()`; the legacy codegen DLL deserializes that AST and walks AST nodes directly.
- This legacy `.oir` file is **not** an `OIR_MODULE` artifact and is not the canonical OIR v1 module representation. `OIR_KIND_MODULE` (kind ID 5), payload schema 1.1, the scalar lowering DLL and the independent OIR Verifier DLL are implemented. OCC now runs `SEMANTIC_AST → OIR_MODULE → OIR Verifier → Codegen`; the current lowering and backend subsets remain intentionally limited. See [OIR_MODULE.md](OIR_MODULE.md) for the contract and implementation boundary.
- The next migration is to add the tested IR Lowering DLL to the OCC CLI stage graph, passing `SEMANTIC_AST` and validating/checkpointing `OIR_MODULE`. Only after that handoff and parity tests are established should Codegen migrate from AST input to `OIR_MODULE` input.
- The frontend remains a shared internal support library linked by stage modules. Its C structs and allocator state do not cross the module ABI.
- Codegen remains a separately loadable legacy shared library and has not yet migrated to `OccStageModuleV1`.
- General checkpoint/stop-after controls, complete diagnostic accumulation, and multi-job scheduling remain open work.

This is a staged migration. A built module target is not considered pipeline-integrated until the OCC host loads, invokes, validates and tests its artifact handoff.


## 11. Standard artifact handoff

All stage-to-stage handoff uses the common OIR container described in [OIR_FORMAT.md](OIR_FORMAT.md). Each artifact declares its payload kind and schema version (for example `TOKEN_STREAM`, `AST`, `SEMANTIC_AST`, `OIR_MODULE`, `ASSEMBLY`, or `OBJECT`). The same artifact may be passed in memory through an opaque ABI handle or serialized as a `.oir` checkpoint.

The file extension is not the type system: a stage must inspect and validate the OIR header and payload schema before consuming an artifact. It must reject unsupported versions, wrong artifact kinds, corrupt lengths and invalid state. A partial artifact is accepted only by stages that explicitly declare partial-input support.

A schema-1.0 OIR AST payload writer/reader and validate-before-allocate frontend materializer are tested across Windows, Linux and macOS, including real parser → OIR AST artifact → frontend AST round-trips. The parser DLL stage is integrated into the OCC CLI through the host loader. The older native-layout AST serializer remains a temporary legacy Codegen bridge, not a cross-platform module interchange format. The user-facing `--stop-after` checkpoint command remains pending.


## 12. Project-owned linker and optional OCL security policy

The final link/package stage will be implemented by the project. The system C linker may be used as a temporary development/test aid, but it is not the final linker and is not a required runtime dependency of the finished toolchain.

The OCL security layer is **optional and user-controlled**, not an always-on compiler restriction:
- Security disabled: the normal OCL/link/package flow runs without applying the optional security policy layer.
- Security enabled: the selected security checks and restrictions are applied according to the user's chosen policy/profile.
- The selected mode and policy must be explicit in build configuration and reproducible in build metadata.
- A module must not silently enable, weaken or override the user's selected security mode.
- The exact policy set, configuration syntax, trust model and enforcement points remain to be designed; this statement records the opt-in requirement, not a claim that security enforcement is implemented.

The project-owned linker and optional security layer are separate design responsibilities. The security layer must not become an unavoidable hidden dependency of ordinary builds.
