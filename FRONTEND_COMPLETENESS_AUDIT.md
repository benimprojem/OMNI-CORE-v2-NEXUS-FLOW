# Frontend Completeness Audit — Lexer, Parser, AST and Semantic Stage

**Status:** Audit plan and initial findings; implementation inventory not yet complete  
**Branch:** `refactor/c-core-roadmap`  
**Purpose:** Establish a verified source-language frontend contract before implementing production IR lowering.

## 1. Why this audit is required

The current frontend has grown incrementally. The AST tag list includes a broad set of language constructs, but the presence of a token or AST enum value does not prove that the lexer, parser, AST payload, semantic checker, serializer, artifact validator and tests all implement that construct consistently.

The compiler must not treat a declared enum value as evidence of a completed feature. Every language feature needs a traceable path from source spelling to a validated semantic artifact. Missing, partially implemented or inconsistent paths must be fixed before the feature is admitted to OIR lowering.

This audit is independent of the legacy AST-based code generator. The target is a clean, artifact-driven frontend whose output can be consumed by the new SSA lowerer.

## 2. Required feature traceability

For each language construct and operator, record these layers separately:

1. **Lexical definition:** token ID, spelling, precedence-independent lexical behavior, comments/strings/escapes and source span.
2. **Parser rule:** grammar production, associativity/precedence, ambiguity resolution and recovery behavior.
3. **AST representation:** node tag, complete payload definition, child ownership, list representation and source provenance.
4. **AST wire codec:** stable node tag, field schema, bounded serialization/deserialization and validation.
5. **Semantic behavior:** name/scope resolution, type rules, conversions, value category, mutability, ownership and diagnostics.
6. **Semantic artifact:** required annotations/relations/conversions and cross-reference validation.
7. **OIR lowering:** explicit supported mapping, or explicit unsupported diagnostic.
8. **Tests:** lexer, parser, AST round-trip, semantic positive/negative, artifact mutation, integration and cross-platform tests.

A feature is **complete** only when all required layers are implemented and tested. A feature may be marked **syntax-only**, **semantic-pending**, **artifact-pending**, **lowering-pending** or **unsupported**; do not label it complete based only on parser acceptance.

## 3. Initial observations requiring verification

The current AST header declares a large set of tags, including declarations, expressions, control flow, handler/flow operations, concurrency and hardware-facing operations. The visible payload declarations cover only a subset of these tags directly. Some tags may intentionally share a payload layout, but that must be explicitly documented and checked in parser construction, AST serialization and AST validation.

The semantic result model already has stable tables for scopes, types, symbols, annotations, relations and conversions. However, the current semantic checker does not yet provide complete durable annotations for every AST kind. Its expression typing helper handles a limited set of expression categories; control-flow and specialized NEXSUS Flow constructs need a more systematic semantic contract.

The lexer and parser are independent pipeline stages, but their end-to-end coverage has not yet been established by a complete feature matrix. Existing tests must be mapped to feature IDs rather than assumed to cover every declared token or AST tag.

These are audit findings, not a declaration that every unlisted construct is broken. Each must be confirmed against implementation and tests.

## 4. Audit work packages

### A. Lexer

- Enumerate every token ID and its source spelling.
- Compare token definitions with lexer dispatch and parser consumption.
- Check longest-match behavior for overlapping operators (including multi-character NEXSUS Flow operators).
- Check numeric literal forms, separators, exponent notation, suffixes, forced-width markers and malformed-number diagnostics.
- Check string/character escapes, UTF-8 policy, comments, whitespace and end-of-file behavior.
- Verify byte offsets and line/column spans for CRLF, LF, tabs and multibyte UTF-8.
- Check that every token stream is deterministic and has explicit bounds.
- Add positive and negative tests for every token family.

### B. Parser and grammar

- Build a grammar inventory from parser functions and token use.
- Verify precedence and associativity for every unary, binary, assignment, range, flow and conditional operator.
- Check declaration forms, type syntax, generic/aggregate forms if present, function parameters/returns and module imports.
- Check all control-flow forms, labels/jumps and loop variants.
- Check NEXSUS-specific handler, flow, area/zone, ownership and intent syntax independently; do not map them to generic syntax without a language rule.
- Verify parser recovery does not silently discard valid following statements.
- Verify every successful parse has a complete AST and every rejected construct has a useful source diagnostic.

