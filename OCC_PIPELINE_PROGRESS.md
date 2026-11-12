## CI verification checkpoint — 2026-11-12

- Verified branch-head commit: `2c7f11b652919c71a2811b65928e1ceda91a8649` (`refactor/c-core-roadmap`).
- GitHub Actions run `43287142976`: all five jobs passed — Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Ubuntu CTest: **73/73 passed**. Sanitizer and Valgrind jobs passed.
- This result supersedes older checkpoint notes below that say the same committed tests are pending/unverified. It does not mean the entire Semantic DLL, AST coverage, OIR or Verifier stages are complete.
- Dashboard updated: Enum and Union Semantic rows are now ✓. Group/Method remains ~ because signature-contract work is still open.

## Thread module boundary checkpoint — 2026-11-12

- `spawn`, `listen`, `send`, `receive` and `done` now parse as ordinary identifier callees and produce generic `AST_FUNC_CALL` nodes; their runtime/API semantics belong to the Thread library.
- Core Semantic no longer assigns Thread-specific intrinsic identity or scheduler/IPC/lifetime behavior. Legacy serialized `AST_SPAWN`, `AST_SEND`, `AST_RECEIVE`, `AST_DONE` tags are retained for codec compatibility and traversed without new Thread rules; legacy `AST_LISTEN` is rejected rather than treated as an active core construct.
- The old `!(listen): ...` special form is no longer part of the active Parser grammar; use `listen(...)` from the Thread module.
- `AST_THREAD_BLOCK` remains core syntax as a named structural container. Core checks non-empty name/body and analyzes the body in an isolated lexical scope only; it does not create or schedule a thread.
- Parser/Semantic regression fixture covers all five ordinary API call spellings and legacy AST_LISTEN rejection. Tests are committed but not executed here; CI remains unverified.

## OS / Thread / I/O intrinsic identity checkpoint — 2026-11-12

- `echo`, `spawn`, `send`, `receive`, `done` identity checks remain in the shared intrinsic validator.
- `prnt`, `prmt`, `write`, `fwrite` now use the same exact AST callee-name validation; malformed/missing/non-identifier callees produce Semantic diagnostics.
- `intr` is also mapped to its exact intrinsic AST callee name; direct AST regression coverage includes the `AST_INTR` tag.
- Direct AST fixtures cover all four accepted identities and a mismatched `fwrite` callee; existing `echo` source fixture remains.
- This validates only front-end AST identity. It does not impose I/O provider, stream, device, file-system, or Thread runtime behavior.
- Tests are committed but not executed in this GitHub workspace; CI status must be checked independently.

# OCC Pipeline İlerleme Kaydı

**Tarih:** 2026-11-09  
**Çalışma dalı:** `refactor/c-core-roadmap`  
**Kaynak depo:** `benimprojem/OMNI-CORE-v2-NEXUS-FLOW`  
**Belge amacı:** Lexer'dan Codegen'e kadar her Nexus Flow yapısının gerçek uygulama ve test durumunu tek tabloda izlemek.

> Bu belge yaşayan takip tablosudur. Durumlar tahmini yüzde değildir; kod, belge ve test kanıtına göre güncellenir. Bir enum, AST tag'i veya DLL dosyasının bulunması tek başına özelliğin tamamlandığı anlamına gelmez.

> **Kullanıcıya dönük genel görünüm:** [NEXUS Flow — 8 Başlıkta Syntax → AST → Verifier Durum Tablosu](NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md). Ayrıntılı node envanteri bu dosyada, sekiz başlıklı toplu durum ve öncelik sırası dashboard'dadır.\n\n## IRQ / register Semantic checkpoint — 2026-11-12

- `irq(number, handler)`: exact two args, integer number, handler identifier/callable; identifier must resolve to declared function. Tests include valid and malformed cases.
- `reg(r, "name")` and `reg(w, "name", value)`: operation, arity, non-empty string literal register name, and integer/address write value checks; regression tests added.
- Syntax reference records the semantic contract and explicitly separates target whitelist/privilege/ABI from front-end validation.
- Not yet executed in CI/workspace. Target register whitelist, IRQ bounds, privilege and handler ABI remain open; `intr` still requires audit.

## Semantic intrinsic audit checkpoint — 2026-11-12

- `addr/len/peek/poke`: argument count and known-type constraints validated; `addr` requires an lvalue, `len` a lengthable value, `peek/poke` integer-or-address and integer-value rules.
- `inb/outb`: exact arity and integer argument checks.
- `area/zone/free`: exact arity, integer positive constant size checks (when statically evaluable), and accepted handle/address type checks.
- Added source-level positive/negative regression cases in `test_semantic.c`. The fixture is committed but not executed in this GitHub workspace; CI status is not inferred.
- Direct malformed AST: dedicated intrinsic tags now reject missing or mismatched callee identifiers; regression fixtures cover both. Still open: null argument and malformed named-arg payload, target capability/privilege, memory width/alignment/bounds, region ownership/lifetime and effect model.

## 1. Durum anahtarı

| İşaret | Anlamı |
|---|---|
| ✓ | Uygulandı ve ilgili test/kanıt mevcut |
| ~ | Kısmen uygulandı; kapsam veya test eksiği var |
| ? | Henüz satır bazında denetlenmedi; tamamlandı kabul edilmez |
| D | Tasarım gereği ertelendi; davranış uydurulmayacak |
| X | Mevcut kapsamda açıkça desteklenmiyor/reddediliyor; sessizce kabul edilmez |
| — | Bu satır/aşama için uygulanamaz |

**Güncelleme kuralı:** Bir hücre yalnızca ilgili aşamanın kodu ve testi incelendikten sonra ✓ yapılır. Testi olmayan kod ~ kalır. Desteklenmeyen özellik sessizce atlanmaz; açık hata/diagnostic üretmelidir.

## 2. Pipeline genel durumu

| Aşama | Girdi → çıktı | Mevcut durum | Kalan ana iş |
|---|---|---|---|
| Source reader / input | Source → SOURCE_UNIT | ✓ | Sınır ve hata enjeksiyon testlerini genişlet |
| Lexer DLL | SOURCE_UNIT → TOKEN_STREAM | ~ | Tüm token envanteri, longest-match, literal/escape ve span matrisi |
| Parser DLL | TOKEN_STREAM → AST | ~ | Her syntax satırını parser fonksiyonu ve pozitif/negatif testle eşleştir |
| AST model | AST tags / payloads | ~ | Her düğümün alan, child, sibling, nullability, source span ve ownership sözleşmesini kapat |
| AST codec | AST ↔ AST artifact | ✓ | Parser error/OOM ve ownership stres testlerini genişlet |
| Semantic DLL | AST → SEMANTIC_AST | ~ | Type/scope/conversion/function/control-flow/ownership matrisi |
| Semantic artifact validator | SEMANTIC_AST → validated SEMANTIC_AST | ~ | Tüm node annotation ve cross-reference zorunluluklarını kapat |
| IR Lowering DLL | SEMANTIC_AST → OIR_MODULE | ~ | Şu an sınırlı scalar + bazı if/elsif CFG kapsamı; tüm AST desteği yok |
| OIR Verifier DLL | OIR_MODULE → verified OIR_MODULE | ~ | Opcode, type, SSA, CFG ve cross-table invariant/test matrisi |
| Optimizer stage | verified OIR → optimized OIR | ? | Pass altyapısı ve verifier tekrar çalıştırma |
| Target resolution | target descriptor → validated target | ~ | Host-default ve gerçek target capability kapsamı |
| Codegen DLL | verified OIR + target → assembly/object | ~ | Yalnızca mevcut dar subset; AST/Semantic/OIR kapıları kapanmadan genişletme yok |
| Assembler / linker / package | assembly/object → final output | ? | Ayrı modül sözleşmeleri, test ve atomik çıktı |
| OCC orchestrator | stage graph / diagnostics / checkpoints | ~ | Genel checkpoint, stop-after, tam diagnostic aggregation ve job isolation |

**Önemli mevcut gerçek:** Lexer, Parser ve Semantic ayrı yüklenebilir aşamalar olarak çalışıyor. Güncel frontend zinciri `SOURCE_UNIT → TOKEN_STREAM → AST → SEMANTIC_AST`. IR Lowering ve bağımsız OIR Verifier DLL'leri de mevcut pipeline'a bağlanmış durumda; OIR_MODULE 1.1'in desteklenen scalar alt kümesi sınırlıdır. Yeni Codegen yolunun entegrasyonu, tüm dilin Codegen desteğinin tamamlandığı anlamına gelmez. Legacy AST tabanlı köprü parity tamamlanana kadar ayrıca izlenir.

## 3. Lexer token envanteri

Lexer bir AST düğümüne bire bir karşılık gelmez. Bu nedenle token takibi ayrı tutulur; AST tablosundaki Parser sütunu tokenın gramerde kullanıldığı yeri izler.

| Token grubu | Lexer | Parser tüketimi | Test / denetim |
|---|---|---|---|
| Identifier / keyword | ~ | ~ | Tüm keyword ve contextual keyword eşlemesi |
| Primitive type tokens | ~ | ~ | `i/u/f`, bool, bit, byte, hex, void, any ve yeni türler |
| Integer / float / null literals | ✓ | ~ | Decimal integer/fraction, `0x`/`0b`/`0o`, scientific exponent, underscore separators and `null`; numeric type suffixes fail closed; rolling `ms` unit token pair; Semantic overflow/nullability rules remain open |
| String / char literals | ~ | ~ | Escape, UTF-8, kapanmayan literal ve newline |
| Single-character operators | ~ | ~ | Token ID ve parser precedence |
| Multi-character operators | ~ | ~ | Longest-match ve çakışma testleri |
| Nexus Flow operators | ~ | ~ | `<-`, `->`, `>>`, `<<`, `_>`, `?->`, `?=>`, `!->`, `!!=` vb. |
| Comments / whitespace / EOF | ~ | — | Line/column/byte span, CRLF/LF, tab ve EOF |
| Invalid / malformed input | ~ | ~ | Hatalı token sonrası güvenli devam ve diagnostic |

> Lexer'da daha önce enum'da bulunup dispatch'te eksik olan 13 token/primitive eşlemesi tamamlandı; source byte offset/line/column ve span length eklendi; kapanmayan block comment hata olarak raporlanıyor. Bu, tüm token envanterinin kapandığı anlamına gelmez.

## 4. AST düğüm bazlı ilerleme tablosu

Bu tablo `OIR_AST_NODE_REGISTRY.md` içindeki mevcut 1–107 kayıtlı OIR düğüm kimliklerini temel alır. ID'ler append-only'dir; mevcut ID'ler yeniden numaralandırılmaz.

**Sütunlar:** Parser = syntax'tan bu düğümü üretme; AST+Codec = payload/lifecycle/serialize/deserialize; Semantic = tip/scope/ownership/control-flow kuralları; OIR Lowering = executable OIR üretimi; Tests = özellik bazlı parser/semantic/lowering testleri. AST codec için mevcut field-level round-trip matrisi tüm kayıtlı tag'leri kapsar; bu, her düğümün parser veya semantic olarak tamamlandığını göstermez.

| ID | AST düğümü | Parser | AST + Codec | Semantic | OIR Lowering | Feature tests |
|---:|---|:---:|:---:|:---:|:---:|:---:|
| 1 | `AST_PROGRAM` | ~ | ~ | ? | ? | ~ |
| 2 | `AST_BLOCK` | ~ | ~ | ? | ? | ~ |
| 3 | `AST_VAR_DECL` | ~ | ~ | ~ | ~ | ~ |
| 4 | `AST_FUNC_DECL` | ~ | ~ | ~ | ~ | ~ |
| 5 | `AST_STRUCT_DECL` | ~ | ~ | ~ | ? | ~ |
| 6 | `AST_ENUM_DECL` | ~ | ~ | ~ | ? | ~ |
| 7 | `AST_UNION_DECL` | ~ | ~ | ~ | ? | ~ |
| 8 | `AST_NEWTYPE_DECL` | ? | ~ | ? | ? | ? |
| 9 | `AST_ENUM_VARIANT` | ? | ~ | ? | ? | ? |
| 10 | `AST_GROUP_DECL` | ~ | ~ | ~ | ? | ~ |
| 11 | `AST_METHOD` | ~ | ~ | ~ | ? | ~ |
| 12 | `AST_EXTERN_DECL` | ~ | ✓ | ~ | ? | ~ |
| 13 | `AST_LITERAL` | ~ | ✓ | ~ | ~ | ~ |
| 14 | `AST_IDENTIFIER` | ~ | ✓ | ~ | ~ | ~ |
| 15 | `AST_BINARY_EXPR` | ~ | ✓ | ~ | ~ | ~ |
| 16 | `AST_UNARY_EXPR` | ~ | ✓ | ~ | ? | ~ |
| 17 | `AST_POSTFIX_EXPR` | ~ | ✓ | ~ | ? | ~ |
| 18 | `AST_ASSIGNMENT` | ~ | ✓ | ~ | ~ | ~ |
| 19 | `AST_FUNC_CALL` | ~ | ✓ | ~ | ? | ~ |
| 20 | `AST_MEMBER_ACCESS` | ~ | ✓ | ~ | ? | ~ |
| 21 | `AST_INDEX_ACCESS` | ✓ | ✓ | ✓ | ? | ✓ |
| 22 | `AST_CAST` | ~ | ✓ | ~ | ? | ~ |
| 23 | `AST_TUPLE` | ~ | ✓ | ? | ? | ~ |
| 24 | `AST_IF` | ✓ | ✓ | ~ | ~ | ~ |
| 25 | `AST_LOOP` | ✓ | ✓ | ~ | ? | ~ |
| 26 | `AST_RETURN` | ~ | ✓ | ~ | ~ | ~ |
| 27 | `AST_BREAK` | ~ | ✓ | ? | ? | ~ |
| 28 | `AST_CONTINUE` | ~ | ✓ | ? | ? | ~ |
| 29 | `AST_ASM_BLOCK` (legacy generic block tag; do not repurpose) | ~ | ✓ | D | D | ~ |
| 30 | `AST_NEXUS_CAPTURE` | ~ | ✓ | ? | ? | ~ |
| 31 | `AST_NEXUS_PIPE` | ~ | ✓ | ? | ? | ~ |
| 32 | `AST_NEXUS_FLOW` | ~ | ✓ | ? | ? | ~ |
| 33 | `AST_NEXUS_LABEL` | ~ | ✓ | ? | ? | ~ |
| 34 | `AST_NEXUS_JUMP` | ~ | ✓ | ? | ? | ~ |
| 35 | `AST_NEXUS_HALT` | ~ | ✓ | ? | ? | ~ |
| 36 | `AST_NEXUS_INTENT` | D | ✓ | D | D | D |
| 37 | `AST_AI_LOGIC` | D | ✓ | D | D | D |
| 38 | `AST_UNPACK` | ~ | ✓ | ? | ? | ~ |
| 39 | `AST_PANIC` | ~ | ✓ | ~ | ? | ~ |
| 40 | `AST_SPAWN` (legacy codec tag; Thread module owns spawn) | D | ✓ | D | D | D |
| 41 | `AST_LISTEN` | ~ | ✓ | ? | ? | ~ |
| 42 | `AST_ECHO` | ~ | ✓ | ? | ? | ~ |
| 43 | `AST_SEND` | ~ | ✓ | ? | ? | ~ |
| 44 | `AST_RECEIVE` | ~ | ✓ | ? | ? | ~ |
| 45 | `AST_DONE` | ~ | ✓ | ? | ? | ~ |
| 46 | `AST_AREA` | ~ | ✓ | ? | ? | ~ |
| 47 | `AST_ADDR` | ~ | ✓ | ? | ? | ~ |
| 48 | `AST_LEN` | ~ | ✓ | ? | ? | ~ |
| 49 | `AST_PEEK` | ~ | ✓ | ? | ? | ~ |
| 50 | `AST_POKE` | ~ | ✓ | ? | ? | ~ |
| 51 | `AST_INB` | ~ | ✓ | ? | ? | ~ |
| 52 | `AST_OUTB` | ~ | ✓ | ? | ? | ~ |
| 53 | `AST_IRQ` | ~ | ✓ | ? | ? | ~ |
| 54 | `AST_INTR` | ~ | ✓ | ? | ? | ~ |
| 55 | `AST_REG` | ~ | ✓ | ? | ? | ~ |
| 56 | `AST_PRNT` | ~ | ✓ | ? | ? | ~ |
| 57 | `AST_PRMT` | ~ | ✓ | ? | ? | ~ |
| 58 | `AST_WRITE` | ~ | ✓ | ? | ? | ~ |
| 59 | `AST_FWRITE` | ~ | ✓ | ? | ? | ~ |
| 60 | `AST_TYPEOF` | ~ | ✓ | ? | ? | ~ |
| 61 | `AST_SIZEOF` | ~ | ✓ | ? | ? | ~ |
| 62 | `AST_IS_OK` | ~ | ✓ | ? | ? | ~ |
| 63 | `AST_IS_ERR` | ~ | ✓ | ? | ? | ~ |
| 64 | `AST_IS_SOME` | ~ | ✓ | ? | ? | ~ |
| 65 | `AST_IS_NONE` | ~ | ✓ | ? | ? | ~ |
| 66 | `AST_IS_NULL` | ~ | ✓ | ? | ? | ~ |
| 67 | `AST_EXIT` | ~ | ✓ | ? | ? | ~ |
| 68 | `AST_SWAP` | ~ | ✓ | ? | ? | ~ |
| 69 | `AST_DEFER` | ~ | ✓ | ? | ? | ~ |
| 70 | `AST_THREAD_BLOCK` | ~ | ✓ | ? | ? | ~ |
| 71 | `AST_USE` | ~ | ✓ | ? | ? | ~ |
| 72 | `AST_ZONE` | ~ | ✓ | ? | ? | ~ |
| 73 | `AST_DATE` | ~ | ✓ | ? | ? | ~ |
| 74 | `AST_DATENOW` | ~ | ✓ | ? | ? | ~ |
| 75 | `AST_TIME` | ~ | ✓ | ? | ? | ~ |
| 76 | `AST_CLOCK` | ~ | ✓ | ? | ? | ~ |
| 77 | `AST_FREE` | ~ | ✓ | ? | ? | ~ |
| 78 | `AST_ZONE_SYNC` | ~ | ✓ | ? | ? | ~ |
| 79 | `AST_ZONE_VIEW` | ~ | ✓ | ? | ? | ~ |
| 80 | `AST_ZONE_LOCK` | ~ | ✓ | ? | ? | ~ |
| 81 | `AST_SELECT` | ✓ | ✓ | ~ | ? | ~ |
| 82 | `AST_SELECT_ARM` | ✓ | ✓ | ~ | ? | ~ |
| 83 | `AST_SCAN` | ✓ | ✓ | ~ | ? | ~ |
| 84 | `AST_SCAN_ARM` | ✓ | ✓ | ~ | ? | ~ |
| 85 | `AST_PATTERN_BINDING` | ✓ | ✓ | ~ | ? | ~ |
| 86 | `AST_PATTERN_CONSTRUCTOR` | ✓ | ✓ | ~ | ? | ~ |
| 87 | `AST_RULESET_DECL` | ✓ | ✓ | ~ | ? | ~ |
| 88 | `AST_RULE_BARRIER` | ✓ | ✓ | ~ | ? | ~ |
| 89 | `AST_RULE_TRIGGER` | ✓ | ✓ | ~ | ? | ~ |
| 90 | `AST_RULE_ALWAYS` | ✓ | ✓ | ~ | ? | ~ |
| 91 | `AST_RULE_DEFAULT` | ✓ | ✓ | ~ | ? | ~ |
| 92 | `AST_APPLY_RULES` | ✓ | ✓ | ~ | ? | ~ |
| 93 | `AST_ON_BLOCK` | ✓ | ✓ | ~ | ? | ~ |
| 94 | `AST_LAMBDA` | ✓ | ✓ | ~ | ? | ~ |
| 95 | `AST_RELOCATE` | ✓ | ✓ | ~ | ? | ~ |
| 96 | `AST_UNROLL` | ✓ | ✓ | ~ | ? | ~ |
| 97 | `AST_ARRAY_LITERAL` | ✓ | ✓ | ~ | ? | ✓ |
| 98 | `AST_ARRAY_TYPE` | ✓ | ✓ | ~ | ? | ✓ |
| 99 | `AST_ARRAY_DIMENSION` | ✓ | ✓ | ~ | ? | ✓ |
| 100 | `AST_BLOCK_HALT` | ✓ | ~ | ? | ? | ~ |
| 101 | `AST_VARIADIC_PARAM` | ✓ | ✓ | ~ | D | ✓ |
| 102 | `AST_NAMED_ARG` | ✓ | ~ | ~ | D | ~ |
| 103 | `AST_MUST_DECL` (immutable `m:` value) | ✓ | ✓ | ✓ | D | ✓ |
| 104 | `AST_FASTEXEC_BLOCK` | ~ | ~ | ~ | D | ~ |
| 105 | `AST_ASM_LABEL` | ~ | ~ | ~ | D | ~ |
| 106 | `AST_STRUCT_LITERAL` | ✓ | ✓ | ✓ | ? | ✓ |
| 107 | `AST_STRUCT_FIELD_INIT` | ✓ | ✓ | ✓ | ? | ✓ |
| 108 | `AST_TYPE_EXPR` | ✓ | ✓ | ~ | D | ✓ |

## 5. OIR ve Verifier kapsam kapısı

OIR AST bridge kaydı ile executable OIR lowering aynı şey değildir. Bir AST node'un OIR node registry'de bulunması, o node'un makineye dönük OIR opcode'larına indirgenebildiği anlamına gelmez.

| Kontrol alanı | Durum | Tamamlanma kanıtı |
|---|:---:|---|
| OIR container / payload schema | ✓ | Versioned container ve bounded reader/writer testleri |
| Semantic AST artifact | ~ | Scope/type/symbol/annotation/relation/conversion tabloları; tüm semantic kurallar henüz tamam değil |
| AST node registry | ✓ | 1–99 sabit ID ve field schema |
| Lowering node coverage | ~ | Her AST kind için supported / partial / intentionally unsupported kaydı |
| Opcode registry | ~ | Arity, operand/result, type, effect, memory ve terminator sözleşmeleri |
| Verifier structural checks | ~ | Table bounds, IDs, references ve record shape |
| Verifier semantic checks | ~ | Type compatibility, SSA dominance, CFG edges, terminators ve function ownership kapsamı |
| Invalid artifact gate | ~ | Invalid/partial artifact'in sonraki aşamaya geçmediği integration testleri |
| Optimizer sonrası verification | ? | Her pass sonrasında verifier yeniden çalıştırılır |
| Codegen input contract | ~ | Yalnızca verifier'dan geçmiş OIR ve doğrulanmış target kabul edilir |

## 6. Tamamlanma kapıları

Bir düğüm veya syntax özelliği ancak aşağıdaki kanıt zinciri sağlandığında **tamamlandı** sayılır:

1. Lexer: token kimliği, spelling, longest-match, literal/escape ve source span.
2. Parser: gramer kuralı, precedence/associativity, belirsizlik çözümü ve hata kurtarma.
3. AST: doğru node kind, eksiksiz payload, child/list/sibling ve ownership.
4. AST codec: field-level round-trip, bozuk/truncated input ve sınır kontrolleri.
5. Semantic: symbol/scope/type/conversion/value-category/ownership/effect kuralları.
6. Semantic artifact: zorunlu annotation ve tüm çapraz referansların doğrulanması.
7. Lowering: açık OIR eşlemesi veya konumlu fail-closed diagnostic.
8. Verifier: üretilen OIR'nin tüm ilgili invariant testlerinden geçmesi.
9. Integration: geçerli artifact'in doğru aşamaya ulaşması; invalid artifact'in sonraki aşamada reddedilmesi.
10. CI: ilgili testlerin desteklenen platform ve sanitizer matrisinde geçmesi.

## 7. Güncel öncelik sırası

1. AST düğüm sözleşmesi ve ownership envanterini makine tarafından kontrol edilebilir hale getir.
2. Lexer token envanterini Nexus Flow Syntax Reference ile satır satır eşleştir.
3. Parser grammar envanterini AST node tablosuna bağla; her satıra parser fonksiyonu ve fixture ekle.
4. Semantic coverage: tuple/lvalue, type promotion, function/member, control-flow ve ownership/effect eksiklerini kapat.
5. Semantic AST artifact zorunlu annotation ve cross-reference kontrollerini tamamla.
6. Her AST düğümü için OIR lowering desteğini veya açık unsupported kararını kaydet.
7. OIR opcode ve Verifier invariant/test matrisini tamamla.
8. Pipeline gate ve parity testlerini tamamla.
9. Ancak bu kapılar kapandıktan sonra Codegen backend kapsamını genişlet.

## 8. Güncelleme kaydı

| Tarih | Değişiklik | Kanıt / referans |
| 2026-11-03 | Pattern binding Semantic açığının kök nedeni doğrulandı: `semantic_check_pattern_bindings()` subject enum tipini almadan pattern slotlarını `any` olarak oluşturuyor. Enum bildirimi sırasında payload field tipleri Semantic Result ilişkilerine yazılıyor, ancak select/scan arm kontrolü constructor'ı subject enum/variant ile eşleştirmiyor. Bu yüzden guard bool kontrolü mevcut olsa da `when a > 0` gibi payload alanı tipine dayalı karşılaştırmalar henüz garanti değil. | `omnicore-sdk/src/c/parser/semantic.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`; Semantic aşamasında subject-aware pattern resolution işi olarak kaydedildi; Parser AST kapsamı tamamlanmadan genişletilmeyecek |
| 2026-11-03 | Scan `else` catch-all kolu eklendi. `AST_SCAN_ARM.pattern == NULL` yalnızca son `else` kolu için ayrıldı; `else` sonrasında başka kol ve yinelenen `else` Parser tarafından reddediliyor. AST printer bu kolu `SCAN_ARM ELSE` olarak gösteriyor; OIR node/field şeması değişmedi, nullable field 1 üzerinden null pattern taşınıyor. Parser, Semantic ve OIR round-trip testleri eklendi; GitHub Actions commit `8722f56`: Windows/Ubuntu/macOS build/CTest, Ubuntu ASan/UBSan ve Valgrind tümü başarılı. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/parser/ast.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/tests/test_semantic.c`, `omnicore-sdk/src/c/tests/test_parser_ast_oir_roundtrip.c`, `OIR_AST_NODE_REGISTRY.md` |
| 2026-11-03 | Select/scan `when condition` guard desteği eklendi. Guard ifadesi `parse_logical_or()` ile Flow okundan önce ayrıştırılıyor ve mevcut AST sözleşmesini bozmamak için arm body içinde `AST_IF(condition, then_branch)` olarak kanonikleştiriliyor. Böylece OIR node/field ID şeması değişmeden kalıyor; Semantic mevcut arm scope içinde pattern binding'leri tanımladıktan sonra guard'ı bool koşulu olarak denetliyor. Parser ve Semantic testleri eklendi; İlk Semantic CI denemesi pattern binding tiplerinin henüz çıkarılmadığını gösterdi; guard scope testleri eşitlik ifadelerine göre ayrıştırıldı ve tip çıkarımı açığı ayrıca belgelendi. Düzeltilmiş commit `646978d` tüm Windows/Ubuntu/macOS, ASan/UBSan ve Valgrind kontrollerini geçti. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/tests/test_semantic.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md` |
| 2026-11-03 | Parser'a mantıksal `&&` / `||` grameri eklendi: `parse_logical_and()` ve `parse_logical_or()` ile AND, OR'dan daha yüksek öncelikte ve her ikisi soldan birleşimli. Parser testi AST önceliğini ve AST codec round-trip'ini doğruluyor; Semantic testleri bool operand kabulü ve integer operand reddini kontrol ediyor. Kısa devre yürütme garantisi OIR/lowering denetimine bırakıldı. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/tests/test_semantic_arithmetic.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; Parser ve Semantic CI testleri başarılı (`2afd58c`, `348e6d1`) |
| 2026-11-03 | Parser'da `TOKEN_PERCENT` eksikliği kapatıldı: `%`, `parse_term()` içinde `*` ve `/` ile aynı çarpımsal öncelikte tüketiliyor. Yeni test `17 % 5 * 2` AST öncelik yapısını, `17 % 5` sabit array extent AST'sini ve AST serialize/deserialize round-trip'ini doğruluyor. Semantic tarafında integer-only ve divide-by-zero kontrolleri için doğrudan testler, ayrıca modulo sabit array extent entegrasyon testi eklendi. Parser-only commit `68cbf19` tüm platform/ASan/Valgrind CI kontrollerini geçti; Semantic test commit'i `348e6d1` ile integer-only, divide-by-zero ve modulo array extent testleri Windows/Ubuntu/macOS, ASan/UBSan ve Valgrind CI kontrollerini geçti. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/parser/semantic.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md` |
| 2026-11-03 | AST ownership kaydı gerçek C payload'ları ve `ast_destroy_recursive()` ile karşılaştırıldı. `AST_NEWTYPE_DECL.target_type` child değil owned string; `AST_LISTEN` / `AST_NEXUS_CAPTURE` aynı payload'ı, `AST_NEXUS_PIPE` / `AST_NEXUS_FLOW` aynı `ASTNexusFlow` prefix'ini kullanıyor; `AST_BREAK` / `AST_CONTINUE` payload'sız base node. Paylaşılan layout ve destroy alanları `AST_NODE_FIELD_OWNERSHIP.md` içinde netleştirildi; `AST_LISTEN` ve payload'sız `AST_BREAK` / `AST_CONTINUE` aile envanterine dahil edildi. | `omnicore-sdk/src/c/parser/ast.h`, `omnicore-sdk/src/c/parser/ast.c`, `AST_NODE_FIELD_OWNERSHIP.md`; alan bazlı parser/codec/OIR sahiplik denetimi sürüyor |
| 2026-11-03 | Parser operator genişletmesi: `^^` sağ birleşimli power; bitwise `&`, `^`, `|` AND>XOR>OR precedence; compound assignment ve `.=` mevcut AST assignment node'ında orijinal tokenla taşınıyor. Parser regression fixture power/bitwise parse ve `+=`/`&=` op korunmasını kapsıyor. Semantic operator type/lvalue/equivalence ve OIR lowering henüz tamam değil. `<<`/`>>` için kullanıcı kararı: bit ve Flow kullanımları operand/target yapısı ile bağlamdan ayrılacak; token spelling tek başına yeterli değil. Şimdilik shift arithmetic production eklenmedi. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; CI henüz yeni commit için çalışmadı |\n| 2026-11-03 | OIR AST field-registry düzeltmesine regresyon eklendi: parser→OIR writer→reader→writer byte-stable round-trip fixture artık `cast(5, i32)` ve `array[0]` içeriyor; `AST_CAST.value/target_type` ve `AST_INDEX_ACCESS.target/index` alanlarını gerçek kaynak AST üzerinden doğruluyor. | `omnicore-sdk/src/c/tests/test_parser_ast_oir_roundtrip.c`; commit `ecfd16b` CI queued |\n| 2026-11-03 | OIR AST field registry C payload karşılaştırması: `AST_INDEX_ACCESS` gerçekte `ASTIndexAccess.target/index`, `AST_CAST` ise `ASTCast.value/target_type` taşıyor; registry önceki ortak operator/left/right ve unary op/operand satırları bu iki node için yanlış şemaydı. Registry alanları C struct ve OIR schema (kind 21/22, 2 NODE_REF) ile eşleştirildi. `<<`/`>>` için de ayrım netleştirildi: numeric shift → `AST_BINARY_EXPR`; Zone Write/Flow Feed → `AST_NEXUS_PIPE`, operand-shape grammar şart; Parser şu an ikisini Flow üretimine yönlendiriyor, shift üretimi eksik. | `OIR_AST_NODE_REGISTRY.md`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; CI docs-only run |\n| 2026-11-03 | Extern function AST ayrımı tamamlandı: `exf:name(...)` Parser artık ortak `ASTFuncDecl` payload'ını `AST_EXTERN_DECL` node kind'ı ile üretir (`is_extern=1`), normal `f:` deklarasyonu `AST_FUNC_DECL` olarak kalır. Keyword regresyonu dedicated AST tag'i doğruluyor. Önceki extern lexer/parser boundary düzeltmesi commit `2c1f735` tüm platform/sanitizer/Valgrind CI'dan geçti; dedicated tag commit `57c2e6b` CI bekliyor. | `omnicore-sdk/src/c/parser/parser.c`, `test_parser.c`, `ast.c` |\n| 2026-11-03 | `exf:` token ayrımı düzeltildi: Lexer daha önce `exf:name` bütününü `TOKEN_KW_EXTERN` olarak üretiyordu fakat Parser adı ayrı token bekliyordu; Parser şimdi extern tokenının `exf:` sonrası kısmını deklarasyon adı olarak çıkarıyor. `exf:name(...)` artık tek-token prefix modeliyle tutarlı. Keyword testinin ilk CI hatası bu token/isim kontratını ortaya çıkardı; düzeltme commit `2c1f735` CI çalışıyor. | `omnicore-sdk/src/c/parser/lexer.c`, `parser.c`, `test_parser.c` |\n| 2026-11-03 | Token→Parser taramasında gerçek eksikler kapatıldı: `exf:` (`TOKEN_KW_EXTERN`) artık `AST_FUNC_DECL.is_extern` işaretli fonksiyon üretir; `echo` `AST_ECHO` intrinsic çağrısına bağlandı; `!(listen):` parser'ın identifier beklentisi Lexer'ın `TOKEN_KW_LISTEN` üretimiyle uyumsuzdu, keyword yolu eklendi. Parser regresyonu extern isim/flag, echo ve listen AST biçimini doğruluyor. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`; CI `78883d1` queued |\n| 2026-11-03 | AST lifecycle/token cross-check: 99 AST tag için `get_node_size`, destroy ve printer dispatch denetlendi. Factory size/destroy switch'te yalnız `AST_BREAK` ve `AST_CONTINUE` payload-free base-node default yolunda; printer 99 tag'in tamamını kapsıyor. Mevcut all-tag AST create/codec testleri var; child ownership/nullability alan matrisi hâlâ açık. Lexer'daki 163 tokenın Parser kaynakta doğrudan referansı olmayan 30'u belirlendi; bunların bir kısmı range check/contextual identifier yoluyla tüketiliyor, kalanlar token→grammar eşlemesinde tek tek kapatılmalı. | `omnicore-sdk/src/c/parser/ast.h`, `ast.c`, `lexer.h`, `parser.c`, `AST_NODE_FIELD_OWNERSHIP.md`; AST lifecycle switch audit ve token static scan, eksik-token review sonraki adım |\n| 2026-11-03 | Semantic operator kontrolleri genişletildi: binary bitwise `& | ^` matching integer operand şartı; compound numeric/bitwise assignment operand uyumu; `.=` için `str` operand şartı; assignment target identifier/member/index dışındaysa diagnostic. Semantic testleri integer bitwise/power ve geçerli-geçersiz compound, string append ve lvalue örneklerini kapsıyor. Yeni CI run `7ff6c70` Ubuntu/macOS/Windows ve ASan/UBSan başarılı; Valgrind son durumu bekleniyor, power fixture commit `6cf3bd5` CI bekliyor. | `omnicore-sdk/src/c/parser/semantic.c`, `omnicore-sdk/src/c/tests/test_semantic_arithmetic.c`; full CI latest commit üzerinden takip ediliyor |\n| 2026-11-03 | Parser operator grameri eklendi: `^^` sağ birleşimli ve unary minus'tan daha yüksek öncelikli; bitwise `&` > `^` > `|`; `+=`, `-=`, `*=`, `/=`, `&=`, `|=`, `^=` ve `.=` `AST_ASSIGNMENT` üzerinde orijinal operator token'ını koruyor. Range `..` bitwise zincirinin altında korunuyor. Parser testleri power/bitwise AST precedence ağacını ve `+=`/`&=` operator kimliğini doğruluyor. `<<`/`>>` yalnız spelling ile ayırt edilmeyecek; bit-shift ve Flow biçimleri operand/target yapısı + grammar context ile ayrıştırılacak, bit-shift parser üretimi şimdilik eklenmedi. | `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; commit `8f1870c`: Ubuntu/macOS/Windows build+CTest, ASan/UBSan ve Valgrind başarılı; precedence assertion commit `f4202e0` CI bekliyor |
| 2026-11-03 | Lexer sayı taramasında desteklenmeyen radix/üs/sonek biçimlerinin geçerli sayı + identifier olarak bölünmesi engellendi. `0xFF`, `0b1010`, `10E3`, `1e-3`, `1_000`, `12u8`, `1.5f32` tek TOKEN_ERROR ile reddediliyor; decimal integer/fraction ve `1..5` range token dizisi için regresyon testleri eklendi. Syntax referansı ve AST/Syntax matrix, `^^` operatörünün Lexer tokenı olmasına rağmen Parser üretimi bulunmadığını açıkça kaydediyor. | `omnicore-sdk/src/c/parser/lexer.c`, `omnicore-sdk/src/c/tests/test_lexer.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; GitHub Actions commit `140c720`: Windows, Ubuntu, macOS build/CTest; Ubuntu ASan/UBSan ve Valgrind tümü başarılı |
| 2026-11-03 | Syntax referansı main dalındaki `docs/language/syntax-reference.md` ile etkin dalın README içine gömülü dil bölümü karşılaştırılarak geri kuruldu. Etkin dalda `omnicore-sdk/docs/syntax.md` normatif giriş noktası oluşturuldu; README proje tanımı ve geliştirme takibine indirgenerek dil metni tekilleştirildi. Eski README'deki güncel Lexer/Parser sınırları, array dimension kuralları ve Halt açıklamaları; main'deki kapsamlı select/scan ve defer semantik açıklamalarıyla birleştirildi. main dalına yazma yapılmadı. | `omnicore-sdk/docs/syntax.md`, `omnicore-sdk/docs/README.md`, `README.md`; içerik taşıma tamamlandı, parser-syntax tamlık denetimi devam ediyor |
|---|---|---|
| 2026-11-03 | İlk merkezi pipeline ve 99 AST düğümlü takip tablosu oluşturuldu. | `OIR_AST_NODE_REGISTRY.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`, `FRONTEND_COMPLETENESS_AUDIT.md`, `AST_OIR_COMPLETION_PLAN.md`, `COMPILER_PIPELINE.md`, `OIR_MODULE.md`, `TASKS.md` |
| 2026-11-03 | Persistent Semantic Result Model, checker mirror integration, SEMANTIC_AST artifact writer ve bağımsız Semantic DLL kaynakta mevcut olarak doğrulandı. Semantic/AST artifact testleri ve OOM testleri mevcut; ancak bu oturumda build/CTest çalıştırılamadığı ve tam semantic coverage matrisi kapanmadığı için Semantic ve artifact validator `~` olarak kaldı. | `omnicore-sdk/src/c/semantic/semantic_result.{h,c}`, `omnicore-sdk/src/c/parser/semantic.c`, `omnicore-sdk/src/c/module/semantic_stage_module.c`, `test_semantic_result.c`, `test_semantic_result_oom.c`, `test_semantic_result_integration.c`, `test_oir_semantic_ast*.c`; CI durumu bu oturumda doğrulanmadı |
| 2026-11-03 | AST factory sınır denetimi eklendi: negatif ve son kayıtlı AST kind sonrasındaki enum değerleri artık `NULL` döndürür; ilk/son geçerli kind ve iki geçersiz sınır için parser testine regresyon kontrolü eklendi. AST genel sözleşmesi hâlâ `~`; tüm 99 düğümün ownership/child/nullability matrisi henüz kapanmadı. | `omnicore-sdk/src/c/parser/ast.c`, `omnicore-sdk/src/c/tests/test_parser.c`; commit `a724cefd08aa9e43e715e7e87a19cd541ae78669`, `379ba144fc4b81dd3cc8be1fef9ab476af39453c`; build/CTest henüz çalıştırılmadı |
| 2026-11-03 | AST factory regresyon testi 99 kayıtlı AST kind'ın tamamında oluşturma, type korunumu ve sıfırlanmış temel alanları dolaşacak şekilde genişletildi; negatif ve üst sınır enum değerleri reddedilmeye devam ediyor. Factory boyut haritası denetiminde yalnızca `AST_BREAK` ve `AST_CONTINUE` temel `ASTNode` boyutunu kullanıyor; destructor/codec tarafında da bu iki payload'sız düğümün boş işlem davranışı mevcut. Bu bulgu tek başına AST sözleşmesini tamamlamaz. | `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/parser/ast.c`, `omnicore-sdk/src/c/parser/ast.h`, `OIR_AST_NODE_REGISTRY.md`; GitHub Actions: Ubuntu build/CTest ve ASan/UBSan başarılı; Windows, macOS ve Valgrind işleri henüz sürüyor; commit `966d1ee3ea6b098a3bc8d67b22ad4d035e98728b` |

