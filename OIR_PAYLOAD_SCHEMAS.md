# OIR v1 Initial Payload Schemas

**Status:** Proposed schema contract for SOURCE_UNIT and TOKEN_STREAM  
**Related:** OIR_FORMAT.md, OIR_TOKEN_REGISTRY.md

This document defines payload bytes carried inside the OIR container's required PAYLOAD section. All integer fields are unsigned little-endian unless explicitly stated. Payloads are byte-packed with no implicit alignment or padding. The OIR container header carries the payload schema major/minor version.

## 1. SOURCE_UNIT — schema 1.0

Producer: source manager / file-loading stage  
Consumer: lexer

One artifact represents one source unit. The raw source bytes are preserved exactly so token offsets always refer to the original input.

### Payload layout

Fixed payload header: 24 bytes.

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | Source ID | 0 for the single source unit in this artifact |
| 4 | 2 | Encoding ID | 1 = UTF-8 |
| 6 | 2 | Flags | Must be zero in schema 1.0 |
| 8 | 4 | Path byte length | UTF-8 byte count; zero means unknown/anonymous |
| 12 | 4 | Reserved | Must be zero |
| 16 | 8 | Source byte length | Exact number of source bytes |

The header is followed by path_length bytes of UTF-8 path (no terminating NUL), then exactly source_length raw source bytes. The path is metadata and must not be used as an instruction or trusted filesystem path by a consumer.

The source content is expected to be valid UTF-8. The producer preserves original newline bytes; byte offsets are measured against this exact content. A consumer that needs normalized text must maintain a mapping back to original byte offsets.

Maximum path and source sizes are bounded by the implementation's configured artifact limits. Length arithmetic must be checked before allocation or pointer movement. The payload must contain exactly the declared number of bytes; trailing bytes are rejected in schema 1.0.

## 2. TOKEN_STREAM — schema 1.0

Producer: lexer  
Consumer: parser, token inspector

A token stream references its source through the source ID and byte spans. Lexeme text is not duplicated: the consumer resolves it against the associated SOURCE_UNIT content. This avoids divergent copies and keeps the token stream compact.

### Payload header

Fixed header: 16 bytes.

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | Token count | Includes exactly one terminal EOF token |
| 4 | 4 | Source ID | Must match the associated SOURCE_UNIT |
| 8 | 4 | Token record size | Must equal 32 in schema 1.0 |
| 12 | 4 | Flags | Must be zero in schema 1.0 |

The header is followed by exactly token_count fixed-size records. Schema 1.0 requires at least one record, and the final record must be EOF. No records may follow EOF.

### Token record — 32 bytes

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 2 | OIR token ID | Stable ID from OIR_TOKEN_REGISTRY.md |
| 2 | 2 | Token flags | Must be zero in schema 1.0 |
| 4 | 4 | Diagnostic ID | Zero unless this is an error token linked to a diagnostic |
| 8 | 8 | Source byte offset | Zero-based offset into SOURCE_UNIT; UINT64_MAX means no source span |
| 16 | 4 | Lexeme byte length | Length of the source slice; zero for EOF and non-source error tokens |
| 20 | 4 | Line | 1-based source line |
| 24 | 4 | Column | 1-based source column |
| 28 | 4 | Reserved | Must be zero |

### Record invariants

- For a normal source token, offset + lexeme_length must be within the source byte length.
- EOF has zero lexeme length and its offset equals the source byte length.
- A source-backed token must have a valid non-sentinel offset.
- A non-source error token uses UINT64_MAX, zero lexeme length and a non-zero diagnostic ID.
- TOKEN_ERROR must reference a diagnostic in the associated DIAGNOSTICS section. If no diagnostics section is present, an error token is invalid.
- Every token ID must exist in the registry version supported by this payload schema.
- Token spans must be nondecreasing and must not overlap unless a future schema explicitly allows overlapping lexical views.
- Line and column are 1-based. Byte offsets and lengths are 0-based byte counts, not Unicode code-point indices.
- Token count, record multiplication and source span arithmetic must be overflow-checked before processing.

## 3. Source association

A TOKEN_STREAM artifact must be accompanied by the exact SOURCE_UNIT artifact it references. The pipeline runner preserves that dependency as an artifact reference and validates the source ID before parser invocation. A standalone token stream without its source unit is not self-contained and must not be treated as a portable parser input.

The first implementation can use a single source unit per compilation job. Multi-file/import compilation will extend the source table and dependency metadata without changing the meaning of source ID zero.

## 4. Important current-lexer compatibility work

The current Token struct contains a pointer to lexeme text plus length/line/column; it does not yet expose a stable byte offset or diagnostic ID. In particular, lexer error tokens currently point at message text rather than the source buffer. An adapter must not calculate offsets by subtracting those unrelated pointers.

Before enabling TOKEN_STREAM serialization:
1. Add explicit source offset and token flags/diagnostic association to the lexer result contract, or introduce a serialization adapter that can safely distinguish source-backed tokens from diagnostic tokens.
2. Fix and test line/column tracking for multiline literals.
3. Ensure unterminated block comments produce a diagnostic rather than being silently consumed to EOF.
4. Audit all 160 token enum entries against actual lexer emission paths; registry presence alone is not evidence of implementation.
5. Test that every serialized source span resolves to the exact original source bytes.

## 5. Implementation order

1. [x] Add a stable C enum-to-OIR token ID mapping by symbolic token name (160-token round-trip test).
2. Add SOURCE_UNIT payload encode/decode and exact-length tests.
3. Extend lexer token metadata with source byte offsets and diagnostic references.
4. Add TOKEN_STREAM payload encode/decode and span-validation tests.
5. Add lexer-to-OIR and OIR-to-parser adapters.
6. Only then split lexer and parser into separate shared libraries using the common module ABI.