### C. AST model and codec

- Create a registry table for every AST tag and its exact payload layout.
- For each tag, specify required/optional child fields, scalar fields, string ownership and linked-list semantics.
- Identify tags without an explicit payload type and document shared layouts where intentional.
- Validate child-pointer ownership and destruction symmetry to avoid leaks/double frees.
- Ensure serializer and deserializer cover every registered tag and reject unknown required tags.
- Ensure the wire format is field-by-field and independent of native pointer size, enum width, padding and endianness.
- Preserve stable node IDs and source spans through AST artifact round-trip.
- Add malformed-input, truncation, excessive-depth and invalid-child-reference tests.

### D. Semantic analysis and semantic artifact

- Require an annotation or explicit non-value-node classification for every AST node that affects executable semantics.
- Record resolved symbol IDs for every identifier use, including nested/member/index/call contexts.
- Record canonical types for every value-producing expression.
- Define lvalue/rvalue/function-designator categories consistently.
- Define declaration, mutability, constness, hard-type and conversion rules.
- Complete function signature, parameter, return and call-result typing.
- Define branch/loop scopes and declaration visibility.
- Define control-flow validation for return completeness, unreachable code policy, break/continue targets and labels.
- Define explicit effect/ownership annotations for handler and NEXSUS Flow operations.
- Ensure semantic artifact validation rejects missing mandatory annotations, dangling IDs and inconsistent scope/type relations.

### E. Frontend-to-OIR readiness

- Keep the frontend artifact-only: no AST or semantic pointers cross the stage ABI.
- Require the lowerer to consume only a validated `SEMANTIC_AST` artifact.
- Use block parameters as the canonical SSA merge model; each predecessor edge supplies the ordered incoming values.
- Reject unsupported constructs explicitly; never silently skip AST nodes.
- Add end-to-end tests from source through `SOURCE_UNIT → TOKEN_STREAM → AST → SEMANTIC_AST`.
- Only enable a lowering feature after its frontend traceability row is complete.

## 5. Initial priority order

1. Generate the token/operator inventory and compare lexer output against parser expectations.
2. Generate the AST tag-to-payload-to-serializer-to-validator matrix.
3. Generate the semantic AST tag-to-annotation coverage matrix.
4. Fix structural inconsistencies and incomplete codecs before adding new lowering opcodes.
5. Add missing semantic metadata required by the first scalar OIR subset.
6. Add complete control-flow semantic metadata before enabling CFG lowering.
7. Audit specialized NEXSUS Flow syntax and semantics before mapping it into OIR.

## 6. Acceptance criteria

The audit is complete only when:

- Every token has a documented lexer and parser status.
- Every AST tag has a documented payload schema and serializer/validator status.
- Every executable AST construct has a defined semantic contract and required artifact annotations.
- All unsupported or incomplete constructs are explicitly listed.
- Tests cover positive, negative, malformed and round-trip cases for each supported feature family.
- CI passes on the supported platforms and sanitizer configurations.
- The feature matrix is maintained as code evolves.

No claim of full language or frontend completeness may be made until these criteria are met.


## 6. İlk kaynak incelemesinden doğrulanan bulgular

Bu bölüm kaynak kodunun doğrudan incelenmesiyle doğrulanan mevcut durumdur. Henüz dil spesifikasyonu gerektiren davranışlar için varsayım yapılmamıştır.

### 6.1 Lexer

