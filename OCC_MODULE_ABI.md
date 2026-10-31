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
