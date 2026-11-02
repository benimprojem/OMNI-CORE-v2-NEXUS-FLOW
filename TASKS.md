# OmniCore SDK — Güncel Görevler

**Ana uygulama dili:** C  
**C++:** Korunan referans uygulaması  
**Güncel plan:** ROADMAP.md  
**Durum raporu:** PROJECT_STATUS.md

> Görevler yalnızca kod yazıldığı için tamamlandı sayılmaz. Build ve test kanıtı gereklidir. Eski yüzde tamamlanma iddiaları bu listede kullanılmamaktadır.

## P0 — Temel güvenilirlik

> CMake/CI altyapısı mevcut. Windows, Ubuntu ve macOS'ta 21/21 CTest başarılı; Ubuntu GCC ASan/UBSan ve Valgrind koşuları da 21/21 geçti.

- [x] C kaynaklarının temiz checkout üzerinde derlenmesini sağla (GitHub Actions: Windows, Ubuntu, macOS).
- [x] parser_build.bat (C++ derlemesi) ile C uygulamasının build akışını açıkça ayır.
- [x] C OCC için Windows build hedefi ve Linux/macOS build hedefleri oluştur.
- [x] Parser, semantic ve codegen için ayrı build hedefleri oluştur.
- [x] Config okuyucuyu bölüm/satır sırasından bağımsız hale getir (bölüm başlıklarından sonra gelen `language = "tr"` ayarıyla tanı testi).
- [x] CLI argüman ayrıştırmasını kaynak dosyası ve seçenek sırasına dayanıklı yap (hedef öncesi/sonrası, eksik hedef, bilinmeyen seçenek ve birden fazla kaynak testleri).
- [x] Path işlemlerindeki NULL pointer karşılaştırmasını kaldır; runtime config/lang/target/codegen yollarını executable konumundan çöz ve farklı CWD'den test et.
- [x] Parser/semantic/codegen hata durumlarını OCC çıkış koduna aktar (kod yolu mevcut; codegen yükleme hatası için ayrı failure-injection testi bekliyor).
- [x] Lexer, parser ve semantic için ayrı CTest programları ekle; token/konum, geçerli-geçersiz syntax, partial AST cleanup, geçerli program ve undefined symbol testlerini üç platformda doğrula (13/13).
- [ ] AST ve semantic bellek sahipliği/free kurallarını doğrula.

## P0 — AST ve kaynak girdisi güvenliği

- [x] AST deserializer tüm alan okumalarında tam okuma kontrolü yapsın ve hatayı üst seviyeye taşısın.
- [x] Geçersiz node türlerini, negatif/aşırı string uzunluklarını, aşırı node sayısını ve aşırı iç içe derinliği reddet.
- [x] Sibling node listelerini iteratif oku; normal uzun statement listeleri derinlik sınırına takılmasın.
- [x] AST_UNPACK ve AST_NEXUS_HALT serialization/deserialization bellek düzenlerini düzelt.
- [x] AST_EXTERN_DECL için AST_FUNC_DECL ile ortak serialization/deserialization ve destroy dallarını ekle; alanların kaybolmasını önleyen round-trip regresyon testi ekle (üç platform CI: 10/10).
- [x] AST enum'undaki tüm düğüm etiketleri için allocation/type/line/col/next smoke round-trip testi ekle.
- [x] AST_LITERAL içindeki token type alanının serialization/deserialization dışında kaldığını düzelt; type, lit_type, int, float ve string payload alanlarını test et.
- [x] Truncated header, geçersiz node, bozuk string, özel node round-trip ve 700 sibling testlerini ekle.
- [x] Kaynak dosyası okuyucusunda fseek/ftell/fread sonuçlarını doğrula.
- [x] AST türlerinin tüm serialized alanları için field-level round-trip fixture matrisi ekle; her türün serialize → deserialize çıktısı byte-equality ile karşılaştırılıyor (Windows/Ubuntu/macOS CI 10/10).
- [x] GCC ASan/UBSan seçeneği ve Ubuntu CI işi ekle; semantic scope içindeki type_name sızıntısı bulundu ve giderildi, sanitizer altında 10/10 test geçti.
- [x] Valgrind Linux CI işi ekle; 10/10 test definite/indirect leak kontrolüyle geçti.
- [x] AST serializer yazma hatalarını API dönüş değeriyle raporla; parser_save_ir serialization ve fclose hatalarını kontrol etsin, OCC kayıt başarısızsa codegen'e geçmesin.

  > AST field-level round-trip matrisi Windows/Ubuntu/macOS'ta; ASan/UBSan ve Valgrind Ubuntu'da doğrulandı. Daha geniş semantic allocation-failure ve ownership testleri açık.

## P0 — Dil ve semantik

