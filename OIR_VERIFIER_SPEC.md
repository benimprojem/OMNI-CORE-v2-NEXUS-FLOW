# OIR Verifier Specification

**Document ID:** OC2-OIR-VERIFIER-001  
**Status:** Accepted design; initial independent stage implemented; extended validation remains in progress  
**Date:** 2 November 2026  
**Related:** [OIR_MODULE.md](OIR_MODULE.md), [OIR_FORMAT.md](OIR_FORMAT.md), [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md), [OCC_MODULE_ABI.md](OCC_MODULE_ABI.md)

## 1. Purpose

The OIR Verifier is an independent compiler stage that checks whether an `OIR_MODULE` artifact satisfies the structural, control-flow, SSA and type rules defined by the active OIR payload schema.

The verifier is a validation boundary between IR lowering/optimization and target-specific code generation. It does not repair, rewrite or silently normalize invalid OIR.

## 2. Pipeline contract

The required stage order is:

```text
Semantic AST
    |
    v
OIR Lowering
    |
    v
OIR Verifier
    |
    +---- if optimization is not requested ----+
    |                                          |
    |                                          v
    |                                        Codegen
    |
    +---- if --o1 / --o2 / --o3 / --o4 --------+
                         |
                         v
                   OIR Optimizer
                         |
                         v
                    OIR Verifier
                         |
                         v
                       Codegen
```

The first verification is mandatory. The post-optimization verification is mandatory whenever an optimizer transformation has produced a new OIR artifact. With no optimizer level specified, OCC skips optimization; there is no need to write an explicit `--o0` option.

## 3. Module identity and ABI

The verifier is an independent shared module:

- Module: `occ_oir_verify_stage`
- Module ID: `0x00010006`
- Stage ID: `OCC_STAGE_OIR_VERIFY`
- Common ABI: `OccStageModuleV1`
- Export: `occ_get_stage_module_v1()`

The verifier MUST use the shared OCC stage-module ABI documented in `OCC_MODULE_ABI.md`. It MUST NOT introduce a parallel verifier-specific public ABI such as a separate `OIRV_Verify()` entry point, report handle, allocator contract or export symbol.

The host owns input and output artifact buffers. The module may use private temporary allocations during processing, but must release them before returning and must not retain caller pointers. Diagnostics are sent through the invocation-scoped host diagnostic interface. The output is returned through the caller-owned output buffer and standard stage result.

Every public ABI structure follows the common ABI versioning and `struct_size` rules. ABI compatibility is checked by the OCC loader before invocation.

## 4. Input and output

### Input

Exactly one OIR container carrying:

- Artifact kind: `OIR_KIND_MODULE` (stable kind ID 5)
- Payload schema: OIR_MODULE 1.1 for the currently implemented verifier
- Validity: `VALID`
- Matching compilation job identity

**Current implementation restriction:** the verifier currently accepts only `OCC_STAGE_OIR_LOWERING` as the producer stage. When the Optimizer is integrated, this input gate must be extended to accept only explicitly registered OIR-producing stages, including the Optimizer stage. It must not accept arbitrary producer IDs.

The verifier independently validates the envelope and payload. A host-provided artifact-kind hint is not a substitute for reading the artifact.

### Output

One OIR container carrying the same OIR_MODULE payload and sections, with producer-stage metadata identifying the verifier stage. The verifier must not silently repair or mutate the module.

The host validates the output envelope, kind, schema, producer stage, validity and job identity before passing it to Codegen or another consumer.

## 5. Validation layers

Validation is dependency-aware. A prerequisite failure must prevent unsafe downstream interpretation, while independent checks may continue when their inputs remain safe. Skipped checks must not be reported as successful.

### 5.1 Input and envelope validation

- OIR magic, container version and header size
- Artifact kind and payload schema version
- Total length, section bounds and required payload count
- Artifact validity and producer/job identity
- Unsupported required flags or sections
- Payload count, table bounds and string-table bounds

### 5.2 Structural validation

- Module table consistency and bounded counts
- Unique and valid function, block, value and instruction IDs
- Function/block ownership
- Entry-block ownership and validity
- Complete, non-overlapping instruction ranges
- Instruction-to-block ownership
- Valid operand references and opcode shape
- Required terminator placement and block termination

### 5.3 CFG validation

- Branch targets exist
- Edges remain within the owning function
- Predecessor/successor relationships are consistent
- Conditional branch operands have the required type and shape
- Terminators are valid and no ordinary instruction follows a terminator
- Reachability is distinguished from structural validity

### 5.4 SSA validation

- Every SSA value has one valid definition
- Every use references a defined value
- Same-block definitions precede their uses
- Cross-block definitions dominate their uses
- Unreachable blocks are not treated as reachable dominators
- Values whose dominance cannot be proven are rejected

### 5.5 Type validation

- Opcode operand and result types are compatible
- Boolean conditions are boolean
- Scalar operation types match the active opcode contract
- Invalid signedness/type-kind combinations are rejected
- Block-parameter and function-signature checks are added when those schema features are introduced

