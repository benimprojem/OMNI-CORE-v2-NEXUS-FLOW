# AST Representation Gaps & Decision Matrix

**Audit date:** 2026-11-05  
**Branch:** `refactor/c-core-roadmap`  
**Purpose:** Close AST design gaps against `docs/language/syntax-reference.md` and the formal function specification before adding speculative node kinds.

> This is an AST design audit, not a claim that Parser/Semantic/OIR are complete. Existing AST tags and generic payloads are not sufficient evidence of a valid representation.

## Decision rules

1. Preserve the syntax reference as the normative language source; this document must not invent grammar.
2. Do not repurpose existing OIR node IDs or AST enum values. Registry additions are append-only.
3. Prefer a typed payload/child contract over encoding structured types in an opaque string.
4. Compile-time directives and preprocessing data must not accidentally enter runtime executable AST.
5. Every new node requires: grammar reference, payload ownership, graph context, codec fields, OIR policy, positive/negative fixtures and CI.
6. A representation may be marked **reuse** only if it preserves all information required by later phases.

## Gap matrix

| Construct | Current representation | AST assessment | Decision / next action |
|---|---|---|---|
| Function pointer `fptr` | `AST_VAR_DECL.type_name` string; lambda is `AST_LAMBDA` | **Partial / opaque.** Signature (parameters, return type, variadic) is not attached to the type. | Add a typed function-type payload only after choosing canonical declarator grammar. Keep bare `fptr` as an unspecified/legacy function pointer form if compatibility requires it. |
| Callback parameter | Function parameter with `type_name="fptr"` | **Partial.** Calls can be parsed, but signature compatibility cannot be represented from this declaration alone. | Reuse function-type model; test callback argument and return compatibility. |
| Generic type `t` | Type-name string | **Partial.** Generic identity/binding is semantic metadata, not just spelling. | Keep syntax node reuse; define explicit generic parameter/binding records in Semantic artifact. Do not add runtime AST node solely for generic substitution. |
| Map `map` | `ASTVarDecl.type_name = "map"` for ordinary `a:map` parameters; `"!map"` remains accepted as explicit hard-typed parameter spelling; local declaration currently uses `v:name!map` | **Syntax representation is sufficient for the unparameterized map type.** No `AST_MAP_TYPE` child or key/value generic arguments are currently required. | Prefer ordinary `map` typing; optional `map<key,value>` constraints are undecided and current Parser does not implement them. Semantic will define per-entry key/value admissibility and runtime typing. |
| SIMD `Vec2/Vec3/Vec4` | Type name + `AST_ARRAY_LITERAL` values | **Reuse is adequate syntactically**, but vector width/type intent is not represented by array literal alone. | Keep vector as named type; Semantic must enforce fixed lane count and element compatibility. No separate vector literal node unless syntax reference adds one. |
| Numeric shifts `<<`, `>>` | `AST_NEXUS_PIPE` | **Collision unresolved.** AST currently cannot represent numeric shift expression distinctly. | Add numeric shift as `AST_BINARY_EXPR` only after parser has deterministic grammar disambiguation. Preserve Flow Zone Write/Feed as `AST_NEXUS_PIPE`; no operand-shape heuristic. |
| `!!=` target/link directives | Generic `AST_ASSIGNMENT` / parser-level directive handling | **Wrong-layer risk.** Target/link settings are compile configuration, not executable statements. | Define a compile-unit directive/preprocessing model outside runtime statement AST. Keep directive payloads in source/config artifact; reject them in function/block context. |
| `!!= macro` / preprocessing | No macro AST node | **Not runtime AST.** Expansion ordering and source provenance are preprocessing concerns. | Add a distinct preprocessing representation only after macro grammar, expansion, hygiene and source-map rules are specified. No `AST_MACRO` runtime node now. |
| Import `use` / `import` | `AST_USE`; `import` may parse as generic call | **Partial.** Module import and resource acquisition must not be conflated. | Keep `AST_USE` for documented module syntax. Specify `import` grammar/resolution before promoting generic call into dedicated node. |
| Export/import DLL metadata | Declaration flags `is_public/is_exported` | **Partial.** Flags exist on selected declaration payloads; ABI/name/version/visibility contract unclear. | Keep flags; define module ABI metadata separately (compile-unit/module table), not a runtime AST node. Audit every declaration kind for flag parity. |
| Target declaration `!!= target` | No stable AST payload contract | **Missing at compile-unit level.** | Represent in compilation configuration/Directive IR; AST root may reference a non-executable directive list only if the artifact contract explicitly requires it. |
| Fixed array extent expression | `AST_ARRAY_DIMENSION.length` expression | **Present.** | Preserve expression tree; Parser constant-expression validation and Semantic compile-time evaluation are separate. |
| Pointer/ref types `*T`, `**T`, `&T` | Type spelling / hard type string | **Partial / opaque.** Nested pointer depth and ref mutability/ownership cannot be reliably inferred from strings. | Add structured type AST (pointer/ref constructors) as part of unified type grammar; avoid one-off pointer nodes. |
| Type identity / qualified member type | `AST_MEMBER_ACCESS` for expressions; type names mostly strings | **Partial.** Expression member access is not a type path. | Unified type-name/path representation required if namespace-qualified types are normative. |
| Function default parameter `a:10` | Parser parameter AST likely shared `AST_VAR_DECL` initializer | **Reuse likely**, but parameter-vs-local declaration context needs explicit validation. | Confirm payload and add default-parameter parser/codec tests; ensure initializer is not mistaken for executable declaration initializer. |
| Array return | `ASTFuncDecl.return_type` plus optional owned `AST_ARRAY_TYPE` child | **Implemented and CI-verified for the documented return-array suffix forms.** Base element type remains in `return_type`; ordered dimensions are represented by `AST_ARRAY_DIMENSION` children. | Preserve append-only AST payload layout and OIR optional field 8. Keep expanding the shared type-expression/declarator audit; do not infer that every type form or function-return semantic rule is complete. Latest CI run `42011297161` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind; explicit v1 function-stream compatibility test is included. |
| `elsif` / conditional action | Canonical nested `AST_IF` | **Adequate canonical AST** if source form need not be preserved. | Keep canonicalization; spans must cover each source branch and preserve diagnostic locations. |
| `when` pattern guard | `AST_IF` wrapper in arm body | **Adequate canonical AST**, but guard/body distinction is implicit. | Keep only if Semantic consistently unwraps guard; otherwise add explicit optional guard field in a future arm schema (not a new node kind). |
| `}?;` block cleanup | `AST_BLOCK_HALT` final block child | **Present/contextual.** | Preserve final-child invariant; codec and graph validator tests remain mandatory. |
| FASTEXEC / raw ASM | `AST_FASTEXEC_BLOCK` + `AST_ASM_LABEL` | **Present.** | Keep opaque assembly text and exact byte length; ASM validation belongs to dedicated FASTEXEC Semantic/target validation, not general AST parser. |

