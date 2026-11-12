# NEXUS Flow — 8 Başlıkta Syntax → AST → Verifier Durum Tablosu

**Dal:** `refactor/c-core-roadmap`  
**Güncelleme:** 2026-11-12  
**Amaç:** Dil referansında kayıtlı syntax yapılarını sekiz başlıkta görünür kılmak; her yapının Lexer, Parser, AST/Codec, Semantic, OIR Lowering ve bağımsız Verifier aşamalarında nerede olduğunu göstermek.

> Bu belge bir tamamlanma ilanı değildir. Durumlar aktif C dalındaki syntax referansı, syntax/AST matrisi, AST düğüm envanteri, OIR kayıtları, ilerleme kaydı ve mevcut test kanıtları birlikte değerlendirilerek verilmiştir. Bir aşamanın tamamlanması sonraki aşamaları otomatik olarak tamamlamaz.

## Durum anahtarı

| İşaret | Anlamı |
|---|---|
| ✓ | Bu satır/aşama için uygulama ve ilgili test kanıtı var |
| ~ | Kısmi; eksik kural, test, lowering veya entegrasyon var |
| ? | Satır bazında doğrulanmamış; tamamlanmış sayılmaz |
| D | Tasarım gereği ertelendi; aktif runtime syntax olarak sayılmaz |
| X | Core Parser tarafından açıkça desteklenmiyor/reddediliyor |
| — | Bu aşama için uygulanamaz |

**Sütunlar:** L = Lexer, P = Parser, A = AST payload + lifecycle + codec, S = Semantic, O = OIR lowering, V = bağımsız OIR Verifier.  
**Önemli:** Verifier ham AST almaz. AST'den Semantic AST'ye, oradan OIR'a indirgenen yürütülebilir davranışı doğrular. Bu nedenle O sütunu kapanmadan V sütunu da o syntax özelliği için kapanmış sayılamaz.

## CI verification checkpoint — 2026-11-12

- Branch: `refactor/c-core-roadmap`
- Verified commit: `2c7f11b652919c71a2811b65928e1ceda91a8649`
- GitHub Actions run: `43287142976`
- Result: all five jobs passed — Ubuntu build/tests, Windows build/tests, macOS build/tests, ASan/UBSan and Valgrind.
- CTest result: 73/73 passed on the Ubuntu test job. The sanitizer and Valgrind jobs also completed successfully.
- This green result supersedes earlier checkpoint notes in this document that say CI is pending for the same committed fixtures. It does not close any remaining design, OIR or Verifier work.

## Group / method Semantic closure — 2026-11-12

- Group method integrity, public alias and implementation-symbol uniqueness, Group-bound nominal type visibility, inferred/explicit `self`, static/instance dispatch and external-scope Group-qualified calls are covered by source and malformed-AST regressions.
- Instance receivers are implicit: callers cannot supply `self`; remaining positional/named arguments are matched against explicit parameters, with excess, unknown, duplicate and positionally conflicting arguments rejected. Static methods require Group/type-qualified calls; instance methods require an instance receiver.
- Duplicate public names reject overloads; method implementation symbols cannot be shared by aliases or collide with direct Group functions. The normative syntax reference documents this contract.
- GitHub Actions run [43305008261](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/43305008261) passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Group / method Semantic is closed; OIR lowering and independent Verifier remain open.

## Thread module boundary checkpoint — 2026-11-12

- `spawn`, `listen`, `send`, `receive` and `done` now parse as ordinary identifier callees and produce generic `AST_FUNC_CALL` nodes; their runtime/API semantics belong to the Thread library.
- Core Semantic no longer assigns Thread-specific intrinsic identity or scheduler/IPC/lifetime behavior. Legacy serialized `AST_SPAWN`, `AST_SEND`, `AST_RECEIVE`, `AST_DONE` tags are retained for codec compatibility and traversed without new Thread rules; legacy `AST_LISTEN` is rejected rather than treated as an active core construct.
- The old `!(listen): ...` special form is no longer part of the active Parser grammar; use `listen(...)` from the Thread module.
- `AST_THREAD_BLOCK` is core syntax for a complete named payload. Core validates its structure, uniqueness and isolated lexical scope; Semantic collects only explicit copy(...) captures and marks their identifier annotations; direct outer-variable access is rejected. Core does not create or schedule a thread.
- Parser/Semantic regression fixture covers all five ordinary API call spellings and legacy AST_LISTEN rejection. Tests are committed but not executed here; CI remains unverified.

## OS / Thread / I/O intrinsic identity checkpoint — 2026-11-12

- `echo` remains a Core/OS intrinsic identity check. `spawn`, `listen`, `send`, `receive` and `done` are generic Thread module calls; Core does not impose their module signatures or runtime identity rules.
- `prnt`, `prmt`, `write`, `fwrite` use their existing exact AST callee-name validation; malformed/missing/non-identifier callees produce Semantic diagnostics.
- `intr` is also mapped to its exact intrinsic AST callee name; direct AST regression coverage includes the `AST_INTR` tag.
- Direct AST fixtures cover all four accepted identities and a mismatched `fwrite` callee; existing `echo` source fixture remains.
- This validates only front-end AST identity. It does not impose I/O provider, stream, device, file-system, or Thread runtime behavior.
- Tests are committed but not executed in this GitHub workspace; CI status must be checked independently.

## IRQ / register intrinsic audit — 2026-11-12

| AST intrinsic | Semantic signature checkpoint | Tests | Remaining |
|---|:---:|:---:|---|
| `AST_IRQ` / `irq(number, handler)` | ✓ | ✓ | target IRQ range, privilege, handler ABI/callable resolution |\n| `AST_INTR` / `intr(number)` | ✓ | ✓ | target interrupt range and privilege semantics |
| `AST_REG` / `reg(r/w,...)` | ✓ | ✓ | architecture register whitelist, read/write availability, privilege and width |
| Intrinsic callee identity (`irq`, `reg`) | ✓ | ✓ | generic malformed AST argument payload audit remains |

## Semantic intrinsic signature audit — 2026-11-12

Intrinsic signature checks and parser-source regression fixtures have been added for the following subset. This is a focused audit checkpoint, not completion of intrinsic semantics; hardware effects, target capability, privilege, memory safety and lowering remain open.

| Intrinsic family | Structural/signature checks | Regression cases | Remaining |
|---|:---:|:---:|---|
| `addr`, `len`, `peek`, `poke` | ✓ | ✓ | pointer validity/lifetime, bounds/alignment, access width/effects |
| `inb`, `outb` | ✓ | ✓ | target capability, privilege and port width |
| `area`, `zone`, `free` | ✓ | ✓ | ownership/region lifetime, allocation failure, target mapping |
| Intrinsic AST direct malformed fixtures | ✓ | ✓ | Missing/wrong callee identifier regression ✓; null argument and named-arg payload audit remains |

## AST structural audit checkpoint — 2026-11-11

A direct source comparison found **108/108 unique AST tags** with explicit dispatch in the factory-size table, destructor, printer, serializer, deserializer and graph validator. The all-tag codec fixture iterates through the current final tag, `AST_TYPE_EXPR`. This is a structural/lifecycle closure only: it does not imply that all 108 tags are active Parser productions, semantically complete, lowerable to OIR or covered by Verifier.

Parser source spans are stored on AST nodes, validated and serialized. Parser stamping exists across the main production families, but the production-by-production assertion matrix is still open; OIR optional source-span TLVs are also open.

## 1. Bildirimler ve tür sistemi

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Program ve blok | `AST_PROGRAM`, `AST_BLOCK` | ✓ | ✓ | ✓ | ✓ | ~ | ~ | Program declaration chain için AST graph/codec doğrulaması, `no:osmodule` policy/span ve ortak node source-span codec round-trip; OIR span metadata ayrı aşamada |
| Değişken `v:`, sabit `c:` | `AST_VAR_DECL` | ✓ | ✓ | ✓ | ✓ | ~ | ~ | `c:` compile-time initializer; `v:` inference, declared-type compatibility, inline array/vector and recursive type validation Semantic ✓; inline_type/type_expr payload round-trip ve ortak source-span codec açık |
| Immutable `m:` | `AST_MUST_DECL` (103) | ✓ | ✓ | ✓ | ✓ | D | — | Derleme zamanı/sabit değer sınırları; runtime lowering gerektirmeyen karar korunmalı |
| NewType `nt:` | `AST_NEWTYPE_DECL` | ✓ | ✓ | ✓ | ✓ | ? | ? | Nominal type identity, alias-chain target validation, assignment/call/return compatibility ve recursive target validation Semantic ✓; target_type_expr payload round-trip ve OIR/Verifier açık |
| Struct / packed Struct | `AST_STRUCT_DECL` | ✓ | ✓ | ✓ | ✓ | ? | ? | Name/duplicate field/type/visibility/export/packed metadata Semantic ✓; ordered field payload round-trip, physical layout and lowering açık |
| Struct literal | `AST_STRUCT_LITERAL`, `AST_STRUCT_FIELD_INIT` (106–107) | ✓ | ✓ | ✓ | ✓ | ~ | ~ | Instance initialization/layout lowering ve Verifier invariant'ları |
| Enum / variant / payload | `AST_ENUM_DECL`, `AST_ENUM_VARIANT` | ✓ | ✓ | ✓ | ✓ | ? | ? | Variant/payload validation, recursive payload type checks, constructor resolution/argument binding, scan payload binding/exhaustiveness and signed-i64 discriminants covered by Semantic regressions; full CI verified; OIR/Verifier enum representation remains open |
| Union | `AST_UNION_DECL` | ✓ | ✓ | ✓ | ✓ | ? | ? | C-like shared-storage contract: no active-member tracking; cross-member reads are permitted as target-ABI-dependent reinterpretation; field/type/member diagnostics and regression fixtures pass full CI; union layout/ABI and OIR lowering remain open |
| Group / method | `AST_GROUP_DECL`, `AST_METHOD` | ✓ | ✓ | ✓ | ✓ | ? | ? | Public alias/implementation symbol uniqueness; Group-bound nominal type visibility; inferred/explicit `self`; static/instance dispatch; implicit receiver exclusion; positional/named argument binding, duplicate/unknown/conflicting argument rejection and malformed AST validation pass full CI; overload desteklenmiyor (public ad benzersiz); OIR/Verifier ayrı aşamalar |
| Pointer / reference `*T`, `**T`, `&T` | Şimdilik type-name string alanları | ✓ | ✓ | ✓ | ~ | ? | ? | Borrow/ownership, mutability, nullability, lifetime ve ABI |
| Implicit type variable `t` | Type-name string / `AST_TYPE_EXPR` | ✓ | ✓ | ✓ | ~ | D | — | Type-variable scope, binding, unification ve inference |
| Map | Adlandırılmış type-name | ✓ | ✓ | ✓ | ~ | ? | ? | Key/value generic sözleşmesi, key constraints ve runtime davranışı |
| Vec / SIMD `Vec2`, `Vec3`, `Vec4` | Adlandırılmış type-name | ✓ | ✓ | ✓ | ~ | ? | ? | Parser type-name ve exact declaration span testleri mevcut; element type, kapasite/uzunluk ve operasyon semantiği açık |

