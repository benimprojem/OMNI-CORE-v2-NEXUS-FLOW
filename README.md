# OMNI-CORE Compiler (OCC) · NEXUS Flow

[![GitHub stars](https://img.shields.io/github/stars/benimprojem/OMNI-CORE-v2-NEXUS-FLOW?style=flat-square)](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/benimprojem/OMNI-CORE-v2-NEXUS-FLOW?style=flat-square)](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/forks)
[![Open issues](https://img.shields.io/github/issues/benimprojem/OMNI-CORE-v2-NEXUS-FLOW?style=flat-square)](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/issues)
[![Last commit](https://img.shields.io/github/last-commit/benimprojem/OMNI-CORE-v2-NEXUS-FLOW?style=flat-square)](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/commits/main)

**OCC** is the compiler/toolchain project for **NEXUS Flow**. This repository contains the language specifications, compiler architecture, intermediate-representation contracts, implementation sources, SDK materials and engineering notes.

> **Project status:** Active development. Specifications and implementation are evolving. A documented feature, source file or checked-in binary is not by itself proof that the complete feature is implemented or production-ready.

## What this project is building

OCC is intended to be a modular compiler rather than one monolithic executable. Compiler stages are designed to have explicit interfaces and independently testable outputs.

Core engineering goals:

- Define NEXUS Flow syntax and semantics precisely.
- Keep frontend, intermediate representation, verification, optimization and code generation as separate stages.
- Exchange versioned intermediate artifacts between modules.
- Reject malformed or unsupported intermediate data before optimization or code generation.
- Preserve deterministic behavior, source mapping and diagnosable failures.
- Own the final linking stage and its security/validation layer.

## Compiler pipeline

The target architecture is:

```text
NEXUS Flow source
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
OCC Linker (custom, with security validation)
       |
       v
Target executable / artifact
```

Each stage consumes a defined artifact and produces the next stage's input. The exact binary contracts, version rules and module ABI are specified separately; the diagram describes the intended architecture, not a claim that every stage is already complete.

A system C linker may be used during early experiments. It is **not** the intended final linker.

## Phase 1 progress — NEXUS Flow AST

The live Phase 1 AST coverage dashboard is available at [NEXUS Flow AST Coverage Dashboard](NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md). It tracks Lexer, Parser, AST/Payload, Semantic, OIR lowering and Verifier coverage, including open work and evidence checkpoints. Status marks are evidence-based; partial or unverified work is not treated as complete.

## Current engineering focus

Recent specification work has concentrated on control flow and reliable scope cleanup:

- Basic blocks, explicit control-flow terminators and CFG verification.
- Forward and backward `jump(label)` transfers, including cycles and multi-predecessor targets.
- SSA/block-parameter consistency across control-flow edges.
- Nested loops, `break`, `continue` and lexical-scope-aware jump cleanup.
- `defer` registration, live lexical bindings, ownership transfer and LIFO cleanup.
- Best-effort cleanup when a deferred action fails, including deterministic error collection.
- OIR 1.1 metadata and Writer/Reader/Verifier/Optimizer invariants for defer-bearing artifacts.

The Multi-Rule Engine specifications also define barrier aggregation, trigger collection, handler ordering and sequential RuleSet application. The numbered MR-OIR tests are documented in the [Multi-Rule Engine specification](docs/language/multi-rule-engine.md).

## Repository guide

| Location | Purpose |
|---|---|
| [`docs/language/`](docs/language/README.md) | NEXUS Flow syntax and language semantics |
| [`docs/oir/`](docs/oir/README.md) | OIR format, CFG/SSA, verification and cleanup contracts |
| [`docs/compiler/`](docs/compiler/README.md) | Compiler modules, implementation and engineering reports |
| [`docs/runtime/`](docs/runtime/README.md) | Runtime, scheduling and concurrency |
| [`docs/abi/`](docs/abi/README.md) | ABI and target calling conventions |
| [`docs/security/`](docs/security/README.md) | Ownership, safety and validation |
| [`docs/architecture/`](docs/architecture/README.md) | Cross-cutting architecture documents |
| [`docs/research/`](docs/research/README.md) | Exploratory research and comparisons |
| [`omnicore-sdk/docs/`](omnicore-sdk/docs/README.md) | SDK and toolchain usage documentation |
| [`docs/project/`](docs/project/status.md) | Project status and implementation notes |
| [`docs/archive/legacy/`](docs/archive/legacy/) | Historical drafts; not the current normative source |

The `docs/` tree is the canonical engineering reference. SDK guides explain usage and link back to the specifications instead of maintaining competing copies.

## Build scripts and implementation caution

The SDK currently contains separate Windows batch scripts for the OCC executable, parser DLL and Analyzer DLL:

- `omnicore-sdk/build_occ.bat`
- `omnicore-sdk/parser_build.bat`
- `omnicore-sdk/analyzer_build.bat`

These are component-level build entry points, not a declaration that a complete, unified end-to-end compiler build is available. Check each script and the implementation-status documents before relying on a particular stage.

## License

The repository includes the custom **NexusFlow Alt Yapı ve Derleyici Lisansı (N-AAL)**. Read [LICENSE](LICENSE) before copying, modifying, redistributing or using any part of the project. Do not assume that public visibility means unrestricted commercial-use permission.

## Documentation principles

1. **Use documented NEXUS Flow syntax.** Examples from other languages must not be presented as NEXUS Flow.
2. **Make implementation-friendly decisions explicit.** When a semantic choice is needed, select and document a coherent rule; unresolved items must be clearly identified rather than silently guessed.
3. **Keep module boundaries explicit.** Compiler stages exchange versioned, validated artifacts.
4. **Verify before transforming.** Malformed OIR must be rejected before optimizer or code-generator processing.
5. **Separate specification from implementation status.** A design contract is not an implementation claim.

---

For language rules, start with the [NEXUS Flow Syntax Reference](docs/language/syntax-reference.md). For compiler internals, start with the [OIR specification](docs/oir/occ-oir-specification.md) and [compiler documentation](docs/compiler/README.md).
