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
| 2026-11-03 | Lexer token envanteri ile Parser tüketim noktaları karşılaştırıldı. `^^`, `%`, `&&`, `||`, binary bitwise, compound assignment ve `.=` tokenları mevcut olmasına rağmen Parser üretimleri bulunmuyor; `<<` / `>>` ise mevcut gramerde Flow operatörleri olarak tüketiliyor, shift arithmetic değil. Bu ayrım syntax referansına ve AST/Syntax matrix'e eklendi; precedence/short-circuit ve Flow/shift çakışması karara bağlanmadan gramer genişletilmeyecek. | `omnicore-sdk/src/c/parser/lexer.h`, `omnicore-sdk/src/c/parser/parser.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; kaynak taraması tamamlandı, operator AST/semantic testleri sonraki iş |
| 2026-11-03 | Lexer sayı taramasında desteklenmeyen radix/üs/sonek biçimlerinin geçerli sayı + identifier olarak bölünmesi engellendi. `0xFF`, `0b1010`, `10E3`, `1e-3`, `1_000`, `12u8`, `1.5f32` tek TOKEN_ERROR ile reddediliyor; decimal integer/fraction ve `1..5` range token dizisi için regresyon testleri eklendi. Syntax referansı ve AST/Syntax matrix, `^^` operatörünün Lexer tokenı olmasına rağmen Parser üretimi bulunmadığını açıkça kaydediyor. | `omnicore-sdk/src/c/parser/lexer.c`, `omnicore-sdk/src/c/tests/test_lexer.c`, `omnicore-sdk/docs/syntax.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`; GitHub Actions commit `140c720`: Windows, Ubuntu, macOS build/CTest; Ubuntu ASan/UBSan ve Valgrind tümü başarılı |
| 2026-11-03 | Syntax referansı main dalındaki `docs/language/syntax-reference.md` ile etkin dalın README içine gömülü dil bölümü karşılaştırılarak geri kuruldu. Etkin dalda `omnicore-sdk/docs/syntax.md` normatif giriş noktası oluşturuldu; README proje tanımı ve geliştirme takibine indirgenerek dil metni tekilleştirildi. Eski README'deki güncel Lexer/Parser sınırları, array dimension kuralları ve Halt açıklamaları; main'deki kapsamlı select/scan ve defer semantik açıklamalarıyla birleştirildi. main dalına yazma yapılmadı. | `omnicore-sdk/docs/syntax.md`, `omnicore-sdk/docs/README.md`, `README.md`; içerik taşıma tamamlandı, parser-syntax tamlık denetimi devam ediyor |
|---|---|---|
| 2026-11-03 | İlk merkezi pipeline ve 99 AST düğümlü takip tablosu oluşturuldu. | `OIR_AST_NODE_REGISTRY.md`, `NEXSUS_AST_SYNTAX_MATRIX.md`, `FRONTEND_COMPLETENESS_AUDIT.md`, `AST_OIR_COMPLETION_PLAN.md`, `COMPILER_PIPELINE.md`, `OIR_MODULE.md`, `TASKS.md` |
| 2026-11-03 | Persistent Semantic Result Model, checker mirror integration, SEMANTIC_AST artifact writer ve bağımsız Semantic DLL kaynakta mevcut olarak doğrulandı. Semantic/AST artifact testleri ve OOM testleri mevcut; ancak bu oturumda build/CTest çalıştırılamadığı ve tam semantic coverage matrisi kapanmadığı için Semantic ve artifact validator `~` olarak kaldı. | `omnicore-sdk/src/c/semantic/semantic_result.{h,c}`, `omnicore-sdk/src/c/parser/semantic.c`, `omnicore-sdk/src/c/module/semantic_stage_module.c`, `test_semantic_result.c`, `test_semantic_result_oom.c`, `test_semantic_result_integration.c`, `test_oir_semantic_ast*.c`; CI durumu bu oturumda doğrulanmadı |
| 2026-11-03 | AST factory sınır denetimi eklendi: negatif ve son kayıtlı AST kind sonrasındaki enum değerleri artık `NULL` döndürür; ilk/son geçerli kind ve iki geçersiz sınır için parser testine regresyon kontrolü eklendi. AST genel sözleşmesi hâlâ `~`; tüm 99 düğümün ownership/child/nullability matrisi henüz kapanmadı. | `omnicore-sdk/src/c/parser/ast.c`, `omnicore-sdk/src/c/tests/test_parser.c`; commit `a724cefd08aa9e43e715e7e87a19cd541ae78669`, `379ba144fc4b81dd3cc8be1fef9ab476af39453c`; build/CTest henüz çalıştırılmadı |
| 2026-11-03 | AST factory regresyon testi 99 kayıtlı AST kind'ın tamamında oluşturma, type korunumu ve sıfırlanmış temel alanları dolaşacak şekilde genişletildi; negatif ve üst sınır enum değerleri reddedilmeye devam ediyor. Factory boyut haritası denetiminde yalnızca `AST_BREAK` ve `AST_CONTINUE` temel `ASTNode` boyutunu kullanıyor; destructor/codec tarafında da bu iki payload'sız düğümün boş işlem davranışı mevcut. Bu bulgu tek başına AST sözleşmesini tamamlamaz. | `omnicore-sdk/src/c/tests/test_parser.c`, `omnicore-sdk/src/c/parser/ast.c`, `omnicore-sdk/src/c/parser/ast.h`, `OIR_AST_NODE_REGISTRY.md`; GitHub Actions: Ubuntu build/CTest ve ASan/UBSan başarılı; Windows, macOS ve Valgrind işleri henüz sürüyor; commit `966d1ee3ea6b098a3bc8d67b22ad4d035e98728b` |

## 9. Kaynak belgeler

- `COMPILER_PIPELINE.md` — hedef pipeline ve DLL sınırları.
- `FRONTEND_COMPLETENESS_AUDIT.md` — lexer/parser/AST/semantic denetim kriterleri.
- `NEXSUS_AST_SYNTAX_MATRIX.md` — Nexus Flow syntax ↔ AST eşlemesi.
- `OIR_AST_NODE_REGISTRY.md` — 1–99 sabit AST node ID ve field kayıtları.
- `AST_OIR_COMPLETION_PLAN.md` — Codegen öncesi AST/Semantic/OIR kapıları.
- `OIR_SEMANTIC_AST.md` — semantic artifact sözleşmesi.
- `OIR_MODULE.md` — OIR_MODULE ve mevcut lowering kapsamı.
- `TASKS.md` / `PROJECT_STATUS.md` — görevler ve genel durum.

**Kural:** Bu tablo güncellenmeden bir AST düğümü veya pipeline aşaması tamamlandı olarak raporlanmaz.