## 2. Fonksiyonlar, callback'ler ve modüller

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Fonksiyon `f:name(...)` | `AST_FUNC_DECL` | ✓ | ✓ | ✓ | ~ | ~ | ~ | params/body, return array, recursive return type, EXF linkname ve source-span payload round-trip; Semantic/ABI açık |
| Extern `exf:` | `AST_EXTERN_DECL` | ✓ | ✓ | ✓ | ~ | ? | ? | External signature, symbol/linkname ve calling convention |
| Lambda / anonymous function | `AST_LAMBDA` (94) | ✓ | ✓ | ✓ | ~ | ~ | ? | Capture, closure conversion, return inference ve ABI |
| Callback/function type `f:(...)!R` | `AST_TYPE_EXPR` (108) ve recursive type payload | ✓ | ✓ | ✓ | ~ | ? | ? | Callback compatibility, function value call ve function-pointer ABI |
| Variadic parameter `...` | `AST_VARIADIC_PARAM` (101) | ✓ | ✓ | ✓ | ~ | D | — | Variadic ABI ve call validation; OIR bridge tek başına yeterli değil |
| Named argument | `AST_NAMED_ARG` (102) | ✓ | ✓ | ✓ | ~ | D | — | name/value payload ve source-span round-trip; Semantic parameter binding açık |
| Default parameter | `AST_VAR_DECL.initializer` | ✓ | ✓ | ✓ | ~ | ? | ? | Default expression evaluation ve çağrı argümanı üretimi |
| Fonksiyon çağrısı | `AST_FUNC_CALL` | ✓ | ✓ | ✓ | ~ | ? | ? | Target resolution, overload/arity/type ve call lowering |
| Module import `use` / alias / dotted path | `AST_USE` | ✓ | ✓ | ✓ | ~ | D | — | Arama sırası, module resolution, import graph ve diagnostics |
| Link directive / external symbol metadata | Mevcut directive/extern alanları | ✓ | ✓ | ✓ | ~ | ? | ? | EXTERN linkname string ve AST payload round-trip; build/link integration AST dışı sorumluluk |

## 3. İfadeler, operatörler ve temel değerler

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Sayı/string/char/bool/null literal | `AST_LITERAL` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Numeric overflow/conversion, null compatibility ve literal type inference |
| Identifier / symbol reference | `AST_IDENTIFIER` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Symbol resolution ve Semantic use-site ilişkileri |
| Unary / postfix (`!`, `~`, `++`, `--`) | `AST_UNARY_EXPR`, `AST_POSTFIX_EXPR` | ✓ | ✓ | ✓ | ~ | ? | ? | Parser fixture verifies postfix `++` / `--`, exact spans and rejects non-normative prefix forms; lvalue, operand type, side effect ve lowering remain later-stage work |
| Arithmetic `+ - * / % ^^ ` | `AST_BINARY_EXPR` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Promotion, overflow, divide-by-zero ve power opcode/lowering |
| Compare/equality | `AST_BINARY_EXPR` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Parser fixtures verify relational `>=` / `<=`, equality `==`, `!=`, `===`, `==!`, unary `!` / `~` and exact expression spans; type rules and lowering remain later-stage work |
| Logical `&& \|\|` | `AST_BINARY_EXPR` | ✓ | ✓ | ✓ | ✓ | ~ | ~ | Short-circuit CFG/lowering ve verifier CFG invariants |
| Bitwise `&, \|, ^, ~`|.. | ✓ | ✓ | ✓ | ~ | ~ | ~ | Width/promotion ve tüm operand kombinasyonları |
| Shift `<<< >>>` | `AST_BINARY_EXPR`; OIR 26/27 | ✓ | ✓ | ✓ | ~ | ~ | ✓ | Scalar lowering sınırlarını genişlet; negatif dinamik count trap ve genişlik kuralları |
| Assignment / compound assignment | `AST_ASSIGNMENT` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Lvalue, compound equivalence, mutation effects ve opcode lowering |
| String append `.=` | `AST_ASSIGNMENT` | ✓ | ✓ | ✓ | ~ | ? | ? | AST_ASSIGNMENT operator token `.=` round-trip; Semantic string type ve runtime concat açık |
| Cast | `AST_CAST` | ✓ | ✓ | ✓ | ~ | ? | ? | Conversion legality, truncation/sign/float behavior ve lowering |
| Tuple | `AST_TUPLE` | ✓ | ✓ | ✓ | ? | ? | ? | Tuple type, destructuring, member/index ve lowering |
| Member access | `AST_MEMBER_ACCESS` | ✓ | ✓ | ✓ | ~ | ? | ? | Field/method resolution, visibility ve address/value category |
| Function/member call | `AST_FUNC_CALL` + member target | ✓ | ✓ | ✓ | ~ | ? | ? | Receiver, dispatch ve call ABI |
| Range `..` | `AST_BINARY_EXPR(TOKEN_RANGE)` | ✓ | ✓ | ✓ | ~ | ? | ? | Inclusive endpoint type, iteration semantics ve loop lowering |

## 4. Diziler, aggregate erişimi ve bellek işlemleri

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Array literal `[a,b]` / nested / empty | `AST_ARRAY_LITERAL` (97) | ✓ | ✓ | ✓ | ~ | ? | ? | Aggregate allocation/value representation ve executable lowering |
| Array type `T[]`, `T[n]`, `T[m][n]` | `AST_ARRAY_TYPE` (98), `AST_ARRAY_DIMENSION` (99), `AST_TYPE_EXPR` (108) | ✓ | ✓ | ✓ | ~ | ? | ? | Type compatibility tamamlanmış dilimlerinin lowering'i ve bounds/runtime layout |
| Array index / indexed assignment | `AST_INDEX_ACCESS` (21) | ✓ | ✓ | ✓ | ✓ | ? | ? | Bounds check, multidimensional flattening, load/store lowering |
| Struct field initialization/access | Struct literal + Semantic field relation | ✓ | ✓ | ✓ | ✓ | ~ | ~ | Aggregate instance layout, field address/value ve Verifier checks |
| Address/length `addr`, `len` | `AST_ADDR`, `AST_LEN` | ✓ | ✓ | ✓ | ~ | ? | ? | Signature checks/tests ✓; pointer lifetime ve lowering açık |
| Memory access `peek`, `poke` | `AST_PEEK`, `AST_POKE` | ✓ | ✓ | ✓ | ~ | ? | ? | Arity/type tests ✓; width, alignment, bounds ve target effects açık |
| Map / Vec indexing and operations | Generic member/index representation | ✓ | ✓ | ✓ | ~ | ? | ? | Map index-read and capture-write Parser forms are covered; Vec/container-specific typing and Semantic constraints remain open |
| Packed bit storage `bit`, width-qualified bit/byte types | Scalar/array type payloads | ✓ | ✓ | ✓ | ~ | ~ | ~ | bit/byte width type strings round-trip; packed indexing, LSB-first bounds/read-modify-write ve OIR contract açık |

## 5. Kontrol akışı, döngüler ve fonksiyon çıkışı

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| `if / elsif / els` | `AST_IF` (24) | ✓ | ✓ | ✓ | ~ | ~ | ~ | Chain canonicalization, scope/condition typing ve CFG round-trip |
| Unified `loop` (range/count/foreach/conditional/infinite) | `AST_LOOP` (25) | ✓ | ✓ | ✓ | ~ | ? | ? | Bound validation, iteration semantics ve CFG lowering |
| `unroll` | `AST_UNROLL` (96) | ✓ | ✓ | ✓ | ~ | ? | ? | Compile-time count, legality ve canonical loop lowering |
| `break / continue` | `AST_BREAK`, `AST_CONTINUE` | ✓ | ✓ | ✓ | ? | ? | ? | Loop target resolution ve CFG edge generation |
| Return `ret` | `AST_RETURN` (26) | ✓ | ✓ | ✓ | ~ | ~ | ~ | Return type, all-path return ve terminator contract |
| Conditional control `(cond) ? action [: alternative]` | Canonical `AST_IF` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Parser follows normative `?` grammar for control-transfer, expression and block arms; `->` remains data Flow pipe; loop context, action legality ve round-trip remain later-stage work |
| Labels / jump `label:>`, `?>(label)`, `jump(label)` | `AST_NEXUS_LABEL`, `AST_NEXUS_JUMP` | ✓ | ✓ | ✓ | ~ | ? | ? | Label identity/scope, duplicate/missing target ve CFG lowering |
| Targeted/targetless halt `?;` | `AST_NEXUS_HALT` | ✓ | ✓ | ✓ | ? | ? | ? | Cleanup ordering, target resolution ve terminal CFG behavior |
| Block cleanup `} ?;` | `AST_BLOCK_HALT` (100) | ✓ | ✓ | ✓ | ? | ? | ? | AST_BLOCK_HALT payload-free header/source span ve legal block ownership validation; OIR cleanup order açık |
| `defer` | `AST_DEFER` (69) | ✓ | ✓ | ✓ | ? | ? | ? | Scope exit ordering, unwind and cleanup lowering |
| `panic(...)` | `AST_PANIC` (39) | ✓ | ✓ | ✓ | ~ | ? | ? | Message restriction tests, trap/abort semantics ve lowering |