The verifier must enforce only semantics represented by the active OIR schema and registered opcode contracts. It must not invent missing function signatures, conversions, memory effects or language promotion rules.

## 6. Diagnostics and failure behavior

Source/IR validity failures are reported as diagnostics through the shared OCC diagnostic sink. They are distinct from infrastructure failures such as allocation failure, invalid invocation, incompatible ABI or internal verifier failure.

**Diagnostic code ranges (stable within the OIR Verifier):**

| Code | Meaning |
|---:|---|
| 8601–8606 | Artifact envelope, section, output-size and serialization failures |
| 8607 | Module resource limit or size arithmetic bound |
| 8608 | Verifier workspace allocation failure |
| 8609 | Malformed OIR payload / registered opcode wire-shape rejection |
| 8610 | Type record |
| 8611 | Block ownership or block instruction-range metadata |
| 8612 | Instruction coverage, overlap or instruction/block identity |
| 8613 | Function type or entry-block ownership |
| 8614 | SSA definition record |
| 8615 | Opcode operand shape / result contract (defensive verifier check; the OIR codec also checks this contract) |
| 8616 | Opcode operand/result type or implemented opcode semantics |
| 8617 | CFG edge and cross-function target ownership |
| 8618 | Missing or misplaced block terminator |
| 8619 | Reachability / dominance analysis |
| 8620 | SSA use ordering, undefined use or non-dominating definition |
| 8621 | Warning: defined SSA value has no uses |
| 8622 | Note: additional unused-value warnings omitted |
| 8623 | Additional invalid type-record errors omitted |
| 8624 | Additional invalid SSA-definition errors omitted |

Validation diagnostics include numeric function, block, instruction, value and type identifiers in the message when the failing record makes them available. These are message context fields; the common OCC ABI remains unchanged. The shared diagnostic `hint` field carries two kinds of guidance where applicable: (1) which dependent validation passes were skipped after a prerequisite failure, and (2) a code-specific suggested correction beginning with `Suggested fix:`. Suggestions are advisory and describe the relevant OIR invariant; they do not rewrite the artifact or claim that a repair was applied. For example, an SSA-use diagnostic recommends defining the value earlier in the same block or in a dominating block, while a function-entry diagnostic recommends using a valid function type and an entry block owned by that function. Infrastructure failures remain distinct from invalid-source/IR diagnostics. The verifier aggregates independent invalid type records before entering structural validation. It reports up to 32 individual type-record errors (code `8610`) with their type IDs; if more exist, code `8623` reports the omitted count. After type validation succeeds, it independently checks SSA value records and reports up to 32 malformed definitions (code `8614`) with their value IDs; code `8624` reports any omitted remainder. If either prerequisite table fails, dependent structural/SSA/opcode/type/CFG/terminator/dominance/use passes are skipped. Instruction-to-value reverse consistency and later semantic passes remain fail-fast until separately proven safe to aggregate.

Diagnostics should include, where available:

- Stable diagnostic code and severity
- Module/function identity
- Block ID
- Instruction ID
- SSA value ID
- A concise description

The verifier should collect all independent safe-to-check errors. If a structural failure makes a dependent analysis unsafe, that analysis is skipped and the report must not imply that it passed.

A verification failure must prevent the invalid artifact from reaching optimization or Codegen. A module execution failure is returned through the common stage status/result contract.

## 7. Immutability and determinism

- The input artifact is read-only.
- No repair, implicit conversion, reordering or canonicalization is performed by the verifier.
- The output preserves the original OIR sections and payload.
- Equivalent inputs and options produce deterministic diagnostics and output metadata, subject to the common job/provenance fields.
- The verifier is reentrant/stateless as declared by its module descriptor; invocation-local workspaces are not shared between jobs.

## 8. Resource limits

Current implementation limits (not language limits):

- Maximum 8,192 blocks per module
- Maximum 1,000,000 function records
- Maximum 1,000,000 instruction records
- Maximum 1,000,000 value records
- OIR string table maximum 16 MiB

The implementation must reject out-of-bound inputs before unsafe allocation or indexing. Limits may change only with corresponding tests and documentation.

## 9. Current implementation status

Implemented and tested:

- Independent `occ_oir_verify_stage` shared module using the common stage ABI
- OIR_MODULE 1.1 envelope and payload validation
- Function/block ownership and entry-block validation
- Complete non-overlapping instruction-range validation
- Dependency-ordered internal validation passes: independent type-record validation, independent SSA value-record validation, structural records, SSA definition cross-checks, opcode/type plus CFG-edge checks, terminators, dominance/reachability and SSA uses
- Terminator, same-function CFG edge, opcode-shape and implemented scalar type checks
- Unique SSA definitions, same-block use order and cross-block dominance
- No silent repair or mutation; regression test proves input-buffer immutability and byte-for-byte preservation of payload plus metadata section data; verifier-produced artifact required by Codegen
- Advisory unused-SSA-value warnings are emitted only after all validity passes succeed; the Verifier still writes a valid OIR artifact, while the existing strict compilation warning gate may suppress final publication
- Independent type-record failures are aggregated (up to 32 individual diagnostics); overflow is reported with code 8623, and dependent validation passes are skipped
- Negative tests for cross-function CFG edges, incorrect entry ownership, missing terminators, duplicate SSA definitions, use-before-definition and non-dominating definitions
- Windows, Ubuntu and macOS CI; Ubuntu ASan/UBSan and Valgrind
- Current input producer gate accepts lowering-stage artifacts only; optimizer-stage producer support is pending
- Stable verifier diagnostic codes 8607–8624 and numeric function/block/instruction/value/type context are implemented; representative exact-code regression cases are covered; warning code 8621 reports defined-but-unused SSA values with value ID context

