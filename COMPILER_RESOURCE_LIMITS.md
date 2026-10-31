# Compiler Resource Limits and Capacity Contract

**Status:** Initial source-verified inventory; product limits and benchmark-derived limits remain open  
**Branch:** `refactor/c-core-roadmap`  
**Scope:** OCC source frontend, parser, AST codec, diagnostics and OIR token payload  
**Policy:** A limit documented here is not automatically a performance guarantee. Each value is classified as an implementation limit, a representation limit, a configurable policy, or an unmeasured practical limit.

## 1. Purpose

This document defines how the compiler describes and enforces resource limits. It prevents an input from exhausting memory or stack through uncontrolled growth, and gives users predictable diagnostics instead of crashes, integer wraparound or silent truncation.

The compiler must distinguish:

1. **Hard implementation limit:** explicitly checked by the current implementation.
2. **Representation limit:** imposed by a C type or serialized field width.
3. **Configurable policy limit:** a product-level cap that may be selected by the host or build configuration.
4. **Practical limit:** depends on available memory, address space, operating system, target and workload; it must be established by measurement.
5. **Not yet specified:** no defensible fixed value has been selected.

A representation limit is not a recommended operating size. For example, a 32-bit count field does not mean that billions of records are practical.

## 2. Current source-verified limits

| Resource | Current limit / behavior | Classification | Source |
|---|---:|---|---|
| Parser nesting | 256 levels | Hard implementation limit | `parser.c`, `PARSER_MAX_NESTING_DEPTH` |
| Expression operator chain | 32 operators per guarded chain | Hard implementation limit | `parser.c` |
| Postfix chain | 32 operations per guarded chain | Hard implementation limit | `parser.c` |
| AST codec node count | 1,000,000 nodes | Hard implementation limit | `ast.c`, `AST_MAX_NODE_COUNT` |
| AST codec recursion depth | 512 levels | Hard implementation limit | `ast.c`, `AST_MAX_RECURSION_DEPTH` |
| AST codec string field | 16 MiB | Hard implementation limit | `ast.c`, `AST_MAX_STRING_LENGTH` |
| Lexer token length | At most `INT_MAX` bytes | Representation / checked implementation limit | `Token.length` is `int`; lexer rejects an oversized token |
| OIR token lexeme length | At most `UINT32_MAX` bytes in the wire record | Representation limit | `OirTokenStreamInput.lexeme_length` |
| Lexer stage token count | At most 1,000,000 records, including EOF | Hard implementation limit | `lexer_stage_module.c`, `LEXER_STAGE_TOKEN_LIMIT` |
| Parser stage token input | At most 1,000,000 records, including EOF | Hard implementation limit | `parser_stage_module.c`, `PARSER_STAGE_MAX_TOKEN_COUNT` |
| Parser stage AST pointer index | Capacity grows up to 2,097,152 entries | Hard implementation limit | `parser_stage_module.c`, `PARSER_STAGE_MAX_INDEX_CAPACITY` |
| OIR token count | At most `UINT32_MAX` records in schema representation; frontend stages apply the stricter 1,000,000-record limit | Representation limit plus stage policy | Token-stream header count is `uint32_t` |
| OIR token record size | 32 bytes, plus 16-byte header | Fixed schema size | `oir_token_stream.h` |
| Diagnostic message / hint length | Each serialized length is at most `UINT32_MAX` bytes | Representation limit | `diagnostics.c`, OIR diagnostic fields |
| Diagnostic text arena | Total serialized text is bounded by `UINT32_MAX` bytes | Representation limit | `diagnostics.c` |
| Source offsets | `size_t` in parser tokens; `uint64` in OIR token records | Representation limit | Parser and OIR token-stream structures |
| Source line / column | Lexer counters saturate at `INT_MAX`; OIR fields are `uint32` | Representation / checked behavior | `lexer.c`, token schema |
| Parser source input | Source is copied into a NUL-terminated allocation; embedded NUL bytes are rejected by the source constructor | API contract | `parser_init_source_ex()` |

### 2.1 Parser nesting and expression chains

The parser rejects nesting beyond 256 levels. The parser header documents this limit. Separate precedence and postfix parsing loops guard operator/postfix chains at 32 operations. These are independent limits: a program may stay within the nesting limit and still exceed an expression-chain limit.