| 2026-11-03 | Parser precedence test expanded: `2^^3^^2` right-associative shape, `-2^^2` unary-minus outside power, and `2^^-2` unary RHS are asserted directly in AST. GitHub Actions run 41608482780 (commit `1b56e5a`) completed success; all five platform/sanitizer jobs passed. Syntax matrix now also explicitly records Lexer-only `no:`, `m:`, variadic ellipsis and ASM token gaps; reserved `r/t/o` remain intentionally inactive. | `test_parser.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`, run 41608482780 |
| 2026-11-03 | Semantic lvalue kapsamı genişletildi: `AST_INDEX_ACCESS` artık const array hedefini özyinelemeli izliyor ve sabit array indeksine yazmayı reddediyor. İkinci düzeltmede index assignment için array element türü ↔ RHS uyumluluğu da kontrol ediliyor. Testler const array write ve `i32[]` elemanına `str` yazma negatif örneklerini kapsıyor. İlk değişiklik commit `45fa4a1` CI 5/5 geçti; sonraki type/test commitleri için CI sürüyor. | `semantic.c`, `test_semantic.c`, run 41638940158 ve yeni run 41639121287 |
## 9. Kaynak belgeler

- `COMPILER_PIPELINE.md` — hedef pipeline ve DLL sınırları.
- `FRONTEND_COMPLETENESS_AUDIT.md` — lexer/parser/AST/semantic denetim kriterleri.
- `NEXSUS_AST_SYNTAX_MATRIX.md` — Nexus Flow syntax ↔ AST eşlemesi.
- `OIR_AST_NODE_REGISTRY.md` — 1–99 sabit AST node ID ve field kayıtları.
- `AST_NODE_OWNERSHIP_AUDIT.md` — 99 AST kind için payload, owned child/string ve lifecycle sözleşmesi; otomatik graph/ownership doğrulaması açık.
- `AST_ASM_DESIGN.md` — fastexec/asm label/raw text/asmcall/asmjmp için Parser öncesi AST tasarım notu; henüz uygulanmadı.
- `AST_OIR_COMPLETION_PLAN.md` — Codegen öncesi AST/Semantic/OIR kapıları.
- `OIR_SEMANTIC_AST.md` — semantic artifact sözleşmesi.
- `OIR_MODULE.md` — OIR_MODULE ve mevcut lowering kapsamı.
- `TASKS.md` / `PROJECT_STATUS.md` — görevler ve genel durum.


| 2026-11-03 | AST allocation/destructor/codec switch'leri `ast.h` payload'larıyla yeniden tarandı: 99 kind aralığı korunuyor; `AST_BREAK` ve `AST_CONTINUE` payload'sız temel node, `AST_LISTEN` capture payload paylaşımı, `AST_NEXUS_PIPE/FLOW` Nexus payload ve `AST_FUNC_DECL/EXTERN_DECL` ortak function payload kullanıyor. Bu ortak payload'lar bilinçli ama node-kind bazında sözleşme testleriyle korunmalı; child ownership hâlâ tüm alanlar için otomatik doğrulanmıyor. HEAD `b322177` GitHub Actions run 41605036105: Windows, Ubuntu, macOS, ASan/UBSan ve Valgrind 5/5 başarı. | `ast.h`, `ast.c`, run 41605036105 |

**Kural:** Bu tablo güncellenmeden bir AST düğümü veya pipeline aşaması tamamlandı olarak raporlanmaz.

| 2026-11-03 | Semantic array-index assignment checks CI 5/5 geçti (run 41639121287); progress-only commit run 41639157932 de başarılı. Assignment target auditinde identifier/member/index dışında kalan hedefler için fail-closed kontrol zaten mevcut; bunun için `(x + 1) = 2` negatif regresyon testi eklendi. Bu testin CI sonucu henüz bekleniyor. | `semantic.c`, `test_semantic.c`, run 41639121287, 41639157932 |

| 2026-11-03 | Compound assignment test kapsamı eklendi: numeric `+=/*=` geçerli, `str += str` ve float bitwise compound assignment reddedilmeli; u32 bitwise compound geçerli fixture olarak eklendi. AST_TUPLE semantiği bilinçli biçimde fail-closed: parser `(a,b)` tuple AST üretiyor fakat tuple type/assignment/destructuring sözleşmesi tanımlanmadan kabul edilmiyor. | `test_semantic.c`; compound/non-lvalue CI sonuçları bekleniyor |

| 2026-11-03 | Compound assignment fixtures: valid i32 numeric/bitwise updates; invalid str numeric and f32 bitwise updates. Fixture type assumptions corrected after CI diagnostics (f32 initializer produced extra existing type errors; u32 literal signedness mismatch). Current run 41646935715: Linux build+CTest passed and ASan/UBSan passed; Windows/macOS/Valgrind still running at record time. | `test_semantic.c`, CI run 41646935715 |


| 2026-11-03 | AST alan sözleşmesi ayrıştırıldı: factory'nin NULL-toleransı ile başarılı Parser sonrası required child ve syntax-optional field birbirinden ayrıldı. `AST_NODE_FIELD_OWNERSHIP.md` içine Optional-field ve Required-child baseline tabloları eklendi; generic graph validator (duplicate ownership/cycle/required child) hâlâ eksik, AST tamamlandı sayılmıyor. Syntax Reference'ta power desteği için eski “Parser tüketmiyor” notu düzeltildi; `^^` right-associative, unary eksi üstü bağlanma ve bitwise `& > ^ > |` precedence eklendi. Syntax matrix'teki `&&/||` Parser yok iddiası da gerçek `parse_logical_and/or` desteğiyle hizalandı; kısa-devre davranışı OIR/lowering kapısında kalıyor. | commits `7b5c1f8`, `22bdce4`, `a54362e`; docs-only; CI sonucu ayrıca doğrulanacak |

| 2026-11-03 | AST lifecycle kind audit: 99 AST tags cross-checked against allocation-size and destructor dispatch. All payload kinds have explicit factory sizing and ownership cleanup; BREAK/CONTINUE correctly use base-node allocation and no payload cleanup. Documented ownership graph invariant and destructor assumptions; generic graph validator (cycles, aliasing, required fields) remains pending. | commit `da8223e`; source audit only, no runtime/CI claim |


| 2026-11-03 | AST Graph Validator API ve test paketi eklendi: ast_validate public result/error modeli, invalid kind, required child, duplicate-node ownership/cycle tespiti ve 512 derinlik limiti. CMake test_ast_validator kaydı; valid manuel AST, child-cycle, sibling self-cycle, shared child, missing operand, NULL root ve Parser çıktısı doğrulama senaryoları. | commits f894523, 130ff14, d9ab181, 7da5885, 950301cb, b044591, 1bfcf56, 361d77e; yerel build/CTest henüz çalıştırılmadı |

| 2026-11-03 | AST Graph Validator genişletildi: tüm 99 AST tag için açık dispatch (child-bearing + leaf), hash tabanlı beklenen O(n) ownership kümesi, sibling-cycle ve ancestor-child-cycle için ayrı tanı, shared-node ayrımı, 512 child-depth limiti. Parser sözleşmesi düzeltmeleri: AST_FUNC_DECL.body zorunlu, AST_EXTERN_DECL.body ve AST_RETURN.operand opsiyonel, select default pattern opsiyonel, scan else pattern yalnızca son arm olabilir. Testler: invalid kind, leaf tag'ler, cycle/shared ownership, required child, depth limit, select default, scan else finality, void return ve Parser AST. | commits e33a043, 397f736, 30d52a9, 6da64f3, 3f8cdfb, 06cdd33, 4615b3c, 5e10a72; Linux + ASan/UBSan başarılı, Windows/Valgrind/macOS sonuçları bekleniyor |

| 2026-11-03 | Legacy AST stream codec artık `ast_validate()` ile bütünlük kontrolü yapıyor: serialize geçersiz grafikte yazmaya başlamadan reddediyor; deserialize tam stream ve graph doğrulamasından sonra kökü teslim ediyor. Testler: shared ownership için no-write, eksik function body içeren bozuk stream reddi, geçerli stream round-trip. Mevcut `ast_rejects_malformed_streams` 99-tag testi de her kind için geçerli fixture kuracak şekilde güncellendi. HEAD `568ed8b`: Linux CTest + ASan/UBSan başarılı; Windows, Valgrind ve macOS işleri sürüyor. | `ast.c`, `ast.h`, `test_ast_validator.c`, `test_ast_deserialize.c`, `AST_NODE_FIELD_OWNERSHIP.md`, `AST_NODE_OWNERSHIP_AUDIT.md`, CI run 41658921530 |

| 2026-11-03 | Parser–AST–OIR round-trip entegrasyon testleri genişletildi: hem geniş Nexus Flow fixture'ının Parser çıktısı hem OIR reader ile geri kurulan AST `ast_validate()` kontrolünden geçmek zorunda; ana test1.nxf Parser AST'si de aynı kontrolde. Böylece Graph Validator yalnızca izole elle kurulmuş düğümlerde değil, gerçek Parser/OIR geçişlerinde de sınanıyor. | `test_parser_ast_oir_roundtrip.c`; CI run sonucu bekleniyor |

| 2026-11-03 | Serializer güvenlik regresyonu genişletildi: shared-child reddine ek olarak self-cycle içeren AST'nin de hiçbir byte yazılmadan reddedildiği doğrulanıyor; test fixture cycle bağlantısını güvenli temizleme öncesi ayırıyor. | `test_ast_validator.c`; CI sonucu bekleniyor |

| 2026-11-03 | AST yapısal denetimi: 99/99 ASTNodeType için açık Graph Validator dispatch doğrulandı; default artık kayıt dışı tag'i fail-closed reddediyor. Kalan gerçek AST kapıları belgelendi: AST üzerinde byte/end source span yok; }?; block-cleanup AST marker'ı yok; inline ASM parser/model yolu yok; aktif dalda NEXUS-FLOW-SYNTAX-REFERENCE.md yerine omnicore-sdk/docs/syntax.md bulunuyor ve kanonik belge yolu uzlaştırılmalı. Bu nedenle AST tamamlandı işaretlenmedi. | ast.c e7582e1; NEXSUS_AST_SYNTAX_MATRIX.md; AST_NODE_OWNERSHIP_AUDIT.md; CI run 41666210514 sürüyor |

| 2026-11-03 | Syntax referansı yol denetimi düzeltildi: main dalında kanonik belge `docs/language/syntax-reference.md`; aktif refactor dalında bu yol yok, onun yerine Parser sınırları ve yeni array/Halt notları eklenmiş `omnicore-sdk/docs/syntax.md` var. Ana dal yalnızca okundu, değiştirilmedi. Kanonik yolun aktif dala tek normatif belge olarak taşınması sonraki dokümantasyon işi. | `NEXSUS_AST_SYNTAX_MATRIX.md`, `AST_NODE_OWNERSHIP_AUDIT.md` |

| 2026-11-03 | AST syntax-reference path reconciliation completed on `refactor/c-core-roadmap`: restored the canonical `docs/language/syntax-reference.md` using the already reconciled active-branch syntax content, removed the duplicate `omnicore-sdk/docs/syntax.md`, and retained `main` unchanged. The parser/syntax comparison is still open; this closes only the competing-document-path issue. | `docs/language/syntax-reference.md`, `omnicore-sdk/docs/syntax.md` (removed), `NEXSUS_AST_SYNTAX_MATRIX.md`, `AST_NODE_OWNERSHIP_AUDIT.md` |
| 2026-11-03 | CI run `41666210514` rechecked: Ubuntu, Windows, ASan/UBSan and Valgrind succeeded; macOS remained queued at inspection time. AST syntax-reference path reconciliation is complete, but the Halt matrix was tightened: `AST_NEXUS_HALT` does not represent block-scoped `}?;`, and `parse_block()` does not consume it. AST remains open. | `NEXSUS_AST_SYNTAX_MATRIX.md`, `AST_NODE_OWNERSHIP_AUDIT.md`; CI run `41666210514` |
| 2026-11-03 | Block-scoped Halt AST representation implemented: appended `AST_BLOCK_HALT` (AST kind 100) without renumbering existing kinds; `parse_block()` consumes the closing `}?;` and appends a final marker; graph validator rejects root-level, out-of-block and non-final markers. OIR registry appended stable ID 100, field schema defines zero fields, frontend writer emits the leaf node, and node table max/count updated. Parser/validator/OIR mapping tests added; full OIR round-trip fixture now includes `}?;`. CI is required before marking this gap complete. | `ast.h`, `ast.c`, `parser.c`, OIR AST map/index/schema/writer/node table, `test_parser.c`, `test_ast_validator.c`, `test_oir_ast_map.c`, `test_parser_ast_oir_roundtrip.c`; commits `86c1584`–`bf4fbda` |
| 2026-11-03 | Block-Halt entegrasyon CI düzeltmeleri: validator artık marker'ın gerçek ebeveynini active stack'te bir üst seviyeden kontrol ediyor; AST factory sınır testi 100. tag'e genişletildi; legacy codec all-tag testi için block marker ayrı, bağlam-geçerli fixture ile doğrulanıyor; geçersiz test grafiğinden ayrılan sibling serbest bırakılıyor. CI run `41687912766`: Ubuntu, Windows, ASan/UBSan ve Valgrind başarılı; macOS queued. Ayrıca kaynak konumlarının AST/legacy codec/OIR boyunca taşınması için normatif tasarım `docs/AST-SOURCE-SPAN-CONTRACT.md` oluşturuldu; uygulama henüz yapılmadı. | `ast.c`, `test_parser.c`, `test_ast_validator.c`, `test_ast_deserialize.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `c0322e8`, `12966b5`, `1b5d46e`, `ad540d4` |
| 2026-11-04 | Syntax Reference ↔ AST model karşılaştırması genişletildi. AST enum/payload ve Syntax Reference örnekleri üzerinden gerçek model boşlukları ayrı sınıflandırıldı: MUST dependency metadata, variadic/named argument, generic type-variable ve function-type/callback modeli, macro expansion AST/phase, target/link directive, DLL export/import resource-kind, ASM call/jump/substitution/clobber, structured map type ve numeric shift expression. `elsif`, conditional action, array declarator ve select/scan guard gibi mevcut AST ile temsil edilebilenler yeni node gerektirmeyen biçimler olarak ayrıldı. Bu audit belge değişikliğidir; bu maddeler implement edilmiş değildir. | `NEXSUS_AST_SYNTAX_MATRIX.md`, commit `ce8ad62`; AST implementation gate açık |


| 2026-11-04 | Normative syntax confirmed: `...` is a variadic declaration-list marker (example `f:liste(a!i32[], ...)`); `callee(a:10,b:20)` is named-call argument syntax. Parser currently lacks both. Contract selected without changing existing node payloads: append zero-payload final `AST_VARIADIC_PARAM` marker and append `AST_NAMED_ARG(name,value)`; positional args remain existing expression nodes. Enforce marker finality/uniqueness, named labels uniqueness and no positional argument after named arguments. This is design decision, not implementation/test completion. Semantic packing/resolution remains later. | `docs/language/syntax-reference.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; implementation pending |

| 2026-11-04 | Main process CLI arguments clarified: canonical syntax already has `f:main(argc, argv)!i32`. `argc` excludes executable name; `argv` is dynamic `str[]`, same order, argc=len(argv), no NULL sentinel. Runtime/OS adapter—not AST—creates values; AST params remain ordinary declarations. Semantic entry validation, UTF-8 boundary, Windows/POSIX adapter, bare-metal injected/empty args and tests pending. | `docs/language/syntax-reference.md`, `NEXSUS_AST_SYNTAX_MATRIX.md` |

| 2026-11-04 | Variadic/named-argument AST implementation started on `refactor/c-core-roadmap`: appended AST kinds 101/102; added payload/lifecycle/codec handling; parser supports final variadic markers and named arguments in ordinary + intrinsic calls; parser rejects duplicate labels and positional-after-named; validator enforces context, finality, required payload, ordering and uniqueness. Added parser codec round-trip and AST validator tests. CI run `41759489746` passed: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. OIR registry/schema/bridge support remains pending, so this is not yet a completed pipeline feature. | `ast.h`, `ast.c`, `parser.c`, `test_parser.c`, `test_ast_validator.c`; commits `32fa50a`, `26fdb24`, `3b88931`, `1b81b18`, `3700189`, `db0503a`, `54aab47`, `8a74d12`, `e2181bd` |

| 2026-11-04 | MUST dependency AST/Parser gap closed at frontend representation level: appended `AST_MUST_DECL` (kind 103) with owned name/source expression; Parser consumes `m:Name = source;`, distinguishes it from `use`, enforces program scope; AST factory/printer/destructor/codec/validator and parser/validator tests updated. CI run `41760397845` passed on Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. OIR ID/schema/bridge and Semantic dependency resolution/failure diagnostics remain pending; AST phase remains open for other audited gaps. | `ast.h`, `ast.c`, `parser.c`, `test_parser.c`, `test_ast_validator.c`, `test_ast_deserialize.c`; commits `0d0beac`, `f999dc5`, `98b6a9f`, `72cc7c8`, `09b9b44`, `a36c4c4`, `a90a2a6`, `233a7b2`, `48bff3d`, `8915d61`, `a36c4c4`, `a90a2a6`, `8e0153b` |

| 2026-11-04 | Generic signature audit refined: current parser already accepts `a!t`, repeated `t` parameter types and `!t` return type; AST stores these spellings losslessly in `ASTVarDecl.type_name` and `ASTFuncDecl.return_type`. Added parser/AST-codec round-trip test. This closes the representation gap without inventing a generic-parameter AST node; type-variable binding, unification and inference remain Semantic work. | `parser.c`, `test_parser.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`; commit `b6a88a7` |

| 2026-11-04 | Combined frontend validation after MUST and generic-signature work: CI run `41760749308` passed all five jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Variadic/named argument and MUST AST tests, plus generic type spelling round-trip, are included. OIR node registry/schema/bridge remains the next pipeline-stage integration; other AST syntax gaps remain open. | `ast.h`, `ast.c`, `parser.c`, parser/validator/deserializer tests, `NEXSUS_AST_SYNTAX_MATRIX.md`; CI `41760749308` |


## 2026-11-04 — Function type / callback AST gap audit

- Canonical syntax currently contains the example `nt:Callback = f:(id!u32)>!bool;`, but this is only an example, not a complete function-type grammar.
- Confirmed `ASTNewTypeDecl.target_type` is a scalar `char*`; it cannot encode parameter list structure or a return-type node. `AST_LAMBDA` models executable lambda bodies and is not a substitute for a function type.
- No implementation was marked complete. Before adding a node, the function-type grammar must explicitly settle: typed parameter list and separators, optional parameter names, variadic placement, return-type delimiter, nested callback types, and function-type values/call compatibility.
- Next AST task: define the minimal grammar from the existing callback example (without changing lambda/declaration syntax), then add an append-only function-type AST payload and lifecycle/codec/validator/printer/parser tests. OIR mapping remains downstream; Codegen remains gated.


## 2026-11-04 — AST source-span implementation gate (audit)

- Rechecked `docs/AST-SOURCE-SPAN-CONTRACT.md` against current `ASTNode`: it still contains only `line/col`; full byte range and end coordinates are absent. Lexer tokens already carry offsets/lengths, but AST parser propagation, versioned legacy codec and OIR optional TLVs are not implemented.
- The contract's compatibility boundary is retained: append `OccSourceSpan` to ASTNode; keep line/col mirrors; version the legacy AST stream with a magic prefix while accepting old unprefixed streams with span absent; keep OIR's 32-byte node record and use optional span TLVs.
- This is a confirmed foundational gap, not a completion claim. Implementation requires a cross-cutting change in token→parser node stamping, AST validator, legacy serializer/deserializer and OIR AST bridge, plus exact-range UTF-8/multiline/legacy compatibility tests.
- **Next implementation priority:** source-span propagation and codec compatibility before more isolated AST syntax nodes. Callback/function-type remains separately blocked on a complete grammar; ASM payload must follow its existing normative syntax and must not be guessed.


### AST Source Span — API/storage başlangıcı (2026-11-04)

- [x] `ASTNode` içine `OccSourceSpan` alanı eklendi; mevcut `line/col` alanları geriye dönük başlangıç-konumu aynaları olarak korundu.
- [x] `ast_set_source_span()` API'si eklendi: NULL ile temizleme, offset sıralama kontrolü ve line/col senkronizasyonu.
- [x] AST validator testine span atama/geçersiz aralık/temizleme kontrolleri eklendi.
- [ ] Parser'ın token başlangıç ve düğüm bitiş konumlarını tüm AST üretim yollarına yayması.
- [x] AST serialization sürümleme + legacy stream uyumluluğu; span round-trip ve legacy unprefixed okuma testleri.
- [x] Truncated magic/version prefix negatif testleri.
- [ ] OIR span TLV aktarımı ve entegrasyon testleri.
- **Durum:** API/storage/validator ve sürümlü legacy codec kaynakları commit edildi; parser stamping ve OIR TLV bekliyor. Source Span tamamlanmış değildir.


### Multi-file source identity — kaynak kayıt tablosu (2026-11-04)

- [x] `OccSourceFileTable` eklendi: derleme işi kapsamında dosya yollarını sahiplenir, tekrar eden yolu aynı kimliğe bağlar ve `source_id -> path` çözümlemesi sağlar.
- [x] Kaynak kimlikleri eklenme sırasına göre atanır; tablo ömrü derleme işiyle sınırlandırılmalıdır.
- [x] OIR `SOURCE_UNIT` payload okuyucu/yazıcısı artık yalnızca `source_id == 0` kabul etmiyor; uint32 kaynak kimliğini saklayıp round-trip yapıyor.
- [x] Diagnostics ve OIR SOURCE_UNIT için çoklu kaynak kimliği testleri eklendi.
- [ ] Parser/lexer ve derleme işi yöneticisi kaynak tablosuna bağlanmalı; her token/AST span doğru source_id ile damgalanmalı.
- [ ] Tanılama biçimleyicisi source_id üzerinden dosya yolunu göstermeli.
- [ ] Çoklu dosya entegrasyon testi: iki kaynakta ayrı hata üretip doğru dosya/satır/sütun eşleşmesini doğrulamalı.
- **Durum:** Kaynak kayıt tablosu ve OIR kimlik desteği commit edildi; uçtan uca parser/diagnostic entegrasyonu ve CI sonucu bekleniyor.


### Parser → diagnostics source identity bağlantısı (2026-11-04)

- [x] `ParserContext` için derleme işi tarafından sahip olunan `OccSourceFileTable` bağlantısı eklendi.
- [x] `parser_attach_source_files()` parser dosya adını/yolunu kayıt tablosuna ekleyip `source_id` değerini context'e atıyor.
- [x] Parser diagnostic kayıtları, konum span'i mevcut olsa da olmasa da context'in `source_id` değerini taşıyor.
- [x] Parser testi: aynı tabloda ikinci dosyaya ayrılmış ID'nin diagnostic span'e taşındığı doğrulanıyor.
- [x] Lexer diagnostic üretimi `ParserContext.source_id` değerini span'e taşıyor; farklı dosya kimliği ve satır/sütun için regresyon testi eklendi (CI bekleniyor).
- [ ] AST node üretim yolları tam başlangıç/bitiş span'i ve source_id ile damgalanmalı.
- [ ] CLI/host diagnostic formatter source_id'yi dosya yoluna çözümleyip göstermeli.
- [ ] İki dosyalı uçtan uca test ve CI sonuçları.
- **Durum:** Parser context ve parser diagnostic kimlik bağlantısı eklendi; tam çoklu dosya derleme ve kullanıcıya gösterilen dosya adı henüz tamamlanmadı.

| 2026-11-04 | AST source span doğrulaması eklendi: `ast_validate()` ters byte aralığını, sıfır bitiş satır/sütununu ve `line/col` başlangıç aynası uyuşmazlığını reddediyor; regresyon testleri eklendi. Parser production stamping, versioned legacy codec ve OIR TLV hâlâ açık. | `ast.h`, `ast.c`, `test_ast_validator.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md` |


| 2026-11-04 | Legacy AST codec sürümlendi: yeni yazıcı `OCCAST\r\n` + version 1 başlığı yazar ve her düğümün `OccSourceSpan` alanlarını padding bağımsız ayrı scalar alanlar olarak saklar. Okuyucu eski başlıksız stream'leri de kabul eder (span absent), bilinmeyen sürümü fail-closed reddeder. Testler: source_id/byte offset/line-column tam round-trip, unprefixed legacy stream okuma ve bilinmeyen sürüm reddi. Parser üretim sınırlarında span damgalama ve OIR optional TLV hâlâ açık; feature tamamlanmadı. | `ast.c`, `test_ast_deserialize.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `89d9ba8`, `6a0ae82`, `b91640d`, `dbe3a47`, `63702f6`; CI run `41786516465` passed 5/5 (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind) |

| 2026-11-04 | AST stream codec negatif sınır testleri tamamlandı: kısmi magic ve tam magic + kısmi version başlıkları reddediliyor; bilinmeyen version reddi ve legacy unprefixed okuma testleri de mevcut. İlk CI turunda testte magic literal'ı yanlış escape edildiği için test başarısız oldu; literal düzeltildi ve yeni CI çalışması başlatıldı. | `test_ast_deserialize.c`; commits `b91640d`, `dbe3a47`, `63702f6`; son CI bekleniyor |

| 2026-11-04 | Parser source-span stamping ilk üretim grubu eklendi: number/string/char literal, boolean literal ve identifier leaf düğümleri gerçek token byte aralığı + satır/sütun + `ParserContext.source_id` ile damgalanıyor; binary ve assignment expression aralığı ilk/son child span'lerinden birleştiriliyor; block aralığı açılış `{` ile kapanış `}` token'larından, varsa block'a ait `}?;` dahil edilerek oluşturuluyor. Parser testleri literal/identifier/binary aralıklarını ve `}?;` dahil tam block aralığını doğruluyor. Variable declaration prefix→semicolon ve function declaration prefix→body-close aralıkları da damgalanıyor; diğer declaration formları ve multiline/UTF-8 sınırları hâlâ açık olduğundan Source Span tamamlanmadı. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `4fc975e`, `cfcd9c7`; CI bekleniyor |

| 2026-11-04 | Block AST source span eklendi: parser'ın tükettiği açılış `{` ile kapanış `}` aralığı damgalanıyor; blok `}?;` ile kapanıyorsa bitiş HALT token'ını kapsıyor. Parser testi body span başlangıç/bitiş offset ve line/column değerlerini tam doğruluyor. Declaration, multiline ve UTF-8 kapsamı hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `3f7d107`, `c54821d`, `34be0e1`; CI bekleniyor |

| 2026-11-04 | Parser declaration span kapsamı genişletildi: variable declaration prefix token'dan tüketilen `;` token'ına, function declaration prefix token'dan body'nin `}`/`}?;` kapanışına kadar damgalanıyor. Parser testleri literal, identifier, binary expression, variable declaration ve function declaration tam offset/column aralıklarını kontrol ediyor. Diğer declaration türleri, multiline ve UTF-8 testleri bekliyor. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `c161a36`, `607de03`; CI bekleniyor |

| 2026-11-04 | Return/break/continue statement span'leri eklendi: keyword başlangıcından tüketilen `;` sonuna kadar gerçek token aralığı kullanılıyor. Parser testinde `ret x;` tam offset ve column aralığı doğrulanıyor. if/loop/flow ve diğer declaration türleri, multiline/UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `e4675a5`, `76f7fe8`; CI bekleniyor |

| 2026-11-04 | Prefix unary expression span'leri eklendi: `-`, `!`, `~`, prefix `++/--` token başlangıcından operand'ın son token'ına kadar kapsanıyor. Negatif sayı operand'ı için exact offset/column parser testi eklendi. Postfix/call/if/loop/flow aralıkları ve multiline/UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `f1ceec9`, `8976d09`; CI bekleniyor |

| 2026-11-04 | Postfix/call span kapsamı eklendi: index `target..]`, member access `object..member`, call `callee..)` ve postfix `operand..++/--` aralıkları korunuyor; named argument label'dan value sonuna kadar damgalanıyor. Parser testleri index/call/named arg/postfix aralıklarını doğruluyor. if/loop/flow, multiline ve UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `d630857`, `400bfcf`; CI bekleniyor |

| 2026-11-04 | Conditional AST span eklendi: `if`/`elsif` keyword başlangıcından son then/else branch üretiminin son token'ına kadar aralık korunuyor. İç içe `elsif` zincirinde her koşul kendi üretim aralığını taşıyor. Parser testi if+els tüm aralığını doğruluyor. loop/flow, multiline ve UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `9f1a652`, `ea63aad`; CI bekleniyor |

| 2026-11-04 | Array literal ve loop span'leri eklendi: array literal `[`→`]`; hem `loop(...)` hem `(loop)` biçimi başlangıç keyword/delimiter'ından body kapanışına kadar damgalanıyor. Parser testi array literal ve `(loop)` body aralığını doğruluyor. Nexus Flow pipe/flow, multiline ve UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `d2d94f8`, `c8342d8`; CI bekleniyor |

| 2026-11-04 | Nexus Flow expression span'leri eklendi: pipe/flow/relocate düğümleri sol ve sağ operand aralıklarını birleştiriyor; unpack düğümü operand başlangıcından `...>` token sonuna kadar uzanıyor. `1 -> 2` AST_PIPE exact aralık testi eklendi. Multiline ve UTF-8 source-span testleri hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `0e61334`, `1cfb8ab`; CI bekleniyor |

| 2026-11-04 | Select/scan span kapsamı eklendi: root `select`/`scan` token'ından kapanış `}`'a; her arm pattern/default/else başlangıcından body'nin son token'ına kadar damgalanıyor. Pattern binding ve constructor pattern node'ları da token aralıklarını taşıyor. `select` root + iki arm için exact span testi eklendi. Multiline/UTF-8 hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `7bc3bdd`, `5737d62`; CI bekleniyor |

| 2026-11-04 | Array type source-span kapsamı eklendi: `AST_ARRAY_TYPE` tüm ardışık dimension aralığını, her `AST_ARRAY_DIMENSION` kendi `[...]` üretimini taşıyor. `T[m][n]` için iki boyutun ayrı offset sınırlarını doğrulayan parser testi eklendi. Multiline ve UTF-8 span testleri hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `c25e0a1`, `6e39d40`; CI bekleniyor |

| 2026-11-04 | Parser statement dispatch'te use/defer/MUST ve struct/union/group/enum/newtype declaration düğümleri başlangıç token'ından üretim son token'ına kadar damgalanıyor. Bu familyalar için özel exact-span fixture'ları ayrıca eklenmeli; CI Linux + ASan/UBSan geçişleri mevcut, çapraz-platform/Valgrind işleri sürüyor. | `parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `1496245`; CI run `41787772753` sürüyor |

| 2026-11-04 | Nexus control statement spans eklendi: `?>` jump, targeted Halt, `@` intent ve `!!` panic düğümleri başlangıç token'ından son tüketilen token'a kadar damgalanıyor. `?>(done);` için exact range parser testi eklendi. Aggregate familyalar için özel span fixture'ları ve multiline/UTF-8 testleri hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `e03ad63`, `0c9229b`, `537cfc3`; CI bekleniyor |

| 2026-11-04 | AST graph validator source-span invariantleri sıkılaştırıldı: byte offset sırasına ek olarak end line'ın start line'dan önce olmaması ve aynı satırda end column'ın start column'dan küçük olmaması doğrulanıyor. İki negatif test eklendi. | `ast.c`, `test_ast_validator.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `5e6adf0`, `d1ed188`; CI bekleniyor |

