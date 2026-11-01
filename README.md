# OMNI-CORE Compiler (OCC)

**Project:** OMNI-CORE v2 / NEXUS FLOW  
**Compiler:** OCC — OMNI-CORE Compiler  
**Primary language:** NEXUS Flow  
**Status:** Architecture and language/compiler specifications in active development

OMNI-CORE Compiler is the compiler project for NEXUS Flow. This repository records the language design, compiler architecture, intermediate representations, validation rules, module boundaries, and implementation roadmap.

> **Documentation split:** This README describes the compiler project. The language's syntax and semantics live in the separate [NEXUS Flow — Syntax Reference Guide](docs/language/syntax-reference.md).

---

## 1. Project goals

OCC is being designed as a modular compiler rather than a single monolithic executable. The intended architecture separates language front-end work, intermediate representation, optimization, code generation, and final linking into independently testable stages.

Primary goals:

- Keep NEXUS Flow language rules explicit and documented.
- Separate compiler stages into independent modules/DLLs.
- Standardize the intermediate artifacts exchanged between stages.
- Validate intermediate representation before optimization or code generation.
- Make compiler behavior deterministic and testable.
- Retain control over the final executable and its security checks through an OCC-owned linker.

## 2. Compiler architecture

The planned high-level pipeline is:

```text
NEXUS Flow Source
       |
       v
Frontend DLL
(Lexer / Parser / AST)
       |
       v
OIR Builder DLL
       |
       v
OIR Writer DLL
       |
       v
OIR Reader + Verifier DLL
       |
       v
OIR Optimization DLL(s)
       |
       v
Code Generation DLL
       |
       v
OCC Linker
       |
       v
Executable / Target Artifact
```

Each stage consumes a defined artifact from the previous stage and produces the next stage's input. Intermediate file formats and module contracts must be versioned and validated so that DLLs can be developed and tested independently.

### 2.1 Intermediate representation

OIR (Omni Intermediate Representation) is the compiler's control-flow and data-flow representation.

The current design direction includes:

- Basic blocks with explicit terminators.
- `JUMP`, `BRANCH`, and `RETURN` control-flow terminators.
- Block parameters for SSA value transfer across control-flow edges.
- Derived predecessor/successor information for CFG analysis.
- Verification of block structure, edge arguments, SSA definitions/uses, dominance, and type consistency.
- Deterministic serialization and reader/writer round-trip validation.

OIR details are documented alongside the language and compiler specifications as they become stable.

### 2.2 Linking strategy

A system C linker may be used during early compiler experiments and test builds. It is not the intended final linker.

The final architecture calls for an OCC-owned linker with a dedicated security/validation layer. Its file format, relocation handling, symbol resolution, and security contract will be specified before implementation.

## 3. Current development focus

The current work is specification-first: settle language semantics and compiler contracts before locking down implementation details.

Recent design work includes:

- NEXUS Flow `select` and `scan` behavior and their CFG lowering.
- Multi-Rule Engine barrier aggregation, trigger collection, handler ordering, and optional `always` / `default` behavior.
- Multi-Rule OIR control-flow lowering, including error accumulation, SSA trigger results, handler dispatch, and separate failure/continuation paths.
- Normative Multi-Rule OIR test MR-OIR-001: aggregate all barrier failures, preserve source order, and prove that trigger/handler/normal flow is unreachable after failure.
- Normative Multi-Rule OIR test MR-OIR-002: evaluate all trigger conditions once, retain SSA match results, and dispatch every matched handler in handler source order.
- Normative Multi-Rule OIR test MR-OIR-003: verify that `return` inside a handler or `always` block terminates the enclosing function path and cannot fall through to later handlers, `default`, or normal continuation.
- Normative Multi-Rule OIR test MR-OIR-004: verify normal handler → `always` → conditional `default` ordering for both matched-trigger and no-match paths.
- Normative Multi-Rule OIR test MR-OIR-005: verify named `always` / `default` trigger emission, dedicated handler stages, name resolution, and matched/no-match dispatch order.
- Normative Multi-Rule OIR test MR-OIR-006: verify sequential `apply rules` execution, per-application state isolation, and failure short-circuiting of later RuleSets.
- Normative Multi-Rule OIR test MR-OIR-007: verify repeated application of the same RuleSet with independent arguments, trigger SSA state, and per-application handler dispatch.
- Normative Multi-Rule OIR test MR-OIR-008: verify that `break` and `continue` inside handlers bind to the handler's lexical loop scope, with correct loop-latch/exit edges and no false fallthrough.
- Normative Multi-Rule OIR test MR-OIR-009: verify function-scoped label resolution for `jump(label)` inside handlers, correct JUMP targets, and rejection of invalid scope transitions.
- Normative Multi-Rule OIR test MR-OIR-010: verify forward and backward `jump(label)` transfers, cyclic CFG edges, conditional cycle exits, and SSA/block-parameter consistency.
- Normative Multi-Rule OIR test MR-OIR-011: verify dominator analysis, natural-loop versus irreducible-cycle classification, and safe CFG optimization constraints.
- Normative Multi-Rule OIR test MR-OIR-012: verify cross-basic-block `jump(label)` as a first-class transfer and safe jump threading, constant-branch simplification, unreachable-block cleanup, and dead-instruction elimination.
- Normative Multi-Rule OIR test MR-OIR-013: verify multi-predecessor common targets, edge-specific block parameters, merge/SSA correctness, and optimizer preservation.
- Normative Multi-Rule OIR test MR-OIR-014: verify composition of if/elsif/els, select, loop and cross-block jumps; repeated merge-and-branch patterns; and no arbitrary fixed jump/predecessor count limit.
- Normative Multi-Rule OIR test MR-OIR-015: verify nested loops, select-branch jumps, nearest-loop break/continue binding, nested loop CFG/SSA merges, and optimizer preservation.
- Normative Multi-Rule OIR test MR-OIR-016: verify lexical-scope-aware jump cleanup, resource preservation for same-scope backward transfers, and rejection of re-entry into exited scopes.
- Normative Multi-Rule OIR test MR-OIR-017: verify defer registration, scope-activation lifetime, LIFO execution, and ordering before automatic resource cleanup.
- Normative Multi-Rule OIR test MR-OIR-018: verify dynamic defer registrations, per-iteration loop-body activations, and LIFO cleanup.
- Normative Multi-Rule OIR test MR-OIR-019: verify live-binding semantics for deferred variable reads/writes, activation-bound variable lifetime, ownership transfer validation, and exactly-once resource cleanup.
- Normative Multi-Rule OIR test MR-OIR-020: verify best-effort defer/cleanup completion, deterministic error aggregation, panic/halt unwinding, and exactly-once resource release.
- Normative Multi-Rule OIR test MR-OIR-021: verify OIR 1.1 defer records, scope activation/cleanup CFG lowering, fixed cleanup-arena capacity handling, Writer/Reader round-trip, and Verifier/Optimizer invariants.
- Separation of source-language syntax documentation from compiler project documentation.