Recorded CI runs: `41375777590` and `41382501057` (59/59 CTest in the recorded Ubuntu run); `41387462122` validates the six-pass decomposition across Windows, Ubuntu and macOS, with Ubuntu ASan/UBSan and Valgrind successful; `41389527526` validates the diagnostic-code regression suite across all three platforms and passes Ubuntu ASan/UBSan plus Valgrind; `41389854136` validates multi-section payload preservation and input immutability across all three platforms and passes Ubuntu ASan/UBSan plus Valgrind; `41390062454` validates exact diagnostics for type records, instruction coverage and opcode/result type mismatches across all three platforms, with Ubuntu ASan/UBSan and Valgrind successful. The cleaned test-source revision `41390126322` also passes Windows, Ubuntu and macOS build/test, Ubuntu ASan/UBSan and Valgrind.

Not yet implemented because the corresponding OIR schema features are not yet available:

- Full function signatures and call validation
- Block parameters / phi-style merge validation
- Explicit memory metadata and effect validation
- Complete source-map validation
- Full language-wide conversion and numeric-promotion validation


## 9.1 Diagnostic severity and advisory warnings

The common OCC diagnostic collector distinguishes notes, warnings, errors and fatal diagnostics. Warnings are advisory diagnostics, not errors: they are retained and surfaced to the caller with warning severity. The current compilation policy applies a strict warning gate, so any warning can suppress final output publication even though the stage itself may successfully produce a valid intermediate artifact. Errors and fatal diagnostics also block output. Collector allocation failure or diagnostic-limit exhaustion remains a host-side failure and blocks output.

The Verifier currently emits warning `8621` when an SSA value has a valid definition but no uses, capped at 32 individual warnings per module. Independent invalid SSA value records are also aggregated (code `8614`, up to 32 records); omitted records are counted by code `8624`. The message identifies the SSA value ID; the hint recommends removing the corresponding source declaration when the value represents an unused source variable, or removing the dead computation. If more than 32 values are unused, note `8622` reports how many additional warnings were omitted. Because OIR currently has no reliable source-variable name mapping in this pass, the diagnostic deliberately says “SSA value” rather than guessing a source identifier. This warning runs only after structural, definition, opcode/type, CFG, terminator, dominance and SSA-use validation succeeds. It never changes the artifact.

## 10. OCC optimizer profile policy

Optimizer selection is controlled by OCC command-line options:

| Option | Profile |
|---|---|
| no optimizer option | Optimization skipped |
| `--o1` | Basic optimization |
| `--o2` | Standard optimization |
| `--o3` | Advanced optimization |
| `--o4` | Aggressive optimization |

There is no required `--o0` option. If no optimization option is supplied, OCC does not invoke the Optimizer stage.

Only one optimization level may be selected per compilation. Repeated or conflicting optimization-level options are rejected as a CLI argument conflict. Each profile selects a defined set/order of passes. Higher levels permit more costly transformations; they do not promise faster output in every workload.

Optimization must preserve observable program behavior. The OIR Verifier runs after any optimizer-produced artifact and before Codegen. These CLI/profile rules are an accepted design decision; implementation and tests are tracked separately in `TASKS.md`.

## 11. Required test matrix

- Valid OIR_MODULE acceptance
- Invalid envelope, unsupported version and malformed section rejection
- Invalid IDs, references, ownership and table ranges
- Missing/misplaced terminators
- Cross-function CFG edge rejection
- Duplicate SSA definition and undefined use rejection
- Same-block use-before-definition rejection
- Valid dominating cross-block use acceptance
- Diamond-CFG non-dominating use rejection
- Type mismatch and invalid condition rejection
- Resource-bound and overflow cases
- Deterministic diagnostics and exact verifier error codes/context for representative malformed OIR cases
- Input immutability / output payload preservation
- Windows, Linux and macOS builds
- ASan/UBSan and Valgrind where available
- OCC pipeline test proving invalid OIR cannot reach Optimizer or Codegen
- Optimizer integration test proving post-pass verification is mandatory and accepts only the registered Optimizer producer stage

## 12. Evolution rule

When OIR_MODULE adds function signatures, block parameters, memory/effect metadata, conversions or source maps, this specification and the verifier tests must be extended before those features are accepted by downstream Codegen. Unknown executable semantics must fail closed.