## Type AST design prerequisite

Before adding `AST_MAP_TYPE`, function types, pointer/ref constructors or return-array payloads independently, design one shared type-expression grammar/model:

- primitive / named / qualified type;
- generic type parameter;
- dynamic/fixed/multidimensional array;
- map marker (`!map`, without key/value type arguments);
- pointer depth and reference;
- function signature (ordered params, variadic marker, return type);
- optional nominal/aggregate references.

This model must not break current `ASTVarDecl.type_name` consumers abruptly. A migration may retain the original spelling as a compatibility mirror while typed children become authoritative. The exact AST/OIR schema and migration are to be specified before implementation.

## Immediate AST closure tasks

1. Add focused Parser fixtures for Group/method, `use` alias/path, `m:` scope rejection, defer/thread payload, and each intrinsic family.
2. Add parameter defaults and function-return array AST/codec tests.
3. Audit declaration visibility flags and parameter payload context.
4. Specify the unified type AST and OIR field IDs; then implement in small append-only steps.
5. Resolve numeric shift grammar with the language spec; do not overload existing Flow parse by heuristics.
6. Keep preprocessing/target/link directives out of runtime AST and define a separate compile-unit contract.

## Completion status

- **Audit document:** complete as a design-gap inventory.
- **AST implementation:** still open; no new node kind is declared by this audit.
- **Parser/codec tests:** remaining items above are not yet claimed complete.
- **Semantic/OIR/Codegen:** unchanged and gated.


## 2026-11-05 clarification — syntax boundaries

