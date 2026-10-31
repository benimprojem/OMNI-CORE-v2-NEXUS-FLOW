# OIR v1 Initial Payload Schemas

**Status:** Initial schema contract for SOURCE_UNIT, TOKEN_STREAM and DIAGNOSTICS  
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
| 4 | 4 | Diagnostic ID | Zero normally; otherwise 1-based index in the associated DIAGNOSTICS payload |
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

The current Token struct exposes source_offset and source_length separately from its lexeme/message pointer and length, plus diagnostic_id. Lexer error tokens still point at message text in Token.start, but carry the consumed source span in the new source fields. When ParserContext is attached to a job-owned collector, parser error reporting assigns lexer error tokens the 1-based diagnostic record index; zero means no associated diagnostic. The TOKEN_STREAM adapter converts frontend Token values to stable OIR IDs and validates the associated diagnostic count.

Before enabling TOKEN_STREAM serialization:
1. Add explicit source offset and token flags/diagnostic association to the lexer result contract, or introduce a serialization adapter that can safely distinguish source-backed tokens from diagnostic tokens.
2. [x] Capture token start line/column/byte offset and fix/test multiline string line tracking.
3. [x] Ensure unterminated block comments produce a diagnostic with a source span rather than being silently consumed to EOF.
4. Audit all 160 token enum entries against actual lexer emission paths; registry presence alone is not evidence of implementation.
5. Test that every serialized source span resolves to the exact original source bytes.

The TOKEN_STREAM codec is implemented in `omnicore-sdk/src/c/oir/oir_token_stream.h/.c`. It validates the 16-byte header, 32-byte records, registry IDs, source identity and bounds, non-overlapping source spans, terminal EOF, and error-token diagnostic references. Its reader returns zero-copy record views; the frontend adapter converts `Token` values without serializing pointers or message strings.


The parser now exposes a borrowed pre-tokenized input attachment. The OIR adapter reconstructs frontend Token records from validated stream records and source/diagnostic views; parser_run() can consume that array without calling the lexer. The caller owns the Token array and source/diagnostic buffers, which must remain alive through parser_run(). This is an internal handoff API, not the frozen cross-DLL ABI.

## 5. Implementation order

1. [x] Add a stable C enum-to-OIR token ID mapping by symbolic token name (160-token round-trip test).
2. [x] Add SOURCE_UNIT payload encode/decode and exact-length tests.
3. [x] Extend lexer token metadata with source byte offsets and diagnostic references.
4. [x] Add DIAGNOSTICS payload encode/decode and malformed-input validation tests.
5. [x] Add TOKEN_STREAM payload encode/decode and span-validation tests.
6. [x] Add lexer-to-OIR and OIR-to-parser adapters.
7. Only then split lexer and parser into separate shared libraries using the common module ABI.


## 6. DIAGNOSTICS — schema 1.0

Producer: any compiler stage / compilation orchestrator  
Consumer: CLI, IDE, pipeline runner, artifact inspector

A DIAGNOSTICS section serializes the job-owned diagnostic collector. Text is stored once in a trailing string table; record offsets are relative to that table. All integers are unsigned little-endian. Records are byte-packed with no implicit alignment.

### Payload header — 16 bytes

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | Record count | Number of diagnostic records |
| 4 | 4 | Record size | Must equal 68 in schema 1.0 |
| 8 | 4 | String table length | Exact number of bytes following the records |
| 12 | 4 | Flags | Must be zero in schema 1.0 |

### Diagnostic record — 68 bytes

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | Diagnostic code | Non-zero stable numeric code |
| 4 | 4 | Stage ID | Stable producer stage identifier |
| 8 | 4 | Message ID | Localization/message registry ID |
| 12 | 1 | Severity | 1 NOTE, 2 WARNING, 3 ERROR, 4 FATAL |
| 13 | 1 | Span flags | Bit 0 indicates a source location; all other bits zero |
| 14 | 2 | Reserved | Must be zero |
| 16 | 4 | Source ID | Source unit identity; zero in single-source frontend |
| 20 | 8 | Start byte offset | Zero-based byte offset |
| 28 | 8 | End byte offset | Exclusive end offset |
| 36 | 4 | Start line | 1-based line |
| 40 | 4 | Start column | 1-based column |
| 44 | 4 | End line | 1-based line |
| 48 | 4 | End column | 1-based column |
| 52 | 4 | Message offset | Offset into string table |
| 56 | 4 | Message length | UTF-8 byte count, excluding terminator |
| 60 | 4 | Hint offset | Offset into string table; UINT32_MAX means absent |
| 64 | 4 | Hint length | UTF-8 byte count, excluding terminator |

Each present message or hint is followed by exactly one NUL byte in the string table. The terminator is not included in its length. Embedded NUL bytes are invalid. A message must be present and non-empty; a hint may be absent. Message and hint offsets must resolve wholly inside the string table.

If span flag bit 0 is clear, every source/span field must be zero. If it is set, line/column values must be non-zero and end byte offset must not precede start byte offset. This initial schema supports one contiguous primary source span; related locations and fix-it edits are reserved for a future schema.

The payload length must equal 16 + (record_count × 68) + string_table_length exactly. Record count multiplication, offsets and lengths must be checked before pointer arithmetic. Unknown severity values, non-zero reserved/flag bits, invalid strings and out-of-range spans are rejected.

Implementation:
- `omnicore-sdk/src/c/oir/oir_diagnostics.h`
- `omnicore-sdk/src/c/oir/oir_diagnostics.c`
- CTest: `oir_diagnostics_payload_roundtrip_and_validation`

The codec exposes zero-copy read views over a validated payload and serializes a job-owned `OccDiagnosticCollector`. The caller retains ownership of the payload buffer. The initial frontend bridge records line/column only; byte offsets and end spans will become populated when source-span metadata is propagated through AST nodes and parser recovery.

