# OIR2 Binary Container — Compatibility Contract

**Status:** Describes the currently checked-in `OIRWriter` / `OIRReader` binary container implementation.  
**Container version:** `0x0002`  
**Scope:** Serialization and structural validation only.

This document is intentionally separate from the semantic OIR/CFG specifications. The historical binary container version (`OIR2`) must not be confused with the semantic OIR 1.0/1.1 revisions used by the newer compiler design.

## 1. File layout

All integer fields are little-endian. The fixed header is 48 bytes.

| Offset | Size | Field | Contract |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `OIR2` |
| 4 | 2 | Container version | `0x0002` |
| 6 | 2 | Mode | `0` release, `1` debug |
| 8 | 32 | Target | NUL-terminated byte string; maximum 31 bytes |
| 40 | 4 | Symbol-table offset | Must equal 48 |
| 44 | 4 | Code-stream offset | Must be between symbol offset and EOF |

The symbol table occupies the byte range `[symbol_offset, code_offset)`. The instruction stream occupies `[code_offset, EOF)`.

## 2. Symbol records

Each symbol record has a 13-byte fixed prefix followed by its name bytes:

| Field | Size | Encoding |
|---|---:|---|
| `id` | 4 | Unsigned 32-bit little-endian; nonzero and unique |
| `prefix` | 1 | Prefix identifier |
| `typeId` | 4 | Unsigned 32-bit little-endian |
| `flags` | 2 | Unsigned 16-bit little-endian |
| `nameLength` | 2 | Unsigned 16-bit little-endian |
| `name` | N | Exactly `nameLength` bytes; no terminating NUL |

Prefix identifiers in the current implementation:

| Value | Symbol kind |
|---:|---|
| `0x01` | Variable (`v:`) |
| `0x02` | Constant (`c:`) |
| `0x03` | Function (`f:`) |
| `0x04` | New type (`nt:`) |
| `0x05` | Group |
| `0x06` | Rule (`r:`) |
| `0x07` | Extern function (`exf:`) |

Current flag bits:

| Bit | Meaning |
|---:|---|
| `0x0001` | Mutable |
| `0x0002` | Public |
| `0x0004` | Extern |

The current Reader accepts symbol names up to 1,024 bytes. The Writer rejects larger names before serialization.

## 3. Instruction records

Every instruction record is exactly 16 bytes:

| Field | Size | Encoding |
|---|---:|---|
| `op` | 1 | Opcode byte |
| `typeId` | 1 | Type identifier byte |
| `symbolId` | 4 | Unsigned 32-bit little-endian |
| `operandId` | 4 | Unsigned 32-bit little-endian |
| `line` | 4 | Unsigned 32-bit little-endian |
| `col` | 2 | Unsigned 16-bit little-endian |

There is no instruction-count field. The count is derived from:

```text
instruction_count = (file_size - code_offset) / 16
```

Therefore, the code-stream byte count must be divisible by 16. A partial trailing record is invalid.

The in-memory `rawData` field is currently used for trace/debug output; it is **not serialized** in this OIR2 record layout. It must not be treated as persistent payload data.

## 4. Structural validation

The Reader rejects a file when any of the following is true:

- File is shorter than the 48-byte header or exceeds the 32-bit offset range.
- Magic or container version is unsupported.
- Mode is not 0 or 1.
- Target field has no NUL terminator inside its 32-byte field.
- Symbol offset is not 48, or code offset is outside the file.
- A symbol record is truncated, has an invalid name length, uses an unknown prefix, or has a zero/duplicate ID.
- The instruction stream contains a partial 16-byte record.

Reader publication is transactional: symbols and instructions become visible only after the entire file passes structural validation. A failed load leaves the public parsed collections empty.

The Writer validates target/name lengths, mode, source positions and 32-bit offset capacity before writing. Integer serialization is explicit little-endian rather than host-endian.

## 5. Compatibility boundary and known limitations

OIR2 is the existing legacy binary container used by the current Parser DLL and Analyzer DLL. It is **not yet the complete transport format for the newer semantic OIR design**.

In particular, this container does not currently define dedicated serialized tables for:

- Basic blocks and explicit CFG edges.
- SSA values and block parameters.
- Defer activation records, lexical environment references or cleanup metadata.
- Versioned feature declarations / mandatory-feature negotiation.
- A persistent source-map or arbitrary instruction payload section.

The newer OIR semantic contracts (including defer-bearing OIR 1.1 artifacts) must not be silently squeezed into unused OIR2 fields. A later format migration must define its own versioned sections and compatibility rules.

## 6. Regression tests

Run from `omnicore-sdk/`:

```bash
g++ -std=c++17 -Wall -Wextra tests/oir_reader_tests.cpp -o tests/oir_reader_tests
./tests/oir_reader_tests
```

The test source covers a valid Writer→Reader round-trip and rejection of unsupported versions, truncated instructions, out-of-range offsets and unterminated target fields. These tests are source files; they must be executed in the intended build environment before claiming a passing test result.