| 2026-11-04 | AST legacy codec reader artık seek gerektirmiyor: prefix sniff sırasında okunan ilk 8 byte, unprefixed legacy stream ise `ASTReader` replay buffer'ından decoder'a aktarılıyor; `ftell/fseek` bağımlılığı kaldırıldı. Non-seekable stream davranışı kodda destekleniyor; platformlar arası pipe fixture henüz eklenmedi. | `ast.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `b4dce01`; CI bekleniyor |

| 2026-11-04 | Tuple ve intrinsic call span'leri eklendi: tuple `(`→`)`, cast/panic ve diğer intrinsic call düğümleri keyword→`)` aralığını taşıyor; intrinsic named arg'lar label→value sonu aralığıyla damgalanıyor. Parser testinde tuple ve cast span'leri doğrulanıyor. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `50a6cd4`, `91e495a`; CI bekleniyor |

| 2026-11-04 | Nexus jump span fixture düzeltildi: ilk testte `done` lexer tarafından `TOKEN_KW_DONE` olarak tanındığı için parser test girdisi geçersizdi; label `target` ile değiştirildi. Jump source-span testi artık parse + AST validate + exact byte/column sınırlarını kontrol ediyor. CI run `41788210860` passed 5/5. Non-seekable codec reader commit `b4dce01` da CI run `41788408361` passed 5/5. | `test_parser.c`, `OCC_PIPELINE_PROGRESS.md`; commits `c54e46d`, `b4dce01` |

| 2026-11-04 | Lambda ve thread-block span'leri eklendi: lambda `fn` keyword'ünden expression/block sonuna; thread block `(` başlangıcından body kapanışına, iç block ise özel `>{` token'ından kapanış `}`'a kadar damgalanıyor. İki exact parser fixture eklendi. Multiline/UTF-8 span testleri hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `80d7581`, `d588da9`; CI bekleniyor |

| 2026-11-04 | Multiline ve UTF-8 source-span testleri eklendi: multibyte `é` içeren string token'ında byte offset/column uzunluğu; 4 satırlı fonksiyon gövdesinde literal, block ve function end line/column değerleri exact doğrulanıyor. Source Span parser stamping hâlâ tüm node üretimlerini kapsamıyor; eksik production'lar audit edilecek. | `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `faa4bad`; CI bekleniyor |

| 2026-11-04 | Nexus capture AST span eksikliği kapatıldı: left identifier span'i korunarak sağ expression sonuna kadar genişletiliyor; left node span kopyalandıktan sonra güvenli biçimde destroy ediliyor. `x <- handler` için exact capture span + AST graph validation testi eklendi. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `1ae3b83`, `b3e29d8`; CI bekleniyor |

| 2026-11-04 | Lambda span fixture, `fn(x)` biçiminin lexer tarafından keyword olarak üretilmediği ve normatif syntax referansında lambda üretimi tanımlı olmadığı için kaldırıldı. `AST_LAMBDA` legacy parser helper'ı syntax matrix kararı verilene kadar desteklenen üretim sayılmayacak; yeni testte yanlış syntax varsayımı korunmadı. Thread-block testi bağımsız şekilde devam ediyor. | `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `7caf6df` |

| 2026-11-04 | Function parameter span'leri eklendi: typed/untyped param node'ları name token'ından type/array suffix sonuna; `...` variadic marker kendi token aralığına damgalanıyor. `a!i32, ...` fixture exact offset ve AST validation testi eklendi. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `dac25de`, `a133730`; CI bekleniyor |

| 2026-11-04 | Aggregate AST child span'leri eklendi: struct/union field'ları prefix→type/array suffix sonu; enum variant'ları name→payload kapanış `)`; enum payload field'ları name→type sonu. Mevcut aggregate parser fixture'ı struct/newtype/enum/union root ve child span offset'lerini exact doğrulayacak şekilde genişletildi. Group member/inline newtype aggregate coverage hâlâ açık. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `cde1aa9`, `e13104d`, `17d58e8`; CI bekleniyor |

| 2026-11-04 | Aggregate span fixture doğrulaması düzeltildi: `Running(pid!u32)` variant span'inin kapanış `)` token'ını da kapsaması gerektiği testte netleştirildi (`enum_data + 8`). CI run `41789562104` passed 5/5 (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). | `test_parser.c`, `OCC_PIPELINE_PROGRESS.md`; commits `e13104d`, `17d58e8`, `42c2371` |

| 2026-11-04 | AST_PROGRAM root span eklendi: `[0, source_length)` tüm source file aralığını kapsıyor; byte-based line/column taraması ile empty/multiline/UTF-8 kaynaklar doğru uç konumlarını alıyor. Multiline parser testine root span exact assertion eklendi. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `1067068`, `b46b802`, `e046760`; CI bekleniyor |

| 2026-11-04 | Normatif lambda syntax yeniden kontrol edildi: `syntax-reference.md` açıkça `f:(a,b)>expr` ve `f:(a,b){...}` tanımlıyor. Önceki `fn(x)` fixture varsayımı hatalıydı; Parser primary dispatch `TOKEN_PRE_FUNC` + `(` biçimini lambda olarak tanıyacak şekilde düzeltildi. Expression/block lambda ve parametre span testleri eklendi. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `a7db881`, `556b27b`; CI bekleniyor |

| 2026-11-04 | AST_PROGRAM root compile fix: `ParserContext.source_len` field used. Nexus label (`label:> statement`) span now starts at label expression and extends through attached statement; empty semicolon no-op block nodes also receive token spans. Exact label span test eklendi. | `parser.c`, `test_parser.c`; commits `44c5552`, `3d11b35`, `f0d52b4`; CI bekleniyor |

| 2026-11-04 | AST enum/validator/parser kapsamı statik audit edildi: `ASTNodeType` toplam 103 değer; `ast_validate()` içinde 103/103 sembol mevcut. Parser kaynak kodunda syntax node üretimi bulunmayan türler: `AST_ASM_BLOCK`, `AST_ZONE_SYNC`, `AST_ZONE_VIEW`, `AST_ZONE_LOCK`. İlk eksik inline ASM, syntax-reference Bölüm 9 ile eşleştirilerek ele alınacak; zone sync/view/lock türlerinin normatif syntax karşılığı ayrıca doğrulanacak. | `ast.h`, `ast.c`, `parser.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`; matrix count 103 güncellendi |

| 2026-11-04 | Targetless `?;` Halt parser bug düzeltildi: Halt token'ından sonra semicolon ayrı token değilse ve current token block/EOF ise expression parse etmiyor; böylece ikinci `;` zorunlu kılınmıyor. AST_NEXUS_HALT null-expression + exact two-byte source span testi eklendi. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `d7678bc`, `b114552`; CI bekleniyor |

| 2026-11-04 | Parser source-span genişletme kilometre taşı: AST_PROGRAM full-source range, group member, lambda expression/block, ruleset/apply/on/unroll, Nexus label/capture, targetless Halt ve AST_BLOCK_HALT marker testleri eklendi. Normatif `f:(params)>expr` ve `f:(params){...}` parser desteği doğrulandı. CI `41790370043`, `41790466624`, `41790585918` passed 5/5; hedefsiz Halt testi `41791145098` üzerinde Ubuntu + ASan/UBSan + macOS geçti, Windows/Valgrind sürüyor. | `parser.c`, `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`, `NEXSUS_AST_SYNTAX_MATRIX.md` |

| 2026-11-04 | AST Parser coverage audit sonucu: 103 AST kind; graph validator 103/103; syntax'ta tanımlı 100 kind'in 99'u parser'da referanslı, tek eksik AST_ASM_BLOCK. AST_ZONE_SYNC/VIEW/LOCK için normative syntax production yok, reserved olarak tutuluyor. Inline ASM payload modeli bilinçli olarak tasarlanmadı; syntax-reference §9 raw instruction, label, `%variable`, `%variable:register`, asmcall/asmjmp ve clobber sahipliğini tanımlıyor; generic AST_BLOCK'e indirmek veya opaque payload uydurmak yerine model kararı gerekiyor. OIR writer tarafında VARIADIC_PARAM/NAMED_ARG/MUST_DECL açık bridge eksikleri olarak kaydedildi. Targetless Halt test CI `41791145098` passed 5/5. | `NEXSUS_AST_SYNTAX_MATRIX.md`, `OCC_PIPELINE_PROGRESS.md`; commits `24bd60f`, `2460d44`, `b38f0bb` |

| 2026-11-04 | Inline newtype aggregate span coverage eklendi: normative `nt:struct: point { ... }` üretiminde AST_STRUCT_DECL root ve iki field node'unun prefix/type byte aralıkları test ediliyor; AST graph validation da doğrulanıyor. | `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `badb44e`; CI bekleniyor |

| 2026-11-04 | Multi-source AST root identity testi eklendi: parser source registry'de ikinci dosya ID=1 iken AST_PROGRAM source_span.source_id=1 ve `[0, source_len)` byte aralığını koruyor; aynı fixture diagnostic source_id kontrolünü de sürdürüyor. | `test_parser.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commit `182b202`; CI bekleniyor |

| 2026-11-04 | Inline newtype aggregate test doğrulandı: `nt:struct: point { ... }` AST root + field span fixture CI `41791509581` passed 5/5 (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). | `test_parser.c`, `OCC_PIPELINE_PROGRESS.md`; commit `badb44e` |

| 2026-11-04 | Inline ASM çağrı girişleri ayrıştırıldı: `asmcall("LABEL")` ve `asmjmp("LABEL")` mevcut AST_FUNC_CALL + AST_IDENTIFIER modelini kullanıyor; yeni node kind icat edilmedi. Her iki çağrının parantez dahil exact span'ini ve callee adını doğrulayan parser testi eklendi. Raw `fastexec/asm:LABEL { ... }` payload modeli hâlâ açık mimari kararı. | `parser.c`, `test_parser.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`; commits `9047485`, `64214fd`; CI bekleniyor |

| 2026-11-04 | AST_PROGRAM multi-source identity test CI `41791636064` passed 5/5. asmcall/asmjmp AST_CALL implementation and exact span tests: run `41791787499` passed Ubuntu, Windows, macOS and ASan/UBSan; Valgrind still running at update time. | `parser.c`, `test_parser.c`; commits `182b202`, `9047485`, `64214fd` |

| 2026-11-04 | asmcall/asmjmp parser + AST_FUNC_CALL source-span testi CI `41791787499` tamamlandı ve 5/5 geçti (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). | `parser.c`, `test_parser.c`; commits `9047485`, `64214fd` |


| 2026-11-04 | FASTEXEC/Inline ASM AST audit: confirmed the Lexer keyword tokens exist, but Parser currently consumes only `asmcall`/`asmjmp` as generic `AST_FUNC_CALL`; FASTEXEC and raw ASM declaration parsing are missing. Approved append-only contract: `AST_FASTEXEC_BLOCK` C/OIR kind 104 and `AST_ASM_LABEL` C/OIR kind 105; preserve existing `AST_ASM_BLOCK` kind 29 as legacy and do not reinterpret its serialized payload. Register already-existing frontend kinds `AST_VARIADIC_PARAM` / `AST_NAMED_ARG` / `AST_MUST_DECL` as OIR 101–103. Raw ASM must be a dedicated Lexer token/span, not ordinary Nexus Flow tokens. FASTEXEC is the required scope for ASM and CPU primitive operations, with GPU/NPU families extensible later. Design and syntax documents updated; no implementation or tests are claimed. | `ast.h`, `ast.c`, `parser.c`, `lexer.c`, OIR AST map/writer/reader/schema, parser and OIR tests, `AST_ASM_DESIGN.md`, `OIR_AST_NODE_REGISTRY.md` |

| 2026-11-04 | FASTEXEC için sıkı derleyici doğrulama politikası tasarıma eklendi: placeholder read/write yönü, uninitialized read, immutable write, tip/operand genişliği, register ve bellek mapping çakışmaları, clobber/live-value koruması, label/call/jump/fall-through kontrolü, target capability ve ABI/hizalama kontrolleri; ham ASM'nin tam statik kanıtlanamadığı yerde fail-safe diagnostic/warning. Derleyici keyfî ASM'yi otomatik düzeltmeyecek veya güvenli olduğunu iddia etmeyecek. `AST_ASM_DESIGN.md` ve normatif ASM spesifikasyonu güncellendi. Yalnızca tasarım/belge değişikliği; C implementasyonu ve test yok. Sonraki kod adımı öncesi OIR map/schema'daki mevcut 100-kind sınırı ve schema kind 101 scalar kuralı uzlaştırılmalı; belge kayıtları tek başına wire implementation değildir. | commits `c0d5773`, `b4d7ba4`; `AST_ASM_DESIGN.md`, `nexus docs/Gelişmiş Inline Assembly — Formal Specification.md`, `OCC_PIPELINE_PROGRESS.md` |

| 2026-11-04 | FASTEXEC AST çekirdeği eklendi: `AST_FASTEXEC_BLOCK` ve `AST_ASM_LABEL` append-only AST tag'leri; factory boyutlandırma, printer, destructor, legacy AST stream serialize/deserialize ve graph validator dispatch tamamlandı. `AST_ASM_LABEL` AST ancestor zincirinde bir `AST_FASTEXEC_BLOCK` kapsamı altında bulunmalıdır; label/source_text sahipliği ve source_length eşitliği doğrulanır. `AST_ASM_BLOCK` legacy kind 29 ve payload'ı değiştirilmedi. AST validator ve raw ASM CRLF codec regresyon testleri eklendi; CTest/CI henüz çalıştırılmadı. | `ast.h`, `ast.c`, `test_ast_validator.c`, `test_ast_deserialize.c`; commits `195cc7f`, `0672567`, `033611b`, `7aa5e8f`, `ed27fc0`, `f11df33`, `c51e0c8`, `5afd432` |
| 2026-11-04 | OIR AST bridge 101–105 tamamlandı: `AST_VARIADIC_PARAM`/`AST_NAMED_ARG`/`AST_MUST_DECL` artık OIR 101–103'e; FASTEXEC/ASM 104–105'e eşlendi. Field schema, frontend child traversal, writer, reader ve `OIR_AST_NODE_KIND_MAX` 105'e çıkarıldı. Eski OIR ID'leri 1–100 korunuyor; kind 101'deki hatalı I64 schema kaydı kaldırılarak variadic leaf olarak tanımlandı. Mapping 105/105 testi ve OIR round-trip fixture'ı 101–105'in tamamını kapsayacak şekilde eklendi. Testler henüz çalıştırılmadı. | `oir_ast_map.h/.c`, `oir_ast_field_schema.c`, `oir_ast_frontend_index.c`, `oir_ast_frontend_writer.c`, `oir_ast_frontend_reader.c`, `oir_ast_node_table.h`, `OIR_AST_NODE_REGISTRY.md`, `test_oir_ast_map.c`, `test_parser_ast_oir_roundtrip.c`; commits `5565a0d`, `f8a7366`, `3474e2d`, `596ebb2`, `0f71732`, `b32a585`, `1eea47a`, `1e6427d`, `946b8b1`, `bb61193`, `74dd1cf`, `09fb9a7` |
| 2026-11-04 | FASTEXEC AST/OIR uygulama kapısı açık: Parser `fastexec { ... }` ve Lexer raw ASM token/span entegrasyonu henüz yapılmadı; ASM dışı scope hatası için Parser/diagnostic testi bekliyor. Source span'in AST düğümlerinden OIR'a tam aktarımı da ayrı açık iş. Yeni test fixture'ları GitHub dalına yazıldı ancak bu çalışma ortamında C derleme/CTest yürütülmedi; son kod commit'i için PR workflow run bulunmadı. Bu nedenle AST veya OIR aşaması tamamlandı olarak işaretlenmiyor. | `parser.c`, `lexer.c`, `test_parser.c`, `test_parser_ast_oir_roundtrip.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md` |

| 2026-11-04 | Lexer raw ASM entegrasyonu eklendi: `asm: LABEL {` dizisi tanınır; gövde `TOKEN_ASM_BODY` olarak Nexus tokenization dışında, kaynak byte aralığı korunarak üretilir. İç içe brace'ler dengelenir, tırnak içindeki brace ve escape'ler delimiter sayılmaz; eşleşmeyen dış brace Lexer diagnostic üretir. `TOKEN_ASM_BODY` OIR token ID 164'e eklendi; token stream sıfır uzunluklu ASM gövdesini de taşıyabilir. Lookahead lexer state'ini kaydedip geri yükler. | `parser.h`, `lexer.h/.c`, `oir_token_map.h/.c`, `oir_token_stream.c`, `OIR_TOKEN_REGISTRY.md`, `test_lexer.c`, `test_oir_token_map.c`, `test_oir_token_stream.c`; commits `1522032`, `4b12fce`, `636836f`, `91e1aac`, `a3d830a`, `4b64053`, `c5e6407`, `e9feb5d`, `021bc84`, `8476484`, `691c5fd`, `2a8e797` |
| 2026-11-04 | Parser FASTEXEC/ASM üretimi eklendi: `fastexec { ... }` → `AST_FASTEXEC_BLOCK`; `asm: LABEL { raw }` → `AST_ASM_LABEL`. Normal Nexus Flow statements FASTEXEC içinde kalır; ASM scope kontrolü Parser'da yapılır ve dışarıdaki ASM declaration hata üretir. İç içe normal bloklardaki ASM etiketleri AST validator tarafından FASTEXEC ancestor kapsamıyla doğrulanır. `AST_BLOCK_HALT` doğrudan FASTEXEC sonunda da kullanılabilir. Parser exact node source span'lerini damgalar. | `parser.c`, `ast.c`, `test_parser.c`, `test_ast_validator.c`, `NEXSUS_AST_SYNTAX_MATRIX.md`; commits `9535a41`, `795da10`, `aac2d41`, `78f5204`, `309ae08`, `f4cf25b`, `ba82611`, `a22e0a5`, `e9f6932`, `7cb888f`, `445694c`, `418be1a`, `dda3888`, `036ba94`, `76e5d55`, `3655f30` |
| 2026-11-04 | FASTEXEC Lexer/Parser regresyon testleri eklendi: raw ASM byte/CRLF korunumu, nested register-list brace ve quoted brace, unterminated body span, Parser FASTEXEC AST/source span, asm declaration ve asmcall dış scope diagnostic, kaynak token stream üzerinden Parser girişi, FASTEXEC HALT cleanup, AST stream round-trip ve OIR 101–105 bridge fixture. OIR token stream boş raw body round-trip testi de eklendi. **Hiçbir C testi bu oturumda çalıştırılmadı:** geçici Git clone ağ/DNS nedeniyle kurulamadı; çalışma dalında açık PR/workflow run bulunmadığından CI sonucu yok. Testler eklenmiş olsa da başarı durumu doğrulanmış değildir. | `test_lexer.c`, `test_parser.c`, `test_ast_deserialize.c`, `test_parser_ast_oir_roundtrip.c`, `test_oir_token_stream.c`, `test_oir_ast_map.c`; test commits `e9feb5d`, `8476484`, `691c5fd`, `ba82611`, `7aa5e8f`, `ed27fc0`, `2a8e797`, `946b8b1`, `bb61193`, `74dd1cf`, `09fb9a7`, `021bc84`, `b69e0c9`, `2dfe749`, `cb5a1ee`, `fb889cf`, `bdf2ada` |
| 2026-11-04 | FASTEXEC pipeline tamamlanmadı: Semantic henüz `asmcall`/`asmjmp` scope ve label çözümlemesi, CPU/GPU/NPU primitive capability, `%variable`/register operand yönü, type-width, mutability/initialization, clobber/live-value, ABI ve control-flow kontrollerini uygulamıyor. OIR AST source-span TLV aktarımı da açık. Parser/lexer implementation ve test fixtures code review + gerçek C build/CTest geçişi bekliyor. Codegen'e geçilmez. | `semantic` modülü, OIR span codec, CI/build, ilgili semantic testleri |

| 2026-11-04 | FASTEXEC scope test assertion düzeltildi: asmcall dış-scope testi kendi diagnostic sonucunu ayrı doğruluyor; önceki testte sonraki `asm:` testi aynı `valid` değişkenini ezerek asmcall kontrolünü etkisiz bırakabiliyordu. | `test_parser.c`; commit `4159a9a`; CI/build bekleniyor |

| 2026-11-04 | CI build gate gerçek linker hatası yakaladı: `parse_block()` içindeki `check_next()` tanımsız çağrıydı; cleanup syntax tek `TOKEN_HALT` (`?;`) olduğundan `match(TOKEN_HALT)` ile düzeltildi ve gereksiz ikinci token tüketimi kaldırıldı. Önceki run `41823136918` tüm platformlarda linker aşamasında fail; düzeltme commit `45eb54b`, yeni CI sonucu bekleniyor. | `parser.c`, `OCC_PIPELINE_PROGRESS.md` |

| 2026-11-04 | CI `41828255627` linker düzeltmesini doğruladı: derleme tamamlandı, 60 CTest'ten 57 geçti. Üç regresyon bulundu: asmcall scope Parser testi yanlış katmanda doğrulama bekliyordu (generic `AST_FUNC_CALL` Parser davranışı doğru; FASTEXEC kontrolü Semantic'e ait), OIR legacy flat fixture 101–105 context-restricted node'ları düz listeye koyuyordu. Parser testi katman sınırına göre düzeltildi; OIR flat fixtures legacy 1–100 ile sınırlandı, append-only 101–105 contextual parser/OIR roundtrip testleri ayrı kapsanıyor. Sonraki CI ile doğrulanacak. | `test_parser.c`, `test_oir_ast_frontend_writer.c`, `test_oir_ast_frontend_reader.c`; commits `e2c9a7e`, `4b55b47`, `1fabeb8` |


| 2026-11-04 | Son FASTEXEC/OIR regression düzeltmeleri CI ile doğrulandı: run `41828504098` (commit `8ea0520`) **success**. Önceki 57/60 CTest sorunları Parser katman beklentisi ve legacy flat OIR fixture şekli kaynaklıydı; düzeltmelerden sonra workflow yeşil. AST ownership audit'i ID 101–105 FASTEXEC/ASM payload'larını kapsayacak şekilde güncellendi. | `AST_NODE_OWNERSHIP_AUDIT.md`; commits `e2c9a7e`, `4b55b47`, `1fabeb8`, `8ea0520`, `d7809ea` |

| 2026-11-04 | AST kapanış audit'inde source span yaygın Parser üretimlerinde mevcut; kalan explicit audit başlıkları: tüm AST kind için parser-production ↔ span-fixture eşlemesi (özellikle rule/condition action ve generated/synthesized node'lar), AST legacy codec allocation-failure cleanup matrisi ve OIR AST optional-span TLV writer/reader. OIR span TLV mevcut normatif sözleşmede `0xFFF0–0xFFF3`; henüz implementation tamamlanmış sayılmaz. FASTEXEC AST ownership ve context testleri CI run `41828504098` ile doğrulandı. | `docs/AST-SOURCE-SPAN-CONTRACT.md`, `AST_NODE_OWNERSHIP_AUDIT.md`, `test_ast_validator.c`; Codegen kapalı kalır |


| 2026-11-04 | `main(argc!i32, argv!str[])!i32` için Parser/AST fixture eklendi: `argc` ve `argv` özel AST node değil, standart `AST_VAR_DECL` parametreleri; `argv` dynamic one-dimensional `AST_ARRAY_TYPE` olarak saklanıyor ve parametre span'leri korunuyor. Runtime argc/argv injection (executable path dışarıda, UTF-8, no NULL sentinel) sözleşmesi syntax reference'da normatif; runtime adapter/semantic doğrulama bu AST işinden ayrı. CI yeni commit için bekleniyor. | `omnicore-sdk/src/c/tests/test_parser.c`; commit `1ad5cbc` |


| 2026-11-04 | `main(argc, argv)` fixture ilk CI'da gerçek AST model ayrıntısını ortaya çıkardı: hard-type marker `type_name` içine de `!` ekliyor (`is_hard_typed` ayrıca true), array `[]` için AST_ARRAY_TYPE altında dinamik AST_ARRAY_DIMENSION düğümü bulunuyor (dimension->length NULL). Test beklentisi AST payload'a göre düzeltildi; bu test hatası Parser regression değil, fixture assertion uyumsuzluğuydu. Düzeltme commit `67ee564`; yeni CI bekleniyor. | `test_parser.c`, run `41829923124` failure (54/60; yalnızca yeni fixture) |


| 2026-11-04 | İkinci CI turu yeni fixture'ın C kaynak yazım hatasını yakaladı: `&&` koşuluna eklenen satır sonu yanlışlıkla literal `\\n` olarak dosyaya girmişti (stray backslash). Bu test kaynak hatası düzeltildi; Parser/AST davranışına ilişkin yeni hata yok. Commit `1b71309`; CI tekrar bekleniyor. | `test_parser.c`, run `41830146241` compile failure |


| 2026-11-04 | `main(argc!i32, argv!str[])!i32` AST fixture düzeltmesi doğrulandı: run `41830313436` Ubuntu CTest **60/60 geçti**, ASan/UBSan ve macOS build/CTest başarılı; Windows ve Valgrind son kontrolü sürüyor. Böylece entry-point argc/argv Parser/AST shape testi yeşil Linux + sanitizer kanıtına sahip. | `test_parser.c`; commit `1b71309`; workflow run `41830313436` |


| 2026-11-04 | AST source span OIR TLV uygulaması ilk doğrulama: run `41838138909` Ubuntu CTest **60/60 passed**, ASan/UBSan başarılı. OIR round-trip fixture root + function span'lerini kontrol ediyor ve tekrar encode canonical bytes eşitliğini koruyor. Windows/macOS/Valgrind hâlâ çalışıyor. | `oir_ast_frontend_writer.c`, `oir_ast_frontend_reader.c`, `test_parser_ast_oir_roundtrip.c`, `docs/AST-SOURCE-SPAN-CONTRACT.md`; commits `d03018f`, `1138659`, `8d60a4a`, `3615021` |


| 2026-11-04 | OIR source-span test genişletildi: yalnız root/function örnekleri değil, AST child/sibling grafiğindeki **bütün parser node span'leri** decode sonrası karşılaştırılıyor (has_location, byte offsets, start/end line-column); source_id OIR extension'da taşınmadığından eşitlik kapsamı dışında. CI run `41838287034` pending. | `test_parser_ast_oir_roundtrip.c`; commit `66a5971` |


## 2026-11-04 — Parser AST source-span audit

- The Parser round-trip tests now recursively require a source span on every node in the extended syntax fixture and the canonical AST fixture. Traversal covers registered child fields and sibling chains.
- The test exposed two missing paths: synthesized AST_IF nodes for select/scan when guards, and AST_PATTERN_BINDING nodes inside constructor patterns such as Range(s,e). Both paths now stamp source spans from their source tokens/child ranges.
- Commits: 9d68b21 (per-node assertion), f1a2700 (guard wrapper), 05559d0 (constructor bindings), 3d547fc (canonical fixture coverage), 5f25115 (diagnostic formatting).
- CI run 41840863533 passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Scope: exercised fixtures are covered; the full grammar-production to Parser-function and positive/negative fixture matrix remains open.


## 2026-11-04 — AST Graph Validator required-payload checks

- `ast_validate()` now rejects absent required owned text payloads for named declarations and expressions, identifiers, literals, member access, patterns, rules, imports, Nexus labels/jumps/intents and related AST forms. Empty literal text remains valid when the owned string pointer is present.
- Generic `AST_FUNC_CALL` requires its callee child. Call-shaped intrinsic nodes retain optional explicit callee children because their AST tag identifies the intrinsic operation.
- Regression fixtures now distinguish payload-free leaves from payload-bearing leaves and cover missing/valid identifier, literal and newtype payloads. Contextual MUST/variadic tests were adjusted to satisfy earlier required fields so each test reaches the intended validator rule.
- Validation: GitHub Actions run `41842413877` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Remaining AST work: full per-production parser/source-span fixture matrix, codec allocation-failure/partial-tree cleanup coverage, and remaining syntax-to-AST gaps already recorded in the AST syntax matrix. Semantic and Codegen remain gated.


| 2026-11-04 | AST stream codec truncation regresyonu eklendi: geçerli AST_IDENTIFIER artifact'inin byte-prefix'lerinin tamamı (0..N-1) deserialize edilerek her kesilmenin reddedildiği doğrulanıyor; kısa magic/version prefix testleri de korunuyor. Run `41858633099`: Ubuntu CTest, Windows CTest, ASan/UBSan ve Valgrind başarılı; macOS job runner tarafından cancelled, dolayısıyla workflow genel sonucu failure (macOS test sonucu yok). Nested payload/partial ownership cleanup testleri ayrıca genişletilecek. | `test_ast_deserialize.c`; commit `f93a2e0` |

| 2026-11-04 | Truncation test derinleştirildi: prefix sweep fixture artık yalnız leaf identifier değil; AST_VAR_DECL → nested literal initializer + inline_type identifier içeren çok katmanlı ownership ağacı. Böylece her byte kesiminde partial child cleanup ve üst düğüm cleanup birlikte exercised edilir. Commit `1e1be2d`; CI run `41868279190` queued, sonuç bekleniyor. | `test_ast_deserialize.c` |

| 2026-11-04 | Nested AST deserialize truncation sweep CI doğrulandı: Ubuntu/Windows CTest, ASan/UBSan ve Valgrind başarılı; macOS runner job'u iptal edildi (platform testi yok). AST ownership cleanup için bu regression güvence altında. | run `41868279190`, test commit `1e1be2d1`; CI-status kayıt commit'i `614dd718` |


| 2026-11-05 | AST node lifecycle audit: AST enum 105 node'a ulaşıyor. Payload-free `AST_BREAK`, `AST_CONTINUE`, `AST_BLOCK_HALT`, `AST_VARIADIC_PARAM` türleri factory, destructor, serializer ve deserializer switch'lerinde açıkça işaretlendi (sessiz default davranışı yerine). Variadic marker'ın payload taşımadığı AST header yorumuyla uyumlu. CI run `41912702421` queued; sonuç henüz yok. | `ast.c`; commit `ce0aa835` |


| 2026-11-05 | AST lifecycle audit CI doğrulandı: payload-free düğüm türlerinin açık factory/destructor/serialize/deserialize davranışı Ubuntu CTest, ASan/UBSan, Valgrind ve Windows CTest'te başarılı. | run `41912702421`, progress correction `4d813d8a` |
| 2026-11-05 | AST→OIR node-kind mapping 105/105; OIR writer 105/105 türü switch'te ele alıyor. Reader AST kind mapping üzerinden türü çözüyor ve ilgili field schema/child contract kontrolüyle decode ediyor. OIR frontend index child-field count tablosundaki payload'sız leaf türleri açık case'lere alındı (NewType, Literal, Identifier, Break/Continue, Nexus Jump, AI Logic, Panic, Use). | `oir_ast_map.c`, `oir_ast_frontend_writer.c`, `oir_ast_frontend_reader.c`, `oir_ast_frontend_index.c`; commits `8d37f948`, `53365649` |

| 2026-11-05 | AST codec type sweep genişletildi: `roundtrips_all_node_type_tags()` artık eski `AST_ARRAY_DIMENSION` sınırı yerine append-only son tür `AST_ASM_LABEL` dahil tüm 105 enum tag'ini kapsıyor. Her tür fixture populate → serialize → deserialize → type/line/column/sibling doğrulaması yapacak. Bu test yeni AST node eklenince kapsama otomatik genişler. | `test_ast_deserialize.c`; commit `4ba6e152`; CI validated by workflow run `42411223520` (all 5 jobs passed) |


| 2026-11-05 | Codec tag sweep genişletme denemesi: enum sınırını `AST_ASM_LABEL` yaptığımda bağlama bağlı AST türleri (`AST_MUST_DECL`, `AST_VARIADIC_PARAM`, `AST_BLOCK_HALT`, `AST_ASM_LABEL`) standalone root olarak graph validator tarafından haklı biçimde reddedildi. Testi hatalı biçimde tüm türleri kök kabul ettirmeye zorlamadım; standalone sweep sınırı `AST_ARRAY_DIMENSION` olarak geri alındı. `AST_FASTEXEC_BLOCK/AST_ASM_LABEL` roundtrip ayrı fixture ile zaten test ediliyor; contextual MUST/variadic/HALT için parent-context fixture'ları ayrı eklenmeli. Bu test denemesi başarısızdır ve tamamlanmış sayılmıyor. | commits `4ba6e152`, `04da05a6`, `05e80c2e`; CI failure (context fixture) |


| 2026-11-05 | AST codec için contextual round-trip fixture eklendi: AST_MUST_DECL → AST_PROGRAM declarations; AST_VARIADIC_PARAM → AST_FUNC_DECL params; AST_BLOCK_HALT → AST_BLOCK statements (son öğe); AST_ASM_LABEL → AST_FASTEXEC_BLOCK statements. Tek program ağacı graph validation, serialize → deserialize → validate → canonical re-encode byte eşitliği ile test ediliyor. Önceki standalone enum sweep sınırı AST_ARRAY_DIMENSION olarak tutuldu; bağlama bağımlı tag'ler artık parent-context fixture üzerinden kapsanıyor. | `test_ast_deserialize.c`; commit `522c84f1`; CI run `41917469888` queued |


| 2026-11-05 | Contextual AST codec test CI ilk çalıştırmasında test çağrısına literal `\\n` kaçışının kaynak koda yazıldığı compile error tespit edildi; test mantığına geçmeden build duruyordu. Çağrı satırı gerçek newline ile düzeltildi; önceki başarısız CI'lar test sonucu olarak değerlendirilmez. | test fix commit `b712509f`; CI validated by workflow run `42411223520` (all 5 jobs passed) |


| 2026-11-05 | `docs/AST_NODE_IMPLEMENTATION_MATRIX.md` eklendi: 105 AST kind için parser direct/helper notu, lifecycle, graph, OIR map/writer ve fixture sütunları; contextual node sahiplik tablosu ve AST kapanış kriterleri. Otomatik kaynak taraması 105/105 OIR map, 105/105 writer, 105/105 fixture switch ve 62 doğrudan parser create çağrısı buldu (62 bir node-completeness ölçütü değildir). | commit `48f33788` |
| 2026-11-05 | Contextual AST test main çağrısında literal `\\n` kalıntısı ikinci düzeltmede bulundu; satır fiziksel newline ile ayrıldı. Commit `264cd247` CI run `41924686996` in progress; bu commit için test sonucu bekleniyor. Önceki `522c84f1`, `b712509f` ve `5a028ae7` CI build failures (aynı escaped newline). |

| 2026-11-05 | Contextual codec fixture derlendi; test 1/60 `ast_rejects_malformed_streams` fail oldu. Cause: ASM fixture raw text `nop\\n` (4 byte literal) iken source_length 3 ayarlanmıştı, graph validator beklenen şekilde reddetti. Fixture payload `nop` ve length 3 olarak düzeltildi; sonraki CI run bekleniyor. | fix commit `17a54718`; previous run `41924686996` failure (98% CTest, 59/60 pass) |


## 2026-11-05 — AST parser production audit continuation

- Added `docs/AST_PARSER_PRODUCTION_MATRIX.md`: parser entry/function → AST family → current named test evidence → remaining positive/negative production gaps.
- Matrix records real C test function names and distinguishes parser acceptance from AST-shape assertions. It does not upgrade AST completion status.
- AST node implementation matrix remains at `docs/AST_NODE_IMPLEMENTATION_MATRIX.md`; syntax/AST baseline remains `NEXSUS_AST_SYNTAX_MATRIX.md`.
- Contextual ASM codec fixture had an actual length mismatch: C string `"nop\\n"` is five source bytes, while fixture declared 3. Corrected to `"nop"` with length 3 in commit `91e93258`. CI for earlier commit `17a54718` failed at `ast_rejects_malformed_streams`; no pass is claimed. New commit CI must verify the correction.
- Next AST work: make syntax rows exhaustive (all intrinsic/operator/declaration modifier forms), then add focused AST-shape/span and paired rejection tests for rows currently marked partial. Continue to gate Semantic/OIR completion and Codegen.


## 2026-11-05 — CI green + AST source-span inventory

- Confirmed full CI success for fixture correction commit `91e93258` (run `41960982443`): Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Confirmed matrix commit `bf7d9aa0` CI success (run `41961046955`) and progress update `553726c1` CI success (run `41961058190`).
- Expanded `docs/AST_PARSER_PRODUCTION_MATRIX.md` with source-span test evidence: UTF-8/multiline, declarations/members, rules, lambda, relocate/retry/unroll, FASTEXEC, capture, params/variadic, group, Halt and asmcall/asmjmp.
- Source-span tests are a subset of productions. Do not infer all-node coverage from this list.
- Next: use syntax-reference rows to define span policy (token/full-production/synthesized) and close explicit AST representation blockers: function-type/callback, macro/preprocessor, compile-time directives/link/export/import metadata, vector typing and numeric shift representation; map declaration spelling is now canonical `!map` and its remaining constraints are Semantic work. First establish normative syntax and stable AST payloads; don't conflate Semantic/runtime work.
- AST remains open; Semantic and Codegen remain gated.


## 9. AST temsil boşlukları — 2026-11-05

- `docs/AST_REPRESENTATION_GAPS.md` eklendi: function pointer/callback signature, pointer/ref, function return-array, numeric shift, compile-time directive/preprocessor ve DLL metadata; map marker `!map` is represented without a dedicated node için mevcut AST temsili ile karar/eksik sözleşmeler kayıt altına alındı.
- Bu audit yeni AST kind veya OIR ID eklemedi; syntax referansında kesinleşmeyen grammar için heuristik parser kararı alınmadı.
- Unified type AST tasarımı (primitive/named/qualified/generic/array/map-marker/pointer/ref/function) uygulama öncesi önkoşul olarak belirlendi.
- Group/method, use alias/path, must scope, defer/thread, intrinsic, default param ve array return Parser/codec fixture eksikleri açık kaldı. AST kapanışı henüz ilan edilmedi.
- CI durumu: `99cd5cf4` ve `609f143c` commit'leri başarılı (Actions run `41966108769`, `41966092730`). Yeni dokümantasyon commit'i için CI ayrıca izlenecek.


## AST Parser test increment — 2026-11-05

- Added a focused Parser AST ownership/context test for `use CoreModule as Core, MathModule;` and a block-form `defer` containing a local declaration: commit `03f4b208`.
- Corrected the fixture's C newline escape in `8b8156a7`; CI for the test commit is pending/queued and will be evaluated against the corrected head.
- Existing parser tests already cover group member spans, thread block spans, MUST declarations/codec, and lambda forms; this increment adds shape/ownership assertions rather than duplicating span-only coverage.
- AST remains open: method implementation and use-path syntax are not equivalent to alias coverage; intrinsic family, parameter defaults, and function array-return representation still require targeted proof.


## Function default parameter AST — 2026-11-05

- Formal function spec documents `a:10` default parameters; Parser previously treated colon only as a type separator and rejected the default expression.
- Parser now parses a colon without hard-type marker as a parameter initializer expression; explicit hard types remain `a!T` / `a:!T`. Commit `3fe950b2`.
- Added AST fixture for `f:sum(a:10, b!i32)!i32 { ret a; }` checking default literal initializer and second parameter type; commit `93e37487`.
- CI: malformed first use/defer fixture head `03f4b208` failed ASan/UBSan due parser recovery leaving unowned ASTUse; newline correction `8b8156a7` is running. Parser implementation + default-param test runs are pending. This failure is recorded, not represented as a passing test.


## Parser sibling-chain ownership fix — 2026-11-05

- CI surfaced a real parser integration defect: when one top-level production returned a sibling chain (multi-module `use`), `parser_run` appended the next statement through the chain head and overwrote the intermediate sibling. The second `AST_USE` became unreachable and leaked.
- Fixed top-level append logic to advance `current_stmt` to the returned chain's tail before the next production is attached. Commit `0da77258`.
- The use-alias/defer test exposed this defect; its initial run failed (not ignored). Corrected chain ownership is now under CI.


## Group method AST contract — 2026-11-05

- Added parser fixture for a group containing a field and public method implementation (get => f:get()!i32 { ... }). It checks AST_GROUP_DECL → AST_VAR_DECL + AST_METHOD → AST_FUNC_DECL, method public name, implementation name/return type/body, and whole-graph validation.
- Commit: fa51b156. CI workflow for this exact head is to be checked; no pass is claimed before Actions completes.
- This closes one behavioral fixture gap only; group visibility/export and method semantic binding remain open.


## Group methods + intrinsic family AST tests — 2026-11-05

- Group method production fixture commit fa51b156 validates member + method implementation shape and ownership.
- Intrinsic mapping fixture commit 8b3ea910 checks nine representative mappings (len, sizeof, typeof, is_ok, addr, clock, datenow, date, time) through variable initializer ASTs.
- CI runs: group test 41979868392 / progress run 41979893923 and intrinsic test 41979938267 are pending/in progress; status must be checked before marking tests green.
- This is representative intrinsic coverage, not the complete intrinsic family; remaining intrinsic arities, invalid forms, and codec round-trip coverage remain open.


## Intrinsic fixture isolation — 2026-11-05

- The first combined intrinsic fixture (8b3ea910) failed in the parser test target; its failure was caused at least in part by the invalid `sizeof(i32)` expression fixture. The test was not treated as evidence of a compiler regression or as passing.
- Replaced it with isolated per-intrinsic sources and explicit failing-index reporting in commit 6164f535. A preceding correction changed `sizeof(i32)` to `sizeof(x)` in 6bd99510.
- CI for 6bd99510 and 6164f535 is running; intrinsic coverage remains unverified until the latest head passes all matrix jobs.
- Group method AST fixture fa51b156 and progress commit d83c25ad both passed CI (runs 41979868392 and 41979893923).


## AST syntax boundary clarification — 2026-11-05

- `docs/AST_REPRESENTATION_GAPS.md` now records that dotted/path-qualified `use` syntax and function array-return syntax are not currently defined by the normative syntax reference. Parser behavior will not be extended by guesswork.
- Array return support remains blocked on canonical type-expression/return-declarator design and AST/codec/OIR schema; existing `AST_ARRAY_TYPE` support for declarations/parameters is not sufficient proof of return support.
- Intrinsic fixture commits 6bd99510 and 6164f535 are being validated. Run 41984394345 has Ubuntu, Windows, macOS and ASan/UBSan green; Valgrind is still running. Run 41984448985 has Ubuntu, macOS and ASan/UBSan green; Windows and Valgrind are still running.


## Intrinsic + MUST scope validation — 2026-11-05

- Intrinsic AST fixtures isolated per expression and verified: commit 6164f535, Actions run 41984448985 successful across Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- `sizeof` fixture uses a normal expression operand; each intrinsic is parsed independently and checked against its AST node kind plus whole-graph validation.
- Added a negative parser test that rejects `m:` MUST declarations inside function/block scope, matching the normative program-scope-only rule: commit 543d21bf. Existing positive MUST AST/codec fixture remains intact.
- CI for the MUST negative test is pending; AST remains open.


## Thread block grammar audit — 2026-11-05

- `docs/AST_REPRESENTATION_GAPS.md` records that Parser currently accepts legacy `(name)>{...}` and creates ASTThreadBlock with name/body while mode/timeout remain NULL.
- The normative syntax reference does not yet specify thread-block grammar, scheduling mode, timeout units, capture or ownership semantics. Existing compatibility/span tests remain; no speculative grammar or semantic behavior was added.


## Array-return syntax audit correction — 2026-11-05

- Rechecked the function section of `docs/language/syntax-reference.md`: it explicitly documents `f:name(a, ...)!i32[] { ... }`. The earlier gap note that array-return syntax was unspecified was incorrect and has been corrected in `docs/AST_REPRESENTATION_GAPS.md`.
- Parser currently consumes only the base return type token; it does not parse `[]` dimensions after the return type. This is a confirmed implementation gap.
- Proposed compatible AST direction: retain `ASTFuncDecl.return_type` as the base-type spelling mirror and add an owned optional `AST_ARRAY_TYPE` child for return dimensions; append codec field 8 (existing function node schema has fields 1–7). Implementation still requires confirming AST destruction/validation/reader behavior and an end-to-end fixture before coding.