- [ ] Token envanterini dil spesifikasyonuyla karşılaştır.
- [x] Enum'da tanımlı fakat lexer'da eşlemesi olmayan 13 token/primitive type'ı tanı (asm, fastexec, break, echo/send/receive, f32/f64, bool/bit/byte/hex/void/any); is_err yanlış uzunluk karşılaştırmasını düzelt.
- [x] Token başlangıç byte offset/line/column ve source span length alanlarını ekle; multiline string line sayımını düzelt ve test et.
- [x] Kapanmayan block comment'i TOKEN_ERROR olarak source span ile bildir; lexer'ın ardından EOF'a ulaşmasını test et.
- [ ] The 15 Keys operatörlerinin token öncelik ve çakışma testlerini ekle.
- [ ] AST düğüm sözleşmesini ve source location bilgisini sabitle.
- [x] Statement-level conditional action shorthand: `(condition) -> break;` / `continue;` ve diğer açık statement başlangıçları koşullu kontrol akışı olarak ayrıştırılır; `source -> target/process()` ve `-> { ... }` Nexus Flow pipe olarak kalır. Parser AST ve semantic loop-context testleri eklendi; CI run `41364158463` Windows/Ubuntu/macOS + ASan/UBSan + Valgrind başarılı.
- [ ] Scope, type promotion, ownership ve control-flow kurallarını testlerle doğrula.
- [ ] Hata ve warning API'sini ortaklaştır.

## Intent sistemi — ertelendi

- [x] Yalnızca kavramsal ayrımı kaydet: `!!=` açık derleyici talimatı, `@` gelecekteki AI katmanına niyet bildirimi.
- [ ] AI katmanı tasarlanmaya başlandığında Intent sistemini uçtan uca yeniden ele al: gramer, veri modeli, bağlam, AST/OIR, AI etkileşimi ve doğrulama.
- [ ] AI katmanı bulunmadan `@` için varsayımsal parser davranışı veya sabit intent kategorileri geliştirme.

> Bu başlık şimdilik aktif geliştirme sırasına alınmamıştır. Ayrıntılı tasarım, AI bileşeninin gerçek gereksinimleri ortaya çıktığında yapılacaktır.

## P0 — Ara temsil

- [x] AST serialization ile OIR container/payload kavramlarını ve API'lerini ayır.
- [x] Kanonik OIR v1 container sözleşmesini ve ilk payload schema'larını tanımla.
- [x] C OIR writer/reader geliştir ve CTest'e bağla.
- [x] Bozuk/truncated/uyumsuz OIR dosyaları için temel güvenlik testlerini ekle.
- [ ] C++ OIR2 ile özellik/format karşılaştırma tablosu oluştur; C++ hattı yalnızca tarihsel referans olarak tutulur.

## P1 — Backend ve optimizasyon

- [ ] Mevcut .target dosyalarını envanterle ve TARGET_MODEL.md sözleşmesiyle karşılaştır.
- [ ] Host OS/architecture tespiti ve host-default target çözümleyicisi tasarla.
- [ ] Windows/Linux/macOS target profillerini mevcut dosya formatına geriye uyumlu biçimde eşle.
- [ ] Target descriptor şemasını OS/arch/ABI/format/CPU feature alanlarına ayır.
- [ ] Host OS + host architecture varsayılan hedef çözümlemesini ekle.
- [ ] Windows, Linux ve macOS hedef matrisi ve ABI farklarını belgeleyip test et.
- [ ] x86-64 ve AArch64 için gerçek backend kapsamını ayrı ayrı belirle.
- [ ] CPU model/feature seçimini backend capability kontrolüne bağla.
- [ ] Target dosyası ile gerçek backend desteğini ayır.
- [x] Target parser'da bounded string parsing ve alan uzunluğu doğrulaması uygula.
- [x] Eksik target dosyası, bilinmeyen alan ve bilinmeyen feature için açık hata üret.
- [x] Target parser testlerini Windows/Ubuntu/macOS CI üzerinde doğrula (geçerli beş descriptor, eksik dosya, uzun satır/alan, eksik alan, duplicate key, unknown key/section, invalid arch/feature; 10/10 CTest).
- [x] Windows DLL, Linux .so ve macOS .dylib codegen yükleme/bağlama yollarını uygula (üç platform CI build/test).
- [x] Codegen modülü yüklenemediğinde veya IR kaydı başarısız olduğunda derlemeyi başarılı göstermeyen hata davranışını uygula.
- [ ] Target listeleme ve target dosyası çözümlemesini platformdan bağımsız hale getir.
- [ ] Optimizer pass altyapısını kur.
- [ ] Her optimizer pass için davranış eşdeğerliği testleri ekle.

## P2 — SDK ve araçlar

- [ ] occ.ocf yapılandırma gramerini tanımla.
- [ ] .nxr build akışını tasarla ve uygula.
- [ ] stdlib/dpslib bağımlılık çözümlemesini doğrula.
- [ ] Tekrarlanabilir paketleme ve kurulum akışı oluştur.
- [x] CI build/test hattı ekle (Windows, Ubuntu, macOS matrisi).

## P2/P3 — IDE ve isteğe bağlı özel işlemci

- [ ] OCE IDE'yi kararlı compiler API'sine bağla.
- [ ] Debugger ve OIR/source-map inceleyicisi geliştir.
- [ ] Runtime/threading özelliklerini test et.
- [ ] Gerekirse ileride özel işlemci target/backend eklentisi ekle; genel derleyici yolunu buna bağımlı yapma.

## Klasör yapısı