- `TOKEN_TRUE` ve `TOKEN_FALSE` türleri ile Parser'ın boolean literal AST üretimi mevcut; ancak Lexer'ın `identifier_type()` fonksiyonu bu iki anahtar kelimeyi tanımıyordu. **Düzeltildi:** `true` ve `false` artık ayrılmış token olarak tanınıyor; `test_lexer.c` kapsamına eklendi.
- Sayı tarayıcısı şu anda yalnızca ondalık rakam dizisi ve ardından isteğe bağlı basit `.digits` biçimini tüketiyor. Hexadecimal, üs gösterimi, ayırıcılar ve tip suffix/forced-width biçimleri için tarama desteği bu kod yolunda görünmüyor. Kabul edilecek tam sayı sözdizimi `syntax.md` ve numeric spec ile eşleştirilerek kararlaştırılmalı.
- String tarayıcısı kapanış tırnağını ararken escape durumunu izlemiyor. `\\"` gibi kaçışlı tırnakların davranışı hatalı olabilir. Escape sözleşmesi belirlenerek düzeltilecek.
- `ctype` fonksiyonlarına `char` doğrudan geçiriliyor. UTF-8 baytları signed-char platformlarında negatif olabileceğinden `isalpha/isalnum/isdigit` çağrıları için güvenli unsigned-char dönüşümü veya ASCII odaklı tarayıcı gerekli.
- `TOKEN_TRUE/FALSE` dışındaki bazı token enum üyelerinin Lexer tarafından üretildiği ve Parser tarafından tüketildiği eşleşme henüz otomatik olarak doğrulanmadı.

### 6.2 Parser ve context yaşam döngüsü

- `parser_init_source()` tüm context alanlarını başlatıyor; dosya tabanlı `parser_init()` ise `ast_root`, `file_handle` ve token başlangıç alanlarının tamamını başlatmıyor. Normal `parser_run()` akışında AST kökü atansa da AST allocation başarısızlığı gibi erken dönüş yollarında `parser_destroy()` başlatılmamış `ast_root` değerini kullanabilir. İki initializer tek bir ortak başlatma yordamına taşınmalı.
- `parser_attach_token_stream()` sıfır uzunluklu ve EOF ile sonlanmayan akışları reddediyor; bu koruma doğru.
- `peek_token_ahead()` içinde `n <= 0` çağrısı yapılırsa yerel `Token t` başlatılmadan döndürülür. Çağrı noktaları incelenmeli; fonksiyon ya pozitif mesafe şartını doğrulamalı ya da güvenli bir varsayılan sağlamalı.
- Parser'daki AST üretimlerinde `ast_create()` başarısızlığının her yerde kontrol edildiği henüz doğrulanmadı.

### 6.3 AST ve eski AST codec

- AST enum'undaki 80 düğüm türünden 78'i allocation, destroy, serialize ve deserialize switch'lerinde açıkça ele alınıyor. Kalan `AST_BREAK` ve `AST_CONTINUE` payload taşımayan temel düğümler; bu iki düğümün yalnızca temel alanlarla taşınması beklenen davranış olarak ayrıca test edilmeli.
- Eski `ast_serialize()` akışı native C enum/int/long long/double temsillerini doğrudan dosyaya yazıyor. Bu nedenle platformlar ve ABI'ler arasında taşınabilir, sürümlenmiş OIR formatı değildir; yalnızca geçiş dönemi AST codec'i olarak görülmeli.
- AST codec round-trip kapsamı her AST türü için ayrı fixture ile ölçülmeli; yalnızca birkaç örnek programın başarılı round-trip yapması tam kapsama kanıtı sayılmamalı.

### 6.4 Semantik AST kapsaması

`semantic_check()` switch'inde 80 AST türünün 69'u için açık case mevcut. Açık case bulunmayan türler:

- `AST_ENUM_VARIANT`
- `AST_EXTERN_DECL`
- `AST_POSTFIX_EXPR`
- `AST_MEMBER_ACCESS`
- `AST_INDEX_ACCESS`
- `AST_TUPLE`
- `AST_IF`
- `AST_BREAK`
- `AST_CONTINUE`
- `AST_ASM_BLOCK`
- `AST_ZONE`

Bu listede özellikle `AST_IF`, `AST_MEMBER_ACCESS`, `AST_INDEX_ACCESS`, `AST_TUPLE` ve `AST_POSTFIX_EXPR` için default dalının alt ifadeleri semantik denetimden geçirmemesi riski var. Her türün semantik davranışı ayrı ayrı tanımlanmalı; salt recursive traversal da tek başına yeterli sayılmamalı.