## MUST scope regression test — 2026-11-05

- Added `test_must_scope_rejected` for an `m:` dependency declaration inside `f:main` body; it requires `PARSER_STATUS_SYNTAX_ERROR` and a recorded diagnostic.
- Commit 543d21bf; CI run 41984668004 successful across Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Follow-up documentation commits (ba33bd88, eb0eba44, 15eced69, 1e062802, 48213a32, 33d95cc1) also passed their CI runs (41984692074, 41984728876, 41984743088, 41984822837, 41984842838, 41984860450).

## Historical AST implementation gap — function array return (resolved in later entry)

- Normative grammar includes `f:name(a, ...)!i32[] { ... }`; current Parser does not parse return-array suffixes.
- Planned compatible representation: optional owned `AST_ARRAY_TYPE` child on `ASTFuncDecl`, with `return_type` retained as the base element-type compatibility string. The child owns ordered `AST_ARRAY_DIMENSION` nodes; no new AST kind is required.
- Before implementation, update AST destroy/validation, versioned AST stream serialization (preserve v1 read compatibility), OIR frontend writer/reader optional field 8, AST graph indexing, and parser positive/negative + AST/OIR round-trip tests. This is a coordinated AST/codec change, not a parser-only patch.


## Thread-block model clarification — 2026-11-05

- Owner clarified intended execution model: `(thread_block_name)>{ ... }` is a named thread code body/template, not inline application execution. `spawn(thread_block_name, addr(nexus)), async, 100);` refers to that body and requests separate thread-context execution.
- Recorded the distinction and open contracts in `docs/AST_REPRESENTATION_GAPS.md` (commit 01fcfb37): declaration/body vs spawn invocation vs runtime scheduling/context/ownership/error behavior.
- Historical state at the time of this note: Parser still emitted `AST_SPAWN`. Superseded by the Spawn/core parser boundary correction below; core Parser now rejects spawn and leaves it to the separate Thread module.
- Next AST work: normative grammar + tests for thread declaration and spawn reference; then Semantic name resolution and explicit execution metadata. Scheduling, async, timeout units, context capture, lifetime, join/error semantics remain open.


## Spawn module boundary correction — 2026-11-05

- Owner clarified `spawn` is intentionally outside the core AST, in a separate thread module/language layer. No `AST_SPAWN` or core scheduling semantics should be introduced.
- Thread/runtime specs are present: `nexus docs/NXF Runtime Threading Specification v1.0.md`, `nexus docs/Thread System Runtime — Full Formal Specification v1.0.md` (SDK copies also exist). Prior audit wording incorrectly implied the runtime specs themselves were absent; only the core syntax/AST vs module boundary needs clarification.
- Correction recorded in `docs/AST_REPRESENTATION_GAPS.md` commit `8814173a`. Core AST work proceeds independently with function array-return support.


## Function array-return implementation — implementation history — 2026-11-05

- Added appended `ASTFuncDecl.return_array_type` (owned optional `AST_ARRAY_TYPE`); base `return_type` remains compatibility spelling: commit `33cf3a85`.
- AST stream bumped to v2; v1 deserialization skips the new trailing child, preserving old layouts; destruction and declaration validation updated: `3a9b6458`, `f4d53163`. AST stream version assertion updated to v2: `7fd62e1e`.
- Parser consumes return declarator suffix after base type and transfers `AST_ARRAY_TYPE` from the temporary declarator: `1b3da079`.
- OIR AST frontend field 8 is optional node-ref; graph index count/child accessor, writer optional TLV, reader attachment: `721213c5`, `601a8a65`, `d02f751f`, `c9bfc748`.
- Parser regression fixture covers `!i32[]`, `!i32[n]`, `!f32[m][n]` and AST stream round-trip: `d6ace431`, `c877bf81`; fixed empty dimension assertion (`[]` has NULL length) in `0324beb2`.
- CI diagnosis: run `42009813807` isolated the remaining OIR round-trip failure to a duplicated assertion that checked the array-return node twice, the second time at the wrong declaration position. Removed the redundant assertion in `27d279dd`; the full five-job matrix passed (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind) in run `42010539184`.
- Added an explicit version-1 function-stream compatibility regression in `4a7569f9`; run `42010711975` passed all five jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind).
- The tested path now covers Parser → AST → OIR frontend payload → AST → canonical OIR bytes for a function with named return-array extent `!i32[n]`. Parser tests cover `!i32[]`, `!i32[n]`, and `!f32[m][n]`.
- Scope remains bounded: this closes the function array-return representation/codec integration gap only. It does not complete the full AST inventory or all syntax-matrix gaps; do not advance to Codegen.


## Function array-return integration — CI verified — 2026-11-05

- `ASTFuncDecl.return_array_type` is now represented as an owned optional `AST_ARRAY_TYPE` child, with ordered `AST_ARRAY_DIMENSION` entries; the existing `return_type` string remains the base-type compatibility field.
- Parser suffix handling, AST lifecycle/validation, AST stream v2 writing with v1 reading, and OIR frontend optional field 8 are connected.
- Parser, AST serialization, OIR round-trip/canonical-byte, and v1 compatibility regressions pass CI run `42010711975` on Ubuntu, Windows, macOS, ASan/UBSan, and Valgrind.
- This is a completed subtask, not an AST-stage completion declaration. Continue the remaining AST syntax/ownership/type-model audit before moving to later pipeline stages.


## Unified type AST — next design gate — 2026-11-05

- Parser audit confirms the current `nt:Callback = f:(id!u32)>!bool;` example is not a complete function-type grammar; `parse_newtype_decl()` stores only one primitive/identifier target token and cannot represent a signature. `AST_LAMBDA` remains an executable expression/block node, not a function type.
- Normative map example `map str, i32` is not represented as key/value children; the variable parser currently consumes only `map` as an opaque type name. `!map` parameter shorthand also needs an explicit typed contract.
- Pointer/ref markers `*`, `**`, `&` lack concrete declarator examples and ownership/lifetime composition rules. No parser or AST change is made from ambiguous placement assumptions.
- The detailed evidence and design gate are recorded in `docs/AST_REPRESENTATION_GAPS.md`. Next step is a grammar/type-expression audit; only after that should an append-only unified type payload and OIR mapping be designed. AST and Codegen gates remain open.


## Function array-return integration — latest CI confirmation — 2026-11-05

- Current branch CI run `42011297161` (commit `1196820a3bcb`) completed successfully across all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- The integrated regression covers Parser forms `!i32[]`, `!i32[n]`, `!f32[m][n]`; the OIR AST payload round-trip preserves the named extent for `!i32[n]` and canonical re-encoding remains byte-identical.
- AST stream v1 compatibility is explicitly exercised by `reads_v1_function_stream_without_v2_return_child()`; the v2-only optional function child is absent in the synthesized v1 fixture and the decoded function validates.
- This closes the function array-return parser/AST/codec/OIR bridge subtask. AST model/parser inventory and Semantic coverage remain open; no Codegen gate is opened.


## Thread-block AST payload regression — 2026-11-05

- Added `test_thread_block_ast()` in `omnicore-sdk/src/c/tests/test_parser.c` (commit `85bc7f35`). It parses `(worker)>{ v:value = 7; }`, validates `AST_THREAD_BLOCK` name/body, confirms the currently unset `mode` and `timeout` remain NULL, checks source span and nested variable ownership, then serializes/deserializes and validates the graph again.
- CI run `42014109396` passed all five jobs: Ubuntu (60/60 tests), Windows, macOS, ASan/UBSan and Valgrind.
- This test covers the core declaration/body AST only. It does not introduce `AST_SPAWN`, spawn syntax ownership, scheduling semantics or thread runtime lowering; those remain with the separate thread module contract.


## Group/method parser-to-codec regression — 2026-11-05

- Extended `test_group_method_ast()` to serialize the parsed `group Pair` AST, deserialize it, re-run graph validation, and verify the group member, method public name, implementation function, return type and body survive ownership transfer (commit `f63406fc`).
- CI run `42014382856` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind; each platform build/test suite reports 60/60 passing tests.
- Group/method AST parser-to-codec path is covered; Semantic method resolution, receiver/dispatch rules and OIR lowering remain open.


## Spawn/core Parser boundary — implemented and CI-verified — 2026-11-05

- Core Parser no longer maps `TOKEN_KW_SPAWN` to `AST_SPAWN`. It rejects the token with an explicit parser-stage diagnostic directing ownership to the separate Thread module (commit `4e57618a`; diagnostic path refinement `4be14f14`).
- `test_spawn_is_outside_core_parser()` verifies syntax rejection and inspects the structured `OccDiagnosticCollector` message rather than the legacy CLI `Error.message` projection (test commit `3d0e541f`).
- Existing `AST_SPAWN` enum/OIR node identity and AST codec support are retained as legacy compatibility only; the tag is not removed or renumbered. `test_ast_deserialize` continues to cover the legacy tag's codec payload.
- CI run `42014920664` / commit `4be14f14939c`: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind all passed; all 60 tests passed in build/test jobs and Valgrind MemCheck.
- Spawn parsing/validation/lowering and runtime integration remain the separate Thread module's responsibility. The core parser rejection is a fail-closed boundary, not an implementation of the Thread module frontend.


## AST ownership inventory reconciliation and MUST parser coverage — 2026-11-05

- Reconciled the function array-return child across `AST_NODE_FIELD_OWNERSHIP.md`, `AST_NODE_OWNERSHIP_AUDIT.md`, and `OIR_AST_NODE_REGISTRY.md`: `ASTFuncDecl.return_array_type` is an optional owned `AST_ARRAY_TYPE` subtree and OIR field 8 is optional. Existing IDs/fields remain unchanged.
- Updated the implementation matrix to include the function array-return compatibility fixture and the parser production matrix to reflect actual group/method, thread-block, FASTEXEC/ASM and array-return tests.
- Added `test_must_top_level_ast()` to assert that `m:CoreModule = "CoreModule.nxf";` produces a top-level `AST_MUST_DECL` with the correct name, literal source, source span and following function declaration. Existing `test_must_scope_rejected()` continues to assert that the same declaration is rejected inside a function block.
- CI first caught a literal `\\n` in the test registration line; after that was fixed, the positive test exposed that string-literal AST payloads preserve their source quotes. The assertion was corrected to compare the exact quoted token spelling (commit `1458c9cb`). Final run `42022856183` on `1458c9cb0ec8` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind; Ubuntu/Valgrind each report 60/60 tests and zero failures.
- This closes a documentation consistency gap and adds positive Parser evidence for MUST scope. It does not close all Parser productions, the full AST gate, or authorize Semantic/Codegen advancement.


## Canonical `!map` grammar and Parser regression — 2026-11-05

- Language decision: map declarations and parameters use only `!map`. The declaration sample is `v:notlar!map;`; the parameter form is `a!map`. No key/value type arguments are written. `map<key, value>` and `!map key, value` are not Nexus Flow syntax.
- Parser fixture added inside `test_parser_accepts_and_rejects_sources()` in `test_parser.c`: verifies a local map declaration is represented as hard-typed `AST_VAR_DECL` with `type_name="map"`, a function map parameter retains `type_name="!map"`, and the graph validates. Both legacy parameterized spellings `!map str, i32` and `!map<str, i32>` are asserted to fail parsing.
- Updated `docs/language/syntax-reference.md`, `docs/AST_REPRESENTATION_GAPS.md`, `NEXSUS_AST_SYNTAX_MATRIX.md` and `docs/AST_PARSER_PRODUCTION_MATRIX.md`. No `AST_MAP_TYPE` or OIR schema change is needed for this marker.
- Initial CI run `42032892220` on code commit `0406d498152e` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind (60/60). Expanded angle-bracket rejection coverage was added in commit `355683f4`; CI run `42033828639` passed all five jobs, with Ubuntu and Valgrind at 60/60. Semantic map key/value rules remain future Semantic work; this does not close AST globally.


## Expression precedence regression — logical AND / bitwise OR — 2026-11-05

- Parser audit found the implementation tier order contradicted the normative precedence table: bitwise operators were parsed below comparison/equality/range, even though the reference places bitwise above comparisons. `parse_logical_and()` also used an incomplete right-operand tier in the earlier intermediate patch.
- Final Parser tier alignment: `parse_bitwise_and()` consumes additive operands; `parse_comparison()` consumes bitwise-OR operands; `parse_range()` remains below equality; `parse_logical_and()` consumes complete range/equality/comparison/bitwise expressions. This yields bitwise > comparison > equality > range > logical AND > logical OR, as documented.
- `test_expression_precedence_and_associativity_ast()` now asserts `a && b | c`, `a | b && c`, `a < b | c`, and `a | b < c` produce the expected AND/LT roots and OR children; it also asserts left associativity for subtraction, division, comparison and equality.
- Intermediate runs `42033412161` (`564dddb74889`) and `42033446920` (`26f1968a17c2`) failed: logical precedence, AST↔OIR round-trip and an extended parser semantic-scope fixture regressed during the tier reordering. Log inspection showed the comparison RHS still called `parse_range()`, while logical AND had a temporary mismatched operand tier. The final correction in `b2446b1e3725` makes comparison operands use `parse_bitwise_or()` and logical-AND operands use `parse_range()`. CI run `42033487422` on `b2446b1e3725` passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. The associativity extensions in `504444879e9d` were then validated by CI run `42033941328`, also passing all five jobs; Ubuntu and Valgrind each report 60/60 tests, zero failures.
- Remaining expression audit: associativity at each binary tier, chained comparison policy, and range/flow operator disambiguation.


## Binary operator associativity expansion — 2026-11-05

- Extended `test_expression_precedence_and_associativity_ast()` to assert left-nested AST shape for bitwise OR/XOR/AND and logical AND/OR chains, in addition to the existing additive, multiplicative, comparison and equality chains. Power remains covered as right-associative.
- This is Parser/AST shape coverage only; Semantic legality of chained comparisons and runtime short-circuit execution remain separate contracts.
- New test commit `1ca084159884` requires the full CI matrix before it is marked verified. Remaining expression ambiguity is the single-range rule and the interaction of `<<`/`>>` with Nexus Flow operators.


## Map type spelling refinement — 2026-11-05

- Owner clarified that map does not need a mandatory hard-type marker or a fixed key/value pair: keys are commonly strings, while values may carry their own type per entry.
- Function parameter spelling `a:map` is the preferred ordinary type form. Existing `a!map` remains accepted as an explicit hard-type compatibility spelling; local declarations currently use the established `v:name!map` grammar.
- `map<str, i32>` is not implemented by the current Parser, but it is no longer documented as permanently forbidden. Whether it should be an optional homogeneous key/value constraint remains undecided; no AST_MAP_TYPE or generic AST change is introduced pending that decision.
- Parser fixture now checks ordinary `items:map` (type_name=`map`, is_hard_typed=0) and compatibility `legacy!map` (type_name=`!map`, is_hard_typed=1). Existing local `v:notlar!map` shape is preserved. The parser's current rejection of `!map<str, i32>` is recorded as unsupported syntax, not a normative language ban.
- Latest expression associativity test commit `1ca084159884`, CI run `42034256447`: all five jobs passed (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Ubuntu and Valgrind both report 60/60 tests.


## Map ordinary parameter spelling — CI correction — 2026-11-05

- The first `a:map` fixture CI attempt failed in the combined Parser regression; it was not marked successful. The test's source newline escape was corrected and a later all-five-job run `42038999247` passed (Ubuntu, Windows, macOS, ASan/UBSan and Valgrind).
- The passing fixture checks ordinary `items:map` as `type_name="map"` with `is_hard_typed=0`, plus the retained `legacy!map` parameter form and local `v:notlar!map` declaration shape. The optional generic map spelling remains unimplemented and undecided.
- A follow-up test-only diagnostic cleanup removes post-destruction diagnostic reads; its CI run is pending. No Parser production behavior changed in that cleanup.

## Range operator single-use regression — 2026-11-05

- Added a positive AST fixture for `1..10`, requiring a binary `TOKEN_RANGE` node with literal endpoints.
- Added a negative Parser fixture for `1..2..3`; the language syntax reference defines a single range operator, so a second unparenthesized range operator must not be accepted as a chained range.
- The new range fixtures passed in workflow `42411223520` (5/5 jobs, 60/60 CTest). Endpoint type rules and runtime range behavior remain Semantic/runtime work.


## Single-range Parser rule — CI verified — 2026-11-05

- Positive fixture confirms `1..10` becomes one `AST_BINARY_EXPR` with `TOKEN_RANGE` and literal endpoints.
- Negative fixture confirms `1..2..3` is rejected; this enforces the syntax reference's single-range rule without adding a new AST node or precedence tier.
- CI run `42039331167` / commit `5800afd55483` passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Ubuntu and Valgrind each report 60/60 tests.
- Range endpoint type validation and runtime iteration semantics remain Semantic/runtime work.


## Numeric shift vs Nexus Flow operand grammar audit — 2026-11-05

- Rechecked the normative `NEXUS FLOW OPERATÖRLERİ` formal specification and the Core Reference Guide. They define `<<` as Zone Write and `>>` as Flow Feed, and list `flow.push` / `flow.pull` as functional counterparts, but do not define concrete operator operand productions or unambiguous source examples.
- The current Parser consumes both tokens in `parse_nexus_flow()` and stores the exact token in `AST_NEXUS_PIPE`. Since both numeric shifts and Flow forms may have identifier operands, choosing `AST_BINARY_EXPR` from identifier/literal shape would be heuristic and unsafe.
- No parser behavior was changed. Numeric shift AST production remains blocked until the language grammar defines a syntax-level discriminator or a neutral intermediate AST contract. Existing Flow token identity and OIR mapping remain unchanged.


## Inclusive Range semantics + Parser chain restriction — 2026-11-05

- Language decision: `1..10` is inclusive at both ends; it must yield 1,2,...,10 (10 values), not stop at 9. Syntax reference now states this explicitly.
- Parser production `parse_range()` now checks for a remaining `TOKEN_RANGE` after constructing a range and rejects a second operator with a diagnostic. This prevents the previous AST fixture from passing while the expression was silently parsed as a chained range.
- Regression source cases: valid `1..10`; invalid `1..2..3`; invalid nested composition `1..(2..3)`. Tests are added; CI verification for this latest implementation/test commit is pending.
- Semantic/runtime work still must implement inclusive endpoint iteration consistently for range loops and range patterns; Parser AST construction alone does not prove runtime behavior.


## 2026-11-05 — Parser precedence regression isolated; break checkpoint

- Latest failed CI (`42046323779`, commit `7f220360ae`) localized the failure to the comparison-chain AST assertion in `test_expression_precedence_and_associativity_ast()` (line 1331): `a < b < c` no longer had the expected left-nested comparison shape.
- Root cause: while adding the range precedence tier, `parse_comparison()` had incorrectly parsed its RHS with `parse_range()`. This let lower-precedence equality/range/logical operators leak into comparison operands. Corrected comparison RHS to `parse_bitwise_or()`; expected tier order is now comparison → bitwise operands, equality → comparison, range → equality, logical AND → range, logical OR → logical AND.
- Fix commit: `4ea2e00589dabde62ec7885fc15c34f5aaff898b`. CI run `42055978416` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind); Ubuntu CTest and Valgrind each report 60/60 tests, zero failures. Parser precedence/range fixture is now CI-verified.
- Inclusive `1..10` is a language rule (10 included); Parser fixtures cover `1..10`, chained `1..2..3`, and nested range rejection. Semantic/runtime inclusive iteration remains unimplemented and must be tracked separately.
- Numeric `<<`/`>>` versus Nexus Flow grammar remains deliberately blocked pending a syntax-level discriminator; no operand-shape heuristic.

## Pause / resume checkpoint (2026-11-05)

- Active branch: `refactor/c-core-roadmap`; `main` remains untouched.
- Resume first: inspect CI for `4ea2e00589dabde62ec7885fc15c34f5aaff898b`; if failing, fix only the concrete regression and rerun the complete matrix. Then close the range Parser task with CI evidence.
- Next AST work: finish intrinsic token → AST → codec/round-trip inventory; source-span completeness audit; then callback/function-pointer type grammar and AST representation. Keep shift/Flow grammar and map generic constraints explicitly open until language decisions are made.
- Stage gate remains AST/Parser completion first. Semantic/OIR/Verifier follow in order; Codegen is not in scope.
- Planned pause: 1–2 days after this checkpoint; no automatic reminder requested.


## 2026-11-07 — Resume after pause: Parser gate closed

- Verified run `42055978416` / commit `4ea2e00589da`: all five CI jobs passed. Ubuntu CTest and Valgrind report 60/60 tests, zero failures. The comparison-chain regression is closed.
- Updated the Parser production matrix and task list to record the green result. No Codegen work was started.
- AST continuation begins with a complete intrinsic token → Parser dispatch → AST node → payload/child → codec → positive/negative test inventory. Current `test_intrinsic_ast_family()` covers only nine representative nodes (`len`, `sizeof`, `typeof`, `is_ok`, `addr`, `clock`, `datenow`, `date`, `time`); `echo`/`listen` have a separate focused test. Other intrinsic AST tags and parser-special forms still need explicit coverage.
- Then continue source-span policy/endpoint audit and contextual codec gaps. Preserve open grammar decisions for `<<`/`>>`, map constraints and unified pointer/reference/function type representation.


## 2026-11-07 — Intrinsic AST family coverage expanded

- Expanded `test_intrinsic_ast_family()` from 9 representative mappings to 32 generic intrinsic token → AST tag cases.
- Every case now requires parser success, expected AST tag, a source location, AST graph validation, binary serialize → deserialize, and decoded graph validation.
- Special forms remain independently covered: `echo` / `listen`, `cast` / `panic`; `reg.member(...)` is included in the generic family to exercise its member-callee representation.
- CI run `42401080171` / commit `237597ca5a64` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind). Ubuntu CTest and Valgrind each report 60/60 tests, zero failures.
- Updated `docs/AST_PARSER_PRODUCTION_MATRIX.md` with the current intrinsic token → AST map. Signature-specific grammar, exact source-span endpoints, and Semantic argument/type contracts remain open.
- Next concrete AST target: audit AST parser productions for exact source-span start/end policy and then add missing endpoint fixtures; do not conflate parser source spans with semantic/runtime behavior.


## 2026-11-07 — Source-span audit: packed struct and listen

- Found two statement-dispatch paths that bypassed the enclosing production span stamp: `!struct` and `!(listen)`.
- Added full-production span stamping for packed struct declarations and listen handlers; AST_LISTEN line/column now point to the opening `!`.
- Extended `test_extern_echo_and_listen_keywords()` to assert exact byte start/end offsets and start line/column for both nodes.
- CI run `42401418322` / commit `ac13f7056143` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind); Ubuntu and Valgrind each report 60/60 tests.
- Continue source-span audit by tracing each parse_statement branch and its enclosing helper. A helper with no local stamp may be stamped by its caller; only confirmed gaps should be changed.


## 2026-11-07 — Source-span audit: import items and defer

- Confirmed `AST_USE` represents one import item per node; the source-span policy is module identifier through optional alias, excluding the shared `use` keyword and final semicolon.
- Extended the use/defer fixture to assert exact source offsets for aliased and unaliased import items and the full `defer` statement.
- Corrected a test-only expected delimiter after the first CI run; Parser production behavior was already correct.
- CI run `42401799340` / commit `41372b18b594` passed all five jobs; Ubuntu and Valgrind report 60/60 tests, zero failures.
- Current source-span work is still an incremental audit, not blanket completion. Next inspect synthesized expression-bodied function nodes and the remaining statement/operator productions for explicit span policy.


## 2026-11-07 — Parser/spec audit: named function expression body

- Found a Parser-only production in `parse_func_decl()`: named functions may currently use `f:name(params) > expression`, lowered structurally to a synthetic `AST_BLOCK` + `AST_RETURN`.
- The normative syntax reference documents expression bodies for lambda `f:(params)>expr`, but named functions are documented with block bodies. No Parser change was made.
- Recorded the discrepancy in `docs/AST_PARSER_PRODUCTION_MATRIX.md`. Before changing behavior, determine whether this named-function shorthand is legacy compatibility syntax or unsupported accidental acceptance.
- Synthetic block/return source-span policy remains open; do not fabricate source locations without deciding whether these nodes inherit the expression range or remain synthetic/no-location.


## 2026-11-07 — Expression-bodied lambda contract and span regression

- Confirmed the normative syntax is `f:(a, b) > a * b`; the expression after `>` is the lambda result. The shorter form does not require `ret`.
- Clarified `docs/language/syntax-reference.md`: expression-bodied lambdas remain `AST_LAMBDA` with `body_is_expression=1` and the body is the direct expression node; Parser must not synthesize `AST_RETURN` for this lambda form. Block-bodied lambdas remain `AST_LAMBDA` and use explicit `ret` when returning.
- The existing named-function shorthand `f:name(...) > expr` is not normative in the syntax reference. It remains untouched as a compatibility/spec discrepancy pending a separate usage audit; do not silently expand the language.
- Added exact lambda start/end byte-offset and direct-expression-child assertions to `test_rules_lambda_relocate_unroll_ast()` in commit `4eb314b8d627789588e132f59a18266e051b9318`. Updated `docs/AST_PARSER_PRODUCTION_MATRIX.md` in `33143e33c7df`.
- The lambda span regression passed in workflow `42411223520` (Windows, Ubuntu, macOS, ASan/UBSan and Valgrind).
- AST remains the active stage. Continue the per-production source-span audit; Semantic return inference, closure/capture semantics, and function-pointer representation remain later AST/Semantic contract work. No Codegen advancement.


## 2026-11-07 — Lambda AST contract regression coverage

- Added a dedicated Parser regression covering both normative lambda forms in one source: expression body `f:(a, b) > a * b` and block body `f:(a) { ret a * a; }`.
- The test asserts the expression body is directly an `AST_BINARY_EXPR` (not a synthetic block/return), the block body is an `AST_BLOCK` containing `AST_RETURN`, parameter-list shape, full graph validity, and AST serialize → deserialize → graph-validation round-trip.
- Added a malformed lambda fixture with a missing body; the test requires Parser rejection or a nonzero diagnostic count.
- Commits: `c4177107`, `ccb4983a`, `bad267e6`; Parser Production Matrix updated in `6a85ab66`.
- CI has not reported checks for these commits yet. Do not mark the regression verified until Windows, Ubuntu, macOS, ASan/UBSan and Valgrind jobs pass.
- Next AST work: continue exact source-span endpoint inventory for the remaining statement/operator productions, then close unresolved syntax-to-AST representation contracts without inventing grammar (especially callback/function-signature type, pointer/reference declarators, macro/directive metadata and shift-vs-Flow token interpretation). Reconcile every remaining row against the node implementation and ownership matrices before declaring AST complete.


## 2026-11-07 — Control-flow AST production coverage

- Added a focused Parser fixture for `ret;`, `ret 7;`, `break;` and `continue;`.
- The fixture checks the two distinct return operand shapes (NULL for void, literal for value), distinct break/continue node tags, source-span presence, full graph validation and AST serialize → deserialize → validate.
- Added a negative fixture requiring a missing `break` semicolon to produce a Parser error/diagnostic.
- Updated `docs/AST_PARSER_PRODUCTION_MATRIX.md` to record this coverage (commit `933efec0`); source test commit `7113e195`.
- The Parser/AST structural fixture passed in workflow `42411223520` (5/5 jobs, 60/60 CTest). Loop legality, reachability and return-type compatibility remain Semantic responsibilities.


### CI update — run 42408436371 (commit `7113e195`)

- Ubuntu CTest: 60/60 passed.
- macOS CTest: 60/60 passed.
- Windows build + CTest: passed.
- ASan/UBSan: passed.
- Valgrind: still running at the time of this update; overall matrix is not yet green.


## 2026-11-07 — Full syntax reconciliation and comparison baseline

- Compared `docs/language/syntax-reference.md` against active `lexer.c`, `lexer.h`, `parser.c`, `ast.h`, AST lifecycle/validator dispatch and the existing production matrix.
- Corrected misleading normative examples: `return` → `ret`, unsupported scientific literal example, unsupported hexadecimal code operands, and the non-Parser `label?>` spelling.
- Removed `d32/d64` from the current built-in type list; documented generic `t`, callback/function-type, pointer/reference, null literal, directive/macro and DLL-export gaps rather than claiming support.
- Added `docs/language/NEXUS-FLOW-LANGUAGE-COMPARISON.md` with distinctive feature combinations, advantages/risks, comparisons to C/Rust/Zig/Go/Kotlin, and a prioritized missing-feature inventory.
- Parser additions: canonical `loop { ... }` infinite loop and contextual `jump(label);` statement alias; legacy `(loop) { ... }` and explicit `?>(label);` remain.
- Added `test_canonical_loop_and_jump_call_forms` for AST shape, label target, graph validation and AST codec round-trip.
- Commits: Parser `fee81b7e`; Parser test `456d8af7`; language comparison `8a2e44b0`; syntax reference `32469a06`.
- CI has not yet validated the new Parser test. Keep Parser/AST status partial until Windows, Ubuntu, macOS, ASan/UBSan and Valgrind checks complete.
- Remaining syntax/AST work: generic type-variable binding/unification/inference, dedicated function-type AST decision, Option/Result and `none` model, numeric Semantic conversion, and the `<< / >>` collision. Pointer/reference, callback signature, numeric spelling and `null` literal Parser support are implemented; Semantic ownership/type rules remain open. Do not enter Codegen while AST contracts remain open.


## 2026-11-07 — Module path syntax closure

- The normative `use msvcrt.exf;` example did not match the old single-identifier `parse_use()` production.
- Parser now accepts dotted module paths and retains the joined module path in `AST_USE.module_name`; aliases and comma-separated imports remain unchanged.
- Added `test_dotted_module_use_path` with AST graph validation and AST stream round-trip.
- Commits: Parser `4738207c`; Parser test `9d1d5f7d`.
- CI validated by workflow run `42411223520` (all 5 jobs passed). Module file resolution and path security remain outside Parser/AST.


## 2026-11-07 — Pointer/reference syntax production

- Added Parser type-name handling for `*T`, `**T` and `&T` across local variables, function parameters, function return types and `nt:` target types.
- AST preserves the exact normalized type spelling in existing owned type-name strings; AST node IDs and OIR node IDs remain unchanged.
- Added `test_pointer_reference_type_declarators` for all declaration contexts, graph validation and AST stream round-trip.
- Updated the normative syntax reference and both Parser/AST matrices. Semantic pointer validity, borrow/lifetime, nullability and ownership remain open.
- Commits: Parser `0b87bd7b`; test `df9ef675`; syntax `5f430749`.
- CI validated by workflow run `42411223520` (all 5 jobs passed).


## 2026-11-07 — Callback/function-type syntax production

- Added Parser type-expression grammar `f:(name!Type, ...)>!ReturnType`.
- Supported contexts: `nt:` aliases, local variable type, function parameter type and function return type.
- The normalized signature is preserved in existing AST type-name strings; lambda expression bodies remain separate `AST_LAMBDA` nodes.
- Added `test_function_type_alias_and_callback_parameter` with positive alias/parameter/invocation checks, AST validation/codec round-trip and malformed-signature negative case.
- Dedicated function-type AST node, Semantic callback compatibility, closure capture and ABI lowering remain open.
- Commits: Parser `486a92ba`; parser lookahead `37862b24`; test `51198999`.
- CI validated by workflow run `42411223520` (all 5 jobs passed).


## 2026-11-07 — AST tag inventory reconciliation

- Enumerated all 105 append-only AST tags and compared Parser construction/dispatch paths.
- Classified `AST_SPAWN` as legacy-only (Thread module boundary), `AST_ASM_BLOCK` as legacy-only (current FASTEXEC/ASM_LABEL path), and `AST_ZONE_SYNC/VIEW/LOCK` as reserved tags (zone member methods currently parse as generic member calls).
- Updated syntax reference and Parser/AST matrices so AST enum presence is not confused with active syntax support.


## 2026-11-07 — Numeric and null literal production

- Extended Lexer numeric grammar: decimal integer/fraction, hexadecimal `0x`, binary `0b`, octal `0o`, scientific exponent and underscore digit separators.
- Kept numeric suffixes such as `u8` / `f32` invalid; `500ms` remains the dedicated rolling-delay token split.
- Appended `TOKEN_NULL` after existing token IDs and added Parser `AST_LITERAL` support preserving the source spelling.
- Added positive/negative Lexer numeric tests and Parser AST/codec coverage for null, radix and scientific forms.
- Updated normative syntax and comparison docs. Semantic conversion, overflow/range, pointer-nullability and Option/None remain open.
- CI validated by workflow run `42411223520` (all 5 jobs passed).


## 2026-11-07 — Syntax production CI checkpoint

- Latest code/test workflow: `42411942789`.
- **CI result: 5/5 jobs passed.** Ubuntu CTest, Windows CTest, macOS CTest, ASan/UBSan and Valgrind all succeeded.
- CTest suite: 60/60 passed on the build/test matrix.
- Covered in this validated checkpoint: canonical `loop {}`, `jump(label);`, dotted `use` paths, pointer/reference type spellings, callback function-type signature strings, implicit `t` type-variable spelling, numeric radix/exponent/separator forms and the `null` literal.
- This checkpoint does not close AST as a whole. Generic type parameters, dedicated function-type AST node, Semantic type/ownership rules, exact source-span audit, AST OOM injection and full node/factory/codec/validator parity remain open.


## 2026-11-07 — Thread block syntax reconciliation

- Added the missing normative syntax for `(name)>{ ... }` to the language reference.
- Confirmed Parser emits `AST_THREAD_BLOCK` with name/body and leaves `mode` / `timeout` NULL.
- Existing `test_thread_block_ast` checks AST shape, codec round-trip and spawn rejection; `test_thread_block_source_span` checks exact byte span.
- `spawn` remains intentionally outside the core Parser and is owned by the Thread module.
- Scheduling, start/join, timeout, cancellation, capture/lifetime and memory-ordering remain Thread/runtime contracts.
- CI workflow `42411942789`: 5/5 jobs passed, 60/60 CTest.


## 2026-11-07 — Implicit generic type-variable AST fixture

- Preserved the existing Nexus Flow model `f:sum(a!t,b!t)!t`; did not introduce a new `<T>` declaration syntax.
- Added `test_implicit_type_variable_spelling` to verify parameter type strings, return type string, graph validation and AST codec round-trip.
- Parser/AST spelling is covered; Semantic type-variable scope, binding, unification and inference remain open.
- CI workflow `42411942789` passed all five jobs (60/60 CTest) for the newly added fixture.


## 2026-11-08 — Callback syntax correction

- Function signature and lambda body delimiter are now distinguished in the syntax audit.
- Canonical proposal: `f:(params)!ReturnType`; body: `>`.
- Example: `f:(id!u32)!bool > (id == 1) ? true : false;`.
- Existing `f:(params)>!ReturnType` Parser behavior is legacy; migration and regression tests remain open.


## Group Parser increment — nested groups + default dispatch — 2026-11-08

- `parse_group_body()` ayrıştırması ayrıştırıldı; `name => group { ... }` nested group alias'ı artık kendi `AST_GROUP_DECL.name` alanında korunuyor.
- `default { ... }` Group gövdesi `AST_METHOD` (`public_name = "default"`) ve `AST_BLOCK` implementation olarak temsil ediliyor. Böylece mevcut AST_METHOD codec/destructor sahipliği yeniden kullanılıyor; AST node ID/ABI değişmedi.
- Nested group içindeki metotlar ve default ile ana Group default'unu doğrulayan `test_nested_group_and_default_ast` eklendi; `ast_validate` ve AST serialize/deserialize round-trip kontrolü içeriyor.
- Parser/test commitleri: `9fd3a151` (implementation), `80946258` (test). Production matrix commit: `54fa75e2`.
- CI run `42416361727` (Parser change) ve `42416388202` (test change) tetiklendi; sonuçları henüz tamamlanmadan PASS olarak işaretlenmeyecek.
- **Açık Group işleri:** default dispatch'in Semantic/OIR davranışı; `self` receiver binding; struct/group isim eşleştirmesi ve instance method çağrısı; `yeni` constructor; visibility/export (`pup`/`exp`) kapsamı; malformed nested/default testleri; Group span kapsamı ve AST Graph Validator bağlam kuralları.
- Bu increment yalnızca Parser/AST biçim desteğidir; Group semantiği ve runtime dispatch tamamlanmış değildir.

- Follow-up düzeltme: `1464fee3` C string terminator literal ve `error_at()` diagnostic argümanını düzeltti. CI run `42416476317` bu commit üzerinde çalışıyor (Ubuntu/macOS/Windows + ASan/UBSan + Valgrind); final sonuç bekleniyor.


## 12. Group + Library/Module Takibi (2026-11-08)

Group'un iki ana rolü ayrı takip edilir: **namespace/library API** ve **Struct bağlı instance metotları**. Yalnızca object ilişkisi tamamlanınca Group bitmiş sayılmaz.

| Özellik | Parser / AST | Semantic | Module/OIR | Durum |
|---|---|---|---|---|
| Named Group + üyeler | ~ | ~ | ? | Parser AST üretimi ve codec mevcut; Group type scope için ilk Semantic sahiplik denetimi eklendi; tam üyelik çözümleme açık |
| Nested Group alias | ~ | ~ | ? | Parser eklendi; üst Group özel tür izolasyonu için Semantic denetimi/testleri eklendi; negatif/nested hata testleri çalıştırılmalı |
| Group default | ~ | ? | ? | AST_METHOD("default", BLOCK) temsili; dispatch/default resolution yok |
| Group method | ~ | ? | ? | AST_METHOD üretimi; callable symbol ve signature checks açık |
| Group-owned Struct / Enum / Union scope | ~ | ~ | ? | Group owner AST kimliği Slot üzerinde tutuluyor; nested Group tür izolasyonu ve dış tür görünürlüğü için testler eklendi, çalıştırma bekliyor |
| Struct–Group eşleşmesi | ? | ? | ? | İsim/ilişki kuralı formal spec'te; binding doğrulaması açık |
| self receiver | ? | ? | ? | İlk parametre receiver sözleşmesi; binding/type/member checks açık |
| Constructor (yeni) | ? | ? | ? | Constructor semantiği ve instance initialization açık |
| Group namespace member access | ~ | ? | ? | AST_MEMBER_ACCESS parse edilir; Group path çözümleme yok |
| Library public/private | ~ | ? | ? | Group top-level pup/exp bayrakları mevcut; üye bazlı API visibility doğrulanmalı |
| Module use / import | ~ | ? | ? | AST_USE dotted path ve `as`/`ns` Parser–AST–OIR desteği mevcut; Module Resolver C graph core eklendi, host source-provider ve Semantic import scope yok. `m:` ayrı olarak immutable value'dur; dosya/dependency çözümlemesi yapmaz |
| Export / ABI | — | ? | ? | Codegen/linker öncesi symbol/ABI kontratı açık |

