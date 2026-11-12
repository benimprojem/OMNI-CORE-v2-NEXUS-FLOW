# NEXUS Flow — 8 Başlıkta Syntax → AST → Verifier Durum Tablosu

**Dal:** `refactor/c-core-roadmap`  
**Güncelleme:** 2026-11-11  
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

## AST structural audit checkpoint — 2026-11-11

A direct source comparison found **108/108 unique AST tags** with explicit dispatch in the factory-size table, destructor, printer, serializer, deserializer and graph validator. The all-tag codec fixture iterates through the current final tag, `AST_TYPE_EXPR`. This is a structural/lifecycle closure only: it does not imply that all 108 tags are active Parser productions, semantically complete, lowerable to OIR or covered by Verifier.

Parser source spans are stored on AST nodes, validated and serialized. Parser stamping exists across the main production families, but the production-by-production assertion matrix is still open; OIR optional source-span TLVs are also open.

## 1. Bildirimler ve tür sistemi

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Program ve blok | `AST_PROGRAM`, `AST_BLOCK` | ✓ | ✓ | ✓ | ~ | ~ | ~ | Program declaration chain, `no:osmodule` policy/span ve ortak node source-span codec round-trip; OIR span metadata ayrı aşamada |
| Değişken `v:`, sabit `c:` | `AST_VAR_DECL` | ✓ | ✓ | ✓ | ~ | ~ | ~ | name/type/flags, initializer, inline_type, recursive type_expr ve ortak source-span payload round-trip; Semantic kuralları açık |
| Immutable `m:` | `AST_MUST_DECL` (103) | ✓ | ✓ | ✓ | ✓ | D | — | Derleme zamanı/sabit değer sınırları; runtime lowering gerektirmeyen karar korunmalı |
| NewType `nt:` | `AST_NEWTYPE_DECL` | ✓ | ✓ | ✓ | ~ | ? | ? | name, target_type ve recursive target_type_expr payload round-trip; Semantic type identity açık |
| Struct / packed Struct | `AST_STRUCT_DECL` | ✓ | ✓ | ✓ | ~ | ? | ? | name, packed/visibility/export flags ve ordered field chain round-trip; layout ve lowering açık |
| Struct literal | `AST_STRUCT_LITERAL`, `AST_STRUCT_FIELD_INIT` (106–107) | ✓ | ✓ | ✓ | ✓ | ~ | ~ | Instance initialization/layout lowering ve Verifier invariant'ları |
| Enum / variant / payload | `AST_ENUM_DECL`, `AST_ENUM_VARIANT` | ✓ | ✓ | ✓ | ~ | ? | ? | Discriminant, payload type, constructor resolution ve exhaustiveness |
| Union | `AST_UNION_DECL` | ✓ | ✓ | ✓ | ~ | ? | ? | union flags ve field chain round-trip; active member ve safe access Semantic kuralları açık |
| Group / method | `AST_GROUP_DECL`, `AST_METHOD` | ✓ | ✓ | ✓ | ~ | ? | ? | members, method public_name/implementation ve flags round-trip; receiver/dispatch Semantic kuralları açık |
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
| Address/length `addr`, `len` | `AST_ADDR`, `AST_LEN` | ✓ | ✓ | ✓ | ? | ? | ? | Signature, result type, pointer lifetime ve lowering |
| Memory access `peek`, `poke` | `AST_PEEK`, `AST_POKE` | ✓ | ✓ | ✓ | ? | ? | ? | Address/value width, alignment, bounds ve target effects |
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
| `scan` / arms | `AST_SCAN`, `AST_SCAN_ARM` (83–84) | ✓ | ✓ | ✓ | ~ | ? | ? | Subject expression type now propagates into arm bindings; constructor payload typing, exhaustiveness/else ve execution lowering |
| Pattern binding | `AST_PATTERN_BINDING` (85) | ✓ | ✓ | ✓ | ~ | ? | ? | Subject type propagation, payload type ve binding lifetime |
| Constructor pattern | `AST_PATTERN_CONSTRUCTOR` (86) | ✓ | ✓ | ✓ | ~ | ? | ? | Enum/variant identity, payload arity/type ve exhaustiveness |
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
| `area / zone` | `AST_AREA`, `AST_ZONE` | ✓ | ✓ | ✓ | ? | ? | ? | Region typing, access permissions and target mapping |
| I/O `echo/prnt/prmt/write/fwrite` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ? | ? | ? | Central intrinsic signature/effect registry and OS module lowering |
| Port I/O `inb/outb`, interrupt `irq/intr`, register `reg` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ? | ? | ? | Target capability, privilege, width and effect validation |
| Reflection `typeof/sizeof/is_*` | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ~ | ? | ? | Compile-time/runtime distinction, type result and lowering |
| Date/time/clock/free | Matching intrinsic AST tags | ✓ | ✓ | ✓ | ? | ? | ? | Signature, platform service and effect contract |
| Zone sync/view/lock member calls | Generic member/call AST; reserved dedicated tags | ✓ | ✓ | ✓ | ? | ? | ? | Intrinsic registry; do not count reserved tags as active productions |
| Intrinsic AST inventory (exit/swap/option checks/date/time/clock/free/zone) | `AST_EXIT`, `AST_SWAP`, `AST_IS_OK`, `AST_IS_ERR`, `AST_IS_SOME`, `AST_IS_NONE`, `AST_IS_NULL`, `AST_DATE`, `AST_DATENOW`, `AST_TIME`, `AST_CLOCK`, `AST_FREE`, `AST_ZONE` | ✓ | ✓ | ✓ | ? | ? | ? | Central signature/effect registry, compile-time/runtime distinction and per-intrinsic type/effect tests |
| Ownership/copy/move and cleanup | Capture/relocate/defer/halt nodes; `copy()` is a generic call | ✓ | ✓ | ✓ | ~ | ? | ? | capture, relocate, defer, halt ve capture-assignment operator payload round-trip; Semantic ownership/effect açık |

