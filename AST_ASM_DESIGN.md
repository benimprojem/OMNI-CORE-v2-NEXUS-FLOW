# FASTEXEC / Inline ASM AST Contract (C frontend)

**Status:** AST node lifecycle and OIR bridge implemented; Lexer/Parser integration is in progress; tests are committed but not yet executed.
**Normative language source:** `nexus docs/Gelişmiş Inline Assembly — Formal Specification.md`.
**Working branch:** `refactor/c-core-roadmap`.

## 1. Execution model

`fastexec { ... }` is a dedicated execution-scope container for low-level primitive operations and inline ASM. It is not itself an ASM instruction.

The scope may contain ordinary Nexus Flow declarations and statements, inline ASM declarations (`asm: LABEL { raw target text }`), recognized CPU primitive operations, and future GPU/NPU primitive operations.

Low-level CPU/GPU/NPU primitive operations, `asmcall`, `asmjmp`, and ASM declarations are legal only inside a `fastexec` scope. The same constructs outside the scope must produce a source-positioned diagnostic. Ordinary Nexus Flow functions and statements are not made FASTEXEC-only merely because they appear in a FASTEXEC body.

FASTEXEC does not encode a fixed hardware kind in the AST. Semantic/target capability metadata will classify primitive operations (CPU now; GPU/NPU when defined) without changing the AST node schema.

## 2. AST contract (append-only)

The existing `AST_ASM_BLOCK` is already published as AST kind 29 / OIR kind 29 and has legacy `ASTBlock` payload semantics. Do not repurpose that ID or reinterpret old serialized streams.

Append the following C AST kinds after the current `AST_MUST_DECL` kind 103:

- `AST_FASTEXEC_BLOCK` (C kind 104; OIR kind 104): `ASTNode base; ASTNode *statements;` It owns the ordered statement chain in the FASTEXEC scope.
- `AST_ASM_LABEL` (C kind 105; OIR kind 105): `ASTNode base; char *label; char *source_text; size_t source_length;`. It owns both strings and has no AST child nodes. `source_length` is authoritative; `source_text` also has a trailing NUL for C APIs.

The current OIR registry omits three already-existing frontend kinds. Register `AST_VARIADIC_PARAM` as OIR 101, `AST_NAMED_ARG` as OIR 102, and `AST_MUST_DECL` as OIR 103 before adding FASTEXEC 104 and ASM_LABEL 105. No published OIR ID is renumbered or reused.

`asmcall(...)` and `asmjmp(...)` remain represented by the existing `AST_FUNC_CALL` shape with the intrinsic callee name. Semantic recognizes these intrinsic names and enforces FASTEXEC scope and label rules. Do not create redundant call-node kinds.

## 3. Lexer and Parser contract

The raw ASM body must not be lexed as ordinary Nexus Flow. The Lexer must emit a dedicated raw-body token/span after recognizing the `asm: LABEL {` prefix, preserving exact source bytes and avoiding false lexical diagnostics for target assembly syntax.

The Parser parses `fastexec { ... }` into `AST_FASTEXEC_BLOCK`, parses ordinary Nexus Flow statements inside its ordered statement list, parses `asm: LABEL { ... }` only within FASTEXEC and creates `AST_ASM_LABEL`, preserves label and exact raw body bytes, parses `asmcall` / `asmjmp` as existing call nodes, and rejects ASM declarations and low-level primitive calls outside FASTEXEC with precise diagnostics.

The raw-body scanner starts immediately after the outer `{` and returns one `TOKEN_ASM_BODY` span without consuming the matching outer `}`. Nested braces are balanced for target forms such as register lists; braces inside single/double quoted strings are ignored and backslash escapes are honored. Target comment syntax remains opaque; unmatched braces inside comments still participate in delimiter balancing. Unterminated bodies fail with source-positioned diagnostics. The stable OIR token ID for `TOKEN_ASM_BODY` is 164.

## 4. Ownership, codec and OIR mapping

- FASTEXEC owns exactly one ordered statement chain.
- ASM_LABEL owns exactly two strings and no child nodes.
- Both node kinds participate in AST factory sizing, printing, destruction, graph validation, binary AST serialization/deserialization, OIR frontend mapping, OIR writer/reader, field schema, and round-trip tests.
- OIR fields: FASTEXEC field 1 = statements (NODE_REF); ASM_LABEL field 1 = label (STRING_REF), field 2 = raw source text (STRING_REF).
- The AST graph validator must reject shared/cyclic child ownership as for all other nodes.
- Existing AST kind 29 and all prior AST/OIR IDs remain byte-compatible.

## 5. Semantic responsibilities

Semantic validates FASTEXEC-only context for ASM declarations, `asmcall`, `asmjmp`, and registered hardware primitive operations; duplicate labels within their containing FASTEXEC scope; unresolved ASM call/jump labels; argument shape and label spelling; primitive-operation availability for the selected target capability profile; and variable references/type compatibility used by `%name` and `%name:reg` substitutions.

CPU is the initial primitive family. GPU/NPU primitive families may be registered later without changing the AST container contract. Target-specific instruction validation/emission remains downstream; general scope, name, type, ownership and capability checks do not move into Codegen.

## 6. FASTEXEC compiler intelligence and conservative validation

