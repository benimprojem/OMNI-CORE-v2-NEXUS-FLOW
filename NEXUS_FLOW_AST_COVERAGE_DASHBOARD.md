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