The diagnostic is a parser error; the implementation must not continue recursively after the guard has fired.

### 2.2 AST codec limits

The AST serializer/deserializer enforces:
- no more than 1,000,000 AST nodes;
- no more than 512 recursive codec levels;
- no individual serialized string field longer than 16 MiB.

These are the legacy AST codec safety limits in `parser/ast.c`. The artifact-oriented Parser DLL uses a separate frontend pointer index whose capacity grows from 1,024 up to 2,097,152 entries. The stage rejects input that would require capacity beyond that ceiling. These are distinct paths and must not be described as one shared AST-node limit. Neither value proves that every AST at the maximum size is affordable on every machine.

### 2.3 Token stream sizing

OIR Token Stream schema 1.0 uses:
- a 16-byte header;
- a 32-byte fixed-size record per token;
- a 32-bit token count;
- 32-bit lexeme length, line and column fields;
- a 64-bit source offset.

The token-record payload size is therefore:

`16 + (32 × token_count)` bytes

This formula excludes the enclosing OIR artifact header, section directory, source payload and diagnostics payload. The serializer checks arithmetic overflow and caller-provided output capacity. The 32-bit count is a format ceiling, not a practical target. The current Lexer DLL and Parser DLL apply a much stricter limit of 1,000,000 token records per compilation, including the EOF record. The Parser DLL checks this independently before allocating its token array, because stage input is an artifact boundary and must not be trusted solely because the Lexer normally emits it.

## 3. Source file size and line count

### 3.1 No fixed source line-count limit is currently specified

The current frontend does **not** define a product-level maximum such as 10,000, 100,000 or 1,000,000 source lines. We must not advertise one until a policy is selected and tested.

The parser stores source length using `size_t`. File loading also depends on the host C runtime's file-position/length representation and successful memory allocation. Consequently, the actual maximum source file is constrained by:
- address-space and `size_t` limits;
- the file API and platform's representable file size;
- available memory;
- allocations made by tokenization, parsing, AST construction and diagnostics.

The source constructor rejects a source length that cannot safely accommodate its terminating NUL byte and rejects embedded NUL bytes.

### 3.2 No independent maximum line length is currently specified

There is no separate configured line-length cap. A very long line is still subject to token-length and total source-memory limits. A token longer than `INT_MAX` bytes is rejected because the current public token structure stores its length in `int`.

### 3.3 Line and column coordinates

The lexer uses integer line/column counters and saturates at `INT_MAX` rather than allowing signed overflow. OIR stores line and column as unsigned 32-bit values. This is a coordinate representation limit, not a promise that a source with billions of lines is practical.

## 4. Limits that are not yet defined

The following values must remain explicitly **TBD** until they are supported by implementation decisions and tests:

| Resource | Current status | Required work |
|---|---|---|
| Maximum source file bytes | No product cap | Add configurable host policy and boundary tests |
| Maximum source lines | No product cap | Decide whether a line-count cap is useful; likely secondary to byte cap |
| Maximum physical line length | No product cap | Decide policy; test long comments, strings and identifiers separately |
| Maximum total token count | Lexer and Parser stages each enforce 1,000,000 records; no host-configurable budget yet | Promote the stage-local constant into the versioned job policy when that policy is implemented |
| Maximum AST nodes during parsing | No parser-side global node budget confirmed | Add a parser allocation/node budget before claiming a parser maximum |
| Maximum declarations / functions / parameters | No product caps specified | Define only if required for predictable resource control |
| Maximum diagnostics per compilation | No product cap | Add configurable diagnostic budget and a final “further diagnostics suppressed” record |
| Maximum total diagnostic bytes | Wire-format ceiling exists; practical policy absent | Add configurable per-job text budget |
| Maximum OIR artifact size | No single global artifact cap established | Define per-artifact and per-section limits in OIR contract |
| Maximum compilation memory | No job-wide accounting | Introduce per-job budget and allocation accounting |
| Maximum compilation time | No cancellation/deadline contract established | Add host cancellation/deadline support |
| Maximum concurrent jobs | Host/runtime policy | Define in OCC orchestrator, not in the parser |
| Maximum import/module graph size | Not established | Define cycle handling, graph-node and dependency-depth budgets |
| Maximum macro/extern expansion | Not established | Define only when those expansion stages are implemented |

## 5. Recommended limit layers