### Open design contracts

The following items remain explicit design work; they should not be treated as implemented behavior:

- The runtime/ABI mechanism used to propagate aggregated Multi-Rule barrier failures (for example, error result, exception, or runtime trap).
- Final OIR opcode names for abstract error-accumulation operations.
- Exact binary/interchange format and versioning for each compiler-stage artifact.
- Final OCC linker format and security-validation contract.
- Remaining open language semantics called out in the Syntax Reference and feature-specific specifications.

## 4. Repository documentation

The repository separates normative language/compiler engineering documents from OCC/SDK usage guides.

| Area | Entry point |
|---|---|
| Language specifications | [docs/language](docs/language/README.md) |
| OIR and control-flow contracts | [docs/oir](docs/oir/README.md) |
| Compiler architecture and implementation | [docs/compiler](docs/compiler/README.md) |
| Runtime and concurrency | [docs/runtime](docs/runtime/README.md) |
| ABI and target design | [docs/abi](docs/abi/README.md) · [docs/compiler/targets](docs/compiler/targets/) |
| Security and ownership | [docs/security](docs/security/README.md) |
| SDK user documentation | [OCC / SDK documentation](omnicore-sdk/docs/README.md) |
| Project status | [Project status](docs/project/status.md) |

Normative language and compiler rules live under `/docs/`. SDK documentation explains how to use OCC and its libraries; it links to normative specifications instead of maintaining duplicate copies.

## 5. Development principles

1. **No assumed syntax:** compiler implementation must follow documented NEXUS Flow syntax; examples from other languages must not be presented as NEXUS Flow.
2. **Explicit semantics:** ambiguous behavior is recorded as an open question until a decision is made.
3. **Modular stages:** compiler stages communicate through defined intermediate artifacts.
4. **Verifier before optimization:** malformed OIR must be rejected before optimizer/codegen processing.
5. **Deterministic outputs:** equivalent inputs and compiler settings should produce structurally stable intermediate outputs.
6. **Own final linker:** external linkers may support experiments, but the final linker and its security layer belong to OCC.

## 6. Scope and status

This repository contains evolving design documents and implementation work. A documented architecture or pseudocode example does not by itself mean the corresponding compiler module is implemented or production-ready.

The project status should be updated here as modules become executable, tests are added, and specifications are finalized. Language-level syntax changes belong in the [NEXUS Flow Syntax Reference](docs/language/syntax-reference.md) and the relevant feature specification.


## Documentation structure

- `/docs/`: canonical NEXUS Flow language specifications, OIR contracts, compiler architecture, optimizer, runtime, ABI, target design, security, research and engineering records.
- `/omnicore-sdk/docs/`: user-facing OCC/SDK installation, CLI/build workflows, configuration, API/stdlib references, practical examples and troubleshooting.
- `/docs/archive/legacy/`: superseded or historical drafts retained for traceability. These files are not the current normative source unless explicitly identified as historical context.

The migration preserves file contents and blob identity. Identical duplicate copies were consolidated; distinct older revisions were retained in the legacy archive. Source code, build outputs, target definitions and SDK implementation directories were not broadly rearranged in this documentation pass.