Hedef yapı PROPOSED_LAYOUT.md içinde tanımlıdır. Fiziksel taşıma, build betikleri ve testler yeni yollarda doğrulanmadan yapılmayacaktır.

## Sonraki tek iş

OCC CLI'de OIR Lowering → OIR Verifier → Codegen DLL hattı entegre edildi. Açık işler: OIR/Codegen kapsamını genişletmek, yeni assembly çıktısını legacy Codegen ile karşılaştırmak ve parity tamamlanınca legacy köprüyü kaldırmak.


## P0 — Modüler derleyici pipeline (uygulama devam ediyor)

- [ ] Her derleme aşaması için bağımsız shared library hedefi oluştur (Windows DLL / Linux SO / macOS dylib).
- [x] Draft sürümlenmiş C module ABI'sini tanımla: descriptor, stage ID, ABI version, artifact tür/sürüm, process/result ve ownership; host-side descriptor/invocation validation testlerini ekle. **ABI henüz freeze edilmedi; stage-specific parser adapter ve dynamic loader testleri açık.**
- [x] İlk job-owned diagnostic collector ve strict output gate C API'sini oluştur; severity count, source span ve owned text storage testlerini ekle.
- [x] Collector'ı ParserContext/SemanticContext'e bağla; OIR DIAGNOSTICS schema 1.0 codec'ini ekle ve round-trip/malformed-input testlerini CI'da doğrula.
- [x] Parser token diagnostics için half-open byte span + başlangıç/bitiş satır ve byte sütunlarını kaydet; tekrarlanan kod parçasının ikinci satırdaki konumunu doğrulayan regresyon testi ekle (Windows/Ubuntu/macOS CI başarılı).
- [x] Hata checkpoint'i için `OIR_VALIDITY_INVALID` SOURCE_UNIT + DIAGNOSTICS birleşik artifact writer'ı ve round-trip testi ekle (36/36 CTest; Ubuntu/macOS/Windows build-test hattı başarılı).
- [x] OCC host failure store: `occ-errors/job-<job_id>.oir` yoluna temp-file + rename ile atomik checkpoint; parser/semantic tanılarını kaynak baytlarıyla birlikte sakla, DLL'lere filesystem yetkisi verme (Windows/Ubuntu/macOS 36/36 CTest; ASan/UBSan ve Valgrind başarılı).
- [x] OCC host kaynaklı OIR yazma ve Codegen yükleme/sembol/çalıştırma hatalarını ayrı stage ID ve structured diagnostics olarak kaydet; hata artifact'ına dahil et.
- [x] İlk per-job CompilationContext: job ID, copied options, stop-after stage, lifecycle state ve job-owned diagnostics; strict output gate testleri.
- [ ] Hata/uyarı biriktirme ve mümkün olan aşamalarda devam etme davranışını uygula.
- [ ] Artifact geçerlilik durumlarını tanımla: valid / partial / invalid / none.
- [ ] Varsayılan strict output gate: error veya warning varsa final artifact yayımlama.
- [ ] `--stop-after <stage>` ve checkpoint artifact yazımını ekle.
- [x] OIR_MODULE tüketen ilk target-specific Codegen DLL'ini ortak `OccStageModuleV1` ABI'siyle ekle (OIR_ASSEMBLY 1.0 üretir; eski `codegen_run` henüz korunur).
- [ ] Legacy `codegen_run` ve host AST materialization köprüsünü parity testleri tamamlanmadan kaldırma; parity tamamlandığında kaldır.
- [x] Lexer, parser ve semantic analyzer'ı ayrı DLL/SO/dylib modüllerine ayır.
- [ ] Bağımsız derleme işlerini paralel çalıştır; modül thread-safety/reentrancy sözleşmesini test et.
- [ ] Kuyruk/back-pressure, job cancellation ve final output atomic publish testlerini ekle.
- [ ] IR, optimizer pass, target validation, backend, assembler ve linker aşamalarını ayrı modüllere böl.

> Mimari karar ve tasarım belgesi eklendi. Bu görevler henüz kodlandı veya CI ile doğrulandı olarak işaretlenmemiştir.

## Sonraki tek iş

OCC CLI'de Semantic → OIR Lowering → OIR Verifier → Codegen stage zinciri çalışıyor. Legacy AST materialization köprüsü yalnızca eski `codegen_run` parity yolu için korunuyor.


## P0 — OIR ortak artifact standardı (tasarım taslağı eklendi)