FASTEXEC is a high-risk boundary. The compiler must perform stricter, context-aware checks here than in an ordinary statement block. This is a compiler responsibility, not a promise that arbitrary target assembly can be fully understood.

### 6.1 Validation layers

- **Lexer:** preserve the raw ASM body byte-for-byte and report unterminated bodies or malformed delimiters at their exact source positions. Do not interpret target mnemonics as Nexus Flow tokens.
- **Parser / AST:** preserve the FASTEXEC scope, statement order, ASM label, raw body, and source spans. Reject malformed FASTEXEC/ASM structure without silently dropping bytes.
- **Semantic:** resolve every `%name` reference against visible declarations; verify read/write intent, type and width compatibility, duplicate labels, `asmcall`/`asmjmp` targets, and FASTEXEC-only primitive use.
- **Target capability validation:** verify register names/classes, instruction-set availability, operand constraints, alignment and ABI requirements against the selected target profile. An unknown target capability must not be treated as supported.
- **OIR boundary:** carry the validated source structure and diagnostics forward. No backend may silently reinterpret or discard an invalid operand mapping.

### 6.2 Variable and register mapping analysis

For every placeholder, Semantic records a use/definition contract:

- Read: ASM consumes the current Nexus Flow value.
- Write: ASM produces a value that must be written back to the Nexus Flow variable.
- Read/write: both the incoming value and the outgoing result are observable.
- Address/memory use: the target backend must provide a valid addressable location and honor the target's memory constraints.

The exact direction must be inferred only where the operand syntax and target contract make it unambiguous. If the compiler cannot prove whether an operand is read, written, or both, it must issue a diagnostic or require an explicit supported constraint; it must not guess.

The compiler checks that a value is initialized before an ASM read, that writes target mutable storage, that the type width matches the operand/register width, and that two simultaneous mappings do not illegally alias the same register or storage. Register allocation must respect live values and preserve values across clobbers and calls. A variable mapped to a register is not automatically safe merely because the register name exists.

### 6.3 Control-flow and machine-state checks

At minimum, the semantic/target validation plan must cover:

- Duplicate and unresolved labels within the owning FASTEXEC scope.
- `asmjmp` paths that bypass required cleanup, leave the valid function/FASTEXEC region, or make subsequent code unreachable.
- ASM blocks with fall-through behavior that conflicts with their call/jump use.
- Register clobber declarations, implicit instruction clobbers, and live-value preservation.
- Stack or call-frame alignment, calling convention, callee/caller-saved register rules, and return-state consistency where the target ABI defines them.
- Memory access width/alignment, invalid or unavailable registers, unsupported instructions, and target privilege/capability restrictions where statically knowable.
- Conflicting or overlapping memory/register mappings and values whose lifetime crosses an ASM call or jump.

The compiler must not invent a return instruction, rewrite arbitrary assembly control flow, or claim to prove arbitrary raw ASM safe. Where static proof is impossible, emit a clearly classified warning or require an explicit future unsafe/opaque contract only after that language feature is specified. Do not add such syntax implicitly in this implementation.

### 6.4 Diagnostics policy

Diagnostics must identify the source file, FASTEXEC/ASM label, exact source span, variable/register/target involved, and a stable diagnostic code. Errors block OIR generation; warnings remain attached to the compilation result and do not disappear at module boundaries.

Suggested diagnostic families (stable numeric codes are assigned in the diagnostics registry, not here):

- Scope: ASM/primitive used outside FASTEXEC.
- Binding: unknown variable, uninitialized read, immutable write, unresolved or ambiguous placeholder.
- Constraint: register unavailable, width mismatch, illegal alias, invalid memory/address mapping.
- Control flow: duplicate/unresolved label, unsafe jump boundary, invalid fall-through/call-return contract, unreachable statements.
- Target/ABI: unsupported instruction/capability, calling-convention or alignment violation, clobber/live-register conflict.
- Opaque analysis: a warning when arbitrary raw ASM behavior prevents a complete static proof.

Diagnostics should be actionable: explain the failed invariant and, when safe, identify the declaration, operand, or target profile that needs correction. The compiler must never silently auto-correct an ASM instruction or register constraint.

## 7. Required tests / completion gate

1. FASTEXEC positive parse and AST shape.
2. Multiple ordinary statements and multiple ASM labels in one FASTEXEC.
3. ASM declaration outside FASTEXEC rejected.
4. CPU primitive and `asmcall` / `asmjmp` outside FASTEXEC rejected.
5. Future GPU/NPU primitive registration uses the same scope rule.
6. Raw ASM body preserves spaces, tabs, CRLF/LF, comments, percent placeholders and punctuation byte-for-byte.
7. Raw ASM body is not tokenized as Nexus Flow and creates no false Lexer errors.
8. Unterminated ASM body, missing/invalid label and malformed call produce source-positioned diagnostics.
9. AST destroy / validator / codec round-trip and malformed-stream tests.
10. OIR registry, field schema, frontend writer/reader round-trip and unsupported-node fail-closed tests.
11. Semantic duplicate-label, unresolved-label, scope and target-capability tests.

No part of FASTEXEC/ASM is considered implemented until the complete AST → Lexer/Parser → Semantic → OIR bridge test chain passes.