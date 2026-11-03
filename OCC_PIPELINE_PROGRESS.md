# OCC Pipeline İlerleme Kaydı

**Tarih:** 2026-11-03  
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
| Integer / float literals | ~ | ~ | Decimal integer/fraction; unsupported radix/scientific/suffix forms fail closed; rolling `ms` unit token pair; overflow and source-span matrix |
| String / char literals | ~ | ~ | Escape, UTF-8, kapanmayan literal ve newline |
| Single-character operators | ~ | ~ | Token ID ve parser precedence |
| Multi-character operators | ~ | ~ | Longest-match ve çakışma testleri |
| Nexus Flow operators | ~ | ~ | `<-`, `->`, `>>`, `<<`, `_>`, `?->`, `?=>`, `!->`, `!!=` vb. |
| Comments / whitespace / EOF | ~ | — | Line/column/byte span, CRLF/LF, tab ve EOF |
| Invalid / malformed input | ~ | ~ | Hatalı token sonrası güvenli devam ve diagnostic |

> Lexer'da daha önce enum'da bulunup dispatch'te eksik olan 13 token/primitive eşlemesi tamamlandı; source byte offset/line/column ve span length eklendi; kapanmayan block comment hata olarak raporlanıyor. Bu, tüm token envanterinin kapandığı anlamına gelmez.

## 4. AST düğüm bazlı ilerleme tablosu

Bu tablo `OIR_AST_NODE_REGISTRY.md` içindeki mevcut 1–99 kayıtlı düğüm kimliklerini temel alır. ID'ler append-only'dir; mevcut ID'ler yeniden numaralandırılmaz.

**Sütunlar:** Parser = syntax'tan bu düğümü üretme; AST+Codec = payload/lifecycle/serialize/deserialize; Semantic = tip/scope/ownership/control-flow kuralları; OIR Lowering = executable OIR üretimi; Tests = özellik bazlı parser/semantic/lowering testleri. AST codec için mevcut field-level round-trip matrisi tüm kayıtlı tag'leri kapsar; bu, her düğümün parser veya semantic olarak tamamlandığını göstermez.

| ID | AST düğümü | Parser | AST + Codec | Semantic | OIR Lowering | Feature tests |
|---:|---|:---:|:---:|:---:|:---:|:---:|
| 1 | `AST_PROGRAM` | ~ | ~ | ? | ? | ~ |
| 2 | `AST_BLOCK` | ~ | ~ | ? | ? | ~ |
| 3 | `AST_VAR_DECL` | ~ | ~ | ~ | ~ | ~ |
| 4 | `AST_FUNC_DECL` | ~ | ~ | ~ | ~ | ~ |
| 5 | `AST_STRUCT_DECL` | ? | ~ | ? | ? | ? |
| 6 | `AST_ENUM_DECL` | ? | ~ | ? | ? | ? |
| 7 | `AST_UNION_DECL` | ? | ~ | ? | ? | ? |
| 8 | `AST_NEWTYPE_DECL` | ? | ~ | ? | ? | ? |
| 9 | `AST_ENUM_VARIANT` | ? | ~ | ? | ? | ? |
| 10 | `AST_GROUP_DECL` | ? | ~ | ? | ? | ? |
| 11 | `AST_METHOD` | ? | ~ | ? | ? | ? |
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
| 29 | `AST_ASM_BLOCK` | ~ | ✓ | X | X | ~ |
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
| 40 | `AST_SPAWN` | ~ | ✓ | ? | ? | ~ |
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