- [x] OIR v1.0 container binary layout'unu (magic, header byte offsets, numeric IDs, flags, section header) tanımla.
- [x] Container in-memory reader/writer C API'sini tanımla (opaque module ABI/ownership entegrasyonu sonraki iş).
- [x] Header/section bounds, truncation, bad magic, total-length mismatch, unknown required section, missing payload ve small-buffer testlerini ekle. [ ] Checksum algoritması ve fuzz/property testleri açık.
- [x] SOURCE_UNIT ve TOKEN_STREAM payload şemalarının ilk 1.0 taslağını tanımla (OIR_PAYLOAD_SCHEMAS.md).
- [x] SOURCE_UNIT schema 1.0 C codec ve round-trip/UTF-8/length güvenlik testlerini ekle.
- [x] TOKEN_STREAM schema 1.0 C codec, stable token ID doğrulaması, source-span/EOF kontrolleri ve diagnostic-reference testlerini ekle.
- [x] Lexer Token → OIR record ve OIR TOKEN_STREAM → parser Token handoff adapter'larını ekle; parser'ın pre-tokenized token array tüketimini uçtan uca test et.
- [x] AST payload schema 1.0 taslağını ve 80 node kind / field ID registry'sini tanımla.
- [x] Frontend AST tag'leri ile sabit OIR node-kind ID'leri arasında bağımsız mapping API'si ve 80/80 bijection testi ekle (Windows/Ubuntu/macOS, ASan/UBSan ve Valgrind CI başarılı).
- [x] Bounded AST payload codec, graph/reference validation ve frontend round-trip testlerini tamamla (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı); parser checkpoint OIR handoff sonraki aşama. Tasarım: OIR_AST_CODEC_IMPLEMENTATION.md.
- [x] Frontend AST pointer index: canonical preorder, caller-owned open-addressing table, 1-based stable IDs, repeated pointer/depth/root/span checks (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] Frontend AST → OIR bounded writer: exact size preflight, all 80 node kinds, TLV/scalar/token/string emission, caller-owned buffers, schema + graph verification (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] OIR AST → frontend reader/materializer: full validate-before-allocate, disconnected node/string allocation, no-fail pointer-link pass, direct cleanup on failure (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] Real parser fixture AST → OIR → AST → OIR canonical byte-for-byte round-trip + her payload byte'ında deterministik tek-bit mutation sweep (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] AST payload'ını standart OIR_KIND_AST container içine saran writer/reader adapter'ı; header, job ID, parser stage, required payload section ve malformed envelope testleri (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] Gerçek parser fixture'ını OIR AST artifact üzerinden geçirip tekrar serialize etme; iç payload byte-eşitliği ve tüm container baytlarında deterministik tek-bit mutation sweep (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] AST payload header/node record bounded writer-reader katmanını ve node/TLV/string reference sınır testlerini ekle (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] AST graph validator: NODE_REF + sibling edge sahiplik sayımı, root reachability, orphan/cycle ve 512 derinlik denetimleri (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] Node-kind field schema validator: 80 node kind için required field/type kontrolü ve unknown optional field uyumluluk testleri (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] AST TLV primitive writer/reader: bounded buffer, canonical field ordering, value-width ve malformed-input testleri (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] DIAGNOSTICS section schema 1.0 payload codec'ini uygula ve malformed-input testlerini ekle.
- [ ] SOURCE_MAP section payload codec'ini uygula.
- [ ] SEMANTIC_AST payload şemasını tanımla.
- [ ] Canonical OIR_MODULE için type system, opcode registry, control-flow, memory/ownership ve SSA kararlarını ayrı spec'te ver.
- [ ] C++ OIR2 ve mevcut C AST stream için gerekirse açık legacy import/migration aracı tasarla; eski dosyaları OIR v1 gibi okumaya çalışma.

 > OIR_FORMAT.md'de container v1.0 byte layout ve başlangıç numeric ID'leri sabitlendi. SOURCE_UNIT, DIAGNOSTICS ve TOKEN_STREAM payload codec'leri eklendi; lexer Token → OIR record dönüşümü ve diagnostic ID referansı test edildi.

## Güncel uygulama sırası

1. OIR container binary layout ve C reader/writer API.
2. Job-owned diagnostic collector ve CompilationContext.
3. [x] SOURCE_UNIT / TOKEN_STREAM payload codec'leri ve lexer Token → OIR record adapter'ı.
4. [x] OIR TOKEN_STREAM → parser token adapter'ı ve gerçek parser handoff.
5. [x] Ortak module ABI, gerçek Parser DLL/SO/dylib ve host-owned dynamic loader.
6. OCC ana derleme akışını SOURCE_UNIT → TOKEN_STREAM → Parser DLL → AST OIR handoff'una bağla; semantic/checkpoint entegrasyonunu uçtan uca doğrula.


## OIR v1 container — ilk C uygulaması eklendi

- [x] İlk v1.0 container byte layout'u belirlendi: 44-byte header, 16-byte section header, little-endian, length-delimited.
- [x] Artifact kind, validity ve section type başlangıç numeric ID'leri sabitlendi.
- [x] `omnicore-sdk/src/c/oir/oir.h` ve `oir.c`: bounded in-memory writer, validating reader ve section view API.
- [x] `test_oir.c`: round-trip, little-endian alan, truncated input, bad magic, total-length mismatch, oversized section, unknown required section, missing payload ve küçük output buffer testleri.
- [x] CMake'e `occ_oir` static utility target'ı ve OIR CTest kaydı eklendi.
- [x] Ubuntu/macOS CMake+CTest 14/14 ve Ubuntu ASan/UBSan doğrulandı. [ ] Windows build/test ve Valgrind sonuçları bekleniyor.
- [ ] Fuzz/property testleri ve header-extension uyumluluk testlerini ekle.
- [x] 160 frontend token türü için sembolik OIR ID mapping ve round-trip testi ekle (Windows/Ubuntu/macOS 16/16; Ubuntu ASan/UBSan ve Valgrind başarılı).
- [x] SOURCE_UNIT payload codec CTest: Windows/Ubuntu/macOS 15/15; Ubuntu ASan/UBSan ve Valgrind 15/15.
- [x] SOURCE_UNIT ve TOKEN_STREAM payload schema 1.0 taslağı; 160 token türü için OIR_TOKEN_REGISTRY.md stable ID listesi.