## 6. Pattern matching ve rule engine

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| `select` / arms | `AST_SELECT`, `AST_SELECT_ARM` (81–82) | ✓ | ✓ | ✓ | ~ | ? | ? | Subject expression type now propagates into arm bindings; closed-enum constructor/type resolution, exhaustiveness ve lowering |
| `scan` / arms | `AST_SCAN`, `AST_SCAN_ARM` (83–84) | ✓ | ✓ | ✓ | ~ | ? | ? | Enum constructor lookup, payload binding types/arity and invalid bare payload variant checks ✓; duplicate arm detection, exhaustiveness and non-enum structural patterns open |
| Pattern binding | `AST_PATTERN_BINDING` (85) | ✓ | ✓ | ✓ | ~ | ? | ? | Enum payload field type propagation and bare no-payload variant tags ✓; duplicate pattern binding and binding lifetime constraints open |
| Constructor pattern | `AST_PATTERN_CONSTRUCTOR` (86) | ✓ | ✓ | ✓ | ~ | ? | ? | Enum/variant identity, positional payload arity/type validation and typed binding ✓; duplicate coverage, exhaustive scan and constructor expression validation open |
| `when` guard | Arm body içindeki canonical `AST_IF` | ✓ | ✓ | ✓ | ~ | ? | ? | Pattern binding type inference ve guard evaluation lowering |
| Ruleset / barrier / trigger / always / default | `AST_RULESET_DECL` + rule nodes (87–91) | ✓ | ✓ | ✓ | ~ | ? | ? | Ordering, termination, effects, event dispatch ve lowering |
| Rule application `apply` | `AST_APPLY_RULES` (92) | ✓ | ✓ | ✓ | ~ | ? | ? | Active apply context, injected effects ve lowering |
| `on` blocks | `AST_ON_BLOCK` (93) | ✓ | ✓ | ✓ | ~ | ? | ? | Active apply context, trigger dispatch ve effects |