**Normatif tasarım kaydı:** Group Formal Specification §8, Group'un Namespace Group ve Object Group rollerini; pup/exp, m:/use, member resolution, Module Resolver ve Library ABI sınırını tanımlar. Bu belge tasarım kararıdır, uygulama tamamlanma kanıtı değildir.

**CI notu:** macOS Clang, parser.c içinde consume() fonksiyonuna message string yerine integer message ID verilmesi hatasını yakaladı. c6859c49 bu beş çağrıyı MSG_EXPECT_LBRACE_FUNC sabitine düzeltti; yeni CI run 42417366333 devam ediyor. Windows, Ubuntu, ASan/UBSan ve Valgrind önceki run'da geçti; macOS düzeltme commit'i için tekrar doğrulanacak.

## 2026-11-08 — Group-owned nominal type scope

- `nexus docs/Group Metodu — Formal Specification.md` §5 ve yeni §9 güncellendi: Group içinde bildirilen Struct/Enum/Union türlerinin yalnızca kendi Group fonksiyon/metotlarında görünmesi; nested, sibling ve dış kapsam izolasyonu; Group dışı türlerin normal lexical görünürlüğü normatifleştirildi.
- Semantic `Slot` kayıtlarına non-owning `group_owner` AST kimliği, `SemanticContext`'e aktif Group sahibi eklendi. Nominal tür çözümlemesi, sembol adı yanında aktif Group kimliğini de denetliyor. Nested Group üst Group'un özel türlerini devralmıyor; Group dışı türler görünürlüklerini koruyor.
- `test_semantic.c` içine Struct/Enum/Union yerel kullanım, Group metodu kullanımı, nested Group izolasyonu, aynı isimli nested türlerin ayrılması, sibling/dış kapsam reddi ve dışarıda tanımlı türün Group içinde kullanımı için testler eklendi. İlk taslak fixture'lar parser'ın gerçek Nexus Flow sözdizimine (`struct:Type`, `f:name(...)`, `method => f:name(...)`) göre düzeltildi (`352aee36`).
- Bu değişiklikler GitHub üzerinde `refactor/c-core-roadmap` dalına yazıldı. **Henüz CMake/CTest çalıştırılmadı; testler eklenmiş fakat doğrulanmamış durumda.** Bu çalışma ortamında GitHub checkout erişimi DNS nedeniyle başarısız oldu ve commit için Actions workflow run bulunamadı. CI sonucu alınmadan Semantic Group scope tamamlandı olarak işaretlenmeyecek.
- Kod commitleri: semantic header `a3fabf48`, Semantic scope `c3a6bcd8` + kapsam daraltma `0709cccd`, testler `bc5eba54` + Group method fixture `d6db22b3`; spec `6fa982d9` + scope/public-member clarification `b9db9979`.

## 2026-11-08 — Group binding / dispatch contract

- Group spec §8.6 eklendi: Group adıyla tam/case-sensitive eşleşen doğrudan Struct/Enum/Union ana nominal binding adayı; eşleşme yoksa Namespace Group; Struct binding instance/self sözleşmesi; Enum/Union binding statik API olarak tanımlandı. Method `public_name` ile implementation adı ayrıldı; `default` ayrı handler olarak normatifleştirildi.
- Mimari inceleme: ASTGroupDecl ve ASTMethod bu yapısal bilgileri taşıyor. Semantic şu anda AST_GROUP_DECL için yalnızca scope açıp kapatıyor; Group sembolü, Group member/public-name sembolleri ve binding ilişkisi henüz üretilmiyor. SemanticResult sembol türlerinde de Group kaydı/Group relation bulunmuyor. Bu nedenle Group binding uygulaması yalnızca `semantic.c` içinde küçük bir if ile yapılmamalı; SemanticResult validator/codec ve OIR handoff sözleşmesi birlikte ele alınmalı.
- Group scope test fixture'ları gerçek Nexus Flow syntax'ına düzeltildi (`352aee36`). CMake/CTest hâlâ çalıştırılmadı; bu ortamdan GitHub checkout DNS hatası verdi ve ilgili commit için Actions run bulunamadı. Test doğrulaması açık.
- Yeni spec commit: `71512edc`; ilerleme belgesi bu kaydı içerir.

## 2026-11-08 — Group member name validation

- Semantic `AST_GROUP_DECL` işlemi öncesinde doğrudan Group üyelerinin API adları karşılaştırılıyor. `AST_METHOD.public_name`, direct function, nested Group, Struct/Union, Enum, variable ve NewType adları aynı Group üye namespace'inde çakışma denetimine alındı.
- Tekrarlanan public method adı, birden fazla `default` handler ve method alias/direct function adı çakışması `ERR_SEMANTIC_REDEFINED` ile raporlanıyor.
- `test_semantic.c` için bu üç negatif senaryo eklendi (`e7b91637`). Semantic uygulama commit'i `64a8e27b`.
- **Test çalıştırılmadı.** CMake/CTest doğrulaması hâlâ bekliyor; test fixture düzeltmeleri ve yeni negatif senaryolar CI veya yerel build gerektiriyor.

## 2026-11-08 — Group scope visibility hardening

- Group-owned Struct/Enum/Union türleri yalnızca çözümlemede değil, bilinmeyen tür için benzer isim önerilerinde de mevcut Group sahibiyle filtreleniyor. Böylece başka Group/dış kapsam tanılama önerileri özel tür adlarını sızdırmıyor (`27b0ed74`).
- Group üye adı doğrulaması `AST_METHOD.public_name` dahil doğrudan API üyelerini tek isim alanında karşılaştırıyor; `default` da aynı kural altında tekil handler olarak korunuyor.
- Bu aşamadaki tüm Semantic testleri kaynak düzeyinde eklendi/düzeltildi; derleme ve çalışma sonucu hâlâ yok. `test_semantic` CMake hedefi `occ_frontend` ile bağlı ve CTest tarafından iki fixture dosyası argümanıyla çağrılıyor; yeni `check_source` vakaları bu test çalıştırıldığında ana test başlamadan önce yürütülüyor.

## 2026-11-08 — Group SemanticResult/OIR relation contract

- SemanticResult mevcut Symbol/Relation tabloları üzerinden Group'u temsil edecek şekilde tasarlandı; paralel Group member tablosu açılmayacak.
- Formal Group spec §10 eklendi: Group Symbol, doğrudan Group üyeliği, nominal type binding, public method → implementation bağlantısı ve default handler için append-only relation türleri tanımlandı.
- OIR Verifier için Group AST node kind eşleşmeleri, relation owner/related-node/related-symbol kontrolleri ve tekil type binding/default handler kısıtları normatifleştirildi.
- Semantic AST OIR reader'ın schema minor değerini tam eşitlikle kontrol ettiği doğrulandı. Bu nedenle Group kayıtları schema 1.1 ile yayımlanacak; yeni reader 1.0 ve 1.1 okuyacak, 1.0 içinde yeni enum değerlerini reddedecek. Record layout değişmeden kalacak.
- **Kod henüz değiştirilmedi ve test çalıştırılmadı.** Sıradaki uygulama: SemanticResult enum/validator → OIR enum/codec → schema 1.0/1.1 reader uyumluluğu → OIR reference verifier → Semantic Group kayıt üretimi.

## 2026-11-08 — Group SemanticResult/OIR foundational implementation

- SemanticResult'a append-only SYMBOL_GROUP ve dört Group relation türü eklendi: doğrudan üyelik, nominal type binding, public method → implementation ve default handler.
- SemanticResult validator Group relation owner türünü, zorunlu related symbol kurallarını, tekil binding/default ordinal değerlerini ve relation adlarını doğruluyor.
- OIR Semantic AST schema minor 1.1'e çıkarıldı. Group symbol için AST_GROUP_DECL node-kind eşleşmesi; Group relation türleri için owner ve related-node doğrulaması eklendi.
- Artifact reader 1.0 ve 1.1'i kabul edecek şekilde güncellendi; 1.0 artifact içinde 1.1 Group enum değerleri varsa reddediyor. 1.0 boş/legacy artifact okuma regresyon testi eklendi.
- Semantic checker artık Group Symbol üretiyor; doğrudan üyelik, eşleşen Struct/Enum/Union binding, public method implementation ve default handler relation kayıtlarını SemanticResult'a ekliyor. Group annotation'ı da Group sembol kimliğine bağlanıyor.
- SemanticResult model, OIR symbol/relation codec ve schema 1.0 okuma uyumluluğu için testler eklendi.
- İlgili kod commitleri: SemanticResult enum/validator dbfc0ef5, 4f755f3f, 9d991fbb, 71b264a4; SemanticResult test f98dce70; OIR schema/validator 92b7298e, d58c6ffe, 2e5fd6ef; artifact reader 8d0e7d7c; schema uyumluluk testi 288d31f7; Group codec test a087ca57, relation verifier b7cdf754; Semantic checker f01ffa1a, e0094cca.
- **Doğrulama durumu: çalıştırılmadı.** GitHub workflow sorgusu bu commitler için run döndürmedi; yerel CMake/CTest de henüz çalıştırılmadı. SemanticResult/OIR değişiklikleri bu nedenle testten geçmiş veya tamamlanmış sayılmıyor.
- Sıradaki işler: Group semantic integration testinde node resolver bağlayarak SemanticResult kayıtlarını gerçek AST üzerinden doğrulamak; OIR artifact 1.1 Group round-trip ve bozuk referans testlerini eklemek; derleme/CI sonuçlarına göre düzeltmek. Ardından Group–Struct binding, self receiver ve çağrı çözümleme uygulamasına geçmek.
### Group SemanticResult/OIR integration follow-up — 2026-11-08

- SemanticResult integration fixture now builds a real Nexus Flow AST containing a Group whose name matches its Struct, a public method alias and a default handler. It attaches the same stable AST pointer index used by the Semantic stage.
- The fixture checks Group Symbol plus six relations: three direct members, one nominal binding, one method implementation and one default handler.
- The same fixture serializes the AST, writes a SEMANTIC_AST schema 1.1 artifact and passes it through the complete artifact reader/Verifier. It also rebuilds that artifact with schema minor 1.0 and requires rejection because the payload contains Group enum extensions.
- SemanticResult and OIR Verifier now additionally require Group type-binding names to match both the Group symbol and nominal type symbol, and GroupMember names to match their related symbol when one exists.
- Incomplete Group member node IDs, missing nominal binding symbols and missing method implementation symbols now fail closed instead of silently omitting relation records.
- Follow-up commits: real Group SemanticResult/OIR integration 71fdb1ce; schema 1.0 Group-tag rejection fixture 46ad6d70; relation name/type identity checks 373089d7, ffb1800a, 60ef47f4; incomplete relation fail-closed handling 1be6360d.
- **Test status remains unverified.** These tests and implementation changes have not been run locally or by GitHub Actions. The integration fixture is added but must not be counted as passing until CMake/CTest and the CI matrix complete.


## 2026-11-08 — Group Object receiver inference (initial implementation)

- Semantic traversal gained an internal single-node mode so Group members can be visited without following their sibling chain. This preserves the AST graph and prevents a binding-first pass from double-processing declarations.
- A Group's direct Struct/Enum/Union declaration whose name exactly matches the Group is now semantically checked before other direct members. The remaining members are then checked once in source order. This makes the nominal binding available to method signatures regardless of whether the binding appears before or after a method.
- Corrected a binding collision discovered during implementation: the Group Symbol and its same-named direct nominal binding intentionally coexist in adjacent lexical scopes. Struct/Enum duplicate checks now permit only the exact active Group's own matching Group Symbol; unrelated collisions remain errors.
- AST_METHOD establishes a temporary method context and resolves the matching direct Struct binding. An initial self receiver is accepted only for Struct-bound Object Groups; a later-positioned self is rejected; an explicit type must exactly match the bound Struct name; an untyped first self receives the bound Struct's type name and type ID before the method body is checked. The parameter Slot, SemanticResult symbol type, annotation and function parameter signature are updated.
- Namespace Group and Enum/Union-bound Group methods do not receive implicit self. A method without self remains a static method. Direct Group functions do not inherit method receiver context.
- Added Semantic regression fixtures for method-before-Struct inference and self.field, explicit matching type, static method, wrong explicit type, non-first self, Namespace Group self rejection and Enum-bound self rejection.
- Commits: context/header e5ba41c4; initial Semantic implementation 0071555f; tests 98bef8fe; method-child traversal isolation follow-up recorded separately.
- Verification status: not run. CMake/CTest and CI are still required. These fixtures and the receiver behavior remain partial until compilation, the full Semantic test target, SemanticResult/OIR integration tests, and platform/sanitizer CI pass.
- Remaining Group work: constructor semantics, resolved Group/member call targets, instance.method(args) receiver argument binding, Module Resolver/import scope, visibility/export and default dispatch.

### Receiver implementation verification update

- GitHub Actions run `42425608949` completed successfully on the active branch.
- Linux, macOS and Windows build/test jobs passed; each CTest run reports **60/60 tests passed**.
- ASan/UBSan passed.
- Valgrind passed.
- The initial run exposed two issues and both were corrected before the successful run: a missing forward declaration for the nominal-slot predicate, and test fixtures using reserved keyword `use` as a Group method alias. The integration fixture also now checks that inferred `self` type is reflected in the SemanticResult symbol, annotation and function-parameter relation.
- This validates the current initial receiver implementation on the tested CI matrix. It does not complete Group method dispatch or Module Resolver work.

## 2026-11-08 — Group callable dispatch

- Added Group callable signature predeclaration. AST_METHOD implementation functions and direct Group functions are registered before Group member bodies are checked, allowing forward calls to later members.
- Group callable resolver supports AST_METHOD public aliases and direct Group function names. AST_METHOD aliases resolve to their internal implementation symbol; direct Group functions are static members under their own names.
- Implemented static calls through Group namespace and bound nominal Type, plus instance calls through the Group's bound Struct. Instance calls implicitly consume the first self parameter; explicit call arguments are checked starting at parameter index 1.
- Static/instance call form mismatches are rejected. A static method or direct Group function cannot be called through an instance; an instance method cannot be called through a Group/Type qualifier. Argument count, argument types and unknown method names are checked.
- Instance dispatch is restricted to the Group's exact nominal binding Struct; another Group-owned Struct does not inherit the Group's method surface.
- Group implementation Slots persist at the containing lexical depth after Group scope exit so qualified Group calls can resolve them. Their internal names remain hidden from ordinary lookup and suggestion filtering outside their owning Group. Scope exit now removes all slots at the exiting depth rather than assuming depth-contiguous list order.
- SemanticResult and OIR Verifier permit a qualified reference to a Group child-scope symbol only when the Group symbol is visible from the reference scope and a GroupMember, GroupTypeBinding or GroupMethodImplementation relation exposes the target. Added a direct SemanticResult test for this exception; sibling-scope references without a Group relation remain rejected.
- Integration tests verify instance call to a later method, receiver offset, call/callee annotations, alias-to-implementation relation, and Group-qualified static call after leaving the Group scope. Semantic tests cover Type/Group static calls, direct Group functions, hidden implementation names, invalid receiver modes, wrong argument count/type, unknown methods and non-binding nominal types.
- Verification: GitHub Actions run `42426973498` succeeded. Linux, macOS and Windows CTest passed 60/60; ASan/UBSan and Valgrind passed.
- Remaining Group work: constructor behavior, nested Group dotted-path resolution/Module Resolver, visibility/export enforcement, default handler dispatch and full Group/Library ABI integration. Codegen remains blocked until the earlier pipeline stages are complete.
### Anonymous Group method implementation and static factory typing

- Preserved the parser's supported alias form where the implementation function name is omitted: `open => f:(self) { ... }`. Semantic uses the public alias as the internal function symbol name only for this anonymous implementation form. Named implementations keep their explicit internal symbol name.
- Added regression tests for anonymous instance methods and anonymous static factory methods whose return type is the Group's bound Struct. `Type.factory(args)` propagates the declared return type to the call expression, enabling a typed local assignment.
- GitHub Actions run `42427536942` passed: Linux, macOS and Windows CTest 60/60; ASan/UBSan and Valgrind passed.
- Actual Struct literal construction and field initialization checks remain pending; the static factory test verifies call/return type flow, not complete constructor lowering.

## 2026-11-08 — Group constructor/factory contract clarification

- Group Formal Specification §4.4 yeniden yazıldı: yapıcı/factory'nin statik Group method olduğu, `Type.alias(args)` biçiminde çağrıldığı, `self` alamadığı ve bağlı Struct nominal türünü dönüş imzasında `!Type` ile bildirmesi gerektiği normatifleştirildi.
- Kullanım örnekleri eklendi: Type-qualified factory call ve typed assignment; factory sonucunu başka fonksiyona/parametreye aktarma; instance method ile static factory ayrımı ve geçersiz çağrı biçimleri.
- Önemli sınır açıkça yazıldı: factory signature/type propagation, instance allocation veya field initialization değildir. Struct literal örneği hedef syntax olarak işaretlendi ve mevcut destek gibi gösterilmesi engellendi.
- Bu commit yalnızca specification netleştirmesidir; constructor/Struct literal uygulaması veya test başarısı iddiası değildir.
- Sıradaki uygulama: Struct literal AST üretimi ve Semantic field mapping/type validation; ardından OIR instance-init temsili. Codegen'e geçilmeyecek.

## 2026-11-08 — Struct literal contract (design checkpoint)

- Group Formal Specification §4.5 now normatively fixes Struct literal syntax as `Type { field = expression, ... }`; field names are declaration-order independent, each field exactly once, unknown/duplicate/missing fields reject, and each initializer is checked against the declared field type.
- SemanticResult/OIR contract requires nominal Struct symbol/type ID and resolved field symbol IDs; OIR initialization follows source expression evaluation order while layout order remains Struct declaration order. No partially valid instance is emitted after diagnostics.
- **Implementation status remains pending (~):** no Parser/AST node, codec schema, Semantic field mapping, OIR instance-init record/opcode or feature tests are claimed complete. This design checkpoint does not advance the implementation completion matrix.
- Next implementation sequence: append-only AST StructLiteral + StructFieldInit tags/payload/ownership → Parser primary/postfix grammar → AST codec/registry round-trip → Semantic nominal type and field validation → SemanticResult serialization → OIR instance-init representation + verifier → positive/negative tests and CI.


## 2026-11-08 — Struct Literal AST/Parser first implementation slice

- AST enum'a append-only `AST_STRUCT_LITERAL` ve `AST_STRUCT_FIELD_INIT` eklendi; mevcut tag ordinal'ları değiştirilmedi.
- AST payload: literal nominal type name + owned field initializer sibling chain; field initializer owned name + value expression. AST destroy/debug-print lifecycle eklendi.
- Parser `Type { field = expression, ... }` biçimini expression context'inde ayrıştırıyor. Field source order ve type spelling AST'de korunuyor.
- `test_parser.c` içine `Point { y = 2, x = 1 }` AST shape/source-order testi eklendi.
- **Henüz test/build çalıştırılmadı:** GitHub API üzerinden kaynak değişikliği yapıldı; yerel checkout/CI sonucu bu oturumda yok. AST codec/validator registry ve Semantic mapping halen bekliyor; bu nedenle feature tamamlandı işaretlenmedi.
- Bir sonraki kapı: AST serialize/deserialize + AST graph validator + OIR AST field schema/registry için iki yeni node'un bütün alanlarını tanımlamak ve round-trip testleri.


## 2026-11-08 — Struct literal AST implementation checkpoint

- `AST_STRUCT_LITERAL` and `AST_STRUCT_FIELD_INIT` tags/payloads are append-only additions; parser syntax and ordered field initializer list are implemented.
- AST lifecycle/debug printing, recursive destruction, serialization and deserialization now include both node kinds.
- AST graph validator now checks required Struct type name, field names, and initializer/value child links. Validator tests cover valid/invalid payloads, empty initializer-list structural acceptance, and child-cycle detection. Empty/missing Struct fields remain a Semantic completeness rule, not an AST graph rule.
- `OIR_AST_NODE_REGISTRY.md` records the field contracts for both node kinds.
- Parser regression test covers `Point { y = 2, x = 1 }` and confirms source initializer order is preserved. AST codec round-trip fixture now covers both Struct literal and field-init nodes.
- SemanticResult mapping reuses the existing `SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD`: owner is the nominal Struct symbol; `related_node_id` identifies the field declaration AST node; name, ordinal and type ID complete the field contract. The field-init annotation carries its expression type; no separate field Symbol kind or new table is introduced.
- **Validation status at this checkpoint:** GitHub Actions run `42455300701` on commit `6a43ce54` passed all five jobs: Ubuntu, macOS, Windows, ASan/UBSan and Valgrind; CTest reported 60/60. This run includes the dedicated legacy AST stream round-trip and invalid-tag boundary update. OIR instance-init lowering and independent Verifier rules remain pending; Struct literal is not end-to-end complete.

## 2026-11-08 — Struct literal Semantic + OIR AST bridge validation

- Semantic traversal now resolves only visible `SLOT_STRUCT` types for Struct literal construction; visible Enum/Union/non-Struct types receive type-mismatch diagnostics, while unknown or Group-inaccessible nominal types receive unknown-type diagnostics.
- Every initializer expression is checked; unknown fields, duplicate initializers, missing declared fields and incompatible values are diagnosed. Field lookup is independent of source order; expression evaluation order remains the AST source order. Empty `{}` is structurally valid and succeeds semantically only for a zero-field Struct.
- Group-private nominal type visibility is preserved. Semantic regression tests cover valid reordered fields, unknown/duplicate/missing fields, string and numeric type mismatches, non-Struct type, empty Struct, inside/outside/nested Group visibility and a Group factory returning a Struct literal.
- SemanticResult integration validates the literal's nominal Struct symbol/type annotation and field declaration relations. `AGGREGATE_FIELD` relation carries the field declaration node ID, name, declaration ordinal and type ID; field initializer annotation carries the expression type.
- Invalid-literal gate is tested: a Struct literal with a missing field produces Semantic diagnostics and `semantic_finalize_result` returns `SEMANTIC_RESULT_INVALID_DATA`; no failed instance can be finalized for OIR handoff.
- OIR AST registry appended IDs 106/107. Map, schema, index child traversal, writer and reader were extended; parser→OIR AST→reader→writer canonical byte round-trip is covered. Parser now stamps source spans on each field initializer.
- AST factory and graph-validator upper bounds were advanced to `AST_STRUCT_FIELD_INIT`; factory-boundary, graph, parser, semantic, OIR schema/map and round-trip regressions were updated.
- Run `42455141289` on commit `d552313d` passed Ubuntu/macOS/Windows CTest, ASan/UBSan and Valgrind; Ubuntu CTest reported 60/60. Follow-up run `42455300701` on commit `6a43ce54` also passed all five jobs after adding the dedicated legacy AST codec round-trip and updated invalid-tag boundary.
- CI validation: `42455300701` (commit `6a43ce54`) passed Ubuntu/macOS/Windows, ASan/UBSan and Valgrind; 60/60 CTest. This confirms the dedicated legacy AST codec fixture in addition to the earlier Semantic/OIR round-trip coverage.
- Follow-up CI run `42455583043` passed all five jobs with 60/60 CTest after adding the negative SemanticResult finalization-gate test.
- Follow-up CI run `42455856774` on commit `830e0dac` passed all five jobs with 60/60 CTest after adding the Group factory-return and nested Group isolation cases.
- Documentation/registry audit follow-up run `42455976764` passed all five jobs; the updated Group spec, AST ownership/implementation matrices and syntax matrix are CI-validated.
- Struct instance design decision (2026-11-08): address identity, not aggregate value-copy semantics. Assignment/parameter/return/instance receiver must preserve the same instance address; `self` is an address-based receiver alias. No hidden heap allocation or aggregate-copy SSA value is permitted. Storage origin, mutable reference model, alias/null/dangling checks and escape/lifetime/ownership remain explicit Semantic/OIR prerequisites.
- SemanticResult now marks object-method `self` with `SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER`; OIR Semantic AST schema 1.2 preserves the flag. The integration test asserts the marker survives SemanticResult → OIR artifact encode/decode; legacy 1.1 artifacts retain zero-only annotation flags. Address receiver marking is not yet a complete pointer/borrow/lifetime verifier.
- CI run `42458567864` on commit `7d3ee685` passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Ubuntu CTest reported 60/60; this includes the address-receiver annotation round-trip, non-`self` rejection and schema 1.1 compatibility regression.
- **Remaining:** complete OIR instance-init representation and independent Verifier validation before Codegen; keep Codegen gated.


## 2026-11-08 — Semantic pointer/reference type shape (partial)

- SemanticResult type kind enum'una append-only POINTER=8 ve REFERENCE=9 eklendi; mevcut kind değerleri korunuyor.
- Semantic checker `*T`, `**T`, `&T` için katmanlı type kayıtları üretir: her pointer/reference kaydının `base_type_id` alanı doğrudan alt türü gösterir. Pointer-depth iki ayrı kayıtla temsil edilir; hard flag dış katmana uygulanır.
- SemanticResult finalize ve OIR Semantic AST type validator pointer/reference base ID ve layer-name invariants kontrol eder.
- OIR Semantic AST schema minor 1.3 oldu. 1.0–1.2 artifact reader eski şemalarda yeni type kind değerlerini reddeder; annotation-flag ve Group schema kontrolleri korunur.
- SemanticResult unit testine Pointer→Pointer ve Reference base ilişkisi/interner ayrımı eklendi.
- **Test/build/CI henüz çalıştırılmadı.** Semantic parser testleri, OIR type round-trip/Verifier testleri ve schema 1.2 legacy artifact regression eklenip doğrulanmalı. Bu slice tamamlandı sayılmaz.
- Sonraki: semantic type compatibility'yi pointer/ref layer eşitliği + base type identity ile kontrol eden ortak helper; ardından nullability/mutability/alias/lifetime kurallarını ayrı kararlar olarak spesifikasyona bağlamak. Codegen kapalı.

- CI ilk denemesi run `42460134137`: derleme başarılı, CTest 59/60; tek başarısız test `semantic_checker_persistent_result_integration` içindeki sabit schema-minor=2 beklentisiydi. Test beklentisi 1.3'e güncellendi; yeni CI run `42460211363` queued/in progress, sonuç bekleniyor. Pointer/reference kodu bu nedenle henüz yeşil CI doğrulamasına sahip değil.

- Pointer/reference syntax follow-up: `&&T` is marked reserved/undefined in the canonical syntax reference; `**T` remains nested pointer (P2P), `&T` remains Ref. No second reference layer is introduced absent a separate semantic use case.
- Struct copy/move design note added to Group spec: default transfer preserves instance address; explicit `copy(x)` creates independent copy; `move(x)` is ownership transfer and source use must be rejected after move. Shallow/deep copy and pointer-field behavior remain unresolved, so copy/move are not yet implemented/complete.


## 2026-11-08 — Pointer/reference Semantic compatibility regression slice

- Added Semantic fixtures for exact pointer assignment/call compatibility and exact reference call compatibility.
- Negative fixtures cover pointer base mismatch, pointer depth mismatch, pointer/reference mismatch, and reference base mismatch.
- These tests exercise the existing exact type-name compatibility behavior; they do not introduce implicit pointer conversions. Hard-type marker handling remains governed by the existing compatibility contract.
- **CI pending:** test source was committed; no local CTest run is available in this session. Verify the new CI run before treating the slice as complete. Struct nominal pointer identity and SemanticResult/OIR recursive type validation remain separate follow-up checks.

## 2026-11-08 — AST contextual ownership and source-span closure slice

- AST graph validator state now records the incoming child-field edge alongside the active parent stack. Context-bound nodes are checked against both parent kind and owning field, not merely parent kind.
- Enforced legal edges/positions for `AST_NAMED_ARG` (`args` only), `AST_VARIADIC_PARAM` (`params`, terminal), `AST_MUST_DECL` (`AST_PROGRAM.declarations`), `AST_BLOCK_HALT` (final `statements` child), `AST_SELECT_ARM` (`AST_SELECT.arms`), `AST_SCAN_ARM` (`AST_SCAN.arms`, catch-all final), `AST_STRUCT_FIELD_INIT` (`AST_STRUCT_LITERAL.fields`), and `AST_ARRAY_DIMENSION` (`AST_ARRAY_TYPE.dimensions`).
- FASTEXEC raw ASM labels remain legal inside nested statement blocks under a FASTEXEC ancestor; labels outside FASTEXEC or outside a statement edge are rejected.
- Generic AST codec fixtures now wrap Select arm, Scan arm and Array dimension records in their legal owner nodes. Dedicated contextual codec tests continue to cover `must`, variadic parameters, block Halt and FASTEXEC ASM.
- Added a negative graph test proving a named argument cannot be attached to a call's `callee` edge. Parser regression now asserts the exact `b:2` named-argument source range (byte offsets and line/column endpoints).
- CI run `42465058674` on validator/context-fixture commit `fc66f81e` passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind (5/5). The follow-up run for the exact named-argument span assertion and documentation updates is pending at this entry's creation.
- **AST remains open.** Remaining closure work: complete syntax-production → parser function → AST shape → positive/negative test inventory; finish per-production source-span endpoint assertions (including nested/synthesized nodes); add deterministic AST factory/codec allocation-failure injection and partial-child cleanup tests; audit remaining contextual parent/edge negative cases. Do not mark AST complete or advance the pipeline gate yet.

- Added exact `AST_PROGRAM` full-source range/line-column assertion and verified that it survives AST codec round-trip; the minimal empty-function fixture also asserts nested function/block ranges in the follow-up test. The initial root-only run `42466136344` passed 5/5.
- Array type span regression now checks dynamic parameter `[]` and multidimensional local `[3][3]`, both the enclosing `AST_ARRAY_TYPE` range and each `AST_ARRAY_DIMENSION` range. CI run `42466136344` passed 5/5 with these source-range checks.

- The minimal program-root span fixture now also asserts the nested `AST_FUNC_DECL` and `AST_BLOCK` ranges. CI run `42466243975` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan and Valgrind).

- Function return array source-span checks now cover dynamic `[]`, fixed `[n]` and multidimensional `[m][n]` forms, including both enclosing `AST_ARRAY_TYPE` and each dimension node. CI run `42466307736` passed all five jobs.

- Nested Struct literal codec regression exposed two missed AST details: serializer payload cases for `AST_STRUCT_LITERAL` / `AST_STRUCT_FIELD_INIT`, and the deserializer's stale maximum-tag bound (`AST_ASM_LABEL`). Both are fixed; the dedicated Struct literal codec test's success/failure return convention was corrected so CTest now observes its result. Nested `Outer { inner = Inner { ... } }` parser→AST→serialize→deserialize→validate passes.
- CI run `42467148216` passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind (5/5) with the nested Struct codec and appended-tag reader fix.
- Static AST switch audit now reports 107/107 tags in factory sizing, serializer, deserializer, graph validator, destroyer and debug printer. The debug printer's missing `AST_VARIADIC_PARAM` and `AST_NAMED_ARG` renderers were added; its follow-up CI is pending.
- Added enum declaration coverage using the syntax-reference `enum:Task` prefix, with payload and no-payload variants, ordered fields, exact declaration/variant/payload source spans, and codec round-trip. Run `42467681177` passed 5/5.
- Added aligned Struct, packed Struct and Union parser fixtures; flag distinctions, field order, declaration ranges and codec round-trip passed all five jobs in run `42467758077`.
- Added canonical `nt:Addr = u64;` to the aggregate fixture to cover NewType target and declaration span. CI run `42467829886` passed all five jobs.\n- Enum AST regression now uses the exact syntax-reference form `nt:enum: Task`. It verifies payload/no-payload variants, ordered payload fields, declaration/variant/field source ranges and codec round-trip.
- Group AST validation now accepts anonymous implementation functions only through `AST_METHOD.implementation`; top-level/normally declared functions still require a name. The nested Group span test was corrected to end immediately after the nested closing brace.
- Generic populated and all-tag codec tests now traverse all 107 AST tags, placing context-bound nodes under their required owner edges (including Group methods, named args, variadic params, `must`, FASTEXEC ASM, Struct field init, Select/Scan arms and array dimensions).
- CI run `42469974956` passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind (5/5) with the all-107-tag fixture expansion.
- **AST remains open:** finish syntax-production → parser function → AST shape → positive/negative test inventory; close remaining exact source-span endpoints; extend negative owner-edge/position cases; finish parser-side allocation-failure and partial-child cleanup injection. No Codegen work is unblocked.
- The Struct literal exact-source-span parser fixture had been nested inside the OOM test's error branch and was therefore not executed on successful test runs. It is now an independent registered test and validates the Struct literal AST graph.
- Parser OOM injection now covers named arguments, Struct literal, Enum, Union and Group method/default syntax; its error path no longer risks destroying the same parser context twice.
- Added exact byte/column endpoint assertions for top-level `must` and named Thread Block source spans.
- Added negative Enum payload cases (missing field name, missing `!type`, trailing comma, malformed second field and missing type). They uncovered a use-after-free/double-free plus a pending payload-field leak. Enum payload ownership now transfers only after the complete list is parsed; early exits release the still-local list.
- AST graph validation now requires `AST_ENUM_VARIANT` to be a child of `AST_ENUM_DECL.variants`. All-tag codec fixtures wrap the variant in its legal owner; negative validator fixtures reject it as an unrelated block statement.
- CI run `42470807011` passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind (5/5) after the Enum ownership and parser cleanup fixes.
- **AST remains open:** complete the syntax-production inventory, broaden exact spans to remaining source forms, add remaining malformed owner-edge/position cases, and audit parser allocation failure at all partial-child boundaries. No Codegen work is unblocked.\n- Confirmed `nt:` means New Type and is the Nexus Flow `typedef`-equivalent declaration prefix; `nt:enum:` is the Enum declaration form within that family. No syntax change was made. The dedicated Enum fixture now mirrors the reference's newline-separated variant form.\n- CI run `42480076522` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind); Ubuntu CTest 60/60.\n- Parser matrix reconciliation: `test_intrinsic_ast_family()` asserts exact byte offsets and line/column endpoints for all 32 generic intrinsic AST tags; documentation now records the actual coverage.\n\n- Added nested-block exact-span regression: verifies outer and inner block byte offsets, start/end line-column coordinates, nested statement ownership, graph validity and codec round-trip. CI run `42480323113` passed all five jobs; Ubuntu CTest 60/60.\n\n- Added exact source-span checks for `Range(s,e)` constructor patterns, both bindings, and simple `item` binding in `scan`; select/scan roots and arm spans remain covered.\n- Callback type parser now accepts canonical `f:(params)!ReturnType`; legacy `f:(params)>!ReturnType` remains accepted and is normalized to canonical text. Tests cover both forms, callback parameter storage, invocation and malformed return rejection. The syntax reference and AST matrices were reconciled.\n- CI run `42480623594` passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind (5/5); Ubuntu CTest 60/60.\n- **Remaining AST architecture gap:** type signatures are still stored as strings in `type_name` / `target_type`; a unified recursive type-expression AST (for nested function/callback types and consistent pointer/reference/array composition) is not implemented. Do not mark AST complete until this is designed and tested.\n

## 10. EXF / Linkname AST notu (2026-11-08)

Syntax sözleşmesi `docs/language/syntax-reference.md` Bölüm 15'e eklendi.

- `!!=linkname = "...";` dış object/library bağlantı kaynağını belirtir.
- `exf:name(params)!return_type;` yalnızca bildirilen dış fonksiyon sembolünü Nexus Flow arayüzüne açar.
- Linkname, tüm kütüphane sembollerini otomatik açmaz; OCL linker yalnız EXF ile ilan edilen sembolleri çözümleme kapsamına alır.
- Bir linkname kaynağı birden fazla EXF bildirimiyle ilişkilendirilebilir.
- `ASTFuncDecl.linkname` alanı append-only payload alanı olarak eklendi; yalnız `AST_EXTERN_DECL` üzerinde geçerlidir. Normal `AST_FUNC_DECL` üzerinde dolu olması Graph Validator tarafından reddedilir.
- Program kapsamındaki `!!=linkname = "...";` directive'i sonraki EXF bildirimlerine uygulanır; yeni directive önceki aktif kaynağı değiştirir. Aynı kaynak birden fazla EXF'ye kopyalanarak ilişkilendirilebilir; directive ayrı bir executable AST statement üretmez.
- AST stream sürümü 3 linkname alanını taşır; sürüm 1 ve 2 okumaları geriye dönük korunur.
- OIR AST frontend writer/reader optional field id 9 ile linkname'i modüller arası handoff'ta korur; EXF OIR AST round-trip regresyonu eklendi.
- ABI/calling-convention, object-format çözümleme ve gerçek OCL linker işi ileriki aşamadır.

**Durum:** EXF semicolon bildirimi, top-level linkname parser'ı, AST ownership/validation, AST stream v3 ve OIR AST frontend handoff uygulandı. CI run `42486110131` Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 geçti; Ubuntu CTest 60/60. Ardından v1/v2 codec geriye dönük uyumluluk testleri eklendi; CI run `42486189216` Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 geçti; Ubuntu CTest 60/60. Linker/ABI çözümlemesi kapsam dışıdır.


## 11. Recursive AST Type Expression — design baseline (2026-11-08)

- Added `AST_TYPE_EXPR_DESIGN.md` as a draft design/implementation contract (commit `11ee81a8d4b572537b22befbe929f9202c3903e1`).
- Current C AST already has `AST_ARRAY_TYPE` / `AST_ARRAY_DIMENSION`, but declarations and callback/function signatures still store base types in text fields (`ASTVarDecl.type_name`, `ASTFuncDecl.return_type`, `ASTNewTypeDecl.target_type`).
- Parser `!!=linkname = "...";` directive'ini yalnızca program kapsamında kabul eder; sonraki EXF bildirimleri aktif kaynağın kendi kopyasını `ASTFuncDecl.linkname` alanında taşır. Tek kaynak birden fazla EXF bildirimiyle ilişkilendirilebilir.
- Design contract: append-only node IDs; legacy strings retained during migration; recursive named/pointer/reference/array/function type forms; exact spans; all codec/validator/destroy/printer/schema paths and tests must be updated together.
- **Status:** implementation phase 3. Append-only `AST_TYPE_EXPR` + six kinds, ownership/destruction/validation/printer, AST stream v4, declaration attachment slots (`ASTVarDecl`, `ASTFuncDecl`, `ASTNewTypeDecl`) and OIR kind 108 writer/reader/schema/child traversal are implemented. Legacy AST stream v1–v3 read paths remain supported. Parser populates named/pointer/reference/hard-qualified/array/function types; callback parameters support fixed, multidimensional and dynamic arrays, and callback return arrays are represented as qualified array return types. Legacy text fields remain for compatibility. Array dimension literals carry both numeric and string payloads required by AST validation. Invalid recursive newtype type construction produces a syntax diagnostic. Variadic markers are valid in `AST_TYPE_FUNCTION.params` and remain final-only. Parser AST codec tests cover callback arrays and return arrays; malformed dimensions and round-trip cases are included. CI run `42695649641` passed Windows, macOS, Ubuntu, ASan/UBSan and Valgrind (5/5 jobs); Ubuntu CTest 60/60. Cleanup-only test commit `b5e03f2` also passed the full CI matrix (5/5). Historical status snapshot: exact per-parameter source spans and nested-type allocation-failure injection were subsequently implemented and CI-verified in §13. Semantic callback compatibility remains open; AST_TYPE_EXPR still has downstream Semantic/OIR work.