> Not: `occ_oir` şu an container yardımcı kütüphanesidir; compiler stage DLL'leri henüz OIR artifact tüketip üretmiyor. Stage ABI ve payload schema entegrasyonu sonraki adımlardır.


## Stage Module ABI / DLL boundary — Draft (not frozen)

- [x] Fixed-width versioned C ABI header: descriptor, OIR input spans, caller-owned output buffer, status/result, capabilities and export symbol.
- [x] Host-side descriptor/invocation validation: ABI versions, struct sizes, artifact kind masks, required inputs, OIR validity, job ID and partial-input capability.
- [x] Test-only pass-through shared library and dynamic load/process/unload test; output-buffer retry and artifact validation (Windows/Ubuntu/macOS + ASan/UBSan/Valgrind CI başarılı).
- [x] Build the real parser-stage DLL/SO/dylib: consume SOURCE_UNIT + TOKEN_STREAM OIR artifacts, reconstruct tokens against source bytes, run parser in memory, and emit OIR_KIND_AST artifact.
- [x] Add stage-module diagnostic callback to the draft ABI; verify code, message, hint, source ID, line/column and byte spans reach the OCC-owned collector. ABI freeze still waits for the real parser-stage adapter.



## Gerçek Parser stage modülü — tamamlandı

- [x] Parser DLL/SO/dylib: SOURCE_UNIT + TOKEN_STREAM OIR girdilerini doğrula, tokenları kaynak baytlarıyla eşleştir, in-memory parser çalıştır, AST OIR üret.
- [x] Parser modülünün tanılarını invocation diagnostic sink üzerinden raporla; hata halinde geçerli AST artifact üretme.
- [x] Gerçek dinamik modül testinde diagnostic callback ve exact source span doğrulandı. OCC failure checkpoint bağlantısı host pipeline entegrasyon testinde ayrıca doğrulanacak.


- [x] Real parser stage DLL end-to-end test: valid SOURCE_UNIT + TOKEN_STREAM -> AST artifact; NEED_OUTPUT_BUFFER retry; invalid syntax -> diagnostic sink and zero published AST output (Windows/Ubuntu/macOS, ASan/UBSan and Valgrind CI).
- [x] Validated OIR AST payload → frontend AST reader/materializer: validate-before-allocate, disconnected node/string allocation, no-fail pointer-link pass and independent failure cleanup (not `ast_destroy()` on a partial graph); real reader tests are included in the cross-platform CTest matrix.


## OCC Dynamic Stage Loader — 31 Ekim 2026

- [x] Host-owned occ_module_loader bileşeni: Windows LoadLibrary/GetProcAddress veya POSIX dlopen/dlsym ile tam modül yolunu yükle.
- [x] Export sembolünü çöz, ABI v1 descriptor'ını doğrula; yükleme hatası, eksik sembol ve geçersiz descriptor durumlarını ayrı status kodlarıyla döndür.
- [x] Ortak invocation köprüsü: host diagnostic sink kur, OIR girdilerini ABI validator'dan geçir, NEED_OUTPUT_BUFFER yanıtında modülün istediği kesin kapasiteyle retry et.
- [x] Başarılı çıktıyı host tarafında tekrar OIR olarak aç; artifact kind, producer stage, job ID ve validity alanlarını descriptor/result ile karşılaştır. Başarısız modül çağrısında çıktı pointer'ı yayımlama.
- [x] Pass-through fixture testi: load/invoke/retry/callback/unload; eksik export ve bulunamayan dosya testleri.
- [x] Gerçek Parser DLL testi artık doğrudan process() çağırmak yerine OCC host loader üzerinden geçerli ve hatalı kaynakları çalıştırıyor; exact diagnostic span ve başarısızlıkta sıfır çıktı doğrulanıyor.
- [x] CI'da loader ve gerçek Semantic DLL testlerini Windows/Ubuntu/macOS + ASan/UBSan/Valgrind matrisinde doğrula (CI run `41325592201` ve sonraki `41364158463` başarılı).
- [x] `main.c` ana akışını host loader + OIR handoff'a taşı; Lexer → Parser → Semantic DLL zincirini gerçek CLI'da çalıştır ve hata checkpoint'ini kaynak girdisine bağla. AST materialization yalnızca henüz ortak stage ABI'ye taşınmamış legacy Codegen köprüsü için tutuluyor.
- [ ] Modül başına çağrı serileştirmesini ve aynı job collector'a diagnostic callback kilidini OCC dispatcher seviyesinde uygula. Loader şu anda host çağrısı başına callback kurar; collector'ın kendisi thread-safe değildir.


## OCC CLI → Parser DLL OIR pipeline — 31 Ekim 2026

