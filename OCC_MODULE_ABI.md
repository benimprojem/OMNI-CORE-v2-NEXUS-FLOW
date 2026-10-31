# OCC Shared-Module C ABI — Draft v1

**Status:** Draft; not frozen  
**Header:** `omnicore-sdk/src/c/module/occ_module_api.h`  
**Purpose:** Common in-process ABI for independently loaded compiler-stage DLL/SO/dylib modules

## 1. Boundary rule

A module boundary carries only:
- Fixed-width scalar values.
- Pointers to caller-owned byte spans.
- Versioned ABI structs containing no compiler-internal AST, Token, ParserContext, semantic symbol, allocator or C++ object layout.

The bytes in an artifact span are a complete OIR container. A module must validate the OIR envelope, artifact kind, schema version, validity and payload before consuming it. It must not trust a host-provided kind hint in place of parsing the artifact header.

The host owns every input and output buffer. A module may allocate private working memory internally, but it must release that memory before returning. It must never return a pointer to module-owned memory or retain an input/output pointer after the call.

## 2. Exported symbol

Every module exports exactly:

`occ_get_stage_module_v1()`

It returns a pointer to a static `OccStageModuleV1` descriptor whose lifetime lasts until the library is unloaded. The host calls the module's `process` function for each job invocation.

The exported symbol uses the platform C calling convention. No C++ name mangling, exceptions, STL types or runtime-owned objects cross this boundary.

## 3. Versioning

- ABI major must match exactly.
- A module may use an ABI minor no greater than the host's supported minor.
- Every public struct begins with `struct_size`; readers must check the minimum known prefix before accessing fields.
- New fields may only be appended in a compatible minor revision. Existing field meanings and offsets must not change within a major version.
- Reserved fields and invocation flags are zero in v1.0.
- A breaking field/layout or ownership change requires a new ABI major and exported symbol.

The header is a draft implementation of these rules. Its layout and exported symbol are not frozen yet.

## 4. Module descriptor

`OccStageDescriptorV1` declares:
- ABI and module semantic versions.
- Stable module ID (zero is unassigned during the draft).
- Stable compiler stage ID.
- Accepted input artifact-kind bitmask.
- Required input artifact-kind bitmask.
- Output artifact kind.
- Minimum and maximum input count.
- Capabilities: reentrant, deterministic, partial-input aware and stateless.
- Static module name.

Artifact-kind bit N is `1ULL << N`, where N is the stable OIR artifact kind ID. The accepted mask is a set; it does not express ordering or multiplicity. A stage must validate its exact input roles and counts itself. Required kinds must all be present.

Initial parser-stage descriptor direction:
- Required: one SOURCE_UNIT and one TOKEN_STREAM.
- Optional: a DIAGNOSTICS section may be carried inside an input OIR container; it is a section type, not a standalone artifact kind.
- Output: AST.
- Partial-input support is not enabled until parser recovery semantics are explicitly tested.

## 5. Invocation and result

`OccModuleInvocationV1` contains a borrowed array of OIR byte spans, one caller-owned output buffer, its 64-bit capacity, job ID and reserved flags.

`OccModuleResultV1` reports:
- Bytes written.
- Required output capacity when the module returns NEED_OUTPUT_BUFFER.
- Diagnostic count.
- Output validity.
- Stable module-specific failure code.

Status behavior:
- `OK`: one complete VALID output artifact was written.
- `PARTIAL`: one PARTIAL artifact was written and diagnostics explain omissions/recovery.
- `NEED_OUTPUT_BUFFER`: no output bytes are committed; bytes_written is zero and required_output_capacity is set. The host may retry with a larger buffer.
- `FAILED`: no output artifact is committed.
- `CANCELLED`: the invocation was cancelled; no output artifact is committed.
- `INVALID_INVOCATION`: ABI struct sizes, pointers, counts or reserved fields are invalid.