Limits should be applied at more than one level.

### 5.1 Host/job policy

The OCC orchestrator should eventually accept a versioned per-compilation resource policy, for example:

- maximum source bytes;
- maximum token count;
- maximum AST nodes;
- maximum diagnostics and diagnostic text bytes;
- maximum OIR artifact bytes;
- maximum compilation memory;
- optional deadline/cancellation;
- maximum dependency graph nodes and depth.

A host policy must be checked before expensive allocations whenever possible. Defaults must be documented and stable for a release. A caller may choose a stricter limit; relaxing a hard implementation limit must not be allowed without changing the implementation.

### 5.2 Stage-local guards

Each DLL stage must independently validate its input artifact and enforce the limits relevant to its own work. A stage must not trust a count, length, offset or depth merely because an earlier stage produced it.

Stage-local limits must return a structured status/diagnostic. They must not terminate the process, overflow counters, silently truncate data or free memory owned by another stage.

### 5.3 OIR validation

The current stage boundary checks also enforce the declared payload schema: Lexer accepts SOURCE_UNIT schema 1.0; Parser accepts SOURCE_UNIT schema 1.0 and TOKEN_STREAM schema 1.0, and requires the token artifact producer to be the Lexer stage. A valid outer OIR container alone is not sufficient.

Every OIR reader must validate:
- token source positions are nondecreasing by line and column, as well as by source offset;
- header and schema version;
- section lengths and section-count arithmetic;
- record counts before allocation;
- all offset/length pairs using subtraction-based bounds checks;
- string and payload lengths;
- graph node counts, references and maximum graph depth;
- total artifact and per-section budgets.

The OIR validator is a security boundary because artifacts may be malformed or supplied independently of the producer stage.

## 6. User-visible failure behavior

When a configured or hard limit is exceeded, the compiler should:
1. stop the affected stage safely;
2. report a stable diagnostic code and source span when available;
3. identify the resource and configured/implementation limit;
4. release all stage-owned partial allocations;
5. preserve host-owned buffers and diagnostic collectors;
6. return a deterministic stage status;
7. avoid presenting a limit failure as an ordinary syntax error or generic crash.

Examples of useful messages:
- `Source file exceeds the configured byte limit.`
- `Parser nesting limit (256) exceeded.`
- `Expression operator-chain limit (32) exceeded.`
- `AST node budget exceeded.`
- `OIR token count is outside the supported range.`
- `Compilation memory budget exhausted.`

## 7. Benchmark and acceptance plan

No practical source-size promise should be made from integer widths alone. Before setting default product limits, add repeatable tests/benchmarks for at least:

1. many short lines;
2. one very long line;
3. a large comment-only source;
4. many small declarations;
5. a large number of tokens;
6. deeply nested blocks and expressions at, below and above the parser limits;
7. wide sibling AST lists;
8. large AST string fields at, below and above 16 MiB;
9. large diagnostic volumes;
10. OIR token streams at exact record-count and buffer-capacity boundaries;
11. malformed/truncated OIR payloads with extreme count/length fields;
12. concurrent compilation jobs under a bounded memory budget.

For each supported CI platform, record:
- source bytes, lines and token count;
- AST node count and maximum depth;
- peak memory where measurable;
- elapsed time;
- result/status and diagnostic count.

Benchmarks must be labelled as measurements from a named machine/build configuration. They must not be presented as universal language limits.

## 8. Current user-facing statement

Until configurable resource budgets are implemented, the accurate statement is:

> The compiler has explicit safety limits for parser nesting, expression chains and AST serialization. It does not yet define a fixed maximum source line count or a universal maximum source-file size. Practical source capacity depends on platform address space, available memory and the allocations required by the compilation pipeline. Oversized individual tokens and malformed serialized inputs are rejected by their respective checked interfaces.

## 9. Change control

Whenever a limit changes:
- update this document and the corresponding public header/spec;
- add exact-boundary and one-over-boundary tests;
- verify 32-bit and 64-bit arithmetic where supported;
- test cleanup after limit rejection;
- verify all stage ABI consumers agree on the value and status;
- record whether the change is backward-compatible for OIR artifacts.

This document is an inventory of the current implementation plus a roadmap for a coherent resource-budget contract. It does not authorize changing language semantics solely to fit an arbitrary source-line count.