- [x] OCC host lexer çıktısını SOURCE_UNIT + TOKEN_STREAM OIR artifact'larına dönüştürür; lexer tanıları TOKEN_STREAM içindeki DIAGNOSTICS section'ında stable diagnostic ID'leriyle taşınır.
- [x] OCC parser stage DLL'sini executable yanındaki runtime modül dizininden yükler ve host-owned dynamic loader üzerinden çağırır.
- [x] Parser DLL'nin AST OIR çıktısını host tarafında yeniden doğrula ve AST materializer ile semantic frontend nesnelerine dönüştür.
- [x] Semantic analiz artık parser DLL'den gelen AST üzerinde çalışır; eski statik parser_run(ctx) ana akıştan kaldırıldı.
- [x] Parser DLL hatasında job diagnostic collector + orijinal kaynak baytları failure checkpoint'e yazılır. CLI CTest, checkpoint dosyasında kaynak baytlarının birebir bulunduğunu doğrular.
- [x] Lokalize OCC kopyasının yanında parser DLL'nin de deploy edilmesini sağla.
- [x] **CI run 41174447616:** Windows, Ubuntu ve macOS build/test başarılı; CTest 39/39; Ubuntu ASan/UBSan ve Valgrind başarılı.
- [x] Lexer'ı bağımsız DLL stage'e ayır; OCC'nin statik lexer çağrısını kaldır ve SOURCE_UNIT → lexer DLL → TOKEN_STREAM → parser DLL zincirini gerçek CLI'da çalıştır.
- [x] SEMANTIC_AST schema/codec, artifact validator/writer ve `occ_semantic_stage` DLL/SO/dylib oluştur; OCC CLI'da AST OIR → Semantic DLL → SEMANTIC_AST OIR zincirini çalıştır. Host AST materialization yalnızca legacy Codegen köprüsüdür; Codegen'in SEMANTIC_AST tüketmesi ayrı açık görevdir.
- [x] OIR_MODULE→OIR_ASSEMBLY üreten yeni Codegen stage'i ortak ABI'ye taşı; legacy `codegen_run` ayrı entry-point olarak parity süresince korunuyor.
- [ ] Eski `codegen_run` el sıkışmasını kaldır ve final assembly çıktısını yeni stage'e geçir; önce parity testleri tamamlanmalı.
- [ ] Aynı job diagnostic collector'a erişen çağrılar için synchronized dispatcher/serialization policy ekle.


## Independent Lexer stage DLL — 31 Ekim 2026

- [x] `occ_lexer_stage` DLL/SO/dylib: tek SOURCE_UNIT girdisini doğrula, stable token ID + source span içeren TOKEN_STREAM üret.
- [x] Lexer hatalarını origin stage `OCC_STAGE_LEXER` ile job collector'a ekle; TOKEN_ERROR kaydına 1-based diagnostic ID yaz ve aynı diagnostics tablosunu TOKEN_STREAM artifact'ına ekle.
- [x] Diagnostic callback yalnızca yeterli output buffer bulunduğunda çağrılır; NEED_OUTPUT_BUFFER retry aynı hataları collector'a iki kez eklemez.
- [x] Gerçek lexer DLL testinde unterminated string, token span, diagnostic payload, callback ve output-buffer retry doğrulandı.
- [x] OCC ana CLI statik lexer döngüsünü kaldırdı; lexer DLL TOKEN_STREAM çıktısı doğrudan parser DLL girdisi oldu.
- [x] **CI run 41174917131:** Windows, Ubuntu ve macOS başarılı; CTest 40/40; Ubuntu ASan/UBSan ve Valgrind başarılı.


## P0 — SEMANTIC_AST OIR and Semantic stage — 31 Ekim 2026

- [x] Inspect current semantic checker and document the boundary between transient `SemanticContext/Slot` state and a durable semantic result model.
- [x] Draft `OIR_SEMANTIC_AST.md`: preserve AST node IDs/payload; define scope, type, symbol, relation and node-annotation tables; reserve explicit conversion records without claiming numeric promotion is implemented.
- [x] Register semantic OIR section IDs and freeze the bounded wire record layout after codec/validator tests.
- [x] Implement SEMANTIC_AST section codecs and round-trip/malformed-input tests.
- [x] Refactor the initial semantic result model to emit scope/symbol/type/annotation/relation records; broader language rules and conversion records remain separate work.
- [x] Implement `occ_semantic_stage` DLL/SO/dylib using `OccStageModuleV1`, AST input and SEMANTIC_AST output.
- [x] Integrate Semantic DLL in OCC and validate its SEMANTIC_AST output; retain host AST materialization only as a temporary legacy Codegen bridge.
- [x] Pass SEMANTIC_AST through the new IR Lowering DLL in the OCC CLI; host validates and checkpoints the OIR_MODULE output (CI run 41366127592).
- [x] Add a target-specific Codegen DLL using the common `OccStageModuleV1` ABI; consume OIR_MODULE 1.1 and emit OIR_ASSEMBLY 1.0.
- [ ] Compare new and legacy assembly behavior on the supported fixture set; remove the legacy `codegen_run` handoff only after parity tests.



## Independent OIR Lowering stage DLL — 2 Kasım 2026