Ek olarak `AST_CAST`, `ASTUnaryExpr` düzeninde tanımlanmış olmasına rağmen semantik switch'inde `ASTCall` kullanan intrinsic grubu içine alınmış. Bu, farklı AST payload düzenlerinin aynı cast altında yorumlanması anlamına geliyor ve olası yanlış alan okuması riski taşıyor. Cast için kendi AST payload'ına uygun ayrı semantik dal oluşturulmalı.

### 6.5 Sonraki düzeltme sırası

1. Parser context başlatma ve lookahead sınır güvenliği.
2. `AST_CAST` semantik payload uyuşmazlığının düzeltilmesi.
3. `AST_IF`, üye/index erişimi, tuple ve postfix semantik traversal/kuralları.
4. Lexer literal grammar'ının dil spec'iyle eşleştirilmesi; escape ve UTF-8 güvenliği.
5. AST türlerinin her biri için parse → destroy → codec → semantic test matrisi.


### 6.6 Kritik AST payload uyuşmazlığı: intrinsic parser

İlave çapraz kontrol, yalnızca semantik katmanla sınırlı olmayan daha yüksek öncelikli bir sorun ortaya çıkardı:

- `parse_intrinsic_call()`, aldığı tür ne olursa olsun `ASTCall*` olarak `ast_create(type)` sonucunu yorumluyor ve `callee/args` alanlarını yazıyor.
- Ancak `get_node_size()` içinde `AST_CAST`, `ASTUnaryExpr` boyutunda; `AST_PANIC` ise `ASTPanic` boyutunda ayrılıyor.
- Aynı türler AST destroy/serialize/deserialize kodlarında da sırasıyla unary-expression ve panic-message payload'ları olarak ele alınıyor.
- Buna karşın Parser, `cast(...)` ve `panic(...)` ifadelerini `parse_intrinsic_call()` üzerinden oluşturuyor. Bu durum en azından bu düğüm türlerinde payload sözleşmesinin katmanlar arasında çeliştiğini; allocation boyutu ve alan yorumlaması açısından bellek taşması/yanlış pointer okuma riski bulunduğunu gösteriyor.

Bu düğümlerin doğru AST biçimi dil sözdizimi ve amaçlanan cast/panic semantiğiyle netleştirilmeden tek bir switch'e eklenerek yamalanmamalı. Öncelik 0 olarak ele alınmalı: tek bir payload şeması seçilmeli ve allocation, parser, destroy, codec, semantic checker ve test fixture'ları aynı şemaya geçirilmelidir.


### 6.7 CAST/PANIC payload düzeltmesi

İlk kaynak bulgusu kod düzeyinde ele alındı ve AST/OIR sınırına kadar tamamlandı:

- `AST_CAST` artık kendi `ASTCast` payload'ına sahip: `value` ve `target_type` ayrı çocuk düğümlerdir. Dil referansındaki `cast(value, type)` biçimi böylece AST'de iki ayrı anlamlı alanla temsil edilir.
- Parser artık `cast()` ifadesini `ASTCall` veya `ASTUnaryExpr` gibi yorumlamıyor; hedef türü identifier düğümü olarak kuruyor.
- Semantik katman dönüşümün değer ifadesini denetliyor ve hedef tür adının tanımlı olup olmadığını doğruluyor.
- AST allocation, printer, destroy, serialize ve deserialize yolları yeni payload ile eşitlendi.
- OIR frontend writer, reader, field schema ve deterministic index artık CAST değer/hedef çocuklarını iki node reference olarak işliyor.
- `panic(message)` artık `ASTPanic` payload'ı ile oluşturuluyor; `ASTCall` alanları küçük `ASTPanic` belleğine yazılmıyor.
- Parser testine `cast(1, i32)` ve `panic("boom")` için AST payload doğrulaması eklendi. AST codec fixture'ı da CAST için yeni şemaya uyarlandı.