- The current normative module-loading grammar is `use <module>` and `use <module> as <alias>`, with comma-separated module identifiers. Dotted/path-qualified module syntax is not specified in the syntax reference; Parser must not invent it. Treat `use Core.Module` as unsupported until the language grammar explicitly defines path separators, resolution roots and source-span behavior.
- Function array-return syntax is normative: the function section includes `f:name(a, ...)!i32[] { ... }`. **Resolved:** Parser now attaches ordered return dimensions through the optional owned `AST_ARRAY_TYPE` child; see the implementation/CI status below. This was an implementation gap, not a grammar ambiguity.
- `AST_ARRAY_TYPE` / `AST_ARRAY_DIMENSION` already model declaration and parameter arrays. Reuse that dimension payload for function returns, preserving `ASTFuncDecl.return_type` as a compatibility mirror while an owned structured return-array child becomes authoritative for dimensions.


## Thread block grammar boundary — 2026-11-05

- The C Parser contains a legacy/special `(name)>{ ... }` thread-block production and ASTThreadBlock payload fields for `name`, `mode`, `timeout`, and `body`; current parser construction populates `name` and `body`, while `mode` and `timeout` remain NULL.
- The normative `docs/language/syntax-reference.md` currently discusses thread handoff conceptually but does not define the thread-block production, its scheduling/mode grammar, timeout units, or ownership/capture semantics.
- Keep existing parser compatibility and span tests, but do not expand mode/timeout syntax or claim the thread AST contract complete until the normative syntax reference defines these fields and their semantic meaning.


## Thread-block intent clarification — 2026-11-05

The intended model is clarified by the project owner: `(thread_block_name)>{ ... }` declares a **code template/body for a thread execution context**, not code that runs inline in the declaring application flow. A call such as `spawn(thread_block_name, addr(nexus)), async, 100);` refers to that named block and requests its execution in a separate thread context; the block body is the payload executed there.

This separates three concepts that must not be conflated in AST/Semantic work:

- **Thread block declaration/body:** named, separately represented executable code; declaration alone must not imply inline execution.
- **Spawn invocation:** references the declared thread block and supplies execution/context arguments (including the address/endpoint expression and async/timeout-like arguments shown in the example).
- **Runtime scheduling and transfer contract:** how the context/address is captured, what `async` means, timeout units/behavior, ownership/lifetime, error propagation, and whether/how a spawned task is joined remain unspecified and require normative decisions.

The existing `AST_THREAD_BLOCK` (name/body/mode/timeout) preserves the declaration shape; Parser currently fills name/body and leaves mode/timeout NULL. Regression `test_thread_block_ast()` now verifies the named body, NULL optional fields, source span, nested payload ownership and AST codec round-trip (CI run `42014109396`, all five jobs passed). The spawn call remains outside the core AST and must not be inferred from this declaration node. Semantic symbol resolution and execution metadata remain separate future work; do not implement scheduling in core AST.


## Spawn/core boundary — owner correction (2026-11-05)

`spawn` is intentionally not part of the core AST: it belongs to a separate thread module/language layer. The runtime/thread specifications already exist in `nexus docs/NXF Runtime Threading Specification v1.0.md` and `nexus docs/Thread System Runtime — Full Formal Specification v1.0.md` (and SDK copies). Thus the earlier statement that the normative specs do not define thread behavior was too broad: runtime behavior is specified there; the core language syntax/AST boundary is the remaining distinction. Core Parser must not emit `AST_SPAWN` or core scheduling semantics. The existing AST enum/OIR node identity and codec support are retained as legacy serialized-artifact compatibility only. Spawn parsing/validation/lowering and thread runtime linkage belong to the separate module contract.


## Array-return implementation verification — 2026-11-05

- `ASTFuncDecl.return_array_type` is an owned optional `AST_ARRAY_TYPE` child. Parser, AST destroy/validation, AST stream v2 codec (including a v1 function-stream compatibility fixture), and OIR frontend optional field 8 are integrated.
- Parser fixtures cover `!i32[]`, `!i32[n]`, and `!f32[m][n]`. OIR frontend round-trip verifies a named return extent survives decode and re-encodes to identical canonical payload bytes.
- CI run `42010711975` passed Ubuntu, Windows, macOS, ASan/UBSan, and Valgrind. This closes only the array-return AST/codec integration gap; other rows remain independently open.


## Unified type representation — Parser audit — 2026-11-05

The next type-model work is intentionally gated on canonical declarator grammar. Current source inspection confirms these concrete gaps:

