# OCC Pipeline İlerleme Kaydı

**Tarih:** 2026-11-07  
**Çalışma dalı:** `refactor/c-core-roadmap`  
**Kaynak depo:** `benimprojem/OMNI-CORE-v2-NEXUS-FLOW`  
**Belge amacı:** Lexer'dan Codegen'e kadar her Nexus Flow yapısının gerçek uygulama ve test durumunu tek tabloda izlemek.

> Bu belge yaşayan takip tablosudur. Durumlar tahmini yüzde değildir; kod, belge ve test kanıtına göre güncellenir. Bir enum, AST tag'i veya DLL dosyasının bulunması tek başına özelliğin tamamlandığı anlamına gelmez.

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

Bu tablo `OIR_AST_NODE_REGISTRY.md` içindeki mevcut 1–105 kayıtlı OIR düğüm kimliklerini temel alır. ID'ler append-only'dir; mevcut ID'ler yeniden numaralandırılmaz.

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
| 101 | `AST_VARIADIC_PARAM` | ✓ | ~ | ~ | D | ~ |
| 102 | `AST_NAMED_ARG` | ✓ | ~ | ~ | D | ~ |
| 103 | `AST_MUST_DECL` | ✓ | ~ | ~ | D | ~ |
| 104 | `AST_FASTEXEC_BLOCK` | ~ | ~ | ~ | D | ~ |
| 105 | `AST_ASM_LABEL` | ~ | ~ | ~ | D | ~ |

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
| Module use / dependency | ~ | ? | ? | AST_USE dotted path ve m: dependency syntax mevcut; resolver/import scope yok |
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