Doğrulama:
- `ast_rejects_malformed_streams` testindeki ilk regresyon, eski ASTUnaryExpr fixture'ının yeni ASTCast payload'ına göre güncellenmesiyle giderildi.
- OIR field-schema testinin eski CAST token alanı beklentisi de yeni iki çocuk referansı sözleşmesine göre güncellendi.
- Commit `d74168fedb6e3883ffa64ce398439283a2caea1c` için Windows, macOS, Ubuntu, ASan/UBSan ve Valgrind kontrollerinin tamamı başarılı oldu.
- Sonraki küçük semantik hedef-tür doğrulama değişikliği ayrı commit `c2c2ee6b3f44d5efeab53d4d0c8d0f6a208df1de` olarak CI'ya gönderildi; sonucu ayrıca doğrulanmalı.

Not: Bu değişiklikler legacy AST codec ve OIR AST payload sözleşmesini etkilediğinden, eski AST dump dosyaları ile sürüm uyumluluğu ayrıca ele alınmalıdır. Bu codec kalıcı OIR modül formatı yerine geçmez; OIR format sürümleme/uyumluluk sözleşmesi bağımsız olarak korunmalıdır.


## 6.8 Parser context ve lookahead güvenliği

**Durum: İlk güvenlik düzeltmeleri uygulandı; allocation-failure denetimi devam ediyor.**

- `parser_init_source()` ve `parser_init()` artık tek bir ortak context initializer kullanıyor.
- Her iki başlatma yolu da `file_handle`, kaynak konumu, token başlangıç konumu, AST kökü, hata listesi, diagnostic collector ve token-stream alanlarını deterministik biçimde sıfırlıyor.
- Dosya başlatıcısı artık `filename == NULL` durumunu güvenli biçimde reddediyor; kaynak belleği ayrıldıktan sonra context tahsisi başarısız olursa kaynak belleği serbest bırakılıyor.
- `parser_run()`, NULL/eksik kaynak bağlamını ve aynı context üzerinde ikinci kez çalıştırılmayı reddediyor. Parser'ın `current` ve `previous` token alanları ilk `advance()` öncesinde başlatılıyor.
- `peek_token()` ve `peek_token_ahead()` ortak, geri-alımlı lookahead uygulamasına taşındı. `n <= 0` için deterministik EOF tokenı dönüyor; kaynak konumu ve token-stream indeksi dahil lexer durumu geri yükleniyor.
- Parser diagnostic Error-list tahsisleri başarısız olduğunda NULL üzerinden yazma yerine temiz şekilde bırakılıyor. Diagnostic collector'a ekleme yolu bundan bağımsız kalıyor.
- Test: boş kaynakla context oluşturma ve `parser_run()` çağırmadan context'i yok etme; eksik filename reddi; mevcut parser testleri.

**Açık işler / bulgular:**

- Parser içinde çok sayıda `ast_create()`, `malloc()` ve `strdup()` çağrısı hâlâ doğrudan NULL kontrolü olmadan kullanılıyor. Bunlar parser genelinde tutarlı OOM hata yayılımına dönüştürülmeli.
- Lexer'da string escape işleme, karakter literalinin tek Unicode scalar/byte tanımı, sayısal literal kapsamı ve UTF-8 karakterlerinin `ctype` fonksiyonlarına güvenli aktarımı ayrıca ele alınmalı.
- `peek_next()` ve lexer ilerletme sınırları için uç durum testleri eklenecek.
- Bu commitlerdeki CI sonuçları tamamlanmadan tüm platformlar için başarılı kabul edilmeyecek.

## 6.9 Lexer literal tarama ve sınır güvenliği

**Durum: İlk düzeltmeler uygulandı; sayısal literal ve UTF-8 tanımı açık.**

- Lexer advance() artık kaynak sonundayken konumu artırmıyor. Bu, son karakteri eksik olan literal taramalarının kaynak uzunluğunun ötesine geçmesini engelliyor.
- peek_next() sınır kontrolü toplama taşmasına dayanmayacak şekilde kalan uzunluk üzerinden yapılıyor.
- ctype çağrılarında karakterler unsigned char olarak değerlendiriliyor; yüksek bitli UTF-8 baytlarının negatif char nedeniyle tanımsız davranış üretmesi engelleniyor. Bu değişiklik tek başına Unicode identifier desteği anlamına gelmiyor.
- String taraması kaçış karakterinden sonraki baytı tüketiyor; kaçırılmış çift tırnak string'i erken bitirmiyor.
- Karakter literalinde kaçış işaretinden sonra EOF gelmesi güvenli hata üretiyor; lexer EOF konumunu aşmıyor.
- Regresyon testleri: kaçırılmış tırnak içeren string, sonunda kaçış işareti bulunan eksik karakter literal'i, EOF sonrası tekrar token alma.

