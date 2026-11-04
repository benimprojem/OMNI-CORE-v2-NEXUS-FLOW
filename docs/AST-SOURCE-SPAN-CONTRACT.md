# AST Source Span Contract

**Status:** Partial implementation — AST field/setter/validator and versioned legacy codec with span round-trip + unprefixed legacy reader are implemented. Parser production stamping and OIR TLV remain pending.  
**Scope:** C frontend AST, legacy AST stream codec, OIR AST bridge  
**Normative dependency:** `docs/language/syntax-reference.md`

## 1. Purpose

Every parser-produced AST node must retain its complete source range. Line/column-only metadata is insufficient for precise diagnostics, editor navigation, source mapping, incremental tooling and later OIR verification.

The lexer already supplies zero-based UTF-8 byte offsets and token byte lengths. The diagnostic layer already defines `OccSourceSpan` with start/end byte offsets and start/end line/column. AST must reuse that contract rather than inventing a second coordinate model.

## 2. Coordinate semantics

- Byte offsets are zero-based offsets into the original UTF-8 source buffer.
- Ranges are half-open: `[start_offset, end_offset)`.
- Columns follow the lexer's existing byte-based convention; they are not Unicode code-point or display-cell columns.
- Start and end line/column positions are one-based.
- A node range covers the complete syntactic production that created the node, including delimiters and terminating punctuation owned by that production.
- For a block, the end is the closing `}`; if the block owns a trailing `}?;`, the end includes that Halt token.
- A synthesized AST node must receive a deterministic range derived from the exact source production it represents; it must not fabricate a location.
- Manually constructed or legacy-decoded nodes may have no complete span. Absence is explicit and must not be confused with a zero-length source range.

## 3. AST representation

The public `ASTNode` header will append an `OccSourceSpan` field. Existing `line` and `col` remain as compatibility mirrors of `span.start_line` and `span.start_column` when `span.has_location` is true.

Invariants when a complete span is present:

1. `start_offset <= end_offset`.
2. Start and end line/column values are non-zero.
3. `line == span.start_line` and `col == span.start_column`.
4. The span lies within the source length known to the parser.
5. The node's range encloses all child ranges; punctuation owned by the parent may extend beyond the first/last child.

No source pointer is stored in AST nodes. AST remains independent of the lifetime of the parser's source buffer.

## 4. Parser propagation

The parser will expose one internal helper that builds a span from a first and last consumed `Token`, using the same byte/line/column rules as diagnostic span construction.

Every AST-producing grammar function must stamp the completed node from its production boundaries. **Current partial implementation:** numeric/string/character literals, boolean literals and identifier leaf nodes are stamped from their exact token span and inherit `ParserContext.source_id`; binary and assignment expressions compose their range from the first and last child spans; block nodes use the consumed opening brace and closing brace, extending through the owned `}?;` token when present. Declarations and other productions still need explicit first/last-token boundaries. For expression nodes, the range includes the operator token; for declarations and blocks, it includes their prefix/brace/terminator syntax. Parent ranges must be computed from the production boundaries, not inferred solely from child ranges.

Token-stream input must preserve and validate its supplied source offsets/lengths exactly as the current token-stream attachment contract requires.

## 5. Legacy AST stream compatibility

The current legacy AST stream is process/build-local and writes native C enum/integer representations. Adding span bytes to every node without framing would make old and new streams ambiguous.

The implementation must therefore introduce a root-level magic/version prefix for the new stream format:

- [x] New writer emits the 8-byte `OCCAST\r\n` magic and a 32-bit version (`1`) before the root node.
- [x] New reader recognizes the prefix and decodes the versioned format.
- [x] New reader also accepts the existing unprefixed legacy stream and leaves source spans absent.
- [x] Unknown future stream versions fail closed.
- [x] Version 1 stores `has_location`, `source_id`, start/end byte offsets and start/end line/column as individual fields after each node's legacy `line/col` header; no C struct padding is written.
- Existing truncation, trailing-data, graph ownership and depth checks remain mandatory.

This does not promise that an old compiler can read a new stream. The legacy codec is not the stable cross-module interchange format; OIR remains that format.

## 6. OIR AST compatibility

Keep the current 32-byte OIR AST node record layout. Do not grow the fixed record or repurpose reserved bytes.

Use optional scalar TLV extensions, emitted after all node-specific fields, with these reserved field IDs:

| Field ID | Value type | Meaning |
|---:|---|---|
| `0xFFF0` | `U64` | start byte offset |
| `0xFFF1` | `U64` | end byte offset |
| `0xFFF2` | `U32` | end line |
| `0xFFF3` | `U32` | end column |

All four fields are emitted together and carry `OIR_AST_TLV_OPTIONAL`. Existing `start_line` and `start_column` record members remain authoritative for the start line/column. Readers that do not know these optional scalar fields may ignore them; readers must never ignore an unknown optional `NODE_REF`, because that reference owns graph structure.

The new reader validates that the four fields are either all present or all absent, have the specified types and lengths, and form a valid half-open range. Old OIR payloads without the extension remain readable with source-span presence unset.

## 7. Validator and test requirements

- `ast_validate()` checks complete-span invariants and rejects malformed ranges. (Implemented and covered by AST validator tests.)
- Parser tests currently assert exact ranges and source identity for numeric literal and identifier leaves, plus a binary expression composed from child spans. Declaration spans, multiline source and multibyte UTF-8 coverage remain pending; the block-Halt test verifies the closing `}?;` is included.
- Legacy AST codec tests cover versioned span round-trip, unprefixed legacy input, unknown-version rejection and truncated magic/version prefixes. (Implemented; CI run `41786516465` passed 5/5.)
- OIR tests must cover full-span preservation, old payloads without span extensions, partial extension rejection and unknown optional scalar compatibility. (Pending.)
- Windows, Linux, macOS, ASan/UBSan and Valgrind must pass before the span feature is marked complete.

## 8. Non-goals

This contract does not define Semantic diagnostics, macro expansion/source maps, generated-code provenance, inline ASM source mapping or incremental parsing. Those may extend the span model later without changing the coordinate semantics above.
