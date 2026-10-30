# Diagnostics and Compilation Context

**Status:** Draft contract for implementation  
**Related:** [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md)

## 1. Design requirements

- Diagnostics belong to one compilation job, never to a process-global list.
- A stage reports all independent findings it can safely discover.
- Reporting a source error or warning does not itself terminate a stage.
- Every diagnostic has a stable code, severity, stage, and source span when available.
- Partial artifacts and diagnostics can coexist.
- Infrastructure failures are represented separately from source errors.
- Final output is controlled centrally by the pipeline runner, not by individual frontend modules.

## 2. Diagnostic record

The initial logical record contains:

| Field | Meaning |
|---|---|
| `code` | Stable machine-readable diagnostic code, e.g. `LEX001`, `PARSE012`, `SEM004` |
| `severity` | `NOTE`, `WARNING`, `ERROR`, or `FATAL` |
| `stage_id` | Pipeline stage that emitted the diagnostic |
| `message_id` | Stable message identifier for localization |
| `message` | Rendered message for CLI/IDE use |
| `source_id` | Source file/document identity |
| `start` | Start line/column and byte offset |
| `end` | End line/column and byte offset |
| `related` | Optional related locations (e.g. original declaration) |
| `notes` | Optional explanatory notes/help text |

Line and column are 1-based. Byte offsets are 0-based and refer to the original source byte sequence. A missing location is explicitly represented as unavailable; it must not be fabricated as line 1/column 1.

Diagnostic codes are stable API identifiers; localized wording may change without changing the code.

## 3. Severity and control flow

Severity does not directly control whether the current stage continues.

- NOTE: additional context.
- WARNING: suspicious or potentially unintended source construct.
- ERROR: source construct or semantic rule is invalid.
- FATAL: the compiler cannot safely continue this job due to a stage/runtime failure.

A stage should continue after NOTE/WARNING/ERROR whenever its recovery strategy can preserve a well-formed partial artifact. FATAL means the current job cannot safely execute dependent stages. It does not cancel unrelated compilation jobs.

The default final-output policy is strict: one or more WARNING or ERROR diagnostics blocks publication of final output. NOTE does not block output. FATAL always blocks output. This policy is evaluated by the orchestrator after all eligible stages have reported. It is not implemented as an early return inside lexer/parser/semantic code.

## 4. Compilation context

Each source compilation receives an independent context with the following logical parts:

- Job ID and cancellation state.
- Source manager: source identities, immutable source buffers and path mapping.
- Build options: selected target, optimization level, diagnostic policy and requested checkpoint.
- Diagnostic collector: append-only, thread-safe or job-local collection.
- Artifact registry: current artifact handles, format/version, validity and ownership.
- Module services: allocator, logging, localization and optional filesystem abstraction.
- Pipeline state: completed stages, skipped stages, failure reason and final-output eligibility.

The context is owned by the orchestrator. A module receives a restricted invocation view and must not retain borrowed pointers after its invocation ends unless the ABI explicitly transfers ownership.

## 5. Stage result

A stage invocation returns a structured result with two independent dimensions:

**Execution status**
- `COMPLETED`: stage ran to completion.
- `RECOVERED`: stage found source diagnostics but produced a usable partial artifact.
- `SKIPPED`: runner did not invoke the stage because a prerequisite was unavailable/unsafe.
- `FAILED`: infrastructure or internal failure prevented stage completion.

**Artifact validity**
- `VALID`
- `PARTIAL`
- `INVALID`
- `NONE`

A completed stage may still have emitted warnings/errors. The runner checks both execution status and artifact validity before scheduling the next stage.

## 6. Recovery and downstream rules

- Lexer may emit error tokens and continue scanning after recoverable invalid characters or malformed literals.
- Parser may synchronize at grammar-defined recovery points and preserve unaffected AST regions.
- Semantic analysis should continue across independent declarations/scopes when symbol/type state remains trustworthy.
- IR lowering and code generation must not consume invalid AST/OIR.
- A partial artifact may be consumed only by a stage that explicitly declares partial-input support.
- No module may silently downgrade INVALID to PARTIAL or VALID.

Recovery boundaries and synchronization tokens are stage-specific and will be tested separately.

## 7. Concurrency and ownership

- Each job owns its diagnostics, source manager and artifact handles.
- Shared immutable module metadata is allowed.
- Shared mutable caches require explicit synchronization and documented lifetime.
- Modules declare `reentrant` and `thread_safe` capabilities.
- If a module is not thread-safe, the runner serializes calls to that module while still allowing other independent modules/jobs to proceed.
- Buffers crossing the ABI boundary must have a single documented owner and release function. The caller must not free memory allocated by a module using an unrelated C runtime allocator.

## 8. Output and checkpoint rules

- Intermediate checkpoints may be written even when diagnostics exist, provided the stage produced an artifact and the user requested it.
- Checkpoint metadata records artifact kind/version, stage ID, validity, job ID and diagnostic counts.
- Final output is written to a temporary destination and atomically published only when all required stages complete and the strict diagnostic gate passes.
- On failure, an existing final output must not be overwritten by a partial file.
- The runner reports all collected diagnostics once the job reaches a terminal state.

## 9. ABI freeze checklist

Before exporting this as a stable module ABI, tests and review must settle:
- Fixed-width integer types and struct size/version strategy.
- String encoding and ownership.
- Source identity/path representation.
- Diagnostic collector callback versus host-owned append API.
- Cancellation and allocation-failure behavior.
- Module compatibility rules for ABI major/minor versions.
- Whether diagnostic ordering is source-order, stage-order, or stable insertion order.

Until then, these are logical contract definitions, not a frozen binary ABI.


## 10. Initial C implementation status

The first job-owned collector implementation is in:
- `omnicore-sdk/src/c/core/diagnostics.h`
- `omnicore-sdk/src/c/core/diagnostics.c`

The current API:
- Stores structured records in a dynamically grown per-collector array.
- Copies message and optional hint text into collector-owned storage; caller string pointers are never retained.
- Preserves diagnostic code, stage ID, message ID, severity and optional source span.
- Counts diagnostics by severity.
- Uses a strict output gate: WARNING, ERROR or FATAL blocks final output; NOTE alone does not.
- Fails closed if collector allocation fails or the collector is missing.
- Rejects invalid severity/code, malformed source spans and embedded NUL in message text.

The collector is currently a standalone C utility library and is not yet wired into ParserContext, SemanticContext, OIR DIAGNOSTICS sections or the OCC pipeline. It is job-owned by design, but this first version is not internally synchronized; the orchestrator must serialize writes to one collector or add a synchronization wrapper before same-job concurrent reporting.