- **Function/callback type:** the syntax reference shows only the alias example `nt:Callback = f:(id!u32)>!bool;`; it does not define a complete function-type grammar (nested callbacks, optional parameter names, variadic placement, qualifiers, or type nesting). `parse_newtype_decl()` currently accepts one primitive-type token or one identifier as `target_type`; it does not consume a `PRE_FUNC` signature or build parameter/return type children. Existing `AST_LAMBDA` represents executable lambda bodies and must not be reused as a signature type.
- **Map type:** `map` is an ordinary unparameterized type. Function parameters accept `a:map`; `a!map` remains an explicit hard-type compatibility form. Local variable declarations currently use the established hard-type syntax `v:name!map`. The parser does not implement `map<key,value>`; whether it may be offered later as an optional homogeneous constraint is undecided. No `AST_MAP_TYPE` child is needed for the current form. Per-entry key/value admissibility, heterogeneous value handling and runtime operations belong to Semantic/runtime contracts.
- **Pointer/reference:** the type table lists `*`, `**`, and `&`, but the normative reference does not provide concrete declaration/declarator examples or define composition order, nullability, mutability, ownership, and lifetime rules. Do not infer prefix/suffix placement or pointer nesting from expression operators.

**Next required step:** complete a grammar-only audit and settle the canonical type-expression/declarator forms before introducing any new AST kind or changing parser behavior. The eventual type representation should be append-only and typed (primitive/named, array, map, pointer/reference, function signature), with explicit ownership, graph contexts, codec fields, OIR mapping, and positive/negative fixtures. This is an architecture gate, not a claim that these forms are already supported.


## Function array-return implementation confirmation — 2026-11-05

- The former Parser gap is closed: the return suffix is parsed after the base return type, with `ASTFuncDecl.return_type` retained as the compatibility spelling and `return_array_type` owning ordered dimensions.
- AST stream v2 writes the optional child; the reader accepts v1 and omits the v2-only child for v1 payloads. The regression `reads_v1_function_stream_without_v2_return_child()` verifies the compatibility path.
- OIR frontend field 8 carries the optional return-array node. The round-trip fixture verifies a named extent survives OIR decode and canonical re-encoding.
- Latest CI run `42011297161` / commit `1196820a3bcb`: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind all passed.
- This does not close the unified type AST design gate or the overall AST stage.


## Thread-block AST regression confirmation — 2026-11-05

- `test_thread_block_ast()` covers `(worker)>{ v:value = 7; }` as a core `AST_THREAD_BLOCK` declaration. It checks the name, block body, nested variable, NULL `mode`/`timeout`, source span, graph validation and AST serialize/deserialize ownership.
- CI run `42014109396` / commit `85bc7f35`: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind all passed. Ubuntu test suite reports 60/60.
- `spawn` remains a separate thread-module concern; no `AST_SPAWN` behavior or core scheduling semantics were added.


## Canonical map declarator decision — 2026-11-05

- Canonical map type spelling is `!map` only. No key/value type arguments are written or parsed.
- The forms `map<key, value>` and `!map key, value` are not part of Nexus Flow syntax and must be rejected.
- `ASTVarDecl.type_name` remains the existing representation; no `AST_MAP_TYPE` node or OIR field is introduced for this unparameterized marker.
- Semantic will own map operation/type constraints without requiring explicit key/value type arguments in the declaration.


## Group/method parser-to-codec coverage — 2026-11-05

- The parser regression for `group Pair { v:value!i32; get => f:get()!i32 { ret value; } }` now verifies both initial AST shape and AST stream round-trip. The decoded graph is validated and the nested `AST_METHOD` implementation function retains its public name, function name, return type and body.
- CI run `42014382856` / commit `f63406fc`: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind all passed (60/60 tests on each build/test platform).
- This is Parser/AST/codec coverage only; it does not claim Semantic dispatch or OIR lowering support.


## Spawn/core Parser boundary implementation — 2026-11-05

- `TOKEN_KW_SPAWN` is no longer mapped to `AST_SPAWN` by the core Parser. The Parser emits a direct structured diagnostic and rejects the construct, leaving it for the separate Thread module frontend.
- The `AST_SPAWN` enum tag and codec branches remain to preserve legacy serialized artifacts and stable node identity. They are not permission for the core Parser to construct new spawn nodes.
- Regression `test_spawn_is_outside_core_parser()` verifies the syntax error and Thread-module diagnostic through `OccDiagnosticCollector`.
- CI run `42014920664` / commit `4be14f14939c`: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind passed.
