# OmniCore Compiler Pipeline

**Status:** Accepted architecture direction; implementation pending  
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

This is the target architecture, not a claim that all listed modules currently exist. The current C implementation has an OCC executable, a combined frontend library and a separately loadable codegen library. The migration to one shared module per stage is pending.

A stage can be skipped only when the selected build mode explicitly permits it and the next stage accepts the supplied artifact format. A stage must not silently reinterpret an unrelated artifact as its input.

## 3. Module contract

Every module must expose a versioned C ABI. The exact exported symbol names and struct layout will be specified in a separate ABI document before implementation.

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

As of this design:
- OCC currently performs orchestration in `main.c`.
- Lexer, parser and semantic code are not yet independent DLLs.
- The current CMake frontend is a combined library. The AST payload codec and OIR artifact adapter are separate static utility targets, but they do not yet constitute an independently loadable parser-stage DLL/SO/dylib.
- Codegen is the only currently separate shared module in the C pipeline.
- Existing parser/semantic control flow still has early-stop behavior and does not yet implement the complete diagnostic-accumulation policy.
- There is no implemented general checkpoint/stop-after interface or multi-job scheduler yet.

These are known gaps to be addressed incrementally. This document records the intended architecture; it does not claim implementation or test completion.


## 11. Standard artifact handoff

All stage-to-stage handoff uses the common OIR container described in [OIR_FORMAT.md](OIR_FORMAT.md). Each artifact declares its payload kind and schema version (for example `TOKEN_STREAM`, `AST`, `SEMANTIC_AST`, `OIR_MODULE`, `ASSEMBLY`, or `OBJECT`). The same artifact may be passed in memory through an opaque ABI handle or serialized as a `.oir` checkpoint.

The file extension is not the type system: a stage must inspect and validate the OIR header and payload schema before consuming an artifact. It must reject unsupported versions, wrong artifact kinds, corrupt lengths and invalid state. A partial artifact is accepted only by stages that explicitly declare partial-input support.

A schema-1.0 OIR AST payload writer/reader now exists and is tested across Windows, Linux and macOS, including real parser → OIR AST artifact → frontend AST round-trips. The older native-layout AST serializer exposed by the parser remains legacy/internal and is not a cross-platform module interchange format. The OCC CLI's stop-after-parser checkpoint command and the independent parser DLL stage entry point are still pending.