## 12. EXF semicolon declaration parser support (2026-11-08)

- Parser now accepts bodyless external declarations in the documented form `exf:add(a!i32, b!i32)!i32;`. The AST remains `AST_EXTERN_DECL` / `ASTFuncDecl`; the declaration has no synthetic function body.
- Legacy EXF body form remains accepted for compatibility. `linkname` directive parsing and AST association are implemented separately; OCL linking and ABI/object-format validation remain out of scope.
- Added parser regression coverage for symbol name, hard-typed parameter list, hard-typed return type, absent body, exact declaration source span and `ast_validate()`.
- Parser implementation commit: `c701064f5c2abd5bf261762535264432e61c0439`.
- Test commit: `6f42147f7dc154663061aa026f55937b18923ab1`.
- CI run `42485259430` was triggered for Windows, macOS, Ubuntu, ASan/UBSan and Valgrind; results pending at the time of this note.


## 2026-11-08 — Recursive Type Expression: callback syntax + array ownership integration

- `docs/language/syntax-reference.md` callback bölümü tamamlandı: NewType alias, doğrudan callback parametresi, iç içe callback imzası ve lambda gövdesi ayrımı örneklerle tanımlandı. Kanonik imza `f:(params)!ReturnType`; `>` yalnızca callable/lambda gövdesini başlatır. Eski `f:(params)>!ReturnType` biçimi Parser uyumluluğu olarak korunur.
- Parser testine doğrudan iç içe callback parametre türü eklendi: `f:(test!f:(value!i32)!bool)!bool`.
- Normal değişken/parametre array declarator'ları ve fonksiyon array return'leri artık `AST_TYPE_ARRAY` wrapper üretir; `child` eleman türünü, `dimensions` ise `AST_ARRAY_TYPE` boyut alt ağacını taşır.
- Tek sahiplik korunur: `ASTVarDecl.inline_type` ve `ASTFuncDecl.return_array_type` yalnızca recursive array node'a borrowed alias olur. AST destroy/validator/codec ve OIR index/writer/reader duplicate ownership edge üretmeyecek, decode sonrasında alias'ı yeniden kuracak şekilde güncellendi.
- Parser AST codec testleri ve OIR round-trip testleri array wrapper/alias kimliğini doğrulayacak şekilde genişletildi.
- Tasarım kaydı güncellendi: `AST_TYPE_EXPR_DESIGN.md`.
- **Doğrulama durumu:** Değişiklikler `refactor/c-core-roadmap` dalında commitlendi; bu oturumda yerel C derlemesi/CTest çalıştırılamadı ve bu commit grubu için doğrulanmış yeni CI sonucu henüz yok. CI tamamlanmadan bu entegrasyon tamamlanmış sayılmayacak.
- **Açık kalanlar:** callback signature içindeki array parametre/return bileşimi, hassas parametre bazlı source span, nested malformed-type ve allocation-failure testleri. AST kapanışı ve Semantic geçişi hâlâ kilitli.