**Açık işler:**

- Sayısal literal grameri hâlâ yalnızca mevcut ondalık tam sayı ve kesirli biçimi kapsıyor. Hex/binary, üs, ayraç, tür eki ve !32 gibi zorunlu genişlik işaretlerinin lexer grameri spesifikasyonla eşleştirilecek.
- Unicode identifier desteği, karakter literalinin byte mı Unicode scalar mı olduğu ve escape biçimlerinin tam listesi dil spesifikasyonunda karara bağlanmadan genişletilmeyecek.
- Lexer token uzunluğu int alanına yazılıyor; çok büyük kaynak/token boyutları için açık üst sınır ve hata davranışı ayrıca tanımlanacak.
## 6.10 Parser primary-expression allocation safety and README synchronization

**Durum: Birincil literal/identifier üretimi düzeltildi; Parser geneli henüz tamamlanmadı.**

- parse_primary() içindeki number/string/char literal üretimi ortaklaştırıldı; AST düğümü ve literal metni tahsisleri ayrı ayrı kontrol ediliyor.
- true/false literal'lerinde AST düğümü ve metin tahsisi kontrol ediliyor.
- Identifier düğümünde AST ve ad metni tahsisi kontrol ediliyor.
- Her başarısız metin tahsisinde kısmi düğüm ast_destroy() ile temizlenip NULL dönülüyor.
- README Bölüm 2.3.1'e mevcut literal tarama kuralları ve cast(value, type)/panic("message") biçimleri eklendi. Desteklenmeyen numeric biçimler açıkça taslak/henüz uygulanmadı olarak işaretlendi.

**Kalan kritik işler:**

- parse_primary() içindeki loop/tuple ve intrinsic alt yolları dahil tüm AST tahsisleri kontrol edilmeli.
- parse_postfix(), ikili operatör katmanları, declaration parser'ları ve statement parser'larında allocation failure kısmi ağaç sızıntısı oluşturmamalı.
- Parser genelinde OOM durumunu ayrı bir fatal durum olarak izleyip parse döngüsünün hatayı sıradan syntax error gibi ele alması önlenmeli.
- Mevcut AST codec ve OIR AST payload sözleşmesindeki CAST alan değişikliği için artifact versioning/compatibility kararı hâlâ açık.

Bu değişiklikler NEXSUS Flow gramerine yeni bir operatör veya üretim kuralı eklemiyor; Lexer'ın mevcut string/character tarama davranışını README ile eşitliyor.
## 6.11 Postfix, unary ve binary expression AST sahipliği

**Durum: İfade ağacının önemli bir bölümü OOM ve parse-hatası temizliği açısından güçlendirildi.**

- Postfix member access, call ve ++/-- düğümlerinde AST tahsisi kontrol ediliyor; başarısızlıkta daha önce oluşturulmuş ifade ağacı temizleniyor.
- Call argümanları ASTCall içine eklendikçe sahiplik kaydediliyor. Sonraki argüman parse edilemezse callee ve önceki argümanlar birlikte yok ediliyor.
- Prefix unary operatörlerde düğüm tahsisi başarısızsa operand ağacı temizleniyor.
- Çarpma/bölme, toplama/çıkarma, karşılaştırma, eşitlik ve range düğümleri ortak parser_make_binary() kurucusuna taşındı. İki operandın sahipliği kurucuya devrediliyor; düğüm tahsisi başarısızsa ikisi de temizleniyor.
- Sağ operand parse edilemediğinde daha önce oluşturulmuş sol operand artık terk edilmiyor.

**Kalan işler:**

- Nexus Flow pipe/flow/unpack, tuple, loop, intrinsic, declaration ve statement AST kurucuları aynı sahiplik modeline taşınmalı.
- Parser OOM durumunu syntax error'dan ayırmalı; parse döngüsü bellek hatasında ilerlemeye çalışmamalı.
- Allocation-failure injection altyapısı eklenerek her tahsis noktasında cleanup test edilmeli.