## 7. Nexus Flow, ownership, memory, I/O ve intrinsics

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Flow pipe / zone operators `-> >> <<` and catch/fallback/ignore | `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | ✓ | ✓ | ✓ | ~ | ? | ? | Parser fixtures verify `<<` / `>>` distinct from numeric `<<<` / `>>>`, `?->` / `?=>` / `!->`, `(e)` binding and catch-block spans; obsolete `=?>` / `<--` forms are rejected; pipe/zone semantics and execution lowering remain later-stage work |
| Handler capture `(h)<-expr` | `AST_NEXUS_CAPTURE` | ✓ | ✓ | ✓ | ~ | ? | ? | Occupancy, ownership, move/copy and lifetime |
| Relocate `_>` | `AST_RELOCATE` (95) | ✓ | ✓ | ✓ | ~ | ? | ? | Source invalidation, destination initialization and explicit move effect |
| Legacy unpack spelling `...>` | Reserved `AST_UNPACK` (38); no active production | ✓ (legacy token) | X | ✓ (reserved codec tag) | D | D | — | Infix operator is rejected by Core Parser. When needed, expose decoding as an ordinary `coremodule.unpack(data)` function after its input/output and error contract is specified; file I/O and normalization remain separate operations |
| `area / zone` | `AST_AREA`, `AST_ZONE` | ✓ | ✓ | ✓ | ~ | ? | ? | Size/type checks ✓; region ownership and target mapping open |
| I/O `echo/prnt/prmt/write/fwrite` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ? | ? | ? | Central intrinsic signature/effect registry and OS module lowering |
| Port I/O `inb/outb`, interrupt `irq/intr`, register `reg` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ~ | ? | ? | `inb/outb` + `irq/reg/intr` signature tests ✓; target capability/privilege open |
| Reflection `typeof/sizeof/is_*` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ~ | ? | ? | Compile-time/runtime distinction, type result and lowering |
| Date/time/clock/free | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ? | ? | ? | Signature, platform service and effect contract |
| Zone sync/view/lock member calls | Generic member/call AST; reserved dedicated tags | ✓ | ✓ | ✓ | ? | ? | ? | Intrinsic registry; do not count reserved tags as active productions |
| Intrinsic AST inventory (exit/swap/option checks/date/time/clock/free/zone) | `AST_EXIT`, `AST_SWAP`, `AST_IS_OK`, `AST_IS_ERR`, `AST_IS_SOME`, `AST_IS_NONE`, `AST_IS_NULL`, `AST_DATE`, `AST_DATENOW`, `AST_TIME`, `AST_CLOCK`, `AST_FREE`, `AST_ZONE` | ✓ | ✓ | ✓ | ? | ? | ? | Central signature/effect registry, compile-time/runtime distinction and per-intrinsic type/effect tests |
| Ownership/copy/move and cleanup | Capture/relocate/defer/halt nodes; `copy()` is a generic call | ✓ | ✓ | ✓ | ~ | ? | ? | capture, relocate, defer, halt ve capture-assignment operator payload round-trip; Semantic ownership/effect açık |

## 8. Concurrency, FASTEXEC/ASM ve ertelenmiş syntax

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Thread block `(name)>{...}` | `AST_THREAD_BLOCK` (70) | ✓ | ✓ | ✓ | ~ | ? | ? | explicit-copy capture metadata eklendi; runtime snapshot/transfer, lifetime ve cancel/sync |
| `spawn(name)` | `AST_FUNC_CALL(identifier)` | ✓ | ✓ | ✓ | ~ | ? | ? | Semantic named-payload lookup/arity/type validation; Thread runtime create/schedule/transfer |
| `listen/send/receive/done` | `AST_FUNC_CALL(identifier)` | ✓ | ✓ | ✓ | ? | ? | ? | Thread module signatures, payload/event effects, synchronization ve lowering |
| FASTEXEC block | `AST_FASTEXEC_BLOCK` (104) | ✓ | ✓ | ✓ | ~ | D | — | ordered statement payload ve node source-span round-trip; target/capability Semantic checks açık |
| ASM label/raw body | `AST_ASM_LABEL` (105), opaque ASM token | ✓ | ✓ | ✓ | ~ | D | — | label/source_text/source_length ve source-span payload round-trip; asmcall/jmp placeholders, register/clobber, ABI ve target checks açık |
| Intent `@` | `AST_NEXUS_INTENT` (36) | D | D | ✓ | D | D | — | Tasarım aktif edilene kadar davranış eklenmeyecek |
| AI logic `??` | `AST_AI_LOGIC` (37) | D | D | ✓ | D | D | — | Payload ve semantics tasarlanmadan uygulanmayacak |
| Legacy generic ASM block | `AST_ASM_BLOCK` (29) | D | X | ✓ | D | D | — | ID/codec uyumluluğu korunur; güncel syntax FASTEXEC/ASM |
| Reserved zone sync/view/lock tags | `AST_ZONE_SYNC/VIEW/LOCK` (78–80) | D | X | ✓ | D | D | — | Üye çağrıları generic call AST olarak temsil edilir |
| Compile-time macros / preprocessing directives | Runtime AST dışında | ? | X | — | — | — | — | Ayrı compile-unit/preprocessor katmanı tasarlanmalı |

---

## Öncelik sırası — bundan sonra neyi bitireceğiz?

Geliştirme sırası kullanıcı tarafından sabitlenmiştir. Her aşama kendi kapsamı, testleri ve belgeleriyle kapatılmadan sonraki aşamaya geçilmez:

| Sıra | Aşama | Kapsam | Aşamayı kapatma ölçütü |
|---:|---|---|---|
| 1 | **L — Lexer** | Token envanteri, spelling, longest-match, literal/escape, source span ve hata davranışı | Normatif syntax/token envanteri eşleşir; pozitif/negatif Lexer testleri geçer. Mevcut durum büyük ölçüde tamam; yalnızca kalan farklar kapatılır. |
| 2 | **P — Parser** | Her aktif syntax için grammar production, precedence/associativity, ambiguity, recovery ve exact source span | Her aktif satır parser fonksiyonu ve pozitif/negatif fixture ile kanıtlanır; ertelenen/ret edilen syntax açıkça işaretlenir. |
| 3 | **A — AST + Payload** | Node payload alanları, child/list/sibling, ownership, lifecycle, source span ve codec | Her aktif üretim doğru AST'yi üretir; field-level round-trip, malformed input, OOM ve graph validation testleri geçer. |
| 4 | **S — Semantic** | Symbol/scope/type/conversion/value category/ownership/effect/control-flow | Aktif syntax'ın anlam kuralları pozitif/negatif testlerle kapanır; Semantic AST artifact zorunlu annotation ve referansları doğrulanır. |
| 5 | **O — OIR** | Semantic AST → OIR lowering, opcode/type/effect/memory/CFG sözleşmeleri | Her executable özellik desteklenir veya konumlu fail-closed diagnostic ile açıkça ertelenir; lowering testleri geçer. |
| 6 | **V — Verifier** | OIR schema, structural/type/SSA/CFG/ownership/resource invariants ve pipeline gate | Geçerli artifact kabul edilir; bozuk/adversarial artifact reddedilir; host downstream aşamaları durdurur; CI matrisi geçer. |

**Çalışma kuralı:** L (Lexer) ve P (Parser) kapıları tamamlandı. A (AST + Payload) kapsamındaki tüm aktif payload aileleri artık round-trip, kaynak aralığı ve sahiplik doğrulamasıyla kapatılmıştır. `X` ve `D` satırları bilinçli olarak aktif kapsam dışındadır. Sıradaki aşama **S — Semantic**; ardından O ve V sırası korunacaktır. Her aşama kendi test ve sözleşmeleriyle kapanmadan sonraki aşamaya geçilmez.

## Mevcut doğrulanmış ilerleme

**A kapanış kanıtı (2026-11-11):** 108 AST tag için lifecycle dispatch ve canonical round-trip; her tag için source-span round-trip; `ASTProgram` policy/span, variable/function/newtype recursive type payloads, EXF linkname, USE namespace, assignment operator variants, six recursive type-expression kinds, bit/byte type strings, FASTEXEC/ASM payloads ve ownership/cleanup nodes test fixture'larında dolu alanlarla doğrulandı. CI run `43088573355` 5/5 başarılı (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind).
**L/P kapanış kanıtı (2026-11-11):** Aktif Lexer tokenları ve Parser üretimleri test kapsamıyla eşleştirildi. Parser testleri ayrı/bitişik declaration öneklerini, expression-body function span'lerini, indexed capture assignment, Vec2/3/4 ve tüm tanımlı bit/byte width type yazımlarını, copy() generic call biçimini kapsar. `...>` aktif operatör değildir; `spawn`, legacy ASM ve diğer `D/X` satırları açık kapsam sınırlarıdır.

**Parser kanıtı:** Bu satırlardaki P=✓ işareti, ilgili üretimin aktif Parser fixture'ı ve AST/source-span doğrulaması bulunduğunu gösterir; Semantic/OIR/Verifier tamamlandığı anlamına gelmez.

- AST Graph Validator, versioned AST codec ve allocation-failure testlerinin önemli bir bölümü mevcut; ancak bu, tüm syntax/AST payload sözleşmelerinin kapandığı anlamına gelmiyor.
- Callback/function-type, array, Struct literal, select/scan, ruleset, FASTEXEC/ASM gibi AST köprüleri eklenmiş durumda; ilgili Semantic veya lowering işleri açık kalabiliyor.
- OIR Verifier için schema 1.2, opcode/type/SSA/CFG ve mixed-width shift-count testleri var. Host'un verifier reddinde downstream aşamaları durdurduğu negatif entegrasyon testi de mevcut.
- 2026-11-11 tarihli CI run **43073952941**: Ubuntu, Windows, macOS, ASan/UBSan ve Valgrind **5/5 başarılı**. Bu CI, son Verifier karmaşıklık/çalışma alanı iyileştirmelerini doğrular; Verifier aşamasının tamamlandığını göstermez.

## Kaynak belgeler

- [Normatif Syntax Reference](docs/language/syntax-reference.md)
- [Ayrıntılı Syntax ↔ AST Matrix](NEXSUS_AST_SYNTAX_MATRIX.md)
- [OCC Pipeline Progress](OCC_PIPELINE_PROGRESS.md)
- [AST Representation Gaps](docs/AST_REPRESENTATION_GAPS.md)
- [OIR Opcode Registry](OIR_OPCODE_REGISTRY.md)
- [OIR Verifier Specification](OIR_VERIFIER_SPEC.md)

**Güncelleme kuralı:** Bu dashboard'daki bir hücre yalnızca ilgili kaynak kodu, test ve CI kanıtı incelendikten sonra ✓ yapılır. Bir syntax satırının tamamı ancak gerekli tüm aşamalar kapanınca tamamlanmış sayılır.

## P — Parser tamamlanma kontrolü

Bu bölüm yalnızca Parser kapsamını izler. Lexer, AST payload, Semantic, OIR ve Verifier sütunlarının durumunu değiştirmez.

| Parser alt kapsamı | Güncel doğrulama | Durum |
|---|---|---|
| Operatör AST üretimi | `.=` , `==`, `!=`, `===`, `==!`, `>=`, `<=`, `!`, `~`, postfix `++/--`, `<<< / >>>` ve `<< / >>` için AST/operand/span fixture'ları eklendi | CI bekliyor |
| Flow ve koşullu kontrol | `?->`, `?=>`, `!->`, `(e){...}`, `(condition) ? action [: alternative]`, normal `->` pipe ve rolling-retry ayrımı için fixture'lar eklendi | CI bekliyor |
| Bildirim ve payload kaynak aralıkları | Fonksiyon imzası/parametreler, EXF, pointer/reference, implicit type, Group/method/nested default, literal ve modulo düğümleri için exact-span assertion'ları genişletildi | CI bekliyor |
| Eski/ertelenmiş syntax sınırı | `...>`, `=?>`, `<--`, `@`, `??`, prefix `++/--` ve eski `(condition)->action` biçimleri negatif Parser fixture'larıyla reddediliyor | CI bekliyor |
| Tüm aktif üretimlerin son taraması | Her aktif syntax satırının pozitif/negatif fixture ve exact source-span kanıtı son kez çapraz kontrol edilecek | Açık |

**Doğrulama notu:** Son Parser değişiklikleri aktif dalda; GitHub Actions işleri henüz kuyruğa alınmış durumda. Bu nedenle yeni fixture'lar başarılı test olarak işaretlenmedi. CI tamamlanınca bu tablo ve ana P sütunu sonuçlara göre güncellenecek.


### Semantic checkpoint — subject-aware Select/Scan binding — 2026-11-12

- Select selector and Scan subject are semantically checked before their arm scopes are analyzed.
- Pattern binding slots inherit the checked subject expression type instead of unconditional `any`; recursive constructor-pattern bindings currently inherit that same subject type until enum payload-specific mapping is implemented.
- SemanticResult symbol type IDs are updated for inferred arm bindings, and durable annotations use the inferred type.
- Added positive and negative Semantic tests for Select/Scan binding inference (`i32` binding accepted, incompatible `str` assignment rejected).
- Commits: `92f43cbb` (implementation), `2af1952e` (tests). CI result pending at checkpoint.
- This is a focused Semantic improvement, not closure of pattern matching: constructor payload-specific typing, enum variant validation, exhaustiveness, and lowering remain open.


### Semantic/Parser CI correction — 2026-11-12

- First checkpoint CI exposed stale Semantic control-flow fixtures using the retired `(condition) -> action` spelling. Updated them to the normative `(condition) ? action` syntax; parser remains strict and does not revive the old form.
- Parser regression fixture had six `ast_validate(root)` calls missing the required diagnostic-output argument; corrected them to `ast_validate(root, NULL)`.
- These are test-harness corrections, not new language syntax. Re-run CI and only mark Semantic checkpoint verified after the latest commit matrix passes.


### Semantic checkpoint — block cleanup marker — 2026-11-12

- `AST_BLOCK_HALT` (`}?;`) now receives a SemanticResult annotation as a `void` rvalue cleanup marker instead of falling through silently.
- Semantic explicitly treats the node as a valid cleanup instruction; execution/unwind ordering remains an OIR cleanup contract and is not claimed implemented here.
- Added a valid nested-block cleanup Semantic fixture.
- Commits: `5db1589f` (Semantic annotation), `3eece891` (test). CI pending.


### Semantic checkpoint — implicit `t` generic inference — 2026-11-12

- `t` is now recognized as the language's implicit type-variable marker and represented by `SEMANTIC_RESULT_TYPE_INFERRED` rather than an unknown named type.
- Type compatibility treats `t` as a unification placeholder. Generic function calls whose return is `t` infer the call expression's result from the first actual argument (the current minimal inference rule).
- Numeric validation permits arithmetic on matching `t` operands inside generic bodies; concrete call-site constraints beyond argument/return compatibility remain limited.
- Added generic identity and generic sum Semantic fixtures with concrete `i32` call sites.
- Commits: `1a8f8765` (Semantic), `18583bf6` (tests). CI pending.
- Limit explicitly retained: no general multi-parameter type-variable unification or constrained generic type system yet.


### Parser span correction — parenthesized Nexus Flow right operand — 2026-11-12

- `AST_NEXUS_FLOW` source range previously ended at the inner right-expression token when the right operand was parenthesized (for example `operation() ?=> (data)`), excluding the closing grouping delimiter.
- Parser now stamps the Flow production from its left child through the actual final consumed token, preserving the full grouped operand span.
- The catch/fallback/ignore exact-span fixture retains the closing `)` and now verifies the complete source production.
- Commit: `5d301071`. CI pending.


### Semantic checkpoint — tuple values and static indexing — 2026-11-12

- Removed the previous fail-closed tuple diagnostic: tuple elements are semantically visited, the tuple expression is annotated as `tuple`, and SemanticResult uses its existing `TYPE_TUPLE` kind.
- Static tuple indexing now returns the selected element's inferred type; negative and out-of-range constant indexes emit Semantic diagnostics. Dynamic tuple indexes remain `any` (runtime dispatch/bounds belongs to OIR).
- Added heterogeneous tuple inference/index positive cases and out-of-range negative case.
- Commits: `37c87fea`, `0b06e621` (Semantic), `cd3795d9` (tests). CI pending.


- Tuple implementation also moved `AST_TUPLE` out of the Semantic fail-closed regression list; `AST_ASM_BLOCK` remains intentionally rejected pending its explicit semantic/ABI contract. The earlier CI failure was a stale test expectation, not a tuple implementation failure.


- Follow-up test exposed that constant tuple indexing through an identifier initially lacked access to the initializer's tuple shape. Semantic now follows the identifier's declaration to its tuple initializer before resolving the element; this fixes both element-type inference and static out-of-range diagnostics. Commit `45752564`; CI pending.


### Parser semantic grammar closure — catch-flow handler block — 2026-11-12

- The normative `?-> (error) { handler }` catch form now requires its handler block; `?-> (error);` no longer parses as a valid catch flow.
- This closes a parser regression fixture that previously passed only because the grammar accepted a missing handler. It is a grammar enforcement correction; Semantic still handles arm/catch scope/effects separately.
- Commit: `4c057072`; CI pending.


### Semantic checkpoint — Loop condition and range validation — 2026-11-12

- Loop conditions are now required to be Boolean unless represented as the language's range form.
- Range endpoints must have compatible numeric types; invalid scalar conditions and incompatible/non-numeric ranges emit Semantic diagnostics.
- Added a negative fixture for `loop(1)`; canonical Boolean loops remain valid.
- Commits: `2f8d4e50` (Semantic), `040fdccd` (tests). CI pending.


### Semantic checkpoint — function type / NewType target validation — 2026-11-12

- Recursive callback/function signature spellings `f:(...)` are now recognized as Semantic function types and interned as `SEMANTIC_RESULT_TYPE_FUNCTION`.
- NewType declarations validate their target type instead of silently accepting unresolved names; callback aliases remain valid targets.
- Added positive callback NewType and negative unknown NewType target fixtures.
- Commits: `88997965` Semantic, `5f287a01` tests. CI pending.


### Semantic checkpoint — variadic function parameters — 2026-11-12

- Semantic function signatures now separate fixed parameters from the final `AST_VARIADIC_PARAM` marker. Fixed parameter type slots no longer cast the marker as an `ASTVarDecl`.
- Variadic calls enforce the minimum fixed argument count and accept extra arguments; fixed arguments remain type-checked. SemanticResult function-parameter relations safely emit a marker relation with no fake variable symbol.
- Added positive variadic call and missing-fixed-argument negative tests.
- Commits: `f081c798` (Slot contract), `2bfddd11`, `f1d1940a`, `0cf7f812` (Semantic), `115ac855` (tests). CI pending.


### Semantic checkpoint — named call arguments — 2026-11-12

- `AST_NAMED_ARG` values now receive Semantic checking and rvalue type annotations.
- Call validation resolves each named argument against the declared parameter name, uses that parameter's type for compatibility, and reports unknown names instead of silently skipping the wrapper.
- Added reordered named-argument success and unknown-name failure fixtures.
- Commits: `e20b2624` (argument mapping), `da982f2a` (node checking/annotation), `24c0cd38` (tests). CI pending.


- Tuple bounds diagnostics were initially emitted both during type inference and during AST index checking. Moved the diagnostic to the dedicated index-check pass so an invalid constant index produces exactly one error. Commit `9ce304ae`; CI pending.


### Semantic checkpoint — inferred array literal element compatibility — 2026-11-12

- Inferred array literals now semantically visit elements, annotate as `array`, and require a compatible common element type. Heterogeneous inferred arrays no longer silently pass.
- Added homogeneous positive and heterogeneous negative fixtures.
- Commits: `7017200b` Semantic, `8102116e` tests. CI pending.


- CI compile caught a missing local `int64_t constant_index` declaration in the tuple AST index checker; added it before the next run. Commit `48ab91cf`. No behavior change.


- Array literal compatibility was already centralized in `get_expr_type`; the first implementation duplicated its diagnostic path. Removed the duplicate Semantic traversal check and retained the single authoritative element compatibility/type annotation path. Commit `aa81d9ae`.


### Semantic test suite checkpoint — 2026-11-12

- On the `aa81d9ae` CI head, Semantic tests (including generic inference, tuple indexing/bounds, NewType, variadic, named args, cleanup marker, loop condition and array homogeneity) passed; the CTest matrix reached 72/73.
- The historical `aa81d9ae` run had one Parser fixture failure. This was subsequently corrected; current branch-head CI run #4269 passes the full matrix.


- Named-argument follow-up: duplicate parameter binding and positional-after-named forms now produce Semantic diagnostics. Added regression fixtures. Commits `3f86c6da` Semantic, `cfb75499` tests; included in green branch-head CI run #4269.


### Lexer/Parser correction — contextual `(e)` catch handler — 2026-11-12

- Lexer no longer collapses every literal `(e)` into a special token. That tokenization broke ordinary expressions such as `echo(e)` inside a catch body.
- Parser now recognizes `(e)` contextually only after `?->`, constructs the handler identifier and consumes its parentheses, then attaches the following block.
- This restores normal call/group tokenization while preserving the catch syntax. Added Parser diagnostic tracing; the exact fixture and cross-platform matrix pass in branch-head CI run #4269.
- Commits: `81f1da3e` (Lexer), `76176c20` (Parser).


- Ownership split correction: duplicate named labels and positional-after-named are already rejected by Parser (and covered in its variadic/named argument regression); removed those parser-invalid sources from Semantic's `check_source` fixtures. Semantic coverage remains reordered named-argument type checking and unknown-name diagnostics, with defensive duplicate/order checks for externally constructed ASTs.


### Semantic checkpoint — shift count operand-width validation — 2026-11-12

- Constant shift counts are now validated against the left operand's integer width: count < width is required.
- Negative counts remain rejected with the offending count in the message. Out-of-range diagnostics now include the actual shift count, left operand type/bit width, and valid range (e.g. `u8`, 8-bit, `0..7`).
- Semantic errors are attached to the binary shift AST node's source span; the collector carries file/line/column, and the legacy Error record retains line/column. i32 shifts by 32/33 and u8 shifts by 8 now produce Semantic errors; boundary-valid u8 << 7 and i64 >> 63 remain accepted.
- Dynamic shift counts remain accepted at Semantic time because their value is not statically known; OIR/runtime must define fail-safe behavior for counts at or above the value width (trap/diagnostic rather than undefined host behavior).
- Added boundary fixtures to test_semantic_arithmetic.c.
- Commits: b1852e7e (Semantic), f0114097 (diagnostic clarity), c6d64269 (tests). Branch-head CI run #4269 passed Linux, Windows, macOS, ASan/UBSan and Valgrind.


### Semantic diagnostics and conditional-control parser correction — 2026-11-12

- Semantic diagnostics now copy the complete AST source span into the shared collector; legacy Error line/column fields fall back to the AST span when compatibility mirrors are absent.
- Shift diagnostics identify the offending count, left operand type and width, and valid range. Bitwise and compound-bitwise diagnostics identify the operator and actual operand types.
- Added regression assertions for diagnostic message content and exact source line/column.
- Arithmetic, shift, comparison, and logical-operator diagnostics now include the operator spelling and offending operand types; `%` explicitly reports its integer-only requirement.
- Unary logical negation and bitwise complement diagnostics identify the rejected operand type. Bitwise regression coverage now includes fixed-width `8bit` operands, mixed representation/integer operands, and compound-assignment diagnostics.
- Assignment validation now fails safely for malformed ASTs with missing target/value children. Compound numeric, bitwise and string-append diagnostics report the exact assignment operator and both operand types.
- Direct variable, indexed-element and aggregate-field assignment mismatches now identify the assignment operator, destination, expected type and supplied value type.
- Named and anonymous struct/union field lists now reject non-field nodes, unnamed fields and duplicate field names; regression fixtures cover duplicate struct and union members.
- NewType targets are validated before symbol registration; invalid targets no longer leak unusable type names into later declarations. Valid alias chains and invalid-target non-leakage have regression fixtures. Nominal identity is also checked for same-type assignment, base-type rejection, sibling-NewType rejection, and function argument/return compatibility.
- Invalid function return types no longer trigger a second dependent return-value mismatch from the function body; the original type diagnostic remains authoritative.
- Added diagnostic regressions for mismatched arithmetic types, invalid shift operand types, non-boolean logical operands, and exact source locations. CI run #4269 (`198f6005`) passed Linux, Windows, macOS, ASan/UBSan and Valgrind after the assignment-diagnostic and duplicate-`AST_FREE` fixes. This confirms the current diagnostic checkpoint; the broader Semantic phase remains open.
- Conditional-control expression arms can now be followed directly by the alternative separator, e.g. `(j == k) ? process(j) : process(k);`; the first expression arm defers its semicolon only when the separator follows.
- Corrected the Parser allocation-failure fixture: it had included the deferred-AI token `??` in a source expected to parse successfully. The fixture now uses the valid halt form `?;`.
- CI run #4195 (`91a7b6e3`) passed the full platform/sanitizer/Valgrind matrix. Semantic as a whole remains open; this checkpoint closes only the documented diagnostic and conditional-control corrections.


### Semantic checkpoint — unary and bitwise operator validation — 2026-11-12

- Unary logical negation now requires a boolean condition; unary minus requires a numeric operand; bitwise complement requires an integer or supported bit-representation operand.
- Unary diagnostics identify the operator and actual operand type. Binary and compound bitwise diagnostics identify the operator and both actual operand types.
- Added positive/negative tests for unary operators, 8bit representation operations, mixed representation/integer operands, and compound &=, |=, ^= operations.
- Added collector assertions for Semantic stage ID, complete source range, and line/column reporting.
- Fixed malformed multi-character C character constants in pointer/reference type validation (`'\\0'` → `'\0'`), parser newline coordinate mapping (`'\\n'` → `'\n'`), and diagnostic test line handling. This also removes compiler warnings and restores the intended terminator/newline checks.
- Valgrind exposed an uninitialized `SemanticContext.current_fast_exec_owner` read in FASTEXEC intrinsic validation. `semantic_init()` now initializes that scope pointer and the reserved `Language` state; regression coverage already exercises `asmcall`/`asmjmp` both inside and outside FASTEXEC.
- After the SemanticContext initialization fix, the latest CI run passed all Linux CTest tests, ASan/UBSan, macOS, and Valgrind (including the FASTEXEC semantic test). Windows also passed; the full CI matrix is green for this checkpoint. This validates the listed changes but does not close the broader Semantic phase.


### Semantic checkpoint — Range expression validation and iterator inference — 2026-11-12

- Range endpoints are now checked for compatible numeric types in every expression context, not only when the range is used as a loop condition.
- Loop range iterator `it` now inherits the lower endpoint's inferred type rather than being hard-coded to i32.
- Added valid/invalid standalone range tests, loop range tests, iterator type-inference coverage, and a source-located diagnostic assertion naming both endpoint types.
- The latest range follow-up is included in branch-head CI run #4269, which passed Linux, Windows, macOS, ASan/UBSan and Valgrind.


### Semantic checkpoint — recursive AST type-expression validation — 2026-11-12

- Recursive type expressions are now semantically validated for named, pointer, reference, array, function and qualified forms.
- Validation is wired into variable declarations, function parameters and return types, NewType targets, and Struct/Enum/Union fields. Unknown nested type names and malformed type-expression shapes report diagnostics at their own AST source spans.
- Function type parameter names must be unique; a variadic marker must be final. The Parser owns rejection of non-final variadic syntax, with a dedicated Parser regression fixture; Semantic retains defensive validation for externally constructed ASTs.
- Invalid NewType targets are not registered in the symbol table. Alias chains and nominal assignment identity are covered: same-NewType assignment is valid, base-type and sibling-NewType assignment are rejected.
- Unknown type diagnostics now name the missing type and its declaration/recursive-type context instead of relying on an untranslated message lookup.
- CI run #4228 (`1628fcda`) passed Ubuntu, macOS, Windows, ASan/UBSan and Valgrind. Semantic remains open for remaining AST families and complete SemanticResult type identity coverage.


### Semantic checkpoint — postfix lvalues, control-flow conditions and array indexing — 2026-11-12

- Postfix increment/decrement now accepts mutable numeric identifiers, aggregate fields and indexed elements; constant/immutable lvalues and non-numeric operands are rejected with operator-specific diagnostics.
- `if`/`elsif` and non-range `loop` conditions now report the actual operand type when it is not `bool`; unknown-name and unary/logical operand diagnostics are not redundantly repeated.
- Array indexing infers an element type from an array-literal initializer, enabling typed indexed operations. Constant negative and out-of-range indices are rejected with the index, array length and valid range; fixed dimensions are checked when available.
- Added tests for identifier/member/index postfix operations, constness, non-numeric operands, if/loop condition types, array element inference and bounds diagnostics.
- This checkpoint is included in branch-head CI run #4269; Linux, Windows, macOS, ASan/UBSan and Valgrind all passed.


### Semantic checkpoint — bit/byte representation operator matrix — 2026-11-12

- Expanded the Semantic arithmetic regression matrix to cover every currently documented bit-vector width (`8bit`, `16bit`, `32bit`, `64bit`, `128bit`), every byte-block width (`1byte`, `2byte`, `4byte`, `8byte`, `16byte`), and scalar `bit` / `byte` operands.
- Added positive binary-operation cases for the full representation matrix, plus focused `128bit`, `16byte`, scalar-bit complement, and compound byte-operation cases.
- Added negative cases for decimal `d64` bitwise operands and incompatible `128bit` / `16byte` representations. Existing diagnostic tests continue to assert operator and actual operand type reporting with exact source locations.
- No Semantic implementation behavior was changed in this checkpoint; the current rule remains that both operands must be bitwise-capable and type-compatible. This closes a coverage gap, not the Semantic phase.
- Test commit: `dad4b6df`. CI verification: pending; do not mark this matrix green until the new branch-head run completes.


### Semantic checkpoint — radix and wide shift-count constants — 2026-11-12

- Closed a constant-evaluation hole in shift validation: the prior signed decimal-only evaluator treated radix-prefixed (`0x`, `0b`, `0o`) and underscore-separated integer literals as non-constant, allowing out-of-width counts to bypass the static range check. Values above `INT64_MAX` could also be mistaken for dynamic counts.
- Added a bounded unsigned-literal parser for decimal/radix forms and digit separators, with explicit `uint64_t` overflow detection. The signed constant evaluator now reuses it for representable non-negative integer literals.
- Shift validation now distinguishes negative constants, in-range unsigned constants, values outside the left operand width, constants exceeding the 64-bit count representation, and genuinely dynamic expressions. Dynamic counts remain deferred to the OIR/runtime trap contract.
- Added positive/negative tests for hexadecimal, binary, octal, digit separators, negative radix counts, `UINT64_MAX`, and values above `UINT64_MAX`; diagnostic tests assert the offending count/range and exact source line.
- Implementation commit: `d074c887`; regression tests: `bdc45c55`. CI verification: pending; do not mark the new cases green until the branch-head run completes.

- Follow-up tests also verify that arithmetic constant expressions containing radix literals are folded for shift-range validation (`0x10 * 2` rejected for i32, `0x3 + 4` accepted for u8). Updated test commit: `d39719dc`.


### Semantic checkpoint — shifts on fixed-width bit/byte values — 2026-11-12

- Found a Semantic mismatch: binary bitwise operators accepted fixed-width representations, but shift validation still required both operands to be ordinary integer primitives. This incorrectly rejected shifts on `bit`, `byte`, `Nbit`, and `Nbyte` values.
- Split the shift contract: the left operand must be bitwise-capable; the shift count must remain an integer primitive. Width validation now uses the exact representation width (`bit`=1, `byte`=8, `Nbit`=N, `Nbyte`=8×N) as well as ordinary integer widths.
- Diagnostics now explain the distinct left-value and count requirements and retain the actual operand types. Added boundary coverage for every documented bit-vector and byte-block width, plus scalar `bit`/`byte`, including out-of-range cases and exact source-line diagnostics.
- Implementation commits: `8421f4c8`, `504ff486`; tests: `35156308`, `cc8fd0e7`. CI verification pending; no green status is claimed until a branch-head workflow confirms the tests.
- Follow-up preserved the existing no-duplicate-diagnostic behavior for unresolved operand types (unknown-name diagnostics remain the primary error). Implementation follow-up: `585020e3`.


### Semantic checkpoint — Map indexing and dynamic values — 2026-11-12

- Aligned Map indexing with the normative syntax examples: documented string-key reads/writes are accepted, and an integer-key case is covered by tests. No key-type whitelist is imposed while the syntax reference leaves key constraints open.
- Map values are intentionally heterogeneous/dynamic in the current language contract, so indexed reads retain the conservative `any` type and indexed writes do not invent a fixed key/value type pair. Key hashability/allowed-type constraints are deliberately left open because the syntax reference does not yet define them.
- Constant-container indexed writes now use a container-neutral diagnostic. Map access no longer receives the array-only integer-index restriction.
- Added positive tests for string-key read/write and integer-key access; no unsupported Map key restriction is introduced.
- Test execution and CI for this checkpoint remain pending; this entry records implementation and regression fixtures, not a green test claim.


### Semantic checkpoint — Vec2/Vec3/Vec4 initializer and arithmetic rules — 2026-11-12

- Implemented the documented fixed dimensions for Vec2, Vec3 and Vec4 array-literal initializers; each component must be numeric and the literal must contain exactly 2, 3 or 4 values respectively.
- Enabled component-wise vector arithmetic for matching Vec2/Vec3/Vec4 dimensions: `+`, `-`, `*`, `/`, `%`, and `^^`; unary minus and compound `+=`, `-=`, `*=`, `/=` are also accepted. Mixed dimensions and vector/scalar mixing remain rejected until a scalar-broadcast rule is specified.
- Added positive Vec2/Vec3/Vec4 initializer tests and Vec3 coverage for every binary arithmetic operator, unary minus and compound arithmetic; negative tests cover dimension mismatch, non-numeric components and vector/scalar mixing. The dimension diagnostic asserts expected/actual component counts and source line.
- Test execution and CI for this checkpoint remain pending; implementation and fixtures are not represented as a green test result.


### Semantic checkpoint — Vector arithmetic syntax and dimension matrix — 2026-11-12

- Normative syntax reference now documents the complete current Vec2/Vec3/Vec4 arithmetic contract: `+`, `-`, `*`, `/`, `%`, `^^`; unary negation; and `+=`, `-=`, `*=`, `/=`.
- The reference explicitly states equal-dimension requirements, rejects scalar/vector mixing pending a language-level broadcast decision, and distinguishes Semantic acceptance from future SIMD Codegen behavior.
- Extended arithmetic regression fixtures so Vec2 and Vec4, like the existing Vec3 fixtures, exercise all six binary arithmetic operators, unary minus and all four documented compound assignments.
- CI status remains pending until a branch-head workflow confirms the expanded test matrix.


### Semantic checkpoint — return statement function context — 2026-11-12

- SemanticContext now tracks function-body nesting independently from the declared return-type pointer. This avoids confusing an invalid/unresolved function return type with being outside a function.
- `ret` at module scope is rejected with an explicit diagnostic; nested/local function bodies establish and restore their own function context naturally.
- Added a module-scope negative fixture and nested-function positive fixture. Syntax reference now documents scope and return-type rules.
- CI remains pending until the updated branch head is tested.


### Semantic checkpoint — nested loop control-flow scope — 2026-11-12

- Confirmed the existing Semantic implementation increments/decrements loop depth around each loop body and resets the inherited loop depth while checking a nested function body, preventing a nested function from borrowing its enclosing loop's control-transfer context.
- Extended the dedicated `test_semantic.c` control-flow suite with nested-loop success, post-loop scope restoration (both `break` and `continue` rejected), and nested-function `continue` rejection. Existing tests already cover loop-external transfers, conditional transfers, invalid loop conditions, and nested-function `break` rejection.
- No Semantic implementation change was necessary: the inspected scope handling already enforces these rules. New fixtures and normative syntax text were committed; CI remains pending.
- Test commit: `bd3a3ebc`.


### Semantic checkpoint — conditional AST structural validation — 2026-11-12

- AST_IF now fails closed when its condition expression or primary branch is absent. Missing condition and missing primary branch produce explicit Semantic diagnostics rather than silently skipping the malformed node.
- Added direct malformed-AST regression fixtures to `test_semantic.c`, covering both missing-condition and missing-primary-branch cases. Existing source fixtures continue to cover boolean conditions, elsif/else chains, and conditional break/continue behavior.
- Syntax reference records the required AST shape and optional else branch. Branch-head CI is not available yet; tests are committed but not marked passed.
- Semantic implementation: `3a538992`; tests: `df8d2bcd`.


### Semantic checkpoint — SELECT/SCAN branch structure — 2026-11-12

- Semantic now validates required selector/subject expressions and arm bodies, rejects invalid arm node kinds, and checks that non-default SELECT arms carry a pattern while the default arm does not.
- SELECT default is unique and final. SCAN else is unique and final; Parser already rejects a non-final SCAN else, while Semantic now defensively validates externally constructed ASTs.
- Added regressions for SELECT default ordering and valid SCAN else placement. Existing fixtures cover boolean guard validation, pattern-binding visibility inside guards, duplicate pattern bindings, and arm-local binding scope.
- The SCAN non-final-else rejection remains Parser-owned and is intentionally not duplicated as a Semantic source fixture.
- Semantic implementation commit: `26f2659e`; regression test commits: `6e9195ac`, `2954d695`.
- CI verification: pending; no test-pass claim is made.


### Semantic checkpoint — pattern AST fail-closed validation — 2026-11-12

- Pattern binding nodes now require a non-empty identifier.
- Constructor patterns require a non-empty constructor name and at least one binding, matching Parser's accepted syntax.
- Pattern lists accept only binding, constructor, and literal nodes; unsupported external AST nodes now emit a Semantic diagnostic instead of being silently ignored.
- Standalone SELECT/SCAN arm nodes now reject missing bodies; SELECT arms enforce default/pattern consistency.
- Added a direct malformed-AST regression using an unsupported pattern node.
- Semantic commit: `4d1f4dc6`; test commit: `fa23a52d`.
- CI verification: pending; no test-pass claim is made.


### Semantic checkpoint — Multi-Rule Engine AST integrity — 2026-11-12

- RuleSet declarations now require a non-empty name and validate that every parameter node is an AST variable declaration before signature registration. Invalid parameter nodes are not cast as variable declarations.
- Rule barriers require both a boolean condition and a diagnostic message; the message must be `str`. Trigger rules require a boolean condition and an action/event name. `always` and `default` require their action/event node.
- `apply rules` requires a RuleSet name. `on` blocks require RuleSet name, event name, and body before name/event resolution.
- Duplicate trigger event names now produce an explicit Semantic diagnostic instead of being silently discarded during event registration.
- Added direct malformed-AST fixtures for missing RuleSet/barrier/trigger/always/default/apply/on fields, plus a source-level duplicate-event fixture.
- Corrected the syntax reference to match the actual Parser AST tags and current grammar: `always`/`default` block syntax is not currently parsed; only the arrow form is supported.
- Semantic commit: `b4c070f3` and parameter safety follow-up: `54614fc9`; test commits: `60ae71b3` and `228e25b3`; syntax reference: `ea1008e7`.
- CI and local execution are not available in this workspace; this checkpoint remains unverified and is not marked test-passing.


### Semantic checkpoint — expression, cast and loop AST integrity — 2026-11-12

- Binary expression and Nexus Flow operation nodes now reject missing left/right operands with explicit Semantic diagnostics.
- Loop nodes reject unsupported loop kinds and missing bodies. Foreach requires both iterator and collection; conditional loop kind requires a condition.
- Cast nodes require both source value and target type. The reserved legacy AST_UNPACK node requires its source expression even though Parser does not emit it.
- Added direct malformed-AST fixtures for binary expression, Nexus Flow operation, loop body, foreach fields, cast and reserved unpack.
- Semantic commit: `b6779629`; tests: `b100fd6d`; syntax reference: `f455bd5c`.
- Local build/test and branch CI remain unavailable/pending; these fixtures are not marked passed.


### Semantic checkpoint — declaration identity validation — 2026-11-12

- Semantic now rejects empty/missing names on immutable `m:`, variable, and `group` declarations before symbol lookup/registration.
- Added direct malformed-AST regression fixtures for all three declaration kinds.
- Semantic commit: `f21111ea`; tests: `29e1b532`; syntax reference: `2e0ae0d7`.
- CI/local test execution remains pending; no test-pass claim is made.


### Semantic checkpoint — function/control AST structural integrity — 2026-11-12

- Function declarations now reject missing names; ordinary function nodes reject missing bodies while method implementation naming remains supported through the enclosing method context.
- Nexus label and jump nodes reject missing/empty label names; forward target resolution is deliberately deferred to control-flow validation.
- defer rejects a missing body. thread_block rejects missing names and bodies before entering its local scope.
- Added direct malformed-AST regression fixtures for each required field and missing function/thread bodies.
- Local build/test and CI evidence remain pending.


### Semantic checkpoint — reserved AI logic node fails closed — 2026-11-12

- AST_AI_LOGIC had an empty Semantic case despite having no active Parser production or defined query validation. It now emits an explicit Semantic diagnostic when encountered in a supplied/legacy AST.
- Extended fail-closed regression coverage for AST_AI_LOGIC alongside the existing raw inline-ASM case. Active source grammar is unchanged.
- Local build/test and CI verification remain pending.


## Semantic alt-kapsam tikleri — 2026-11-12

> Bu tikler ilgili Semantic AST yapısal kontrolünün kodda bulunup regression fixture ile temsil edildiğini gösterir. Ana tabloda S=~ kalır; çünkü fonksiyon ABI/return-flow, label hedef çözümü, defer unwind ve thread scheduling/lifetime gibi tam Semantic sözleşmeleri henüz kapanmadı. CI bu yeni commit için henüz doğrulanmadı.

| AST / özellik | Kontrol edilen alt sözleşme | Kod | Regression fixture | CI |
|---|---|:---:|:---:|:---:|
| AST_FUNC_DECL (4) | Boş isim ve normal fonksiyonda eksik body reddi; method public-name fallback korunur | ✓ | ✓ | ? |
| AST_EXTERN_DECL (12) | Önceki signature/linkname Semantic işleri mevcut; calling convention/ABI tamamlanmadı | ~ | ~ | ? |
| AST_NEXUS_LABEL (33) | Boş label adı reddi | ✓ | ✓ | ? |
| AST_NEXUS_JUMP (34) | Boş hedef adı reddi; forward target çözümü ayrı CFG işi | ✓ | ✓ | ? |
| AST_NEXUS_HALT (35) | Payload/cleanup ve terminal control-flow sözleşmesi açık | ~ | ~ | ? |
| AST_DEFER (69) | Eksik body reddi; unwind/cleanup sıralaması OIR işi | ✓ | ✓ | ? |
| AST_THREAD_BLOCK (70) | Boş isim, eksik/yanlış body, nested/duplicate payload reddi; explicit copy(...) dış SLOT_VAR capture listesi; direct access reddi; child AST_BLOCK tek lexical scope sahibi | ✓ | ✓ | ? |
| AST_AI_LOGIC (37, reserved) | Parser üretimi yok; dışarıdan gelen reserved AST fail-closed | ✓ | ✓ | ? |


### Semantic checkpoint — Nexus/argument AST structural integrity — 2026-11-12

- Added fail-closed validation for AST_NEXUS_CAPTURE (handler + expression), AST_NEXUS_PIPE (both operands), AST_NAMED_ARG (name + value) and AST_LISTEN (handler name before scope entry).
- Added direct malformed-AST regression fixtures. Full handler/event lifetime, pipe effects and listen runtime semantics remain open.
- Dashboard alt-coverage checkmarks represent implementation + fixture presence; CI is pending.


### Semantic checkpoint — OS I/O ve Thread module sınırı — 2026-11-12

| AST/API | Core Semantic | Sahip modül | Not |
|---|:---:|---|---|
| `AST_ECHO` / `echo(...)` | ✓ (callee identity + AST traversal) | OS module | Terminal/console bağımlılığı; bare-metal garantisi yok |
| `input(...)` | — | OS module / target input driver | AST özel intrinsic değil; module API olarak çözülür |
| `AST_PRNT` / `prnt(...)` | ~ (genel call traversal) | Runtime output sink (tasarım açık) | UART/framebuffer/debug/host callback ortak ABI henüz yok |
| `AST_PRMT` / `prmt(...)` | ~ (genel call traversal) | Runtime input/prompt sink (tasarım açık) | Bare-metal provider arayüzü kararlaştırılmalı |
| `AST_WRITE` / `AST_FWRITE` | ~ (genel call traversal) | File/stream module | Filesystem/stream provider'a bağlı |
| `spawn(name)` generic call | ✓ (named payload registry + argument validation) | Thread module | Scheduler, transfer ve thread lifetime Core dışında |
| `listen/send/receive/done` generic calls | ~ (AST call traversal only) | Thread module | API signatures, IPC, handle/lifetime Semantic'i Thread modülünde |

> OS ve Thread modülü ayrımı syntax reference'a işlendi. Bu checkpoint, modül API imzalarını veya bare-metal driver ABI'sini tamamlamaz; ana Semantic sütunları `~` kalır. Test/CI pending.


### Semantic alt-kapsam — OS / Thread AST call identity — 2026-11-12

| AST | Structural kontrol | Regression fixture | CI |
|---|:---:|:---:|:---:|
| AST_ECHO | callee identifier zorunlu ve adı `echo` | ✓ | ? |
| Legacy AST_SPAWN / AST_SEND / AST_RECEIVE / AST_DONE | Codec compatibility tags only; new Parser emits generic AST_FUNC_CALL | ✓ | ? |
| `spawn(name)` | One identifier argument must resolve in the thread payload registry | ✓ | ? |
| `listen/send/receive/done` | Generic module calls; Core does not validate Thread module signatures | ✓ | ? |

Bu iş yalnızca AST tag/callee bütünlüğünü doğrular; Thread modülünün scheduler/IPC/handle sözleşmesi core Semantic kapsamına alınmadı. `prnt/prmt` ve `write/fwrite` genel call traversal aşamasında; API imza kuralları sağlayıcı modülünde tanımlanacak.


### Semantic checkpoint — thread_block AST body and lexical-scope ownership — 2026-11-12

- Core Semantic now requires `thread_block.body` to be an actual `AST_BLOCK`, not merely a non-NULL node. Malformed externally supplied ASTs fail with an explicit diagnostic.
- Removed the extra scope previously opened by `AST_THREAD_BLOCK`. Its child `AST_BLOCK` is the single owner of lexical scope creation and cleanup; this avoids a redundant SemanticResult scope level while preserving local isolation.
- Added regression fixtures for a non-block body, a valid local reference inside the block, and rejection of a local variable referenced after the thread block.
- Thread names are unique registry keys. Only outer variable references inside explicit copy(...) calls are captured by value; direct access is rejected. Scheduler/IPC semantics remain outside Core.
- Implementation and fixture commits are recorded separately; CI remains pending until a workflow reports a result.


### Semantic checkpoint — independent thread payload boundary — 2026-11-12

- `thread_block` represents one complete payload to be transferred by the Thread module's `spawn(worker)` API to a newly created thread. It is not an ordinary nested execution block in the caller's flow.
- Nested `thread_block` declarations are rejected by Core Semantic. A context depth guard ensures the rule also applies to manually constructed ASTs.
- The payload's child `AST_BLOCK` remains the sole lexical-scope owner. Core does not implement thread creation, scheduling, IPC or execution.
- Added a source-level regression fixture proving nested payload rejection. CI remains pending; tests have not been reported as executed.


### Semantic checkpoint — thread payload registry and spawn resolution — 2026-11-12

- Added a SemanticContext-owned registry for named AST_THREAD_BLOCK payloads. Entries borrow the AST declaration pointer and own only a copied name; the registry is released by semantic_destroy.
- Duplicate thread-block names are rejected. spawn is validated against this registry and requires exactly one identifier argument that resolves to a registered payload. The payload name is treated as a registry key, not as an ordinary variable/function identifier.
- Initialized the thread-block depth and registry pointers explicitly in semantic_init.
- Added source fixtures for a valid registered payload, unknown payload names, invalid argument count/type, and duplicate declarations. Existing Thread module calls (listen/send/receive/done) remain ordinary calls and are not assigned Core runtime semantics.
- Current lookup follows Semantic AST traversal order: the payload declaration must be encountered before its spawn use. Forward-reference collection has not been added yet. This ordering is recorded as current implementation behavior, not as a finalized language-wide declaration-order decision.
- CI/local execution remains pending; tests are committed but not reported as executed.


### Semantic checkpoint — thread payload explicit-copy capture — 2026-11-12

- `ThreadBlockEntry` now records a deduplicated capture list only for outer-scope variable symbols referenced inside explicit copy(...) arguments. Each record stores symbol identity, type, source scope/depth, declaration and first-use AST references; owned names/types are released with the registry.
- Capture collection is active only while checking explicit copy(...) arguments in the payload. Direct outer-variable references are rejected. Symbols declared inside the payload (including nested lexical blocks) are not captured. Function/type/group symbols are not value captures.
- Explicit copy capture means copying the variable value into the spawned payload at spawn time. It does not copy the caller's stack address. Capturing a pointer copies the pointer value only; pointed-to storage is not implicitly deep-copied. Actual snapshot/transfer is a Thread runtime and OIR/Codegen responsibility.
- Added regression fixtures that verify explicit copy captures one outer variable exactly once, direct outer access is rejected, and payload-local variables are excluded.
- `listen`/`done` result transport remains owned by the Thread module and is not reimplemented in Core Semantic. Runtime spec documents the done payload kinds and listen consumer behavior.
- Tests are committed but not executed here; CI remains pending.


### Semantic checkpoint — thread payload capture refinement — 2026-11-12

- Outer `SLOT_VAR` references inside a named payload are rejected unless they occur within an explicit `copy(...)` argument. Explicit captures are collected once by symbol identity. Payload-local declarations, including nested lexical-block locals, are excluded from capture metadata.
- Capture metadata records the variable's type, source scope/depth, declaration and first-use node. Each outer identifier annotation also receives `SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE`, making the capture signal durable in the SemanticResult artifact. The regression fixture checks the flag and verifies payload-local references are not marked; another fixture verifies duplicate uses are captured once.
- Capture is explicit through `copy(...)` and becomes a value snapshot at spawn time. Direct outer-variable access is rejected. Pointer values are copied shallowly; the pointed-to allocation is not deep-copied. `done`/`listen` output transport remains governed by the Thread runtime specification.
- Earlier dashboard statements claiming thread names had no uniqueness rule or that Core performed `spawn` callee-identity validation are superseded by the current registry implementation above.
- Tests added; execution and CI remain pending.


### SemanticResult / OIR artifact capture-flag compatibility — 2026-11-12

- Added `SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE` and its OIR annotation flag counterpart. The OIR Semantic AST annotation reader/writer now accepts this defined flag while continuing to reject unknown bits.
- Added SemanticResult integration coverage proving explicit-copy outer references are marked, direct outer references are rejected, and payload-local references are not marked. Added artifact-level annotation round-trip coverage for the capture flag; legacy schema 1.0 remains readable with zero annotation flags.
- Artifact serialization copies annotation flags field-for-field, so no AST payload schema version bump was needed (current Semantic AST schema minor is already 4).
- CI/local execution remains pending.


### Explicit-copy capture validation refinement — 2026-11-12

- Thread payloads now reject direct references to outer `SLOT_VAR` symbols with a location-aware Semantic error. An outer value is admitted only inside `copy(...)` in the payload; that call requires exactly one source expression.
- Repeated copies of the same outer symbol are deduplicated by symbol identity. Payload-local references remain ordinary local accesses and are not captured.
- Regression fixtures cover valid repeated copy capture, direct outer access rejection, zero/multiple copy arguments and payload-local non-capture. SemanticResult integration checks the capture annotation flag.
- Thread module `listen`/`done` behavior is unchanged and remains in the existing runtime specification.


### Semantic checkpoint — Program ve block declaration scope — 2026-11-12

- Program-level declarations share the root semantic scope and are visible to function bodies declared in the same program.
- Each `AST_BLOCK` opens and closes a lexical scope. Nested scopes may shadow outer declarations; declarations do not leak into sibling or enclosing scopes.
- Added source-level regression fixtures for global-to-function visibility, duplicate global declarations, nested shadowing, nested-block leakage and sibling-block isolation.
- Program/block Semantic scope is marked complete in the dashboard. Program declaration-chain AST graph/codec validation, `no:osmodule` policy/source-span handling and OIR span metadata remain separate open work.
- Test fixtures are committed; execution and CI status have not yet been verified.


### Semantic checkpoint — `c:` compile-time constant rules — 2026-11-12

- `c:` declarations now require an initializer. Semantic accepts literal expressions, pure unary/binary constant expressions, constant array/tuple/struct literals, casts of constant values and references to previously declared `c:` values.
- Runtime function calls, mutable variable reads and `m:` runtime-immutable values are rejected as `c:` initializers. The distinction is preserved: `m:` remains an immutable binding whose initializer may execute at runtime.
- Added positive and negative source fixtures for literals, arithmetic, prior-constant references, aggregate constants, missing initializers, runtime calls, mutable reads and `m:` reads.
- Updated the normative syntax reference. The declaration row remains partial because inferred `v:` typing, recursive `type_expr` integration and source-span payload coverage are still open.
- Test source and implementation are committed; local execution and CI remain pending.


### Semantic review — Variable, NewType and Struct declarations — 2026-11-12

- Closed the remaining `c:` constant-initializer gap: compile-time constants require a value and reject runtime calls, mutable reads and `m:` values.
- Reviewed the existing `v:` path: initializer inference, declared-type checks, inline array dimensions/initializer validation, vector component count/type validation and recursive `type_expr` validation are already present and covered by Semantic fixtures.
- Reviewed NewType semantics: target resolution, recursive target validation, nominal identity, alias chains, and assignment/function argument/function return compatibility are covered by positive and negative fixtures.
- Reviewed Struct semantics: name/duplicate field/type checks, packed/public/exported metadata, field visibility and aggregate access/literal checks are covered by Semantic implementation and fixtures.
- Marked only the Semantic column complete for these rows. AST payload round-trip, physical aggregate layout, OIR lowering and Verifier remain open.
- CI for the new `c:` fixtures is still pending/unreported; these marks describe implementation plus committed regression coverage, not a verified test run.


### Semantic checkpoint — Enum variant and payload validation — 2026-11-12

- Enum declarations now reject missing type names; variant lists reject non-variant nodes, empty names and duplicate variant names.
- Variant payloads reject non-field nodes, empty/duplicate field names, missing field types and unknown named types. Recursive payload type expressions continue through the shared type-expression validator.
- Added direct AST regressions for valid payload declarations, malformed variant/payload node kinds, missing enum/variant names, duplicate variant names, duplicate payload field names and missing payload types.
- Updated the normative syntax reference with the enforced name/type rules and explicitly left constructor resolution, payload argument binding and exhaustiveness open.
- Constructor, payload, pattern binding, scan exhaustiveness and discriminant checks are now covered by the green branch-head C workflow (73/73 CTest; ASan/UBSan and Valgrind also green). The Enum Semantic column is closed; OIR representation and independent Verifier work remain open.


### Semantic checkpoint — Struct/Union field declaration validation — 2026-11-12

- Struct and Union fields require an explicit type unless a recursive `type_expr` supplies it. Missing field types produce a source diagnostic.
- Hardened named and inline aggregate field walks: non-`AST_VAR_DECL` payload nodes are skipped after the shared validator reports them, rather than being blindly cast.
- Direct AST regressions cover untyped Struct/Union fields and malformed aggregate field node kinds. Source fixtures cover duplicate names, unknown types and member type compatibility.
- Union now has an explicit C-like shared-storage contract: Semantic does not track an active member and permits reading a different declared member. The resulting reinterpretation is target-ABI-dependent, not a portable conversion or safety guarantee.
- Added source regressions for cross-member read/write, unknown Union member, incompatible member write and unknown member type. Struct and Union Semantic checks pass the green branch-head C workflow (73/73 CTest; ASan/UBSan and Valgrind also green).
- Updated the normative syntax reference and AST syntax matrix. Union layout/ABI and OIR representation remain open.


### Semantic checkpoint — Enum scan constructor binding — 2026-11-12

- `scan` constructor patterns now resolve against the enum type of the subject expression. Unknown variants and constructor patterns on non-enum subjects produce diagnostics.
- Payload bindings are matched positionally to the variant's declared fields; each local binding receives the corresponding field type. Incorrect payload arity, invalid binding node kinds and bare matching of payload-bearing variants are rejected.
- Bare variant identifiers are treated as tag matches only when the enum variant has no payload; other identifiers remain ordinary pattern bindings.
- Added direct Semantic AST regressions for valid typed payload binding, unknown variant, wrong payload arity and invalid bare payload-variant matching.
- Updated the syntax reference and coverage rows. Constructor expression calls and exhaustive enum scan coverage are implemented and pass the green branch-head C workflow. Non-enum structural pattern semantics remain open.


### Semantic checkpoint — Enum scan exhaustiveness — 2026-11-12

- Added an explicit `has_guard` field to `ASTScanArm`; Parser records whether an arm came from a `when` clause instead of relying on ambiguous `AST_IF` body shape.
- Enum scans now detect repeated unguarded variant arms and report every uncovered declared variant. Guarded arms do not count toward exhaustive coverage; an `else` arm suppresses missing-variant diagnostics.
- Added Semantic AST fixtures for exhaustive coverage, unknown/malformed patterns, duplicate variants, guard-only coverage and else fallback.
- Constructor expression resolution, positional/named payload argument validation and enum SemanticResult annotations are implemented; the complete branch-head C workflow passes. OIR representation and independent Verifier rules remain open.


### Semantic checkpoint — Enum constructor expressions — 2026-11-12

- Calls shaped as `EnumType.Variant(...)` are now resolved before Group method dispatch, preventing the enum type qualifier from being misdiagnosed as a runtime value/member receiver.
- Constructor resolution verifies that the enum exists and the variant belongs to it. Positional payloads follow declaration order; named payloads resolve by declared field name.
- Added checks for unknown variants, argument count, payload type compatibility, unknown named fields, duplicate named fields and omitted named fields. The call and callee receive SemanticResult annotations as enum-valued constructor expressions.
- Added direct AST regression fixtures for valid positional/named construction and invalid variant, arity, payload type and named-field cases. Enum Semantic remains partial for discriminant representation and recursive payload type-expression compatibility.
- Fixtures are committed but have not been compiled or executed in this workspace; CI remains unverified.


### Semantic checkpoint — Group method uniqueness regressions — 2026-11-12

- Added source regressions for duplicate public method aliases, duplicate implementation function names across aliases, and a method implementation name colliding with a direct Group function.
- Existing coverage also exercises inferred/explicit `self`, static versus instance dispatch, calls before declaration, external-scope Group-qualified calls, unknown methods, argument count/type, and malformed method AST payloads.
- Group / method Semantic remains partial (`~`): the current C test suite passes, including uniqueness regressions. Receiver visibility and the remaining signature contract are still open; overload behavior is not inferred from duplicate-name rejection.


### Semantic checkpoint — implicit receiver argument mapping — 2026-11-12

- Fixed named-argument parameter lookup for instance methods: the implicit `self` parameter is skipped when resolving caller-supplied names, while the resolved parameter index retains the receiver offset for type validation.
- Added regressions proving `instance.method(value: 7)` resolves the first explicit parameter and that callers cannot pass `self:` as a named argument.
- Documented the object/static method contract and Group-owned nominal type visibility. The green branch-head C workflow verifies the receiver-offset named-argument mapping and rejects caller-supplied `self:`; Group / Method remains partial because other signature-contract details remain open.