- [x] `occ_oir_lowering_stage` DLL/SO/dylib: accepts one validated SEMANTIC_AST artifact and emits a complete OIR container carrying OIR_MODULE 1.1.
- [x] Stage-specific input validation: artifact kind/schema, producer stage, validity and job ID; semantic AST graph and cross-table validation before lowering.
- [x] Per-invocation workspace ownership; no AST or semantic pointers cross the module ABI. Current stage resource cap: 65,536 AST nodes.
- [x] Deterministic/reentrant/stateless descriptor and stable stage/module IDs (`OCC_STAGE_OIR_LOWERING=7`, module ID `0x00010004`).
- [x] Dynamic module-loader test covers all supported scalar fixtures, exact output-buffer retry from a 64-byte initial buffer, OIR_MODULE envelope/payload validation and malformed-input rejection.
- [x] **CI run 41365084763:** Windows, Ubuntu and macOS build/test successful; Ubuntu ASan/UBSan and Valgrind successful.
- [x] Add the lowering DLL to the OCC CLI stage graph after Semantic DLL; validate the descriptor, artifact header, job/stage identity and OIR_MODULE payload, then persist `.module.oir` via a same-directory temporary file and rename. **CI run 41366127592:** Windows/Ubuntu/macOS build and test successful; Ubuntu ASan/UBSan and Valgrind successful; Ubuntu CTest 57/57.
- [x] Migrate the new Codegen path to the common stage ABI and verified OIR_MODULE input; keep the temporary host AST materialization bridge until output parity is established.

## P3 — Native NEXSUS linker and OCL — future architecture requirement

- [ ] Define OCL as a first-class NEXSUS Flow tool/language component when the toolchain design reaches the linker phase.
- [ ] Implement a project-owned native linker; the platform C linker may be used only as a temporary development/test bridge and must not be a final runtime/build dependency.
- [ ] Specify the linker input/object format, symbol resolution, relocation model, section/layout rules and target ABI independently of the host C linker.
- [ ] Define a linker security specification before implementation: validate all object/module inputs; enforce bounds and overflow checks; constrain symbol visibility and resolution; validate relocation targets and memory-region permissions; verify module/object identity and integrity; reject malformed or policy-violating inputs; produce deterministic, auditable link results.
- [ ] Add adversarial malformed-object, relocation-boundary, duplicate/ambiguous-symbol, permission-escalation and tampered-input tests.
- [ ] Remove the temporary C-linker bridge only after the native linker passes the full target and security test matrix.

This is a planned end-state requirement, not part of the current Semantic DLL milestone.


## OCC CLI integration of OIR Lowering — 2 Kasım 2026

- [x] OCC resolves the platform-specific `occ_oir_lowering_stage` DLL/SO/dylib beside the executable.
- [x] The host checks the loaded descriptor's stage ID and output artifact kind before invocation.
- [x] The validated `SEMANTIC_AST` artifact is passed with the current job ID and the job-owned diagnostic collector.
- [x] The host validates OIR container kind, OIR_MODULE schema 1.1, producer stage, validity, job ID, unique payload section and the OIR_MODULE payload codec before accepting output.
- [x] The validated artifact is persisted as `<source-basename>.module.oir` using a same-directory temporary file and platform-appropriate replacement.
- [x] CMake makes the lowering shared library a build dependency of OCC and links the OIR_MODULE reader into the host.
- [x] A CLI integration fixture exercises the supported scalar lowering subset and checks the persisted OIR signature and artifact size.
- [x] The legacy AST materialization and `codegen_run` path remain active as a compatibility/parity bridge; the new Codegen DLL is invoked after the OIR Verifier stage.
- [x] **CI run 41366127592:** Windows, Ubuntu and macOS build/test passed; Ubuntu ASan/UBSan and Valgrind passed; Ubuntu CTest **57/57**.

**Current lowering scope:** the first lowering implementation supports a bounded scalar subset (integer/float/bool returns, straight-line arithmetic and initialized local values/assignments). Unsupported AST constructs still fail closed in this stage. The CLI integration fixture is intentionally inside this implemented subset; broader parser/semantic fixtures remain covered by their own tests.

**Next compiler task:** expand the scalar OIR lowerer and verifier fixtures; then compare new and legacy assembly outputs. Keep the legacy AST bridge until parity tests cover the existing supported source set.


## Independent OIR Codegen stage DLL — 2 Kasım 2026