- **CI doğrulaması:** Workflow `42497533312` tüm 5 işi başarıyla tamamladı (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Ubuntu CTest: 60/60. İlk iki CI denemesinde ortaya çıkan zorunlu OIR field-slot ve hard-qualified array test beklentisi hataları düzeltildi; başarılı run bu düzeltmelerin tamamını içeriyor.


## 12. Callback / function type takip notu (2026-11-09)

- `AST_TYPE_FUNCTION` modelinin parser üretimi mevcut; function-type alias, legacy `>` dönüş yazımı, nested callback parametresi ve fonksiyon parametresinde callback AST'si `test_function_type_alias_and_callback_parameter()` tarafından kontrol ediliyor. AST codec round-trip ve `ast_validate()` de bu testte yer alıyor.
- `docs/language/syntax-reference.md` §16'ya callback alias, doğrudan callback parametresi, callback çağrısı, nested signature ve array hedef yazımları örnekleri eklendi.
- **Uygulandı ve doğrulandı:** `parser_build_type_expr()` recursive type spelling içindeki array suffix'ini `AST_TYPE_ARRAY` + `ASTArrayType` + ordered `ASTArrayDimension` olarak kuruyor. Numeric fixed dimensions AST_LITERAL'da `int_value` ve NUL-terminated `string_value` taşır; `[]` NULL length ile korunur. Function type parametre/return builder'ındaki array kısıtı kaldırıldı. Testler `BufferCallback[16]`, `MatrixCallback[4][4]`, dynamic `[]`, `!u8[8]` return array, codec round-trip ve malformed dimensions (nonnumeric, missing bracket, int64 overflow) kapsamını içeriyor. AST graph validator variadic callback parametresinin function type `params` altında final olmasını da doğrular. CI run `42695649641` beş işi de geçti.
- **Sınır:** Bu imza codec'i numeric ve boş dimension ile sınırlı; arbitrary expression dimensions, semantic callback compatibility ve ABI doğrulaması tamamlanmış değil. GitHub CI sonucu henüz alınamadı; test geçişi onaylanmış sayılmaz.
- ABI/calling convention ve callback compatibility Semantic/backend sınırında; parser bunları doğruluyor kabul edilmez.


### Callback array type follow-up (2026-11-09)
- Added parser test for callback function return arrays (`!u8[8]`) and AST serialize/deserialize preservation.
- Added rejection tests for nonnumeric dimensions, missing closing bracket, and signed-64-bit dimension overflow.
- Callback parameter fixed/multidimensional/dynamic array cases remain covered in the existing callback AST test.
- GitHub CI verification for commit `b86f922` is pending; local build is unavailable in this GitHub-only workspace. Do not mark these tests passed until CI reports success.

## 13. Recursive callback array AST fixes and verification (2026-11-09)

- parse_newtype_decl() artık function type alias metninde '[' bulunduğu için target_type_expr üretimini atlamaz. Fixed, multidimensional ve dynamic array suffix'leri callback signature AST ağacına bağlanır.
- AST_VARIADIC_PARAM validator sözleşmesi, normal/extern fonksiyonlara ek olarak AST_TYPE_FUNCTION.params bağlamını da kapsar; final-only kuralı korunur.
- AST_TYPE_ARRAY dimension literal'ları zorunlu string_value alanını ve numeric int_value alanını birlikte taşır. NUL terminator ve allocation sınırları ASan ile doğrulandı.
- Recursive newtype type construction başarısız olduğunda Parser açık syntax diagnostic üretir; malformed callback dimensions sessizce kabul edilmez.
- Test kapsamı: callback alias, direct callback parameter, nested callback, fixed/multidimensional/dynamic callback parameter arrays, callback return array, AST serialize/deserialize, invalid dimension rejection.
- CI run `42696347944` / commit `ac88929c624489290064ad2da4c2e9a14409ac4f`: Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 başarılı; Ubuntu CTest 60/60. Recursive callback array construction, variadic validator ve NUL terminator düzeltmeleri doğrulandı.
- Callback recursive type builder'ın nested allocation noktaları mevcut deterministik parser OOM injection fixture'ına eklendi. CI run `42697397038` / commit `74a1ecec536f62db722d82c5a7d81b34c3fe63b5`: Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 başarılı; Ubuntu CTest 60/60.
- Function-type callback parametreleri için normalize edilmiş signature karakterleri özgün kaynak byte aralıklarına eşleniyor. `AST_VAR_DECL` span'i yalnızca kendi `name!type` / `name:type` segmentini kapsıyor; nested `type_expr` ve dönüş tipi de kendi aralıklarıyla kuruluyor. Satır/sütun konumu kaynak başlangıcından yeniden hesaplanıyor.
- Tek satırlı ve çok satırlı callback parametreleri için kesin offset + line/column testleri eklendi. CI run `42698879629` / commit `80c9b9f3c1110c95c5aaf98fad69336a43b13c5b`: Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 başarılı; Ubuntu CTest 60/60. Geçici tanılama çıktıları temizlendi; cleanup commitleri `c8eccf6` ve `011f0bb` için CI devam ediyor.
- Parser allocation-failure injection fixture'ı nested callback + nested fixed array türlerini de kapsıyor. CI run `42699233464` / commit `7dba16e177817262343b8148c834c4b60647e94b`: Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind 5/5 başarılı; Ubuntu CTest 60/60.
- AST tarafında callback signature parse/build, recursive type payload, exact parameter source spans, codec round-trip ve nested allocation-failure cleanup testleri mevcut. Semantic callback compatibility / function value call-compatibility henüz Semantic aşamasında tamamlanmadı; ABI/calling convention doğrulaması da açık.
- Açık kalan callback işi Semantic katmanındadır: callback signature uyumluluğu, function value çağrı uyumluluğu ve ABI/calling convention kuralları. Bunlar Semantic tamamlanmadan Codegen'e taşınmaz.

## 2026-11-10 — Callback AST verification and matrix reconciliation

- Confirmed callback recursive-type implementation and source-span/OOM regression coverage from commit `7dba16e177817262343b8148c834c4b60647e94b`.
- GitHub Actions run `42699233464`: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind all passed (5/5); Ubuntu CTest 60/60.
- Corrected `docs/AST_PARSER_PRODUCTION_MATRIX.md`: `AST_TYPE_EXPR` ID 108 and OIR/codec v4 are now reflected; the stale “no dedicated AST type node” claim is removed.
- Callback parser/AST work is verified. Remaining callback gaps are Semantic signature/function-value compatibility and ABI/calling convention—not parser syntax or Codegen.
- Next AST task: continue the source-production inventory and exact-span / malformed owner-edge / parser partial-child allocation audit. Codegen remains blocked until AST and Semantic/OIR/Verifier gates are complete.


## 2026-11-10 — Parser gap: USE path validation and source spans

- Added a focused Parser regression for dotted `use` path exact spans (`alpha.beta.gamma as mod`) and malformed trailing-dot, doubled-dot, missing alias and missing-semicolon forms.
- This was a test-coverage gap; `parse_use()` already rejects these forms, so no unnecessary grammar change was made.
- Test commits: `5aa64c44` (initial), `dee01b4` (fixture correction); matrix update `2264d21`.
- CI for corrected test commit is pending. This item remains unverified until Windows/Ubuntu/macOS/ASan-UBSan/Valgrind pass.
- Next Parser gaps from the inventory: comma-list `use` span/ownership + OOM; then source-span and partial-child allocation audits by production family.

- Added comma-list `use alpha as a, beta.gamma as b;` AST sibling/alias and per-node exact-span regression. Corrected expected token boundaries in `e8c5bfd`; matrix record commit `f3b6b89`. CI remains pending.


## 2026-11-10 — USE namespace prefix suppression
- Syntax reference `omnicore-sdk/nfsyntax.md` now defines `use module.path as _;` as the explicit prefix-suppression/import-into-current-scope form, with a nested `group db.mysql.sql` example and normal alias contrast.
- Added `test_use_namespace_prefix_suppression_alias()` to verify Parser accepts `as _`, retains the complete dotted module path and underscore alias in AST, and passes `ast_validate()`.
- This is a Parser/AST syntax acceptance test only. Semantic module-member injection, visibility and duplicate-name diagnostics remain to be implemented/verified; no claim of runtime/import resolution is made.
- CI for the latest parser commits is pending; do not mark this test green until the branch workflow runs.


## 2026-11-10 — USE alias and namespace selector

- Normative USE options are now distinct and ordered: optional `as alias`, then optional `ns namespace`.
- `as` assigns the imported item a local alias. `ns` selects the namespace root for the imported Group; `ns _` explicitly requests unqualified/current-scope exposure. `as _` is no longer documented as namespace suppression and remains an ordinary alias spelling.
- Appended `ASTUse.namespace_name`; Parser keeps module path, alias and namespace in separate owned strings. The contextual `ns` spelling is recognized only in USE declarations and is not globally reserved.
- AST stream format advanced from v4 to v5 to serialize the namespace payload. Deserializer continues accepting v1-v4 streams unchanged.
- Added Parser/AST regression coverage for combined alias+namespace, namespace-only, ordinary alias `_`, unqualified `ns _`, comma-separated imports, malformed selectors, per-item spans, graph validation and v5 codec round-trip.
- Updated `omnicore-sdk/nfsyntax.md` and `docs/AST_PARSER_PRODUCTION_MATRIX.md`.
- Semantic Group/member import resolution is not claimed complete. OIR frontend payload mapping for the appended namespace field, USE allocation-failure injection, and end-to-end resolver tests remain open. CI must validate the commits before this Parser/AST addition is marked verified.

- Follow-up OIR bridge parity: frontend writer emits optional AST_USE field 3, reader restores/frees `namespace_name`, and field schema accepts the optional string while preserving compatibility with older payloads that contain only fields 1–2. Added `test_use_namespace_oir_roundtrip()` for Parser → OIR AST payload → AST → canonical payload byte equality. CI remains unverified.
- AST stream regression updated to assert newly written v5 headers and successful reading of a legacy v4 stream. The v4 fixture uses an AST kind whose payload layout is unchanged; old AST_USE nodes remain decoded without a namespace value.

- CI run `42769028209` / commit `189cb9c` exposed one failing existing USE path span assertion in the Parser CTest (59/60 other tests passed in the ASan/UBSan run). The fixture expected end offset 23 for `use alpha.beta.gamma as mod;`, but the parser correctly spans through the alias token: exclusive byte end 27, end column 28. Corrected the test expectation in `c6155d4`; rerun is pending. This was a test expectation correction, not a parser grammar change.

- Follow-up CI root cause: `test_use_comma_list_spans` had a double-escaped newline in its C fixture (`\\\\n` in source), so the lexer correctly reported two trailing invalid characters after the semicolon. Restored the fixture's normal C newline escape in commit `abb7cd2`; CI run `42770341740` is queued. The parser grammar and span calculation were not changed for this failure.
- Added the normative `USE` import contract to `nexus docs/Group Metodu — Formal Specification.md` §8.5.1. It separates Parser payload production from Semantic symbol/visibility validation and Module Resolver dependency loading, namespace construction and dependency-cycle checks. This explicitly preserves the architecture boundary; no fake local-only module resolution is claimed.
- CI run `42770341740` for commit `abb7cd2`: Ubuntu, macOS, Windows and ASan/UBSan jobs passed; Valgrind was still running at the time of this update.
- CI run `42770341740` completed successfully on commit `abb7cd2`: Windows, macOS, Ubuntu, ASan/UBSan and Valgrind all passed. This verifies the USE Parser/AST/OIR regression suite and current repository test matrix for that commit. The later documentation/progress-only commits are not represented by that exact run.


## 2026-11-10 — Module Resolver boundary and architecture contract

- Inspected the current Semantic implementation and stage boundary. `AST_USE` is still a no-op in `semantic_check()`; the Semantic DLL currently accepts exactly one AST artifact and has no project/module context. Imported-symbol resolution is therefore not implemented and must not be represented as completed.
- Added `docs/MODULE_RESOLVER_DESIGN.md` as a draft architecture contract. It separates portable module identity/dependency-graph logic from host-owned source lookup, keeps Parser free of filesystem access, and keeps imported-symbol/type/visibility checks in Semantic.
- Defined the intended host source-provider boundary: project-root/search-path policy and platform file I/O remain host responsibilities; the resolver receives canonical identities and bounded source data. Exact manifest/search-path and extension policy are intentionally open pending review of project configuration and existing `m:` required-dependency semantics.
- Defined dependency graph states, canonical-identity cache reuse, alias/namespace independence from cache keys, deterministic initial cycle rejection, exact AST_USE source-span diagnostics, and the future validated OIR imported-interface handoff.
- No resolver C API, dependency artifact, or Semantic import behavior is claimed implemented yet. Next implementation gate: inspect project configuration and `m:` semantics, then specify the portable C resolver core API and its ownership/failure contract before coding the graph.
- Codegen remains blocked; no downstream stage was advanced by this architecture work.

- Follow-up repository audit: `m:Name = source;` is currently parsed into `AST_MUST_DECL{name, source}`, but `semantic.c` has no `AST_MUST_DECL` handler; checked-in `occ.ocf` dependency sections are empty. It is therefore a Parser-only construct today, not an existing dependency resolver contract and not interchangeable with `use`. Clarified this explicitly in `docs/MODULE_RESOLVER_DESIGN.md`; its future semantics must be specified separately.


## 2026-11-10 — Initial Module Resolver C graph core

- Added `omnicore-sdk/src/c/module/module_resolver.h/.c` and independent CMake target `occ_module_resolver`. The core is portable C11 and has no filesystem or Parser-internal dependency.
- Implemented canonical module identity registration with stable per-graph IDs, same-identity/fingerprint cache reuse, and rejection of inconsistent fingerprints within one compilation job.
- Implemented owned dependency edges preserving original dotted USE path, importer/dependency IDs, AST_USE node ID, and full source span. Added constrained DISCOVERED → RESOLVING → RESOLVED/FAILED transitions; an edge to a RESOLVING module is retained and reports CYCLE for exact USE-site diagnostics.
- Added configurable module/edge/text limits and rejection of failed dependencies. Borrowed graph views are valid until mutation/destruction.
- Added `test_module_resolver.c` for cache identity, fingerprint mismatch, edge/span ownership, duplicate AST_USE edge, cycle detection, state transitions, failed dependency handling and configured limits.
- **Verification pending:** GitHub Actions must compile and run the new C target on Windows, macOS, Ubuntu, ASan/UBSan and Valgrind. No local build was available in this GitHub-only workspace.
- **Not complete:** host-owned source provider, recursive dependency scheduling through lexer/parser DLLs, dependency/interface OIR artifact + independent validation, and Semantic import binding/visibility checks. The Semantic `AST_USE` branch remains a no-op until that validated handoff exists; no imported symbol support is claimed.
- Updated `docs/MODULE_RESOLVER_DESIGN.md` and the USE row in `docs/AST_PARSER_PRODUCTION_MATRIX.md`. Codegen remains blocked.


## 2026-11-10 — Correction: `m:` is an immutable value, not a dependency

The earlier 2026-11-04/2026-11-10 notes describing `m:` as a compile-time dependency or required source dependency were based on an incorrect interpretation and are superseded by this entry. The language contract clarified by the project owner is:

- `m:name = expression;` declares a normal readable value whose binding cannot be reassigned or updated after initialization.
- The initializer is an expression; its type is inferred by Semantic. Examples: `m:a = 5;`, `m:pi = 3.141592653589793;`.
- `m:` does not check file existence, load source, or create module dependency edges. Source/module loading is performed by `use`.
- Parser/AST now accepts `m:` at program and block scope; AST kind/payload name `AST_MUST_DECL` remains unchanged for append-only compatibility.
- Semantic blocks reassignment/update through the existing immutable-lvalue checks, but records `m:` with a new `IMMUTABLE` symbol flag rather than `CONST`. `c:` retains the separate `CONST` flag for compile-time constants. Parser and Semantic tests cover program/block declarations, reading, assignment, increment and duplicate declarations.
- Updated `omnicore-sdk/nfsyntax.md`, `docs/AST_PARSER_PRODUCTION_MATRIX.md`, `OIR_AST_NODE_REGISTRY.md`, and `docs/MODULE_RESOLVER_DESIGN.md` to reflect the corrected contract.
- OIR already has stable node kind 103 with name + initializer-expression fields; its field schema/round-trip remains part of the validation gate. The legacy AST struct member `source` is the initializer payload, not a file path.
- CI verification for these code changes is pending. The Module Resolver remains exclusively for `use`; the previous resolver document's erroneous association of `m:` with source-provider work is superseded.

- Current-summary correction: `AST_MUST_DECL` is the compatibility-preserved AST kind for immutable `m:` values; it is not a module dependency. The AST graph validator accepts its initializer-bearing node in program and ordinary block statement contexts. Earlier historical notes describing it as program-only or as a dependency are superseded by the 2026-11-10 correction section above.

- CI verification for the corrected `m:` implementation and parser→OIR round-trip: run `42774871338` on code commit `1c4b2013e6e0924951ebf0bc4f31eb5eef8a9615` passed all five jobs — Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Ubuntu CTest: 61/61 passed. Later changes in this correction series after that code commit were documentation-only.


## 2026-11-10 — Module Resolver host source-provider boundary

- Reviewed the active OCF files: `omnicore-sdk/occ.ocf` and `omnicore-sdk/src/c/occ.ocf` currently declare `[include] ./lib` and empty `[dependencies]` sections. No C OCF project-config loader was found in the active source tree. OCF parsing and search-policy construction therefore remain host/project-config responsibilities; the portable resolver will not parse OCF or open paths.
- Added `OccModuleSourceProvider` callback API to `module_resolver.h`: resolve receives dotted USE path, optional importer identity and exact source span; success returns canonical identity and borrowed source bytes; release ends the borrow. Provider statuses distinguish not-found, denied, ambiguous, I/O and provider errors.
- Implemented provider status mapping, provider-response validation, exact release behavior, independent text/source budgets (source default 64 MiB), and deterministic 64-bit FNV-1a content fingerprint for cache consistency. Fingerprint is explicitly non-cryptographic and is not a trust/security primitive.
- Expanded `test_module_resolver.c` to cover fake-provider success, identity/fingerprint registration, exactly-once release, not-found/denied/ambiguous/I/O mapping, malformed success cleanup and source-size limit cleanup.
- Updated `docs/MODULE_RESOLVER_DESIGN.md` with the callback ownership contract and OCF host-policy boundary. No real filesystem/package provider, OCF parser, recursive Lexer/Parser scheduler, dependency OIR interface or Semantic import binding is claimed implemented.
- CI for this provider API/test addition is pending. Codegen remains blocked.

- Architecture boundary: `occ_module_resolver` is currently a portable static C core library for graph/provider contracts, not yet a pipeline DLL. The resolver-stage DLL wrapper and its standard input/output artifact ABI are deliberately deferred until the versioned dependency/interface artifact and host scheduler contract are specified. This preserves the project's module-DLL pipeline rather than pretending the helper library is already an integrated stage.


## 2026-11-10 — Separate `m:` immutable and `c:` compile-time constant symbol contracts

- Reconciled the normative `docs/language/syntax-reference.md` with the SDK syntax reference: `c:` denotes compile-time constants; `m:` denotes a normal value whose binding cannot be reassigned. `m:` is not a compile-time constant and does not load a file/module.
- Added `SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE` (bit 6). `CONST` (bit 0) remains reserved for `c:`; the flags are mutually exclusive and IMMUTABLE is valid only for variable symbols. Semantic still uses its internal const-lvalue protection for both, but the exported symbol metadata distinguishes their language meaning.
- Added corresponding `OIR_SEMANTIC_SYMBOL_FLAG_IMMUTABLE`, bumped SEMANTIC_AST payload schema minor from 1.3 to 1.4, added old-schema rejection for the new flag, and allowed IMMUTABLE variable declarations to reference AST node kind 103 (`AST_MUST_DECL`). Existing 1.0–1.3 artifacts remain readable under their previous constraints.
- Added an integration test that compiles `c:CompileValue = 5; m:RuntimeValue = 6;`, verifies distinct SemanticResult flags, serializes SEMANTIC_AST schema 1.4, reopens it through the full artifact validator, and verifies the wire flags remain distinct.
- Updated the normative and SDK syntax references, Group spec, OIR Semantic AST schema doc, AST matrix, and Module Resolver design. Compile-time evaluation/folding for `c:` remains a separate Semantic capability and is not claimed completed by adding the flag distinction.
- **CI pending** for the schema 1.4 and new integration test changes. The earlier 1.3-era `m:` implementation CI success does not cover this later schema/flag refinement.

- CI verification for the `m:`/ `c:` flag separation and SEMANTIC_AST schema 1.4: run `42776942902` on code commit `b1362d5c95fbcd4a4b5f320484d0faadf7b268cd` passed all five jobs — Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Ubuntu CTest: 61/61 passed, including the SemanticResult/OIR schema 1.4 integration and rejection of an IMMUTABLE flag under legacy schema 1.3.
- Module Resolver source-provider callback tests (including the fail-closed test harness correction) passed all five CI jobs in run `42775933125`.

- OCF follow-up audit: all four checked-in `occ.ocf` examples use `[include] ./lib` and empty `[dependencies]`; the root example also has package metadata, flags, default macros and default libraries. C/OCE comments propose automatic `coremodule` loading and `no:coremodule;` opt-out, but the normative syntax reference explicitly says `no:CoreModule` is not recognized by the current Lexer/Parser. This remains an unresolved config/default-module proposal, not active syntax. Current Parser `use` accepts dotted identifiers, not quoted filename strings; corrected the SDK example to `use CoreModule as core;`. Recorded these boundaries in `docs/MODULE_RESOLVER_DESIGN.md`.


## 2026-11-10 — `coremodule` / `osmodule` ayrımı ve `no:` Parser/AST desteği

- Dil modülü adları netleştirildi: eski OS bağımlı `coremodule` rolü `osmodule` olarak adlandırılır; eski OS bağımsız `core` rolü `coremodule` olur.
- `osmodule`, file/print/echo/input gibi OS servislerine bağlı standart fonksiyonları taşır ve normalde otomatik yüklenir. `coremodule` OS bağımsız genel fonksiyon kütüphanesidir.
- `no:` genel modül kapatma mekanizması değildir; yalnızca program kapsamındaki `no: osmodule;` otomatik osmodule yüklemesini kapatır. `no: coremodule;`, tekrar bildirim, eksik noktalı virgül ve iç içe kapsam kullanımı reddedilir.
- `ASTProgram` payload'ına append-only alanlar eklendi: `disable_osmodule` ve `disable_osmodule_span`. Yeni AST kind/ID eklenmedi.
- AST factory size, printer, destructor, graph validator ve stream codec güncellendi. AST stream v6 yazılır; v1-v5 okuma yolları korunur. Directive span alanları ABI bağımlı ham struct olarak değil, tek tek wire alanları olarak yazılır.
- Parser testleri eklendi: başarılı directive, tam byte/line/column span, v6 serialize/deserialize round-trip, geçersiz hedef, duplicate, nested-scope ve eksik semicolon.
- SDK syntax reference, normatif language syntax reference, AST/Parser üretim matrisi ve Module Resolver tasarım belgesi güncellendi. OCF örneklerindeki eski `no:coremodule;` yorumları `no: osmodule;` olarak düzeltildi.
- **Doğrulama:** GitHub Actions run `42788198833` commit `7349e78579cc75f43f3c14b5498234643af841ae` üzerinde beş işin tamamında başarılı: Ubuntu, macOS, Windows, ASan/UBSan ve Valgrind. Ubuntu CTest 61/61 geçti. Bu run no-directive Parser/AST testini, AST v5 Program uyumluluk testini ve implicit osmodule edge regression testini kapsar.
- **Entegrasyon açığı:** Host scheduler henüz ASTProgram bayrağını okuyup implicit `osmodule` bağımlılığını ekleme/atlama işlemini yapmıyor. Resolver graph API şu an explicit `AST_USE` edge modelinde; implicit dependency edge sözleşmesi ve host entegrasyonu sonraki adım.
- `coremodule` için otomatik mi yoksa açık `use` ile mi yükleneceği kararı açık bırakıldı.


### 2026-11-10 — Standard library source rename follow-up

- Önceki standart modül taslağının yeri yanlıştı; kullanıcı kararıyla kanonik önceden tanımlı modül dizini `omnicore-sdk/occ/libs/` olarak düzeltildi.
- `occ/libs/osmodule.nxf` ve `occ/libs/coremodule.nxf` başlangıç dosyaları oluşturuldu. İkisi de bilinçli olarak yalnızca açıklama başlığı içeriyor; gerçek API/uygulama daha sonra yazılacak.
- Eski karışık `omnicore-sdk/stdlib/osmodule.exf` taslağı `temp/legacy-stdlib/osmodule.exf` konumuna arşivlendi ve aktif `stdlib/` yolundan kaldırıldı.
- Her iki `occ.ocf` kopyasındaki include yolu `./occ/libs` olarak değiştirildi.
- `.nxf` standart Nexus Flow kaynak modülüdür. `.exf` harici fonksiyon/nesne dosyası bağlantı tanımları için ayrılmıştır; standart modülün ana kaynak dosyası olarak kullanılmayacaktır.
- `osmodule` varsayılan otomatik yükleme kapsamındadır; `no: osmodule;` bunu kapatır. `coremodule` için otomatik yükleme politikası henüz karara bağlanmadı.


### CI doğrulaması — `no: osmodule;` AST/Parser değişikliği

- Commit `176527fd9d4c33d70abad19fcc4eb3622d4c778e` için GitHub Actions run `42787304556` beş işin tamamında başarılı oldu: Ubuntu, macOS, Windows, ASan/UBSan ve Valgrind.
- Ubuntu CTest: **61/61 geçti**; Parser regression testi `parser_accepts_and_rejects_sources` dahil.
- Bu doğrulama ASTProgram alanı, AST codec v6, Parser bildirimi ve eklenen no-directive regresyon testlerini kapsar.
- Sonraki commit'ler yalnızca stdlib dosya adı ve dokümantasyon güncellemeleri içerir; bunların push CI çalışmaları ayrıca izleniyor.


### 2026-11-10 — Implicit `osmodule` dependency edge

- Module Resolver graph API artık explicit `AST_USE` ve implicit `osmodule` bağımlılıklarını ayrı edge türleriyle temsil ediyor: `OCC_MODULE_EDGE_EXPLICIT_USE` / `OCC_MODULE_EDGE_IMPLICIT_OSMODULE`.
- `occ_module_resolver_add_implicit_osmodule()`, AST_USE node ID'si olmayan standart bağımlılığı `ast_use_node_id = 0` ve program root source span'i ile kaydeder. Ownership, text/edge limitleri, module state ve cycle kontrolleri explicit edge yolu ile ortaktır.
- Resolver testine implicit edge span/kind, duplicate rejection ve aynı modüle explicit USE edge eklenebilmesi regresyonu eklendi.
- **Sınır:** Bu API tek başına otomatik yüklemeyi başlatmaz. Host scheduler'ın ASTProgram.disable_osmodule alanını okuyup provider ile `osmodule`'ı çözmesi ve bu API'yi çağırması henüz entegre edilmedi.
- **CI doğrulandı:** Run `42788198833` tüm beş işte başarılı; Ubuntu CTest 61/61. Yeni implicit edge regression testi `module_resolver_dependency_graph_identity_and_cycles` altında geçti.


### Son doğrulama özeti — 2026-11-10

- Çalışma dalı: `refactor/c-core-roadmap`; `main` dalına müdahale edilmedi.
- CI run: [42788198833](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42788198833) — Ubuntu, macOS, Windows, ASan/UBSan ve Valgrind **5/5 başarılı**.
- Ubuntu CTest: **61/61 başarılı**.
- Doğrulanan yeni kapsam: `no: osmodule;` program kapsamı ve exact span, AST v6 encode/decode, v1-v5 uyumluluk (özellikle v5 AST_PROGRAM payload), implicit osmodule resolver edge, explicit USE edge ayrımı.
- Açık kalan: host scheduler ASTProgram bayrağını okuyup `osmodule` provider çözümlemesini/otomatik bağımlılık eklemeyi henüz yapmıyor. `coremodule.nxf` ve `osmodule.nxf` başlangıç iskeletleri `occ/libs/` altında mevcut; içerik ve API tasarımı henüz yazılmadı.


### 2026-11-10 — Canonical standard library location

- Canonical path: `omnicore-sdk/occ/libs/`.
- Added `osmodule.nxf` and `coremodule.nxf` as comment-only starter files. No unverified functions or API declarations were carried into the new sources.
- Moved the previous mixed `.exf` draft out of the active lookup path to `temp/legacy-stdlib/osmodule.exf` for later reference.
- Updated package configuration paths: source-root `omnicore-sdk/occ.ocf` uses `./occ/libs`; runtime `src/c/occ.ocf` uses `../occ/libs` because the executable is `bin/occ`. Updated SDK README and standard-library/OS bridge documentation.
- `.nxf` is the standard module source format; `.exf` is reserved for external function/object linkage declarations.
- No compiler C source or parser semantics changed in this layout correction. The existing CI result does not constitute a runtime module-search test; package include resolution still needs a dedicated test when the host loader is integrated.


- **CMake staging adjustment:** the first CI run exposed a path collision: the runtime executable is `bin/occ`, so creating `bin/occ/` for libraries is invalid. The canonical source remains `omnicore-sdk/occ/libs/`; CMake now stages that directory at `${CMAKE_BINARY_DIR}/occ/libs/`, and the runtime config resolves it via `../occ/libs`. The staging test checks this sibling layout.


### CI verification — canonical `occ/libs` staging

- GitHub Actions run [42791692984](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42791692984) passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Ubuntu CTest: **62/62 passed**. The new `occ_predefined_standard_libraries_staged_for_runtime` test confirms that `occ.ocf`, `occ/libs/osmodule.nxf` and `occ/libs/coremodule.nxf` are staged into the expected build layout and that the runtime include path matches.
- Valgrind CTest: **62/62 passed**.
- This validates packaging/staging only. It does not claim that the C host has started resolving/importing Nexus Flow modules; the host provider and recursive module scheduler remain the next integration task.


- **Host integration inspection:** `main.c` currently resolves `occ.ocf` and calls `get_config_language()`, but does not parse `[include]` or `[dependencies]`. Therefore the new include paths and CMake staging are packaging-ready, not yet active module resolution. Next implementation unit is a bounded C OCF include/dependency reader plus a filesystem source-provider adapter; only then should recursive module scheduling consume the existing Resolver API.


### 2026-11-10 — OCC kaynak / dağıtım dosya sistemi ayrımı

- Kullanıcı kararı: repository içindeki SDK adı `omnicore-sdk` olarak korunur; kaynak kodu, specs, belgeler ve build/test altyapısı burada kalır.
- Dağıtım kökü, CMake binary directory ile aynı ağaçtır. Yayınlama aşamasında `build/` dizini `occ/` adıyla paketlenebilir; derleme sırasında ayrıca yeniden klasör taşıma gerekmez.
- Paket sözleşmesi: `bin/` içinde `occ`, `ocm` ve stage DLL'leri; kökte `occ.ocf`, `libs/`, `dsplibs/`, `targets/` ve `lang/` bulunur.
- `occ/libs/` kaynak ağacından paket kökündeki `libs/` içine stage edilir. `occ/dsplibs/` aynı şekilde paket kökündeki `dsplibs/` içine stage edilir.
- Eski SDK kökü `dpslib/` kaldırıldı; merkezi bağımlılık deposu `occ/dsplibs/` olarak konumlandırıldı. OCM'nin bağımlılık indirme/ekleme, sürüm ve çözümleme işlemleri henüz uygulanmadı.
- `main.c` executable dizinini DLL yükleme için korur; config, targets ve lang yollarını executable dizininin parent'ından çözer. Böylece çalışma dizinine bağımlılık kalmaz.
- CMake staging testi artık paket kökünde config, standart modüller, dsplibs, targets ve lang varlığını kontrol eder. Locale regression kopyası da `locale-tr/bin/occ` + paket kökü config/lang düzenini kullanır.
- **CI doğrulaması bekleniyor.** Bu değişikliklerden sonra henüz build/CTest veya Windows/macOS/Valgrind sonucu alınmadı. Host OCF include/dependency okuyucusu ve recursive Module Resolver scheduler hâlâ sonraki entegrasyon adımıdır.


- **İlk paket-layout CI sonucu:** run [42797859769](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42797859769) Ubuntu ve ASan/UBSan işlerinde başarısız oldu; 62 CTest'in 61'i geçti. Tek başarısız test `occ_uses_configured_language` idi: locale test kopyasında dil dosyası arama yolu `bin/lang` olarak kalmıştı.
- Düzeltme: host artık config/targets/lang yollarını executable dizinine göre `../occ.ocf`, `../targets`, `../lang` biçiminde kuruyor. Böylece `bin/occ` ile paket kökü arasındaki ilişki açık ve platformlar arası yol birleştirme helper'ı kullanılarak korunuyor. Yeni CI run [42798199142](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42798199142) bekleniyor; henüz başarılı sayılmıyor.

- **Locale regression kök neden düzeltmesi:** ikinci CI denemesinde OCC host path'lerinin `bin/../...` biçiminde doğru olduğu debug çıktısıyla doğrulandı; başarısızlık Semantic stage DLL'in kendi modül klasöründe (bin/) `occ.ocf` ve `lang/` aramasından kaynaklanıyordu. `semantic_stage_module.c` artık `../occ.ocf` ve `../lang/messages_<language>.conf` kullanıyor. Geçici debug çıktısı `main.c` içinden kaldırıldı. Bu düzeltmenin CI doğrulaması henüz bekleniyor.


### CI doğrulaması — OCC paket kökü ve DLL runtime yolları

- Run [42798680457](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42798680457), commit `1580f7194de88dfcf442ff29638fc045831479a6`: **5/5 iş başarılı** — Ubuntu, Windows, macOS, ASan/UBSan ve Valgrind.
- Her beş işte de CTest **62/62 başarılı**.
- Locale regression testi artık `bin/occ` ve `bin/occ_semantic_stage` kopyalarının paket kökü config/dil dosyalarını `bin/../occ.ocf` ve `bin/../lang` üzerinden bulduğunu doğruluyor.
- Bu doğrulama yalnızca paket asset staging ve runtime path çözümlemesini kapsar. OCF include/dependency okuyucusu, gerçek filesystem source provider, OCM ve recursive module scheduler henüz uygulanmadı.


### 2026-11-10 — Bounded OCF include reader

- Added `occ_project_config.h/.c` as a standalone C host/config component. It reads the OCF file's project-root directory and bounded `[include]` entries; it does not open include directories or resolve module source files.
- Limits: 1 MiB total OCF text, 4 KiB line/path, 64 include roots. Duplicate include strings are collapsed; quoted paths with spaces are supported; malformed sections/quoted paths and I/O failures return distinct statuses.
- `[dependencies]` is intentionally opaque to this reader. OCM owns dependency declaration syntax and the centralized `occ/dsplibs/` package store. No project-local dependency copies are created.
- Added CTest coverage for include parsing, quoted paths, duplicate removal, ignoring dependency/other sections, malformed quote rejection and missing-file handling.
- **CI pending** for this new C component and its tests. Filesystem provider, canonical path security checks and recursive scheduling remain unimplemented.


### CI doğrulaması — bounded OCF include reader

- Run [42799442668](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42799442668), commit `f1693de3de91a66f7704c1b5cbbc377236f751cd`: **5/5 iş başarılı** — Ubuntu, Windows, macOS, ASan/UBSan ve Valgrind.
- Her işte CTest **63/63 başarılı**; Valgrind MemCheck de 63/63 testte geçti.
- Yeni test `occ_project_config_bounded_include_parsing`: include listesi, quoted path/space, duplicate eleme, dependency/diğer section'ları yorumlamama, hatalı quote ve missing config durumlarını doğruluyor.
- OCM merkezi dependency manager için `docs/OCM_DESIGN.md` taslağı eklendi. Paket metadata, registry, sürüm/lockfile ve CLI syntax açık karar olarak bırakıldı; uygulama varmış gibi gösterilmiyor.
- Sıradaki adım: bu config API'sini host proje keşfine bağlamak ve filesystem source-provider sözleşmesini (importer-relative arama, include roots, standard libs, dsplibs, canonical identity, ambiguity ve path confinement) testlerle tanımlamak. Recursive module scheduler henüz uygulanmadı.


### 2026-11-10 — OCM proje oluşturma ve proje çalışma alanı sorumluluğu

- Kullanıcı kararı: OCM yalnızca dependency manager değildir; sıfırdan yeni Nexus Flow proje klasörü oluşturabilir ve başlangıç dosya/klasör yapısını bir şablondan üretir.
- OCM proje oluşturma ve dependency yönetimini üstlenir. OCC ise oluşturulan proje klasöründe kaynakları derler ve build işlemlerini yürütür.
- SDK dağıtım kökü ile kullanıcı proje kökü ayrıldı. SDK `bin/occ`, `bin/ocm`, stage DLL'leri, standart `libs/` ve merkezi `dsplibs/` deposunu taşır. Proje kökü kendi `occ.ocf`, `src/`, `include/`, `tests/`, README ve proje-local `build/` alanını taşır.
- Proje klasörlerine OCC/OCM executable'ları, DLL'ler, standart kütüphane veya üçüncü taraf dependency payload kopyalanmaz. Bağımlılıklar merkezi SDK `occ/dsplibs/` deposunda tekil tutulur.
- `docs/OCM_DESIGN.md` OCM sorumlulukları, proje oluşturma, OCC ile çalışma akışı, güvenlik ve uygulama kapılarıyla genişletildi.
- `docs/OCM_PROJECT_TEMPLATE.md` başlangıç proje ağacı, örnek OCF, kaynak dosyası ilkeleri ve güvenli oluşturma gereksinimleri için eklendi.
- **Uygulama durumu:** Bu commit'ler dokümantasyon/tasarım kararıdır. OCM CLI, proje şablon motoru, OCC proje keşfi ve proje-local build output entegrasyonu henüz kodlanmadı; örnek OCF dependency syntax'ı da normatif değildir.
- **Sonraki teknik adım:** önce mevcut host path/config akışını proje root discovery sözleşmesine göre inceleyip tasarlamak; ardından C tarafında proje keşfi ve filesystem provider entegrasyonunu testlerle geliştirmek. OCM'nin proje scaffold motoru, CLI/process sözleşmesi netleşince ayrı C modülü olarak uygulanmalı.


### 2026-11-10 — Parser allocation-failure ve Semantic compound/index kontrolleri

- Parser'da mevcut deterministic allocation-failure injection regression fonksiyonu `test_parser_allocation_failure_injection()` tanımlıydı ancak `test_parser` main test akışında çağrılmıyordu. Ana test akışına eklendi; her allocation noktasında OOM status/result eşleşmesi ve sonunda fail index'i allocation sayısını aştığında başarı yolu denetleniyor.
- Semantic compound assignment testleri genişletildi: immutable `m:` ataması, `.=` string append pozitif/negatif örnekleri, array index ve struct member compound assignment, constant index/member write kontrolleri eklendi.
- Semantic array index kontrolü ortak integer primitive sınıflandırmasını kullanacak şekilde düzeltildi. Böylece `i8/i16/i32/i64` ve `u8/u16/u32/u64` türleri geçerli index türü olarak kabul edilir; float gibi integer olmayan türler reddedilir. Genel `any` index türünün mevcut dinamik davranışı korunur.
- Index hedefi array değilse type diagnostic üretilir; hedef türü çözümlenememişse ayrıca ikinci bir hedef-type diagnostic üretilmez. Inferred `array` türleri index kullanımında desteklenir.
- Semantic regression testleri: undefined index identifier, float index variable, inferred array index, narrow signed/unsigned integer index ve mevcut invalid target/type örnekleri.
- **CI doğrulaması:** Parser allocation-injection testini çalıştıran commit `d152a385a5be80780cfa0e6ceea9c5cbd0b6a503`, run [42802957950](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42802957950) ile 5/5 job başarılı oldu.
- Array index semantic düzeltmesi commit `8d36b309673d947cec8357c81a417e929d278ec8`, run [42803438524](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42803438524) ile 5/5 job başarılı oldu; Ubuntu CTest ve Valgrind MemCheck 63/63 geçti.
- Narrow integer index ek testinin düzeltilmiş fixture commit'i `ab43b51234da03410c67d8fa77374bb6709adc5d` için run [42803824486](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42803824486): Ubuntu, macOS, Windows ve ASan/UBSan başarılı; Valgrind job'u henüz tamamlanmadı. Bu son CI kapısı tamamlanmadan son commit'in tam 5/5 doğrulandığı söylenmeyecek.
- **Kalan Semantic ana işleri:** `AST_USE` modül import semantiği henüz no-op; gerçek çözümleme Module Resolver/host scheduler ile birlikte tasarlanmalı. Tuple type/destructuring ve inline ASM semantic desteği mevcut fail-closed davranışta. Semantic AST annotation/cross-reference matrisi henüz tüm AST node'ları için kapanmış değil.

## 2026-11-10 — Semantic primitive types: d32 / d64

- Semantic primitive recognition now includes `d32` and `d64` in the primitive-result/type-defined registries.
- Both are recognized as numeric primitives for arithmetic/compound-assignment validation.
- Added Semantic regression coverage for declaring `d32`/`d64` values and using numeric compound assignment.
- Commit(s): `2b5a1ceb` (Semantic), `76ede2b7` (tests), `517567aa` (syntax reference).
- CI run [42806777590](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42806777590) completed successfully: Windows, Ubuntu, macOS, Valgrind and ASan/UBSan (5/5). Future `d128`, `i128`, `u128`, `f128` are intentionally not implemented yet; the primitive lists remain explicit and must be extended with tests when those types are introduced.

## 2026-11-10 — USE alias / namespace syntax açıklaması

- Normatif syntax reference `use ... as ...` ve `use ... ns ...` ayrımını örneklerle açıklıyor.
- `as _` alias alanına, `ns _` namespace seçimi alanına kaydedilir; aynı anlama gelmez.
- Parser regression testleri `db.mysql.sql`, alias, namespace seçimi, `as _`, `ns _`, AST codec round-trip ve malformed biçimleri zaten kapsıyor (`test_use_alias_and_namespace_ast_codec`, `test_use_path_malformed_and_exact_spans`).
- Parser/AST syntax tarafı mevcut testlerle korunuyor. Import yükleme, namespace-prefix uygulaması, alias sembol bağlama ve görünürlük Semantic/Resolver entegrasyonunda hâlâ açık; `AST_USE` Semantic traversal şu an no-op ve bunu tamamlandı saymıyoruz.
- Belge commit: `51b1f67c`.


## 2026-11-10 — Module Resolver dependency schedule ve yükleme ağacı belgesi

- `docs/MODULE_RESOLVER_DESIGN.md` bölüm 14'e modül yükleme ağacı, recursive discovery algoritması ve discovery/Semantic iki-pass modeli eklendi.
- Belge açıkça mevcut graph core ile hedef host scheduler'ı ayırıyor; filesystem provider, AST_USE tarayııcı ve DLL çağrıları henüz uygulanmış sayılmıyor.
- `module_resolver.h/.c` içine `occ_module_resolver_dependency_order()` API'si eklendi. Deterministik DFS ile dependency-first modül sırası üretir; kapasite hatasında veya cycle durumunda `out_count=0` olur.
- `test_module_resolver.c` bağımlılık sırası (B → A → Root), yetersiz kapasite ve cycle rejection testlerini içeriyor.
- Commit'ler: `4dd2fcaf` API, `a79714f0` implementasyon, `60c9d460` test, `294a86dc` test temizliği, `597ca704` cycle testi, `6266a97f` resolver tasarım belgesi.
- **CI durumu:** Bu değişikliklerin CI run'ları başlatıldı; sonuçlar henüz bekleniyor. Testler tamamlanmadan bu API doğrulanmış/entegrasyon hazır olarak işaretlenmeyecek.
- **Sonraki iş:** Host scheduler için AST_USE traversal ve Parser DLL recursive invocation sözleşmesi; ardından filesystem provider ve Semantic imported-interface handoff.


## 2026-11-10 — AST_USE top-level visitor ve Resolver korelasyonu

- AST API'sine `ast_visit_top_level_uses()` eklendi. Yalnızca `ASTProgram.declarations` doğrudan bildirim zincirini dolaşır; iç içe fonksiyon/blok gövdelerini import olarak değerlendirmez.
- Visitor, her `AST_USE` için kaynak sırasını koruyan 1 tabanlı modül-içi ordinal verir; AST'yi değiştirmez ve bellek ayırmaz. Callback ile erken durma, geçersiz kök/girdi ve ordinal taşması durumları ayrıştırılır.
- Test: iki top-level USE düğümü, nested AST_USE'nin dışlanması, alias/namespace alanlarının korunması, ordinal sırası ve callback erken durması.
- Resolver edge alanı `ast_use_node_id` açıklaması, kalıcı AST kimliği değil host visitor tarafından atanan bu modül-içi ordinal olarak netleştirildi.
- `docs/MODULE_RESOLVER_DESIGN.md` bölüm 14.6 ile API ve uygulanmış/henüz bekleyen host köprüsü sınırı belgelendi.
- Ayrıca dependency schedule testindeki iki sözleşme hatası düzeltildi: modül durum geçişi için `begin()` çağrısı ve yetersiz kapasite için API'nin mevcut `INVALID_ARGUMENT` dönüşü.
- Commit'ler: `2531bfc4` (AST API), `20d2135c` (visitor implementasyonu), `391f88c4` / `0a319324` (test ve CMake), `d6463720` / `a89349b8` (Resolver test düzeltmeleri), `0e13442c` (edge ordinal sözleşmesi), `67d52d13` (belge).
- **CI:** Sonuçlar bu kayıt hazırlanırken devam ediyor; tüm platformlar, Valgrind ve ASan/UBSan tamamlanmadan bu adım doğrulanmış sayılmayacak.
- **Sonraki iş:** Host compilation coordinator içinde AST visitor callback'ini Resolver source-provider isteğine bağlamak; ardından Parser DLL recursive discovery ve modül AST artifact saklama sözleşmesini tamamlamak.


## 2026-11-10 — Callback-driven Module Scheduler çekirdeği

- Resolver'dan ayrı `module_scheduler.h/.c` eklendi. Scheduler AST bağımlılığını yalnızca bu host koordinasyon katmanında tutuyor; `module_resolver.c` AST'den bağımsız kaldı.
- Root kaynak ve her yeni bağımlılık, provider callback üzerinden canonical identity + source bytes olarak alınıyor. Parser callback AST_PROGRAM üretip scheduler'a devrediyor; AST doğrulanıyor ve top-level USE kayıtları kaynak sırası, span ve ordinal ile kopyalanıyor.
- Recursive C çağrı zinciri yerine açık DFS frame stack kullanılıyor. Böylece modül keşfi derinliği C call-stack tüketimine dönüşmüyor. Yeni modül ilk keşifte parse ediliyor; tekrar eden canonical identity aynı graph node/AST ile paylaşılıyor.
- `ASTProgram.disable_osmodule == 0` ise implicit `osmodule` bağımlılığı explicit USE kayıtlarından önce ekleniyor. Kaynağın nereden bulunacağı provider sorumluluğunda.
- Tüm discovery başarılı olursa Resolver dependency-first sırasına göre Semantic callback çağrılıyor. Discovery hatasında Semantic pass başlamıyor. AST'ler `occ_module_scheduler_destroy()` çağrısına kadar tutuluyor; provider view her parse işleminden sonra serbest bırakılıyor.
- Hata dönüşleri source-not-found/denied/ambiguous/I/O/provider/resource-limit, parse, AST, cycle ve Semantic olarak ayrıştırıldı.
- Test: ortak bağımlılık B'nin bir kez parse edilmesi, B → A → Root Semantic sırası, provider release sayısı ve Root → A → Root cycle tespiti.
- CMake'e `occ_module_scheduler` statik hedefi ve `module_scheduler_dependency_first_semantic_order` CTest kaydı eklendi.
- Commit'ler: `78c38739` API, `04dfe1c3` implementasyon, `a4649a04` test, `97c535e5` CMake, `91b21875` cycle testi, `1bcdebdd` / `e78ae4f9` hata kodları, `7a575a70` temizleme.
- **Entegrasyon sınırı:** Bu commit seti Scheduler koordinatör çekirdeğini sağlar; gerçek filesystem/OCF include provider'ı, Lexer/Parser/Semantic DLL callback adapter'ları ve `main.c` bağlantısı henüz yapılmadı. Mevcut main pipeline hâlâ tek root modül için Parser'dan hemen sonra Semantic çalıştırıyor. Bu nedenle çok dosyalı USE yüklemesi henüz uçtan uca çalışır olarak işaretlenmiyor.
- **CI:** Bu yeni commit setinin CI sonuçları henüz alınmadı. Platform, sanitizer ve Valgrind kontrolleri yeşile dönmeden doğrulanmış kabul edilmeyecek.
- **Sonraki adım:** OCF + include + `occ/libs` politikasıyla somut C source provider'ı oluşturmak; ardından main pipeline'ı önce tüm modülleri parse/discover edecek, sonra dependency-first Semantic çalıştıracak şekilde refactor etmek. Semantic imported-interface handoff bu sırada ayrı API olarak ele alınacak.


### Module Scheduler CI ara doğrulaması

- Fixture düzeltmesinden sonraki commit `c72ba77a` için CI run [42813278689](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42813278689) **5/5 başarılı**: Ubuntu CTest 65/65, macOS, Windows, ASan/UBSan ve Valgrind.
- Bu run, scheduler'ın dependency-first ve cycle testlerinin çalıştığını doğruluyor.
- Sonraki güvenlik sınırı commit'i `b663a9f5` AST_USE kopyalama/collection aşamasını Resolver'ın `max_edges` ve `max_text_bytes` limitleriyle sınırlıyor. Bu commit'in CI run'ı [42813616875](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42813616875) **5/5 başarılı** oldu: Ubuntu, macOS, Windows, ASan/UBSan ve Valgrind.


- Ek regresyon: `e85b4586` ile AST_USE path metninin `max_text_bytes` sınırını aşması durumunda Scheduler'ın Semantic aşamasına geçmeden `RESOURCE_LIMIT` döndürdüğü test eklendi. Bu son test commit'inin CI run'ı [42813781295](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42813781295) **5/5 başarılı** oldu: Ubuntu, macOS, Windows, ASan/UBSan ve Valgrind. Ubuntu CTest ve Valgrind MemCheck 65/65 geçti.


## 2026-11-10 — Filesystem Module Provider ve tek dosya USE eşlemesi

- Kullanıcı kararı: `use db;` → `db.nxf`; dotted modül adları klasörlere bölünmez, ör. `use db.mysql.sql;` → `db.mysql.sql.nxf`.
- Arama sırası: host tarafından verilen `dsplibs/`, ardından `libs/`, proje kökünün sabit `include/` dizini, son olarak proje OCF `[include]` girdileri (bildirim sırası korunur). İlk bulunan dosya seçilir.
- `filesystem_module_provider.h/.c` eklendi. Resolver'ın dosya sistemi bağımsızlığı korundu; sağlayıcı yalnızca source provider callback'lerini uyguluyor.
- Modül adları dot-separated identifier segmentleri olarak doğrulanıyor; slash/backslash/absolute path/traversal kabul edilmiyor. Root input (`importer_identity == NULL`) doğrudan kaynak dosya yoludur.
- Kaynak dosyası okuma boyut limiti, provider-owned buffer/identity ve tam bir kez release sözleşmesi eklendi. Büyük dosya ayrı resource-limit statüsüyle Resolver'a aktarılıyor.
- CMake'e `occ_filesystem_module_provider` hedefi ve `filesystem_module_provider_single_file_mapping_and_search_order` testi eklendi.
- Fixture testleri: dotted filename, `db.nxf`, traversal reddi, dsplibs önceliği, libs fallback, proje include fallback, OCF ek include fallback, bulunamadı ve kaynak boyutu limiti.
- **Entegrasyon sınırı:** Provider henüz `main.c` tarafından kullanılmıyor. CLI hâlâ SDK runtime OCF'den yalnızca dil ayarını okuyor; proje OCF seçimi ve `dsplibs/` + `libs/` köklerinin runtime çözümü sonraki entegrasyon adımı. Bu nedenle USE uçtan uca çalışıyor olarak işaretlenmiyor.
- **CI:** Son test düzeltmesi commit'i `189815d1` için run [42816468235](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/42816468235) **5/5 başarılı**: Ubuntu 66/66, Windows 66/66, macOS 66/66, ASan/UBSan ve Valgrind MemCheck 66/66. Windows checkout CRLF farkı test karşılaştırmasında normalize edildi; provider kaynak baytlarını değiştirmiyor.
- Commit'ler: `589f0543` / `ef1e9807` provider API/ilk uygulama, `64fe4480` CMake/test kaydı, `56651ecf` test, `b163d7ed` / `79c1bc6c` / `bf139265` limit statüsü, `07ee6e14` limit testi, `4e164007` / `930cbdfb` / `1e9942d5` include kökü, `675ecc1e` fixture, `69557d8f` include index düzeltmesi.


- Son provider test/CI düzeltme commit'i: `ccf78e13` (traversal reddi + POSIX realpath bildirimi), `2aaa8921` (erişim reddi statüsü), `189815d1` (platformlar arası satır sonu testi). Son CI tüm kontrollerde yeşil.

### Module Scheduler ek regresyon kapsamı — 2026-11-10

- Scheduler testlerine aynı modülün birden fazla top-level use ile istenmesi durumu eklendi. Beklenen davranış: canonical identity tek graph node olarak kalır, Parser callback modülü yalnızca bir kez çalıştırır, Semantic callback de modülü yalnızca bir kez işler.
- ASTProgram.disable_osmodule == 0 durumu için ayrı test eklendi. Root modülün implicit osmodule bağımlılığı explicit importlardan bağımsız kaydedilir ve Semantic sırası osmodule → root olur. Test fixture'ında osmodule kendi implicit bağımlılığını disable_osmodule ile kapatır.
- Mevcut testler cycle, resource limit, dependency-first sıralama ve provider view release sayısını da korur.
- **CI:** Test genişletme commit'leri için Actions çalışıyor; tüm platformlar, ASan/UBSan ve Valgrind sonuçları tamamlanmadan yeni kapsam doğrulanmış sayılmayacak.

### Semantic module interface snapshot — 2026-11-10

- Added occ_module_interface C library, built from finalized SemanticResult data without coupling the Resolver to Semantic internals.
- Snapshot carries canonical module identity, schema version 1.0, publication-candidate symbols, referenced type closure (including base types), and relations owned by published symbols.
- Publication candidates are selected by the existing PUBLIC or EXPORTED SemanticResult flags. Both flags are preserved exactly; no unverified equivalence or access rule is imposed by the snapshot layer.
- Source-module symbol/type IDs remain stable and explicitly source-local. Importer Semantic must map references rather than reusing these IDs as local IDs.
- Group/member relation metadata is retained only when both owner and related symbol are publication candidates; private related declarations are not leaked through the snapshot.
- Added CTest coverage for public/exported Group + member data, type and relation preservation, private symbol omission, and rejection of non-finalized SemanticResult input.
- Not complete: OIR serialization/validation of the interface, AST_USE alias/namespace binding, importer-side collision/visibility/type checks, actual Semantic DLL input extension, and main.c scheduler integration. This snapshot is a tested in-memory foundation only.

### USE binding transfer preparation — 2026-11-10

- Scheduler use records now retain module_name/requested path, alias, namespace_name, source span and visitor ordinal independently.
- Added occ_module_scheduler_get_import_binding(), which maps importer module ID + USE ordinal to the resolved dependency module ID and returns borrowed alias/namespace strings and the original source span.
- Added regression coverage for `use a as math ns core`-equivalent AST data; alias and namespace are verified independently. This preserves the distinction between `as _` and `ns _` for the Semantic layer.
- The accessor is scheduler-lifetime scoped; it does not yet apply visibility, namespace lookup, collision or symbol-binding rules.


### Module Interface OIR artifact codec — 2026-11-10

- Added the append-only artifact kind `OIR_KIND_MODULE_INTERFACE = 10`; existing artifact kind values and SEMANTIC_AST section IDs remain unchanged.
- Added a dedicated `OMI1` payload schema 1.0 inside the generic OIR envelope. The payload carries canonical module identity, source-local symbol/type IDs, PUBLIC/EXPORTED flags, type closure, and published relations.
- Added a C writer and validating reader. The reader checks envelope kind, Semantic producer stage, validity, required payload uniqueness/version, bounded record counts, string boundaries/NUL safety, duplicate IDs, and symbol/type/relation cross-references. Decoded data is owned and released through the artifact destroy API.
- The writer validates the in-memory snapshot before serialization and does not modify caller output on validation/allocation failure; OIR container write still enforces caller capacity.
- Added CTest coverage for snapshot → OIR bytes → decoded snapshot round-trip, source identity/job ID, visibility flags, type and Group relation preservation, private symbol exclusion, and invalid/non-finalized input rejection.
- This is the transport codec and structural validation layer only. It is not yet wired into the Semantic DLL or host scheduler; importer visibility/collision/namespace rules and the independent verifier DLL's dedicated MODULE_INTERFACE validation remain next integration work.


### Independent Module Interface Verifier DLL — 2026-11-10

- Added `occ_module_interface_verify_stage`, a separate C shared-library stage for OIR Module Interface artifacts. It does not alter or weaken the existing OIR_MODULE verifier contract.
- The stage accepts exactly one Semantic-produced, valid Module Interface artifact for the current job, validates it through the dedicated OIR Module Interface reader, and republishes the same sections with producer stage `OCC_STAGE_OIR_VERIFY`.
- Output-buffer negotiation is side-effect free: it reports the exact required capacity before emitting output or diagnostics. Invalid artifacts produce stable stage diagnostics.
- The Module Interface artifact reader now accepts both Semantic-produced input and Verifier-stamped output, allowing downstream consumers to re-open the verified artifact.
- Added a dynamic DLL test using the host module loader. It checks descriptor validation, retry/output sizing, artifact identity/job preservation, verifier producer stamping and clean diagnostic behavior.
- This verifier is an independent validation gate for module-interface transport; it does not perform importer visibility, alias, namespace, collision or type-binding semantics.


### Scheduler USE binding canonical identity — 2026-11-10

- Extended `OccModuleImportBinding` with `dependency_identity`, a borrowed pointer to the resolved dependency's canonical identity in the Resolver graph.
- `occ_module_scheduler_get_import_binding()` now returns both the resolved dependency module ID and canonical identity alongside the requested dotted spelling, alias, namespace selector and source span.
- The new field remains borrowed for scheduler lifetime; no path is re-resolved and no alias/namespace normalization is performed.
- Added regression assertion against the fake provider's canonical identity.


### OIR Module Imports manifest — 2026-11-10

- Appended `OIR_KIND_MODULE_IMPORTS = 11`; existing artifact IDs 1–10 remain unchanged.
- Added the `OIM1` payload schema 1.0 and a C-owned manifest model for one importer's explicit top-level USE declarations.
- Each record preserves the 1-based USE ordinal, original requested module spelling, canonical resolved dependency identity, alias, namespace selector and complete source span. Scheduler-local module IDs are deliberately not serialized as stable identities.
- Added `oir_module_imports_build_from_scheduler()`: it collects the importer's resolved bindings and obtains the importer canonical identity directly from the Resolver graph, then creates the owned manifest.
- Added OIR writer/reader with bounded strings and record counts, duplicate ordinal detection, span validation, embedded-NUL rejection and owned decode cleanup.
- Added CTest round-trip coverage, including independent alias/namespace values, preservation of `as _`, job identity and malformed duplicate-ordinal rejection. Scheduler now exposes a tested binding-count accessor.
- Not yet integrated: host `main.c` does not yet pass this manifest plus the matching verified Module Interface artifacts to the Semantic DLL. Semantic imported-symbol visibility, alias/namespace lookup, collision checks and imported type mapping remain pending.


### Independent Module Imports Verifier DLL — 2026-11-10

- Added `occ_module_imports_verify_stage`, an independent C DLL for host-produced `OIR_KIND_MODULE_IMPORTS` artifacts.
- It validates the `OIM1` payload through the dedicated reader, enforces the active job ID, and republishes the unchanged sections with producer stage `OCC_STAGE_OIR_VERIFY`.
- The reader now permits both host-produced and verifier-stamped manifests, while still rejecting any other producer stage.
- Added a dynamic module-loader CTest for ABI descriptor, invocation, job preservation, verifier stamp and clean diagnostics.
- This verifies transport integrity only; the importing Semantic DLL has not yet consumed the manifest.


### Semantic import bundle matcher — 2026-11-10

- Added `occ_module_import_bundle_bind()` as a C-side boundary validator between verified host imports and verified Module Interface artifacts.
- It requires the expected importer canonical identity to match the manifest, requires verifier-stamped valid OIR artifacts, and requires every interface artifact to share the manifest job ID.
- It maps each import record to exactly one matching dependency canonical identity. Repeated USE declarations of one module share the same decoded interface; duplicate interface artifacts, missing dependencies, unused extra interfaces and importer mismatches are rejected.
- Returned binding views borrow the decoded manifest/interface storage; destroy frees only the mapping array.
- Added tests for repeated dependency mapping, independent alias/namespace retention, importer mismatch, missing interface and duplicate interface rejection.
- Still pending: Semantic DLL must accept and pass the verified manifest/interface set into this matcher, then apply the language's visibility and binding rules. This matcher does not itself expose symbols to expressions.


### Semantic DLL verified import inputs — 2026-11-10

- Extended the Semantic stage ABI descriptor to accept AST plus optional Module Imports and Module Interface artifacts (up to 64 total inputs); AST remains the required first input and legacy AST-only invocations remain supported.
- Defined the input order: AST at index 0, verifier-stamped OIM1 manifest at index 1, then one verifier-stamped Module Interface artifact for each unique dependency.
- Semantic DLL decodes the manifest and interfaces, checks job IDs and verifier producer stamps, and runs `occ_module_import_bundle_bind()` before semantic analysis. A missing, duplicate, mismatched or unverified interface bundle is rejected.
- Added dynamic Semantic DLL tests for the existing AST-only path, a valid three-input import bundle, and rejection when a required dependency interface is missing.
- Important boundary: this wires and validates imported artifacts at the Semantic DLL boundary; imported symbols are not yet injected into local name lookup. PUBLIC/EXPORTED access policy, alias/namespace resolution, collision diagnostics and imported type mapping remain pending. The host CLI scheduler is also not yet wired to construct and pass this multi-artifact input list.


### Module Interface snapshot from Semantic OIR — 2026-11-10

- Added `occ_module_interface_build_from_oir_view()`, allowing the host to build a public/exported interface snapshot directly from a fully validated `SEMANTIC_AST` OIR view instead of reconstructing a mutable `SemanticResult`.
- It preserves source-local symbol/type IDs and PUBLIC/EXPORTED flags, copies only publication candidates, follows referenced type base chains, and retains relations only when their owner and any nonzero related symbol are published.
- The adapter uses OIR table accessors and copies all strings into the owned Module Interface snapshot; it does not retain pointers into the source artifact.
- The real Semantic DLL integration test now marks a source symbol PUBLIC in a validated SEMANTIC_AST artifact, builds the interface snapshot from that artifact view, and verifies the published symbol and type closure.
- This enables dependency interfaces to be derived from Semantic stage output. Host Scheduler/main.c still needs to run each dependency through Semantic, invoke the interface writer + independent verifier, and pass the verified set to the importer Semantic call.

### Semantic import environment — 2026-11-10

- Added semantic_import_environment.h/.c, a C read-only Semantic-side index built only from a successfully matched verified import bundle.
- Each explicit USE binding remains a separate view with its source ordinal, requested dotted spelling, canonical dependency identity, as alias and ns selector. Repeated imports share the interface object but do not collapse binding metadata.
- Added scoped symbol lookup and source-local type lookup. Source module symbol/type IDs are preserved, and PUBLIC/EXPORTED flags are returned unchanged.
- Duplicate same-name published symbols are reported as ambiguous; missing symbols/types have separate status.
- The environment owns only its binding index. OIR strings and interface data remain borrowed and must outlive the environment.
- Added semantic_import_environment_binding_and_source_local_lookup CTest for alias/namespace preservation, repeated module bindings, visibility flags, ambiguity, missing lookup and cleanup.
- Boundary: this does not yet inject imported symbols into ordinary Semantic lexical lookup or decide PUBLIC/EXPORTED, alias-default, namespace-selection and collision policies. Those rules remain explicit next Semantic work; no end-to-end import claim is made.

### Semantic import selector lookup — 2026-11-10

- Extended semantic_import_environment with exact lookup of explicitly declared as aliases and ns namespace selectors.
- Alias and namespace lookup are separate APIs; repeated selectors return AMBIGUOUS, missing selectors return NOT_FOUND.
- NULL, empty and _ selectors are not treated as implicit bindings. In particular, as _ and ns _ do not create a lookup key, and no default alias is inferred here.
- Added regression assertions for alias lookup, namespace lookup, missing selectors and underscore behavior.
- This remains a binding-index API. It does not yet inject names into ordinary expression/type lookup or define collision precedence.

### Primitive bit/byte type audit — 2026-11-10

- User clarified the language rule: `!` is the explicit type-forcing/coercion marker, not part of the type name. The syntax reference shows explicit typed declarations for `bit`, `byte`, all five fixed-width bit vectors and all five fixed-width byte blocks.
- Semantic distinction: `bool` is a logical value, `bit` is a one-bit data representation, `byte` is a raw 8-bit storage unit, `u8` is an arithmetic integer, `Nbit` is a bit vector and `Nbyte` is a raw byte block. Equal physical widths do not make these types implicitly interchangeable.
- **Storage order decided and documented:** standalone `bit` uses one byte-addressable storage slot; bit 0 (LSB) carries the value and bits 1–7 are zero/reserved. Independent scalar `bit` variables are not implicitly packed together. Packed flags use `Nbit` vectors. Within `Nbit`, bit index 0 maps to byte 0 / bit offset 0 (mask `0x01`), index 7 to byte 0 / bit offset 7 (mask `0x80`), and index 8 to byte 1 / bit offset 0. This canonical bit-vector byte order is independent of native integer endianness.
- Syntax reference defines `byte_index = bit_index / 8`, `bit_offset = bit_index % 8`; `Nbit` storage is N/8 bytes for currently supported widths, while `Nbyte` stores N raw bytes. `sizeof(bit)` and `sizeof(byte)` are specified as 1 byte; fixed vector/block sizes are documented.
- **Index syntax:** `Nbit[index]` reads/writes a `bit`; `Nbyte[index]` reads/writes a `byte`. Semantic now classifies these indexed expressions and requires an integer index; statically evaluable negative and out-of-range indices are rejected. Tests cover bit-vector read/write at LSB, MSB and the first bit of the second byte, byte-block read/write at first/last byte, floating-point index rejection, bit-vector upper-bound rejection, byte-block upper-bound rejection and negative-index rejection. Dynamic index bounds checks remain pending for OIR/Verifier/runtime.
- Lexer emits dedicated token kinds for all ten numeric-leading type spellings. Exact decimal width + suffix is required; unsupported widths and malformed suffix continuations remain invalid numeric suffixes.
- Parser preserves type spelling in AST type-name fields and stores the hard-type marker separately. AST frontend OIR codec regression checks `bit`, `byte`, all ten numeric-leading type spellings and each hard-type marker through encode/decode/re-encode canonical byte equality.
- Semantic recognizes all types as defined, classifies them as primitive types in SemanticResult, and permits bitwise `&`, `|`, `^`, `~` plus compound `&=`, `|=`, `^=` only for compatible bitwise-capable operands. Width/type mismatches are rejected; representation types do not gain numeric arithmetic.
- Regression coverage includes all lexer tokens, unsupported suffixes, every declared width, binary masks, compound operations on `bit`, `byte`, `Nbit` and `Nbyte`, equality, complement, arithmetic rejection, width mismatch and representation indexing. Compound assignment const checks remain active for identifiers, members and indexes.
- Explicit `!` coercion into bit/byte representation types is now checked in Semantic: decimal integer constants must fit the target width; unsigned integer sources no wider than the target and representation sources no wider than the target are accepted as safe zero-extension. Out-of-range constants and unprovable dynamic narrowing are rejected. Regression tests cover `bit`, `byte`, `8bit`, `16bit`, safe unsigned conversion, safe representation widening and rejected narrowing. Successful explicit conversions are recorded in `SemanticResultConversion` with source/destination type IDs, conversion kind and explicit flag; an end-to-end SemanticResult → SEMANTIC_AST OIR artifact regression verifies all four conversion records survive artifact write/open unchanged.
- OIR_MODULE payload schema 1.2 now adds distinct `BIT`, `BYTE`, `BIT_VECTOR` and `BYTE_BLOCK` type kinds without changing the 12-byte type record. The reader accepts legacy schema 1.1 only with its original kinds and rejects 1.2-only kinds mislabeled as 1.1. The primitive type mapper and independent OIR Verifier type-record pass recognize the full current bit/byte family while excluding these representation kinds from integer arithmetic. Codec, mapper, verifier and 1.1-compatibility regression tests have been added. CI run `42851762072` has passed Ubuntu, Windows, ASan/UBSan and Valgrind; macOS remains queued, so the matrix is not yet 5/5.
- Full OIR instruction lowering for bit extraction/insertion, raw byte access, representation constants, dynamic index bounds checks, general runtime coercion checks (including signed/dynamic narrowing) and final ABI alignment/padding remain pending. `<<` and `>>` remain Nexus Flow stream/zone operators; no bit-shift meaning is assigned.


### OIR bitwise opcode registry 1.1 — 2026-11-10

- Canonical syntax reference `docs/language/syntax-reference.md` now specifies the bit/byte representation families, explicit `!` coercion, one-byte scalar `bit` storage slot, LSB-first `Nbit` indexing, `Nbyte` indexing and the supported bitwise operator family with Nexus Flow examples.
- Corrected the canonical operator reference: `<<` and `>>` are Zone Write / Flow Feed operators, not bit shifts. No numeric shift interpretation is assigned.
- OIR opcode registry minor version advanced from 1.0 to 1.1. Stable wire IDs 22–25 are `BIT_AND`, `BIT_OR`, `BIT_XOR` and `BIT_NOT`; existing IDs and meanings are unchanged.
- The independent OIR Verifier now checks bitwise instruction result/operand contracts: only integer, BIT, BYTE, BIT_VECTOR and BYTE_BLOCK types are accepted, and every operand type ID must exactly match the result type ID. Boolean and floating-point operands are rejected.
- Added registry descriptor/shape assertions to `test_oir_module`; extended the independent verifier test fixture with a well-typed integer bitwise instruction; the type contract explicitly excludes boolean and floating-point types.
- **Not yet implemented:** Semantic AST → OIR bitwise instruction emission; 128-bit constant/immediate representation; Nbit extraction/update and Nbyte indexed load/store lowering; dynamic index bounds checks. These remain OIR work and do not authorize Codegen work.
- CI run `42856277976` for the bitwise verifier changes has passed Ubuntu, Windows and ASan/UBSan; Valgrind is still running and macOS is queued at the time of this update. Do not treat this as a 5/5 result.


### Binary bitwise AST → OIR lowering — 2026-11-10

- `oir_lowering_expr.c` maps AST token IDs for `&`, `|` and `^` to `OIR_OP_BIT_AND`, `OIR_OP_BIT_OR` and `OIR_OP_BIT_XOR`. Lowering only accepts integer and bit/byte representation type kinds; semantic analysis remains responsible for operand compatibility and verifier rechecks exact OIR type IDs.
- `test_oir_lowering.c` now has a real Parser → Semantic → Semantic AST artifact → OIR lowering regression for `ret 5 & 3;` and asserts opcode, value references and return linkage.
- Bit-shift syntax decision documented in canonical syntax reference: `<<<` / `>>>`; `<<` / `>>` stay Zone Write / Flow Feed. Shift behavior is bit-pattern based, zero-fills, truncates width overflow, and returns zero for shift amount >= width.
- Lexer recognizes `<<<` / `>>>` as dedicated tokens; Parser places shifts between additive and bitwise AND precedence. Existing `<<` / `>>` Flow operators remain unchanged.
- Semantic requires integer operands, allows the count operand to use a different integer type, and rejects statically known negative counts. Runtime-negative dynamic counts are defined as traps; shift amounts greater than or equal to the left operand width produce zero.
- OIR registry IDs 26/27 (`bit.shl` / `bit.shr`) and the binary, result-producing, PURE/MAY_TRAP descriptors are present. The lowering path maps both stable tokens, with the current single-configured-scalar-type limitation.
- Semantic regression cases cover valid shifts, mixed integer count widths, floating operands and negative constants. The latest follow-up commit adds a real frontend-to-OIR `<<<` fixture and independent Verifier acceptance/rejection fixtures.
- **Still pending:** `>>>` end-to-end lowering fixture; general mixed-width shift-count lowering; shift count boundary/runtime behavior tests; binary `|` and `^` end-to-end lowering tests; unary `~`; bit-vector extract/update and byte indexed load/store.
- Commit `bbb4158` introduced the initial shift contracts; its CI run failed on all five test jobs. The follow-up commit `9a2b641` fixes the Verifier's main instruction switch and adds the missing lowering/Verifier regressions. CI run `42874827012` is queued/running; do not mark this subtask verified until that run completes.


### Integer bit-shift codec integration follow-up — 2026-11-10

- Shift token stable IDs 165 (TOKEN_BIT_SHL) and 166 (TOKEN_BIT_SHR) were appended to the token registry, but the AST TLV primitive still capped token IDs at 160. This caused the real Parser → Semantic → AST OIR writer path to reject shift expressions before OIR lowering.
- Raised the AST TLV accepted stable-token upper bound to 166 and updated the TLV regression to accept IDs 165/166 while rejecting 167. The AST TLV primitive remains independent of Parser/Lexer headers.
- Added Semantic boundary acceptance checks for shift counts equal to and greater than the left integer width (both produce zero by language contract).
- Added a real frontend-to-OIR lowering fixture for >>>, separate from <<<, and an independent Verifier acceptance fixture for OIR_OP_BIT_SHR.
- OIR registry assertions now require both shift descriptors to be PURE and MAY_TRAP. Dynamic negative-count execution remains a runtime/backend behavior; current tests verify opcode metadata and verifier contract, not an executed runtime trap.
- Mixed-width shift-count Semantic acceptance is covered, but general mixed-width OIR lowering remains unsupported by the current single-configured-scalar-type lowering slice.
- CI verification is pending for the follow-up commits. Do not mark the shift package complete until the full matrix passes.


### OIR shift verifier fixture and CI closure — 2026-11-11

- The OIR Verifier's BIT_SHL and BIT_SHR instructions now pass through the same shared instruction type-contract validation path as the other typed opcodes; the earlier shift-specific bypasses are removed.
- The contract requires an integer result and integer left/right operands, with the left operand type exactly matching the result type. The shift-count integer type may differ by design. The regression fixtures accept valid BIT_AND, BIT_SHL and BIT_SHR, and reject a shift whose result and left operand types disagree.
- The initially missing SHR diagnostic was traced to the test harness: the artifact header job ID and module invocation job ID differed. The fixture invocation now derives its job ID from the variant, matching the artifact header. The verifier itself was not bypassing the type check.
- Unused SSA result reporting is advisory: warning-sink/formatting failures do not invalidate otherwise verified OIR or suppress output.
- Temporary diagnostic tracing was removed. The required standard I/O include remains because the warning formatter uses snprintf.
- CI run 43066571877 passed all five jobs: Ubuntu build/CTest, macOS build/CTest, Windows build/CTest, Ubuntu ASan/UBSan and Ubuntu Valgrind. The run is tied to commit b94b52c8f7bc9dd7258ef93b0cf3a0950b3b5fe2.
- Follow-up coverage added in commit `7ece065`: variant 23 constructs a separate `u64` shift-count SSA value while the SHR left operand and result remain `i32`; the fixture asserts the serialized operand/value type relationships and invokes the independent OIR Verifier. The fixture also exercises cross-block dominance for both operands. CI run `43069499665` passed all five jobs (Ubuntu build/CTest, macOS, Windows, ASan/UBSan and Valgrind).
- Existing Verifier regression coverage also exercises block/function ownership, overlapping instruction ranges, function entry ownership, missing terminators, type-record rejection, invalid runtime value types, reverse SSA result links, opcode-local type contracts, CFG edges, diamond dominance, local/cross-block SSA uses, duplicate SSA definitions and advisory unused-value warnings.
- The shift-count width fixture closes that specific coverage gap; the OIR Verifier stage remains open pending a full requirement-by-requirement audit and any missing tests/documentation. Do not treat this focused CI closure as stage completion.


### OIR Verifier schema and host-gate audit — 2026-11-11

- Added the missing independent OIR Verifier regression for a mixed-width shift count: SHR left/result are i32 while the count is a separate u64 SSA value. The serialized fixture is inspected before invoking the verifier. Commit: 7ece065.
- CI run 43069499665 passed all five jobs: Ubuntu build/CTest, Windows, macOS, ASan/UBSan and Valgrind.
- Corrected OIR_VERIFIER_SPEC.md references from OIR_MODULE 1.1 to the active 1.2 schema. The verifier's input contract and implemented-status list now describe the current representation type records.
- Audited the host pipeline in omnicore-sdk/src/c/main.c: the verifier is loaded and invoked after lowering; its output is host-validated before it is returned as the module artifact. Any verifier load, invocation or output-validation failure takes the cleanup path. The downstream Codegen dispatch is reached only after the pipeline function succeeds.
- Remaining verification gap: add a dedicated negative end-to-end host-pipeline test that proves an invalid OIR artifact cannot reach Codegen/Optimizer. The source-level gate is present, but this dedicated negative integration test is not yet implemented. The OIR Verifier stage remains in progress.


### OIR Verifier schema diagnostic consistency — 2026-11-11

- Corrected the independent Verifier's envelope-rejection diagnostic: it now names the active OIR_MODULE 1.2 schema instead of stale 1.1 wording.
- Added a regression fixture that supplies a schema-1.1 artifact and asserts rejection code 8601 plus the active-schema diagnostic text.
- This is a verifier-stage regression only. The separate negative host-pipeline integration test (invalid lowered OIR must not reach Codegen/Optimizer) remains open; no end-to-end gate coverage is claimed by this change.


### Host verifier gate negative integration coverage — 2026-11-11

- Added a test-only rejecting OIR Verifier DLL and an isolated-runtime CTest scenario. The normal lexer, parser, semantic and lowering stages run; the substituted verifier deliberately rejects the module.
- The test asserts the host reports the verifier's diagnostic and emits no OIR_MODULE publication, OIR_ASSEMBLY, stage assembly or legacy Codegen output.
- Added the OIR Verifier DLL to the production `occ` target's explicit build dependencies, so a non-test build also produces the required stage module.
- Scope note: this proves the host gate stops downstream consumers when the independent verifier rejects. The Verifier's own invalid-SSA/type/CFG rejection remains covered by its direct stage tests; this host test does not claim to inject a semantically malformed lowering artifact.
- Initial CI exposed two test-isolation issues: a C comment typo (fixed) and the test DLL's output name colliding with the production verifier DLL. The latter caused unrelated CLI and direct-verifier tests to load the rejecting fixture; the fixture now has a unique output name and is copied under the production name only inside the isolated test package.
- CI run `43072785755` for commit `3332b4253dda61d495242b94a164dd35d0bf4e0e` passed all five jobs: Ubuntu build/CTest, Windows build/CTest, macOS build/CTest, Ubuntu ASan/UBSan and Ubuntu Valgrind. The isolated host verifier-gate test is now CI-verified on the full matrix.
- This closes the dedicated host gate regression gap, not the OIR Verifier stage itself. The next task remains the requirement-by-requirement audit of opcode descriptors, operand/result types, SSA, CFG, cross-table references, diagnostics and adversarial artifact bounds.


### OIR Verifier complexity audit — 2026-11-11

- Reviewed the independent verifier's opcode contracts, validation pass ordering, CFG/SSA checks and resource bounds against the current implementation.
- Corrected `OIR_OPCODE_REGISTRY.md` from stale registry 1.1 to the implemented registry 1.2 and documented stable shift IDs 26/27, integer operand requirements, mixed-width count allowance, PURE/MAY_TRAP flags and zero result for counts at or above the left width.
- Removed a redundant full block-table scan for every function in `validate_structure()`. A function's entry block is already range-checked and its ownership is checked against the precomputed block-owner table; that single condition proves the function owns a block. This removes an unnecessary O(function_count × block_count) scan without weakening the invariant.
- Existing verifier fixtures cover valid function entry ownership and reject an entry block owned by another function. The code change is awaiting the full CI matrix; do not treat it as verified until CI completes.

- Dominance analysis previously allocated and freed a temporary bitset for every reachable non-entry block on every fixed-point iteration. Replaced that repeated heap traffic with a fixed-size stack scratch row bounded by the verifier's 8,192-block limit (129 64-bit words maximum). This also removes an allocation-failure path that was being reported as a dominance invariant error instead of an infrastructure workspace error. Existing CFG/dominance fixtures exercise the same algorithm; full CI is pending for the combined verifier audit changes.


### 2026-11-11 — Sekiz başlıklı syntax/AST görünürlük tablosu ve Verifier CI kapanışı

- Nexus Flow syntax envanteri sekiz başlık altında ayrı bir kullanıcı görünümüne taşındı: bildirim/tür, fonksiyon/modül, ifadeler/operatörler, aggregate/bellek, kontrol akışı, pattern/ruleset, Nexus Flow/intrinsics, concurrency/ASM/deferred syntax.
- Her satır Lexer, Parser, AST+Codec, Semantic, OIR Lowering ve bağımsız Verifier sütunlarında ayrı durumlandırıldı. Parser veya AST desteği, sonraki aşama tamamlandı gibi gösterilmiyor.
- Öncelik sırası AST sözleşmesi ve Parser/span/codec kapanışından başlayıp Semantic, ownership/effect, Semantic artifact validator, lowering, Verifier ve pipeline entegrasyonuna kadar sıralandı. Codegen genişletmesi son kapı olarak kaldı.
- Son Verifier karmaşıklık/çalışma alanı değişikliklerinin CI run 43073952941 sonucu kontrol edildi: Ubuntu, Windows, macOS, ASan/UBSan ve Valgrind 5/5 başarılı.
- Bu CI sonucu yalnızca ilgili değişiklikleri doğrular; OIR Verifier aşaması hâlâ gereksinim bazlı tam denetim bekliyor.
- Dashboard dosyası: `NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md`.


### AST structural inventory audit — 2026-11-11

- Compared the active `ASTNodeType` enum directly with `get_node_size`, `ast_destroy_recursive`, `ast_print_recursive`, `ast_serialize_recursive`, `ast_deserialize_recursive` and `ast_validate`: all **108/108 unique tags** have explicit dispatch in all six paths.
- `test_ast_deserialize.c` iterates through `AST_TYPE_EXPR` (the current final tag) in the all-tag codec fixture. `test_ast_validator.c` covers graph ownership, sibling/ancestor cycles, required children and source-span invariants.
- This closes the AST structural lifecycle/tag-dispatch inventory only. It does not close Parser production/span tests, Semantic rules, OIR lowering or Verifier contracts.
- Initial Parser source-span audit found no reason to mark helper-level absence as a defect when the enclosing `parse_statement` / group parser stamps the completed production (for example variable, defer, immutable and thread declarations). Production-by-production span assertion coverage remains open; dedicated coverage currently exists for 26 parser test functions, with additional syntax families still needing explicit exact-range assertions.
- Next AST gate: close the exact source-span fixture matrix for remaining active syntax productions, then audit OIR optional span TLVs. Keep Semantic and Codegen expansion gated.
- Detailed view: `NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md`; structural ownership reference: `AST_NODE_FIELD_OWNERSHIP.md`.


### Parser source-span regression fixtures — 2026-11-11

- Added exact source-range assertions for `AST_PANIC` and ordinary `AST_NEXUS_PIPE` to the existing Parser suite. The checks include byte offsets and invoke `ast_validate()` on the resulting graph.
- CI run 43076506449 for commit `3dd54b1fc6f95fa42944dca2e25bd78943704c4a` passed all five jobs: Ubuntu, Windows, macOS, ASan/UBSan and Valgrind.
- Continued Parser production audit found a higher-priority unresolved case: `TOKEN_UNPACK` allocates `AST_UNPACK`, then recursively parses and returns the continuation without attaching/returning the wrapper. The allocated node/subtree is therefore lost on a successful continuation; no dedicated Parser test exists, and `...>` is absent from the normative syntax reference. It remains incomplete pending a precise postfix-vs-target grammar decision; do not mark it complete.


### Semantic checkpoint — Select/Scan subject type propagation — 2026-11-12

- **Implementation:** Select/Scan now analyze the selector/subject expression first, then visit each arm in its own scope while passing the subject type into pattern binding creation.
- Pattern bindings no longer default to `any` when the subject type is known. Their local Symbol slot, SemanticResult type ID, and AST annotation carry the inferred subject type. Constructor pattern recursion currently propagates the subject type as a conservative baseline; enum payload field mapping remains pending.
- **Regression coverage:** added positive and negative cases for both Select and Scan. `i32` subject → binding assigned to `i32` succeeds; assigning the inferred binding to `str` fails.
- Commits: implementation `92f43cbb5166891aedec098babcb3714c0820686`; tests `2af1952e54cb84acc1f33e8e62935690fe07752c`; dashboard update `4939686935405b8067d0a7b486e0c0ff092279c1`.
- CI was triggered by the implementation commit; test commit followed while the first run was active. Recheck the latest run after the documentation commit before marking this checkpoint verified.
- Remaining Semantic priorities: enum-aware constructor patterns and payload types; exhaustive Select/Scan; tuple typing/destructuring; function/callback signature compatibility and return-flow; import environment/type exports; ownership/effects and full AST annotation/reference validation. No OIR work starts until Semantic closure.


### CI feedback — Semantic fixture and Parser test corrections — 2026-11-12

- The first new Semantic test run compiled the updated Semantic source and `test_semantic`, but CTest caught two stale test-harness issues: conditional-control tests still used retired `(condition) -> action` spelling, and six Parser AST validation assertions omitted the required `ASTValidationError*` argument.
- Updated conditional fixtures to `(condition) ? action` and supplied `NULL` to the six validation calls. No production syntax was loosened.
- Commits: `51f8370f` (Parser validation call fixes), `2e5f1c9d` (Semantic fixture syntax correction), `ba52c43d` (dashboard log).
- Full CI for the corrected head is pending; no green status claimed yet.


### Semantic checkpoint — AST_BLOCK_HALT annotation — 2026-11-12

- Added explicit SemanticResult annotation for the payload-free `AST_BLOCK_HALT` cleanup marker (`void`, rvalue) rather than silently ignoring the active AST node.
- Semantic accepts the cleanup marker; scope unwinding/defer ordering and executable cleanup remain OIR responsibilities.
- Added a nested block + `}?;` Semantic fixture.
- Commits: `5db1589f` (implementation), `3eece891` (test), `35b0bd87` (coverage dashboard).
- CI is still running for prior Parser-span fixture correction and new Semantic slices; final green matrix not yet confirmed.


### Semantic checkpoint — implicit type variable `t` — 2026-11-12

- `t` is accepted as a type variable and interned as `SEMANTIC_RESULT_TYPE_INFERRED`; generic compatibility treats it as a unification placeholder.
- Generic call expression return `t` resolves to the first argument's concrete type for the present identity/sum cases. Arithmetic over matching `t` parameters is accepted.
- Added `identity(value!t)!t` and `sum(a!t,b!t)!t` positive Semantic cases instantiated with `i32`.
- This is a bounded implicit inference slice, not full generic constraints/unification. Multiple type variables, nested callback/array generic composition and constrained type parameters remain open.
- Commits: `1a8f8765` Semantic; `18583bf6` tests; `f12592b2` dashboard.


### Parser source-span audit — grouped Nexus Flow right operands — 2026-11-12

- Fixed `AST_NEXUS_FLOW` source stamping to use the final consumed token rather than the inner AST child end. This includes closing parentheses in grouped right operands such as `?=> (data)`.
- Kept the exact-range test including `)`; this is a production span correction, not a syntax change.
- Commit: `5d301071f0ea056aa92850e8d5cf0feea1995628`; coverage record `2d4c6d6f`. CI pending.


### Semantic checkpoint — tuple typing/indexing — 2026-11-12

- `AST_TUPLE` now semantically checks all elements and receives a `tuple` rvalue annotation. The persistent SemanticResult type interner uses its existing `SEMANTIC_RESULT_TYPE_TUPLE` kind.
- Constant tuple indexing resolves the selected element type; negative/out-of-range indexes are diagnosed. Dynamic indexes intentionally remain `any` until runtime lowering.
- Tests cover heterogeneous tuple inference, `i32`/`str` element indexing and a statically out-of-range index.
- Commits: `37c87fea`, `0b06e621` Semantic; `cd3795d9` test; `64cbef9d` dashboard. CI pending.


- Tuple's former fail-closed test was updated after Semantic support landed: `AST_TUPLE` is no longer expected to reject; `AST_ASM_BLOCK` remains the pending fail-closed node. Commit `bd041149`; dashboard `b3b974b1`.


- Tuple index follow-up: identifier-based tuple indexing now follows the declaration's tuple initializer to resolve element types and bounds (direct tuple-expression-only resolution was insufficient). Semantic fix `45752564`, dashboard `9be8efac`; CI pending.


- Catch-flow parser regression: `?-> (error)` without a handler block was being accepted. Parser now requires the canonical `?-> (error) { ... }` block form. Commit `4c057072`; dashboard `fdbf8a29`; CI pending.


### Semantic checkpoint — loop condition typing — 2026-11-12

- Loop conditions now require `bool`; range expressions are the explicit exception and require compatible numeric endpoints.
- Added `loop(1)` negative Semantic regression; canonical Boolean loop fixture retained.
- Commits: `2f8d4e50` Semantic, `040fdccd` test, `74a42571` dashboard. CI pending.


### Semantic checkpoint — callback type and NewType validation — 2026-11-12

- Function-type signatures beginning `f:(...` are recognized by Semantic type resolution and assigned the existing FUNCTION type kind.
- NewType target resolution now validates the target name/signature and reports unknown targets; callback NewType targets are accepted.
- Added valid callback NewType and invalid unresolved target tests.
- Commits: `88997965` Semantic, `5f287a01` test, `51011f5d` dashboard. CI pending.


### Semantic checkpoint — variadic signature and calls — 2026-11-12

- Added `Slot.is_variadic`; fixed parameter count/type array excludes `AST_VARIADIC_PARAM`. Signature relation generation handles the marker without unsafe ASTVarDecl casts.
- Call validation enforces all fixed parameters and allows extra arguments only when variadic; fixed argument types still undergo compatibility checks.
- Added variadic positive call and missing fixed argument negative fixture.
- Commits: `f081c798` header, `2bfddd11` signature, `f1d1940a` argument count, `0cf7f812` SemanticResult relations, `115ac855` tests, `1022a4ad` dashboard. CI pending.


### Semantic checkpoint — named argument resolution — 2026-11-12

- Named argument nodes now recursively check/annotate their value and call validation maps argument names to function parameter declarations, checking the selected parameter's type.
- Unknown parameter names emit an explicit Semantic diagnostic; reordered named arguments are covered.
- Commits: `e20b2624`, `da982f2a` Semantic; `24c0cd38` tests; `971d847d` dashboard. CI pending.


- Tuple index bound checking now emits its diagnostic only in AST_INDEX_ACCESS validation; expression-type inference is side-effect-free for invalid indexes. This prevents duplicate errors for a single out-of-range access. Semantic commit `9ce304ae`, dashboard `a463c32e`.


### Semantic checkpoint — inferred array literal element types — 2026-11-12

- Inferred array literals now visit all elements, receive an `array` SemanticResult annotation, and check element type compatibility. Heterogeneous inferred array values report a mismatch rather than being accepted as untyped.
- Added `[1,2,3]` positive and `[1,"x"]` negative tests.
- Commits: `7017200b` Semantic, `8102116e` test, `4feb7d81` dashboard. CI pending.


- Tuple index semantic checker build fix: declared its local `int64_t constant_index` used for static bounds validation. Commit `48ab91cf`; dashboard `1b6b0870`.


- Array literal diagnostic deduplication: element homogeneity was already enforced in `get_expr_type`; removed a duplicate check from AST_ARRAY_LITERAL traversal so a malformed literal produces one Semantic error. Commit `aa81d9ae`; dashboard `40bf0b6a`.


### Semantic test suite checkpoint — 2026-11-12

- CI run for `aa81d9ae` passed the Semantic test target (including the new generic, tuple, NewType, variadic, named argument, block cleanup, loop-condition and array checks). CTest was 72/73.
- Remaining single failure was Parser test `parser_accepts_and_rejects_sources` / catch-fallback-ignore fixture; the Semantic target itself passed. Overall cross-platform CI is still pending.
- Dashboard checkpoint: `4ead5149`.


- Named argument follow-up: reject duplicate parameter assignment (including positional/named collisions) and positional arguments after named arguments. Added duplicate-name and ordering negative tests. Commits `3f86c6da` Semantic, `cfb75499` tests, `20236581` dashboard. CI pending.


### Parser catch handler contextualization — 2026-11-12

- Root cause of catch-flow Parser regression: Lexer emitted a special `TOKEN_ERR_HANDLER` for every `(e)`, so ordinary `echo(e)` in the catch block lost its parenthesis/identifier tokens.
- Lexer now always emits ordinary parenthesis tokens; Parser recognizes the `(e)` handler only in the `?->` operator context and attaches the following block.
- Commits: `81f1da3e` Lexer, `76176c20` Parser, `0ae26a9d` dashboard. CI pending.


- Named-argument ownership note: Parser already rejects duplicate labels and positional-after-named (its variadic/named argument tests cover these). Removed these invalid parser inputs from Semantic fixtures; Semantic tests continue to cover reordered argument typing and unknown parameter names. Commit `a8e6ce41`, dashboard `8a458bfd`.


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


### Semantic AST integrity audit — 2026-11-12 (alt kapsam)

| Kontrol | Semantic kod | Regression fixture | Durum |
|---|:---:|:---:|:---:|
| Function name/body structural validation | ✓ | ✓ | CI pending |
| Nexus label/jump non-empty identity | ✓ | ✓ | CI pending |
| defer body required | ✓ | ✓ | CI pending |
| thread_block name/body before scope entry | ✓ | ✓ | CI pending |
| Reserved AST_AI_LOGIC fails closed | ✓ | ✓ | CI pending |

Bu tikler yalnızca yukarıdaki AST integrity kontrollerini kapatır. AST ID satırlarının Semantic sütunu, tüm type/scope/effect/control-flow kuralları tamamlanmadığı için ~ kalır.


### Semantic checkpoint — Nexus/argument AST structural integrity — 2026-11-12

- Added fail-closed validation for AST_NEXUS_CAPTURE (handler + expression), AST_NEXUS_PIPE (both operands), AST_NAMED_ARG (name + value) and AST_LISTEN (handler name before scope entry).
- Added direct malformed-AST regression fixtures. Full handler/event lifetime, pipe effects and listen runtime semantics remain open.
- Dashboard alt-coverage checkmarks represent implementation + fixture presence; CI is pending.


### Semantic sınır checkpoint — OS I/O / Thread modülü — 2026-11-12

- Kullanıcı kararı netleştirildi: `echo` ve `input` OS module kapsamındadır; core dilin bare-metal garantisi değildir.
- `spawn`, `listen`, `send`, `receive`, `done` ayrı Thread modülü/runtime alanıdır. Core Semantic scheduler/IPC/lifetime uygulamaz; AST structural doğrulaması ile modül API sözleşmesi ayrıdır. Legacy `AST_SPAWN` tag'i yeni syntax olarak değerlendirilmez.
- Bare-metal `prnt` / `prmt` için çözüm önerisi: terminal çağrısı yerine target/runtime sink abstraction (UART, framebuffer console, debug channel veya host callback). Ortak ABI ve sürücü seçimi henüz kararlaştırılmadı; şimdilik Semantic bunlara target yeteneği varsayımı eklemiyor.
- `write` / `fwrite` file/stream provider'a bağlı olarak belgelendi.
- Normatif syntax reference commit: `d8268b7f`; dashboard commit: `c7695712`.
- Bu not dokümantasyon checkpoint'idir; AST intrinsic signature testleri ve Thread modül implementasyonu tamamlanmış sayılmaz. CI pending.


### Semantic AST checkpoint — OS / Thread call identity — 2026-11-12

- `AST_ECHO`, legacy `AST_SPAWN`, `AST_SEND`, `AST_RECEIVE`, `AST_DONE` özel call tag'leri için Semantic intrinsic callee kimliği denetimi eklendi. Eksik callee veya yanlış identifier adı mevcut açık diagnostic mekanizmasına düşer.
- Regression testleri: beş AST tag için doğru callee doğrudan AST fixture'ı ve `echo("semantic")` kaynak testi eklendi.
- Semantic commit: `b4ddabcc`; test commit: `b035063a`; dashboard: `67e2ee9b`.
- CI/local C test çalıştırması bu ortamda yürütülmedi; pending olarak kalır.
- Thread runtime/IPC semantics ve `prnt/prmt` target sink ABI'si açık tasarım işleri olmaya devam ediyor.


### 2026-11-12 — thread_block scope ownership correction

- Tightened the AST contract: `thread_block` must contain an `AST_BLOCK`; a non-NULL arbitrary node is not accepted.
- The child `AST_BLOCK` is now the sole lexical-scope owner. Removed the redundant outer scope from `AST_THREAD_BLOCK`; local declarations remain isolated by ordinary block semantics.
- Added direct malformed-AST and source-level scope-leak fixtures. Core remains unaware of Thread scheduling, IPC, handles and lifetime.
- CI/test execution is pending; fixture presence is not a pass claim.


### 2026-11-12 — thread_block independent payload model

- Clarified the language model: each named `thread_block` is a complete independent payload; Thread module `spawn(worker)` transfers the named payload to a newly created thread. The payload is not also executed in the caller's flow.
- Nested thread blocks are now rejected by Semantic, including manually supplied AST trees via a context depth guard.
- Updated the normative syntax reference and AST coverage dashboard. Runtime payload lookup/transfer and thread-only execution remain Thread runtime / later OIR-Codegen integration work, not Core Semantic claims.


### 2026-11-12 — Thread payload registry / spawn name validation

- SemanticContext now owns a named thread payload registry populated from valid AST_THREAD_BLOCK nodes.
- spawn(name) resolves only through this registry; exactly one identifier argument is required. Unknown names, malformed arguments and duplicate payload names are rejected with source diagnostics.
- Thread module boundaries remain intact: scheduler, IPC, handle/lifetime and actual payload transfer/execution are not implemented in Core Semantic.
- Registry cleanup and explicit initialization are included. Regression fixtures were added; CI and local test execution are pending.


### 2026-11-12 — Thread payload capture semantics

- Adopted explicit by-value capture for outer-scope variables referenced inside a named thread payload: only `copy(...)` arguments capture an outer variable; direct outer-variable access is a Semantic error. Captures are snapshotted when the payload is spawned; caller stack addresses are not shared implicitly.
- SemanticContext records unique external variable captures used inside explicit `copy(...)` arguments, with symbol/type/scope and AST provenance. Direct outer references are rejected; payload-local variables are excluded. Pointer captures copy only the pointer value, not the pointed-to allocation.
- Thread result export remains `done(...)` and result consumption remains `listen(...)` / target-specific listener behavior, as specified by the Thread runtime documents. Core does not implement their runtime behavior.
- Added metadata regression coverage. Test execution and CI remain pending.


### 2026-11-12 — Durable thread capture annotation / artifact compatibility

- Added `SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE` to identifier-use annotations for outer variable references in active thread payloads.
- Confirmed the Semantic AST artifact writer copies annotation flags; extended the OIR annotation schema's accepted mask with the corresponding `OIR_SEMANTIC_ANNOTATION_FLAG_THREAD_CAPTURE` bit.
- Added integration coverage for explicit-copy semantic marking and artifact round-trip, while preserving legacy schema 1.0 behavior when annotation flags are zero.
- This is artifact compatibility work required by Semantic output; OIR lowering/Verifier phase has not started.
- Tests are committed but not executed in this workspace; CI remains pending.


### 2026-11-12 — Explicit copy-only capture validation

- Corrected the capture rule to preserve Nexus Flow's explicit copy/move ownership model: direct access to an outer `SLOT_VAR` inside a thread payload is rejected; an outer value must appear within `copy(...)`.
- `copy(...)` in a thread payload is checked for exactly one source expression. Captures are deduplicated by source symbol; nested payload-local variables remain local and do not become captures.
- Added negative tests for direct access and malformed copy arity, plus repeated-copy deduplication and durable SemanticResult flag coverage.
- `listen` and `done` result export/consumption remain Thread module responsibilities under the existing runtime spec. No scheduler, IPC or runtime code was added to Core.
- Tests are committed; local execution and CI are still pending.


### 2026-11-12 — Thread payload explicit-copy implementation closure update

- The earlier implicit by-value capture draft was corrected to match the explicit copy/move ownership model. A thread payload may not directly read/write an outer `SLOT_VAR`; an outer value must be referenced inside `copy(...)`.
- Core Semantic recognizes `copy(...)` only while validating an active thread payload, requires exactly one source expression, records unique outer symbols and marks their identifier annotations with `SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE`. Payload-local identifiers are not marked.
- Added source tests for valid repeated copies, direct outer access rejection, and zero/multiple copy arguments. SemanticResult integration checks explicit capture annotations and local non-capture. OIR artifact tests verify the flag survives annotation serialization/validation; unknown flag bits remain rejected.
- `listen` and `done` output transfer/consumption remain Thread module rules already present in the runtime specification. Core changes are limited to front-end payload validation and capture metadata; no scheduler/IPC behavior was introduced.
- Latest test additions are committed. CI has not reported a workflow/status, so execution remains pending.


### Semantic checkpoint — Program and block declaration scope — 2026-11-12

- Confirmed the existing semantic scope behavior: program declarations share the root scope; each block owns a nested lexical scope.
- Added regression fixtures for global declaration visibility inside functions, duplicate program-level declarations, legal nested shadowing, and rejection of nested/sibling block scope leakage.
- Updated `NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md`: Program/block Semantic column is now ✓. AST declaration-chain validation, `no:osmodule` source policy/span and OIR span metadata remain open and are not counted as Semantic work.
- Test source is committed. No local build is available in this workspace; CI has not yet been checked for the new commit.


### Semantic checkpoint — `c:` compile-time constant rules — 2026-11-12

- Added a bounded constant-expression classifier to Semantic. It permits literals, pure unary/binary expressions, constant array/tuple/struct literals, casts of constant values and references to earlier `c:` declarations.
- `c:` now requires an initializer and rejects runtime calls, mutable variable reads and `m:` values. `m:` remains a runtime-capable immutable binding.
- Added positive/negative fixtures covering constant literals/expressions/references/aggregates and the invalid initializer cases; updated the normative syntax reference and coverage dashboard.
- The `AST_VAR_DECL` dashboard row remains `~`: inferred `v:` typing, recursive `type_expr` integration and common source-span round-trip are not closed.
- Test source is committed but has not been executed locally. CI status is pending/unreported.


### Semantic review — Variable, NewType and Struct declarations — 2026-11-12

- Completed the missing `c:` compile-time initializer rule and added tests for required initializers, pure expressions, prior constant references, aggregate constants, runtime calls, mutable reads and `m:` reads.
- Reviewed `v:` inference and declared-type compatibility, inline array/vector checks and recursive `type_expr` validation; existing Semantic tests cover valid and invalid cases.
- Reviewed NewType target/recursive type validation, nominal identity, alias chains and assignment/call/return compatibility; existing fixtures cover these contracts.
- Reviewed Struct field name/type/duplicate validation, packed/public/exported symbol metadata, aggregate member access and literal completeness.
- Updated the AST coverage dashboard: Semantic columns for `AST_VAR_DECL`, `AST_NEWTYPE_DECL` and `AST_STRUCT_DECL` are now ✓. AST payload round-trip, layout, OIR and Verifier work remain open.
- Implementation and test fixtures are committed. CI has not reported a status/workflow for the current commits; no test pass is claimed.


### Semantic checkpoint — Enum variant and payload validation — 2026-11-12

- Hardened `AST_ENUM_DECL` Semantic handling: validates enum name, variant node kind, non-empty/unique variant names, payload field node kind, non-empty/unique field names and explicit payload field types.
- Unknown named payload types and malformed recursive type expressions are diagnosed; malformed AST payload chains no longer get blindly cast to `ASTVarDecl`.
- Added direct malformed/valid AST fixtures and updated the normative syntax reference plus the AST coverage dashboard.
- Enum Semantic remains partial: constructor lookup/argument binding and exhaustive pattern analysis are not complete. No OIR work has started.
- CI/local test execution is pending; no pass claim is made.


### Semantic checkpoint — Struct/Union field declaration validation — 2026-11-12

- Added required-type validation for Struct and Union fields, while allowing a valid recursive `type_expr` to supply the type.
- Hardened named and inline aggregate field traversal against malformed child node kinds; malformed nodes are diagnosed by the shared field validator and are not cast as `ASTVarDecl`.
- Added direct AST tests for missing Struct/Union field types and malformed field nodes. Existing aggregate tests cover duplicate field names, unknown types, and member compatibility.
- Struct Semantic remains closed. Union field declaration/type resolution is implemented, but active-member tracking and safe-read behavior remain open pending a language decision. This policy was not invented.
- Updated the syntax reference and coverage dashboard. Local tests and CI remain unverified.


### Semantic checkpoint — Enum scan constructor binding — 2026-11-12

- Implemented enum-aware `scan` constructor pattern resolution using the subject expression's Semantic type.
- Constructor patterns now resolve the variant within that enum and validate payload arity. Positional pattern bindings inherit the declared payload field type and are registered as typed arm-local variables.
- Rejects unknown enum variants, constructor patterns against non-enum subjects, malformed payload binding nodes, incorrect payload counts and bare matching of payload-bearing variants.
- Bare enum variant patterns are accepted as tag matches only for no-payload variants.
- Added direct Semantic AST tests for valid typed capture and invalid paths: unknown variant, wrong payload arity, bare payload-bearing variant and constructor pattern on a non-enum subject.
- Updated `docs/language/syntax-reference.md` and `NEXUS_FLOW_AST_COVERAGE_DASHBOARD.md`.
- Still open: constructor expression call/argument binding, duplicate scan arm coverage, exhaustive enum scan analysis, non-enum structural pattern semantics, and discriminant model. CI/test execution has not been confirmed; no pass claim is made.


### Semantic checkpoint — enum scan coverage — 2026-11-12

- Parser/AST now retain explicit scan-arm guard metadata (`ASTScanArm.has_guard`).
- Semantic validates enum scan exhaustiveness, ignores guarded arms for coverage, detects duplicate unguarded variant matches and accepts a final else arm as fallback.
- Added direct Semantic fixtures for duplicate coverage, guarded non-coverage and else fallback; existing constructor pattern tests now include the other enum variant so coverage diagnostics do not mask their intended assertions.
- Updated the normative syntax reference and AST coverage dashboard. No OIR work started. Test execution and CI remain pending/unverified.


### Semantic checkpoint — enum constructor calls — 2026-11-12

- Added enum constructor-call resolution for the `EnumType.Variant(...)` AST form, distinct from instance member and Group method dispatch.
- Positional arguments bind to payload fields by declaration order. Named arguments bind by field name and reject unknown, duplicate or omitted fields.
- Constructor call checks validate payload count and declared `type_name` compatibility; constructor call and callee AST nodes receive enum type/function-designator annotations.
- Added direct AST regression fixtures for valid positional/named constructors, unknown variant, missing payload, incompatible payload type and unknown named payload field.
- Updated syntax reference and AST coverage dashboard. Enum Semantic remains partial for discriminant model and recursive payload type-expression compatibility. Tests and CI are not yet verified.

### Semantic checkpoint — Group method AST integrity — 2026-11-12

- Hardened AST_METHOD: a method now requires a non-empty public name and an implementation node.
- Ordinary methods accept only function or extern implementation nodes. The special default handler may also use a block body, matching the existing group-default syntax.
- Invalid implementation node kinds are rejected before receiver inference or implementation traversal.
- Added direct malformed-AST fixtures for missing method name, missing implementation and invalid implementation node kind. Existing source fixtures continue to cover inferred/explicit self, static/instance dispatch, forward method resolution, method argument count/type and default-handler behavior.
- Group/Method Semantic remains partial; receiver/visibility and complete signature policy still need review. Tests have not been executed in this workspace and CI is unverified.


### Enum discriminant implementation — 2026-11-12

- Extended `ASTEnumVariant` with an owned optional `discriminant_expr`, a resolved signed 64-bit value and an explicit resolved-state flag. The new fields are appended to preserve the existing struct prefix.
- Updated AST printer, destructor, graph validator and stream codec. AST stream format advanced from v6 to v7; deserialization continues to accept legacy v1–v6 streams and reads the new enum fields only for v7.
- Parser now accepts an optional `= expression` after a variant name or payload, and the variant source span includes the discriminant.
- Semantic resolves omitted values as 0 / previous+1 and explicit values as compile-time integer arithmetic. It rejects duplicate values, identifiers/runtime expressions, floating-point values and implicit increment overflow. Signed `INT64_MIN` and `INT64_MAX` boundaries are handled; an explicit value can reset the implicit sequence after the maximum.
- Added Parser/AST codec regression coverage and Semantic cases for implicit/explicit sequencing, negative values, arithmetic expressions, duplicate values, non-constant values, floating-point values and signed-range overflow.
- Updated Syntax Reference, AST Syntax Matrix and Coverage Dashboard. OIR/target physical enum representation remains deferred to the OIR stage; recursive payload type-expression compatibility remains open.
- **Verification status:** code and tests have been committed, but no local build or CI result has yet been observed. Keep the dashboard Semantic status partial until the C test suite passes.

### Semantic checkpoint — C-like Union shared storage — 2026-11-12

- Confirmed the language contract: Union members share storage; Semantic does not track an active member and does not reject reads solely because another member was written most recently.
- Cross-member reads are permitted as target-ABI-dependent reinterpretation, not as a portable conversion or a universal safety guarantee.
- Added Semantic source fixtures for cross-member read/write, unknown member, incompatible member write and unknown member type. Existing tests cover duplicate fields and missing field types.
- Updated the normative syntax reference, AST coverage dashboard and AST syntax matrix. Union layout, alignment, physical representation and OIR mapping remain deferred to their proper later phase.
- **Verification:** test source and documentation are committed; CI run `43253848909` is still queued and the new Union test additions have not yet been executed. Union Semantic remains partial until the C test suite passes.


## 2026-11-12 — Group method uniqueness regression checkpoint

- Added Semantic source fixtures for duplicate Group method public aliases, duplicate implementation function names across aliases, and collisions between a method implementation name and a direct Group function.
- Existing Group/Method fixtures cover inferred and explicit `self`, invalid receiver declarations, namespace/static calls, object/instance calls, method visibility through Group-qualified lookup, unknown methods, argument count/type, and malformed AST guards.
- The first CI run exposed two literal `\\n` sequences accidentally inserted into `test_semantic.c` outside C string literals; corrected them in commit `578a819cb7bf41953d4e2e3e9a8b7c1b1f165f90`.
- Updated Group uniqueness tests are in commit `e0655d389d2e49f9b2e805ecc528f9938859249a`. CI is pending; Group Semantic and the preceding Union/Enum checkpoints are not marked complete until the full C workflow passes.


## 2026-11-12 — Group method implicit receiver argument mapping

- Corrected named-argument resolution for instance methods: parameter-name lookup now excludes the implicit first `self` receiver, while type checking retains the full declaration index.
- Added valid named-argument regression (`instance.store_value(value: 7)`) and invalid explicit `self:` named-argument regression.
- Updated the normative syntax reference to define implicit receiver omission, static/object dispatch, and Group-owned nominal type visibility.
- CI has not yet verified the change; Group / Method and earlier Enum/Union Semantic rows remain uncompleted in the dashboard until the C workflow passes.

- Clarified that Group public method names are unique and method overloading is not supported by the current language contract; duplicate public aliases are rejected.