## 6.12 Loop, tuple, intrinsic ve Nexus Flow AST sahipliği

**Durum: İlk OOM/parse-hatası temizliği uygulandı; Parser geneli tamamlanmadı.**

- `loop(...)` AST düğümü tahsisi kontrol ediliyor. Başlangıç/koşul/artış/koleksiyon ifadelerinden biri üretilemezse kısmi loop ağacı temizleniyor.
- Parantezli tuple ve brace tuple üretiminde ASTTuple tahsisi kontrol ediliyor. Tuple elemanları eklendikçe ağaç sahipliği kaydediliyor; sonraki eleman parse edilemezse tüm tuple temizleniyor.
- Boş brace tuple için AST tahsisi kontrol ediliyor.
- Intrinsic çağrılarında ASTCall ve `reg.member` identifier tahsisleri kontrol ediliyor. Argümanlar eklendikçe call düğümünün sahipliğine geçiriliyor; parse hatasında kısmi çağrı temizleniyor.
- Nexus Flow pipe/flow düğümü veya unpack düğümü tahsis edilemezse operand ağacı temizleniyor.
- Capture ve assignment düğümlerinde AST tahsisi, isim kopyası ve sağ taraf parse sonucu kontrol ediliyor; başarısızlıkta sahip olunan operandlar temizleniyor.

**Açık işler:**

- `parse_defer`, `parse_use`, `parse_thread_block`, declaration/statement parser'ları ve kalan tüm `ast_create/malloc/strdup` çağrıları denetlenmeli.
- OOM fatal durumu henüz ParserContext düzeyinde ayrı bir durum değil. Hata yayılımı ve allocation-failure injection testleri tamamlanmadı.
- Mevcut değişiklikler yeni NEXSUS Flow grameri eklemiyor; README'ye yeni syntax örneği eklenmesini gerektirmiyor.

## 6.13 AREA / ZONE bellek modeli tasarım girdisi

**Durum: Tasarım notu eklendi; dil/semantic/OIR sözleşmesi henüz uygulanmadı.**

- `AREA_ZONE_MEMORY_MODEL.md` belgesi `area` ve `zone` için OS bağımsız mantıksal bellek alanı yaklaşımını tanımlar.
- `area`, çekirdek veya çekirdek grubu yakınlığını; `zone`, daha geniş paylaşımlı erişim alanını ifade eden mantıksal niyet olarak ele alınır.
- Bu sınıflar doğrudan L1/L2 veya L3/L4 garantisi değildir. L1/L2'nin private/group-shared oluşu ve daha üst cache seviyelerinin paylaşımı hedef topolojisine göre değişir.
- Bare-metal runtime ile OS adapter aynı dil kontratını uygulamalıdır; OS entegrasyonu dil semantiğini değiştirmemelidir.
- Fiziksel havuz seçimi, scope, paylaşım, fallback, sahiplik, lifetime ve memory-ordering kararları açık bırakıldı. Bu aşamada yeni syntax veya fiziksel cache garantisi eklenmedi.


## 6.14 Defer, use, thread-block ve statement AST sahipliği

**Durum: İlgili parser yollarında ilk tahsis/temizlik kontrolleri uygulandı.**

- `defer` düğümü ve gövdesi kontrol ediliyor; gövde üretilemezse kısmi düğüm temizleniyor.
- `use` import listesinde modül adı ve alias kopyaları kontrol ediliyor. Listeye eklenmemiş düğüm ve daha önce oluşturulmuş liste, hata halinde ayrı ayrı temizleniyor.
- Thread-block düğümü tahsis edilemezse çağıranın verdiği isim serbest bırakılıyor; gövde üretilemezse düğüm ve adı temizleniyor.
- Boş statement, variable/function/struct/group declaration dispatch, listen, jump, halt, intent ve panic yollarında AST tahsis kontrolleri eklendi.
- Return, block ve variable declaration oluşturma yollarında AST tahsisi, isim/tip kopyası ve initializer/return expression başarısızlıklarında cleanup eklendi.
- Bu değişiklikler mevcut grameri değiştirmiyor; README'de yeni syntax güncellemesi gerektirmiyor.

