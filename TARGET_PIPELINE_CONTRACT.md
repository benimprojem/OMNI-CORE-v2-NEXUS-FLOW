# Target Pipeline Contract

**Status:** Architecture contract; runtime integration is staged  
**Related:** [TARGET_MODEL.md](TARGET_MODEL.md), [OIR_MODULE.md](OIR_MODULE.md), [OIR_LOWERING_PLAN.md](OIR_LOWERING_PLAN.md)

## 1. Core rule

The selected target descriptor (or the resolved default descriptor) is a first-class compilation input. OCC must load, parse and validate it before any target-sensitive compilation stage runs. A stage must not silently fall back to hard-coded host or backend defaults when the descriptor is missing, malformed or unsupported.

The descriptor is authoritative for target properties such as OS, architecture, bitness, endianness, ABI, instruction-set profile, memory model, register constraints, output format and target-specific NexusFlow primitives.

## 2. Target selection and resolution

Selection order:

1. An explicit command-line target selection (for example, `--target Linux64`) wins.
2. Otherwise, a project/build configuration target selection is used when that integration is available.
3. Otherwise, OCC resolves a default target from the host OS and architecture using an explicit target mapping.
4. If no matching default descriptor exists, compilation stops with a diagnostic. OCC must not silently choose an unrelated target such as BareMetal64.

A selected target name resolves to one descriptor under the configured target search paths. A future CLI may accept an explicit descriptor path; path handling must remain bounded and must not allow untrusted project metadata to escape approved target roots without an explicit user policy.

The resolved descriptor identity and its content/version fingerprint are recorded in the compilation job so logs, caches and downstream artifacts can identify the exact target input.

## 3. Target-aware stage contract

The normalized target descriptor is passed as a separate, validated stage input to every stage that makes target-dependent decisions.

Target-sensitive stages include:

- Target-aware conditional compilation and macro selection (for example, target predicates used by NexusFlow source directives).
- Target capability validation for requested intrinsics, ISA features and platform facilities.
- Target-specific lowering only where a language construct explicitly requires a target-dependent expansion.
- Backend instruction selection, register allocation, calling-convention lowering and object emission.
- Link planning and output-format selection.

A stage that does not need target data should not receive or depend on it. In particular, canonical generic OIR remains target-neutral: it records language-level operations, types, control flow and effects, not physical register names, target stack offsets or host-specific ABI details.

If target-aware conditional compilation removes or selects source branches, that decision must occur before the resulting semantic representation is frozen. The chosen target identity must remain attached to the compilation provenance.

## 4. Normalized target artifact

The OCC host loads the `.target` text file through the target parser and produces a versioned, bounded target descriptor artifact for module handoff.

The artifact must contain normalized values rather than a raw file path or pointers into parser memory. It must preserve at least:

- Descriptor schema version and stable identifier.
- OS, architecture, bitness and endianness.
- ABI and output format.
- Instruction-set feature profile and declared target capabilities.
- Memory model values used by code generation.
- Register classes/reservations and calling-convention data.
- Target-specific primitive mappings, represented as validated data rather than executable instructions.

The source path and content fingerprint are provenance metadata only. A consumer must not reopen an arbitrary path supplied inside an artifact.

The target artifact has its own schema/version and validation rules. It is not embedded as target-specific fields in the generic OIR_MODULE payload.

## 5. Fail-closed capability checks

A syntactically valid descriptor does not prove that a backend exists for it. Before code generation:

1. Resolve the backend registered for the target architecture and output format.
2. Verify that the backend supports the requested ABI and required feature profile.
3. Verify every target-specific intrinsic or instruction requirement.
4. Reject unknown required features and unsupported combinations with a structured diagnostic.

A backend may support a conservative subset of a descriptor's declared features. It must never emit an instruction merely because the descriptor names that feature.

## 6. Determinism and cache identity

For identical source inputs, compiler options, normalized target descriptor and compiler/backend versions, target-sensitive outputs must be deterministic.

Compilation cache keys must include the normalized target descriptor identity/fingerprint and all target-sensitive options. Changing the target file must invalidate affected target-sensitive stages even when its filename and identifier remain unchanged.

## 7. Current implementation gap

The current OCC CLI accepts `--target`; when omitted, it now selects a host default for Windows x86-64 (`Win64`), Linux x86-64 (`Linux64`) and macOS AArch64 (`macOS_AArch64`). Other host combinations fail with a request for an explicit target instead of silently falling back to BareMetal64. This initial mapping is not yet project-configurable and does not cover every host architecture. The legacy codegen module loads and validates the selected descriptor, but the frontend pipeline does not yet receive a normalized target artifact. Target-aware conditional compilation handoff, target artifact serialization and target fingerprinting remain implementation tasks.

This document defines the intended contract; it does not claim those integrations are complete.