## 8. Concurrency, FASTEXEC/ASM ve ertelenmiş syntax

| Syntax / özellik | AST temsili | L | P | A | S | O | V | Kalan ana iş |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| Thread block `(name)>{...}` | `AST_THREAD_BLOCK` (70) | ✓ | ✓ | ✓ | ? | ? | ? | mode/timeout, scheduling, capture/lifetime, cancel/sync |
| `spawn` | Legacy `AST_SPAWN` (40) | D | X | ✓ | D | D | — | Core Parser üretmez; Thread modülünün grammar/runtime sözleşmesi ayrı tanımlanmalı |
| Listen/send/receive/done | Matching AST tags (41–45) | ✓ | ✓ | ✓ | ? | ? | ? | Thread/event payload, synchronization, effects ve lowering |
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
- Remaining failure in that run is isolated to `parser_accepts_and_rejects_sources` / Nexus Flow catch-fallback-ignore Parser fixture. Semantic suite is green in that run; whole CI is not green yet.


- Named-argument follow-up: duplicate parameter binding and positional-after-named forms now produce Semantic diagnostics. Added regression fixtures. Commits `3f86c6da` Semantic, `cfb75499` tests; CI pending.


### Lexer/Parser correction — contextual `(e)` catch handler — 2026-11-12

- Lexer no longer collapses every literal `(e)` into a special token. That tokenization broke ordinary expressions such as `echo(e)` inside a catch body.
- Parser now recognizes `(e)` contextually only after `?->`, constructs the handler identifier and consumes its parentheses, then attaches the following block.
- This restores normal call/group tokenization while preserving the catch syntax. Added Parser diagnostic tracing; verify exact fixture and cross-platform CI next.
- Commits: `81f1da3e` (Lexer), `76176c20` (Parser). CI pending.


- Ownership split correction: duplicate named labels and positional-after-named are already rejected by Parser (and covered in its variadic/named argument regression); removed those parser-invalid sources from Semantic's `check_source` fixtures. Semantic coverage remains reordered named-argument type checking and unknown-name diagnostics, with defensive duplicate/order checks for externally constructed ASTs.