- [x] Add `occ_codegen_stage` DLL/SO/dylib with stable module ID `0x00010005` and `OCC_STAGE_CODEGEN`.
- [x] Consume one validated OIR_MODULE 1.1 artifact; validate producer stage, validity, job identity, unique payload, module structure and the first backend's SSA/use-def subset.
- [x] Emit OIR_ASSEMBLY schema 1.0 with a bounded two-pass NASM text writer; NEED_OUTPUT_BUFFER retry does not publish partial output or diagnostics.
- [x] Initial x86-64 BareMetal backend supports one function, multi-block CFG, i32/i64/f32/f64/bool constants, integer arithmetic, SSE scalar float arithmetic, BRANCH, boolean COND_BRANCH, UNREACHABLE and scalar returns; unsupported opcodes fail closed.
- [x] OCC validates the selected legacy target profile, loads the Codegen stage DLL, validates OIR_ASSEMBLY and persists `.assembly.oir` plus `.stage.s`.
- [x] Preserve the legacy `codegen_run` path and its `.s` output; parity is **not** yet claimed.
- [x] Add direct dynamic-stage tests and end-to-end CLI artifact tests.
- [x] Direct stage tests cover f64 `MULSD/ADDSD` and f32 `CVTSD2SS/MULSS/ADDSS`, including scalar return-bit transfer through RAX.
- [x] **CI run 41367164418:** Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **58/58**; Ubuntu ASan/UBSan and Valgrind passed.
- [x] **CI run 41367815321:** f32/f64 Codegen tests passed; Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **58/58**; Ubuntu ASan/UBSan and Valgrind passed.

**Next Codegen work:** add comparison lowering (integer signed/unsigned and IEEE float comparisons), then calls and multi-function modules; define a target descriptor artifact/selection contract; compare output behavior against legacy Codegen; only then remove the AST bridge. Multi-block branch emission is now covered by direct DLL tests.


## Independent OIR Verifier stage — 2 Kasım 2026

- [x] Add `occ_oir_verify_stage` as an independent shared module using `OccStageModuleV1` (module ID `0x00010006`, stage ID `OCC_STAGE_OIR_VERIFY`).
- [x] Validate OIR_MODULE 1.1 envelope, producer stage, validity, job identity and exactly one payload.
- [x] Validate function/block ownership, entry blocks, complete non-overlapping instruction ranges and instruction ownership.
- [x] Validate terminators, same-function CFG edges, opcode shape and implemented scalar type constraints.
- [x] Validate one definition per SSA value, same-block use order and cross-block dominance.
- [x] Preserve unreachable blocks without treating them as reachable dominators; disallow cross-block SSA uses that cannot be proven to dominate.
- [x] Republish the original OIR sections with producer stage `OCC_STAGE_OIR_VERIFY`; no silent repair or mutation.
- [x] Insert the verifier into the OCC host pipeline between OIR Lowering and Codegen; persist the verified module artifact and require verifier-produced input at Codegen.
- [x] Add direct DLL tests for a valid module, same-block use-before-definition rejection, a valid cross-block dominating definition and a diamond-CFG non-dominating SSA rejection; keep assertions active in Release CI.
- [x] Add CFG dominator coverage: predecessor definition accepted only when its block dominates the use block.
- [ ] Extend verifier checks when function signatures, block parameters/phi nodes, memory metadata and source maps enter the OIR schema.
- [x] Add negative fixtures for cross-function CFG edges, mismatched function-entry ownership, missing block terminators and duplicate SSA definitions.
- [x] **CI run 41375777590:** Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **59/59**; ASan/UBSan and Valgrind passed.

- [x] **CI run 41382501057:** Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **59/59**; ASan/UBSan and Valgrind passed.
**Current verifier resource bounds:** 8,192 blocks/module; 1,000,000 function, instruction and value records; OIR string table remains capped at 16 MiB. These are implementation/resource limits, not NEXSUS Flow language limits.

**Next compiler task:** extend source-level OIR lowering to structured loop CFG and multi-statement branches, with explicit SSA merge/phi rules before allowing values to flow out of branches. Keep Codegen's calls/multi-function work and legacy parity as separate milestones.


## Multi-block CFG Codegen — 2 Kasım 2026

- [x] Extend the x86-64 NASM backend from one basic block to multiple blocks in one function.
- [x] Emit deterministic `.Lbb_<id>` labels and an explicit jump to the function entry block.
- [x] Emit `BRANCH`, boolean `COND_BRANCH` and `UNREACHABLE` (`UD2`) terminators.
- [x] Allow scalar `RETURN` terminators in any block; each return emits the shared function epilogue.
- [x] Keep SSA dominance validation in the independent OIR Verifier; Codegen retains backend type/shape checks and requires verifier-produced input.
- [x] Add a three-block conditional CFG fixture and assert both branch labels and the conditional jump sequence.
- [x] **CI run 41376095890:** Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **59/59**; ASan/UBSan and Valgrind passed.

**Next Codegen increment:** add calls and multi-function module support, define the target descriptor/backend-selection contract, then extend the lowerer to emit comparisons and multi-block CFG from source-level control flow. Preserve legacy Codegen until parity is demonstrated.


## Comparison Codegen — 2 Kasım 2026

- [x] Add signed and unsigned integer CMP_EQ/NE/LT/LE/GT/GE lowering.
- [x] Add f32/f64 comparisons using UCOMISS/UCOMISD.
- [x] Preserve IEEE unordered behavior: NaN is false for ordered relations/equality and true for inequality.
- [x] Add boolean EQ/NE support; ordered boolean comparisons fail closed.
- [x] Feed a signed integer CMP_LT result into a three-block conditional CFG test.
- [x] Feed an f64 CMP_LT result into a three-block conditional CFG test; assert unordered-flag filtering instructions.
- [x] **CI run 41376451054:** Windows, Ubuntu and macOS build/test passed; Ubuntu CTest **59/59**; ASan/UBSan and Valgrind passed.