**Kalan işler:**

- Function/parameter, struct/union/enum/group body parser'larında kalan doğrudan tahsisler kontrol edilmeli.
- Statement label oluşturma ve parser genelinde OOM'un syntax error'dan ayrılması tamamlanmalı.
- Allocation-failure injection ile her tahsis noktasının cleanup yolu doğrulanmalı.


## 6.15 Function, parameter ve statement-label AST sahipliği

**Durum: İlk düzeltmeler uygulandı; diğer declaration türleri ayrıca denetlenecek.**

- Function ve parameter AST tahsisleri kontrol ediliyor. Parametre listesi fonksiyon düğümüne eklendikçe sahiplik fonksiyonda tutuluyor; sonraki parametre veya type-name tahsisi başarısızsa tüm liste tek kökten temizleniyor.
- Fonksiyon adı, parametre adı, parametre tipi ve dönüş tipi kopyaları kontrol ediliyor.
- Lambda biçimindeki `> expression` gövdesinde expression, block ve return düğümlerinin her hata yolundaki sahipliği açık hale getirildi.
- Normal fonksiyon gövdesi üretilemezse kısmi fonksiyon ağacı temizleniyor.
- Statement label üretiminde tek elemanlı tuple açma işlemi artık sonsuz döngüye girmiyor. Label adı kopyalandıktan sonra yalnızca kaynak olarak kullanılan ifade ağacı temizleniyor; label düğümü veya ad kopyası tahsis edilemezse sahiplik temizleniyor.

**Kalan işler:**

- Struct/union/enum/group ve method parser'larında allocation ve child ownership kontrolleri.
- Parser OOM'unu fatal durum olarak ayırma ve tahsis hata enjeksiyonu testleri.


## 6.16 HALT tekil ve blok kapsamlı kaynak temizliği sözleşmesi

**Durum: Kullanıcı tarafından semantik ayrım netleştirildi; dokümantasyon güncellendi, compiler desteği henüz yok.**

- `resource ?;`: herhangi bir uygun statement noktasında belirli bir kaynak/handler'ı kapatır veya temizler. Örnek kaynak sınıfları: açık dosya, tablo/collection handle, yüklenmiş DLL.
- `}?;`: hemen önceki bloğun sonunda, o blokta açılmış ve hâlâ o bloğun sahipliğinde kalan tüm açık kaynakları topluca kapatır.
- Blok dışına `_>` ile taşınan kaynak eski bloğun cleanup kümesinden çıkar. İç blok, dış kapsamdan ödünç alınmış kaynağı açık sahiplik devri olmadan kapatamaz.
- Kapatılan kaynak geçersiz olur; sonraki kullanım semantic error olmalıdır. Cleanup deterministik ve en fazla bir kez çalışmalıdır.
- Cleanup hatalarının sıralaması ve hata birleştirme davranışı runtime sözleşmesinde ayrıca tanımlanmalıdır.

**Mevcut uygulama açığı:**

- Lexer `?;` tokenını tanıyor ve Parser bunu isteğe bağlı expression içeren `AST_NEXUS_HALT` olarak oluşturuyor.
- `ASTHalt` şu an kaynak hedefini ayrı bir alan olarak taşımıyor.
- `ASTBlock` üzerinde blok sonu cleanup işareti veya cleanup kümesi bulunmuyor.
- Semantic analizde kaynak sahipliği/cleanup listesi ve use-after-halt denetimi henüz yok.
- Bu nedenle `resource ?;` ve `}?;` örnekleri dokümante edilmiş hedef sözleşmedir; uygulandı olarak işaretlenmemelidir.
- Sonraki tasarım adımı AST/semantic/OIR temsilini belirlemek; mevcut `AST_NEXUS_HALT` ve OIR AST codec şemasını sessizce değiştirmemek. OIR AST payload sürümlemesi incelenmelidir.

Bu semantik açıklaması README ve operatör formal spec ile eşzamanlı güncellendi.