### Semantic checkpoint — shift count operand-width validation — 2026-11-12

- Constant shift counts are now validated against the left operand's integer width: count < width is required.
- Negative counts remain rejected with the offending count in the message. Out-of-range diagnostics now include the actual shift count, left operand type/bit width, and valid range (e.g. `u8`, 8-bit, `0..7`).
- Semantic errors are attached to the binary shift AST node's source span; the collector carries file/line/column, and the legacy Error record retains line/column. i32 shifts by 32/33 and u8 shifts by 8 now produce Semantic errors; boundary-valid u8 << 7 and i64 >> 63 remain accepted.
- Dynamic shift counts remain accepted at Semantic time because their value is not statically known; OIR/runtime must define fail-safe behavior for counts at or above the value width (trap/diagnostic rather than undefined host behavior).
- Added boundary fixtures to test_semantic_arithmetic.c.
- Commits: b1852e7e (Semantic), f0114097 (diagnostic clarity), c6d64269 (tests). CI pending; not marked verified.


### Semantic diagnostics and conditional-control parser correction — 2026-11-12

- Semantic diagnostics now copy the complete AST source span into the shared collector; legacy Error line/column fields fall back to the AST span when compatibility mirrors are absent.
- Shift diagnostics identify the offending count, left operand type and width, and valid range. Bitwise and compound-bitwise diagnostics identify the operator and actual operand types.
- Added regression assertions for diagnostic message content and exact source line/column.
- Arithmetic, shift, comparison, and logical-operator diagnostics now include the operator spelling and offending operand types; `%` explicitly reports its integer-only requirement.
- Unary logical negation and bitwise complement diagnostics identify the rejected operand type. Bitwise regression coverage now includes fixed-width `8bit` operands, mixed representation/integer operands, and compound-assignment diagnostics.
- Assignment validation now fails safely for malformed ASTs with missing target/value children. Compound numeric, bitwise and string-append diagnostics report the exact assignment operator and both operand types.
- Direct variable, indexed-element and aggregate-field assignment mismatches now identify the assignment operator, destination, expected type and supplied value type.
- NewType targets are validated before symbol registration; invalid targets no longer leak unusable type names into later declarations. Valid alias chains and invalid-target non-leakage have regression fixtures. Nominal identity is also checked for same-type assignment, base-type rejection, sibling-NewType rejection, and function argument/return compatibility.
- Invalid function return types no longer trigger a second dependent return-value mismatch from the function body; the original type diagnostic remains authoritative.
- Added diagnostic regressions for mismatched arithmetic types, invalid shift operand types, and non-boolean logical operands. Linux and ASan/UBSan passed on the previous checkpoint; full cross-platform verification for the latest diagnostic changes is pending.
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
- Linux CTest and ASan/UBSan passed on the preceding range checkpoint; full matrix for the latest follow-up is pending.


### Semantic checkpoint — recursive AST type-expression validation — 2026-11-12

- Recursive type expressions are now semantically validated for named, pointer, reference, array, function and qualified forms.
- Validation is wired into variable declarations, function parameters and return types, NewType targets, and Struct/Enum/Union fields. Unknown nested type names and malformed type-expression shapes report diagnostics at their own AST source spans.
- Function type parameter names must be unique; a variadic marker must be final. The Parser owns rejection of non-final variadic syntax, with a dedicated Parser regression fixture; Semantic retains defensive validation for externally constructed ASTs.
- Invalid NewType targets are not registered in the symbol table. Alias chains and nominal assignment identity are covered: same-NewType assignment is valid, base-type and sibling-NewType assignment are rejected.
- Unknown type diagnostics now name the missing type and its declaration/recursive-type context instead of relying on an untranslated message lookup.
- CI run #4228 (`1628fcda`) passed Ubuntu, macOS, Windows, ASan/UBSan and Valgrind. Semantic remains open for remaining AST families and complete SemanticResult type identity coverage.
