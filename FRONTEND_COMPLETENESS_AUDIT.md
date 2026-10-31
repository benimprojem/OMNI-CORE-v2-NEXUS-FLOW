# Frontend Completeness Audit — Lexer, Parser, AST and Semantic Stage

**Status:** Audit plan and initial findings; implementation inventory not yet complete  
**Branch:** `refactor/c-core-roadmap`  
**Purpose:** Establish a verified source-language frontend contract before implementing production IR lowering.

## 1. Why this audit is required

The current frontend has grown incrementally. The AST tag list includes a broad set of language constructs, but the presence of a token or AST enum value does not prove that the lexer, parser, AST payload, semantic checker, serializer, artifact validator and tests all implement that construct consistently.

The compiler must not treat a declared enum value as evidence of a completed feature. Every language feature needs a traceable path from source spelling to a validated semantic artifact. Missing, partially implemented or inconsistent paths must be fixed before the feature is admitted to OIR lowering.

This audit is independent of the legacy AST-based code generator. The target is a clean, artifact-driven frontend whose output can be consumed by the new SSA lowerer.

## 2. Required feature traceability

For each language construct and operator, record these layers separately:

1. **Lexical definition:** token ID, spelling, precedence-independent lexical behavior, comments/strings/escapes and source span.
2. **Parser rule:** grammar production, associativity/precedence, ambiguity resolution and recovery behavior.
3. **AST representation:** node tag, complete payload definition, child ownership, list representation and source provenance.
4. **AST wire codec:** stable node tag, field schema, bounded serialization/deserialization and validation.
5. **Semantic behavior:** name/scope resolution, type rules, conversions, value category, mutability, ownership and diagnostics.
6. **Semantic artifact:** required annotations/relations/conversions and cross-reference validation.
7. **OIR lowering:** explicit supported mapping, or explicit unsupported diagnostic.
8. **Tests:** lexer, parser, AST round-trip, semantic positive/negative, artifact mutation, integration and cross-platform tests.

A feature is **complete** only when all required layers are implemented and tested. A feature may be marked **syntax-only**, **semantic-pending**, **artifact-pending**, **lowering-pending** or **unsupported**; do not label it complete based only on parser acceptance.

## 3. Initial observations requiring verification

The current AST header declares a large set of tags, including declarations, expressions, control flow, handler/flow operations, concurrency and hardware-facing operations. The visible payload declarations cover only a subset of these tags directly. Some tags may intentionally share a payload layout, but that must be explicitly documented and checked in parser construction, AST serialization and AST validation.

The semantic result model already has stable tables for scopes, types, symbols, annotations, relations and conversions. However, the current semantic checker does not yet provide complete durable annotations for every AST kind. Its expression typing helper handles a limited set of expression categories; control-flow and specialized NEXSUS Flow constructs need a more systematic semantic contract.

The lexer and parser are independent pipeline stages, but their end-to-end coverage has not yet been established by a complete feature matrix. Existing tests must be mapped to feature IDs rather than assumed to cover every declared token or AST tag.

These are audit findings, not a declaration that every unlisted construct is broken. Each must be confirmed against implementation and tests.

## 4. Audit work packages

### A. Lexer

- Enumerate every token ID and its source spelling.
- Compare token definitions with lexer dispatch and parser consumption.
- Check longest-match behavior for overlapping operators (including multi-character NEXSUS Flow operators).
- Check numeric literal forms, separators, exponent notation, suffixes, forced-width markers and malformed-number diagnostics.
- Check string/character escapes, UTF-8 policy, comments, whitespace and end-of-file behavior.
- Verify byte offsets and line/column spans for CRLF, LF, tabs and multibyte UTF-8.
- Check that every token stream is deterministic and has explicit bounds.
- Add positive and negative tests for every token family.

### B. Parser and grammar

- Build a grammar inventory from parser functions and token use.
- Verify precedence and associativity for every unary, binary, assignment, range, flow and conditional operator.
- Check declaration forms, type syntax, generic/aggregate forms if present, function parameters/returns and module imports.
- Check all control-flow forms, labels/jumps and loop variants.
- Check NEXSUS-specific handler, flow, area/zone, ownership and intent syntax independently; do not map them to generic syntax without a language rule.
- Verify parser recovery does not silently discard valid following statements.
- Verify every successful parse has a complete AST and every rejected construct has a useful source diagnostic.

### C. AST model and codec

- Create a registry table for every AST tag and its exact payload layout.
- For each tag, specify required/optional child fields, scalar fields, string ownership and linked-list semantics.
- Identify tags without an explicit payload type and document shared layouts where intentional.
- Validate child-pointer ownership and destruction symmetry to avoid leaks/double frees.
- Ensure serializer and deserializer cover every registered tag and reject unknown required tags.
- Ensure the wire format is field-by-field and independent of native pointer size, enum width, padding and endianness.
- Preserve stable node IDs and source spans through AST artifact round-trip.
- Add malformed-input, truncation, excessive-depth and invalid-child-reference tests.

### D. Semantic analysis and semantic artifact

- Require an annotation or explicit non-value-node classification for every AST node that affects executable semantics.
- Record resolved symbol IDs for every identifier use, including nested/member/index/call contexts.
- Record canonical types for every value-producing expression.
- Define lvalue/rvalue/function-designator categories consistently.
- Define declaration, mutability, constness, hard-type and conversion rules.
- Complete function signature, parameter, return and call-result typing.
- Define branch/loop scopes and declaration visibility.
- Define control-flow validation for return completeness, unreachable code policy, break/continue targets and labels.
- Define explicit effect/ownership annotations for handler and NEXSUS Flow operations.
- Ensure semantic artifact validation rejects missing mandatory annotations, dangling IDs and inconsistent scope/type relations.

### E. Frontend-to-OIR readiness

- Keep the frontend artifact-only: no AST or semantic pointers cross the stage ABI.
- Require the lowerer to consume only a validated `SEMANTIC_AST` artifact.
- Use block parameters as the canonical SSA merge model; each predecessor edge supplies the ordered incoming values.
- Reject unsupported constructs explicitly; never silently skip AST nodes.
- Add end-to-end tests from source through `SOURCE_UNIT → TOKEN_STREAM → AST → SEMANTIC_AST`.
- Only enable a lowering feature after its frontend traceability row is complete.

## 5. Initial priority order

1. Generate the token/operator inventory and compare lexer output against parser expectations.
2. Generate the AST tag-to-payload-to-serializer-to-validator matrix.
3. Generate the semantic AST tag-to-annotation coverage matrix.
4. Fix structural inconsistencies and incomplete codecs before adding new lowering opcodes.
5. Add missing semantic metadata required by the first scalar OIR subset.
6. Add complete control-flow semantic metadata before enabling CFG lowering.
7. Audit specialized NEXSUS Flow syntax and semantics before mapping it into OIR.

## 6. Acceptance criteria

The audit is complete only when:

- Every token has a documented lexer and parser status.
- Every AST tag has a documented payload schema and serializer/validator status.
- Every executable AST construct has a defined semantic contract and required artifact annotations.
- All unsupported or incomplete constructs are explicitly listed.
- Tests cover positive, negative, malformed and round-trip cases for each supported feature family.
- CI passes on the supported platforms and sanitizer configurations.
- The feature matrix is maintained as code evolves.

No claim of full language or frontend completeness may be made until these criteria are met.