The host must independently validate every returned OIR artifact. A module status is not a substitute for validating its output.

## 6. Concurrency and lifetime

A module may be called concurrently only if it declares REENTRANT. Stateless modules may declare STATELESS. The host must serialize calls otherwise.

Input bytes, input array and output buffer remain valid only for the duration of `process`. A module may not retain them. The host must not unload a module while calls are active.

No allocator callback is part of v1.0. This deliberately avoids cross-CRT allocation/free ownership on Windows. The module allocates and frees its internal AST, token and scratch objects within its own runtime, then writes the completed OIR container into the host's buffer.

## 7. What remains before ABI freeze

1. Assign and document stable module IDs.
2. Confirm parser invocation inputs against SOURCE_UNIT, TOKEN_STREAM and DIAGNOSTICS artifact wrappers.
3. Define stage-specific diagnostic behavior and whether diagnostics are embedded in the output artifact or returned as a companion artifact.
4. Add a sample parser-stage shared library using this header.
5. Add dynamic load/unload tests on Windows, Linux and macOS.
6. Add ABI prefix-size, wrong-major, too-small-output and concurrent-invocation tests.
7. Review the header and this document before declaring ABI 1.0 frozen.

The draft does not yet claim that lexer, parser or semantic analysis are independent shared libraries.


## 8. Host-side draft validators

`occ_module_validate_descriptor_v1()` checks the module/descriptor size prefixes, ABI major/minor compatibility, reserved fields, process pointer, nonzero stage ID, output artifact kind, input count range, accepted/required mask consistency and module name. Module ID zero remains legal while the registry is still a draft, as specified above.

`occ_module_validate_invocation_v1()` validates invocation/result prefixes, count and pointer consistency, zero v1 flags/reserved values, and opens every input as a complete OIR container. It derives each input kind from the OIR header, checks the accepted-kind mask, and verifies that every required kind is present. It never trusts a caller-supplied artifact-kind hint.

These are host-side validation helpers, not a dynamic loader and not yet a stage implementation. The module must still validate stage-specific input roles, schemas and validity states, and the host must validate the produced output artifact independently.

CTest: `occ_stage_module_abi_validation_helpers` supplements the original layout/process contract test. The new fixture checks a real OIR AST container as input and rejects incompatible ABI, inconsistent masks, null bytes, nonzero input flags and missing required input.


## 8. Draft implementation status

Implemented:
- `occ_module_api.h`: fixed-width, versioned C structs, module descriptor, borrowed OIR input spans, caller-owned output buffer, status/result contract and export macro.
- `occ_module_api.c`: host-side descriptor and invocation validation. It checks ABI version/struct sizes, module/stage IDs, known artifact-kind masks, required-versus-accepted masks, capability bits, input count, each complete OIR container, validity/partial-input policy and job-ID consistency.
- `test_module_abi.c` and `test_occ_module_api.c`: descriptor, input kind, required inputs, job identity, partial input, reserved flags and output-buffer retry behavior.
- Test-only shared library `occ_test_passthrough_module`: exports `occ_get_stage_module_v1`, validates one AST artifact, and copies it into the caller's output buffer without transferring memory ownership.
- `test_dynamic_stage_module.c`: loads the shared library with LoadLibrary/GetProcAddress or dlopen/dlsym, checks the descriptor, tests NEED_OUTPUT_BUFFER retry, validates the returned OIR artifact and unloads the library.

The test module is deliberately a pass-through ABI fixture, not the real parser stage. The parser now has an in-memory source initialization API, and the OIR AST artifact adapter is ready, but the actual parser-stage shared library that consumes SOURCE_UNIT + TOKEN_STREAM and emits AST is still pending.

The ABI remains **Draft v1.0, not frozen**. Module IDs are test values until a central registry is assigned. Dynamic-loader CI run 41161737825 passed on Windows, Ubuntu and macOS; Ubuntu CTest 34/34, ASan/UBSan and Valgrind passed.
