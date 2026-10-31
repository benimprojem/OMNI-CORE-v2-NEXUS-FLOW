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
- [ ] Scope, type promotion, ownership ve control-flow kurallarını testlerle doğrula.
- [ ] Hata ve warning API'sini ortaklaştır.

## Intent sistemi — ertelendi

- [x] Yalnızca kavramsal ayrımı kaydet: `!!=` açık derleyici talimatı, `@` gelecekteki AI katmanına niyet bildirimi.
- [ ] AI katmanı tasarlanmaya başlandığında Intent sistemini uçtan uca yeniden ele al: gramer, veri modeli, bağlam, AST/OIR, AI etkileşimi ve doğrulama.
- [ ] AI katmanı bulunmadan `@` için varsayımsal parser davranışı veya sabit intent kategorileri geliştirme.

> Bu başlık şimdilik aktif geliştirme sırasına alınmamıştır. Ayrıntılı tasarım, AI bileşeninin gerçek gereksinimleri ortaya çıktığında yapılacaktır.

## P0 — Ara temsil

- [ ] AST serialization ile OIR kavramlarını/API'lerini ayır.
- [ ] Kanonik OIR v1 sözleşmesini tasarla.
- [ ] C OIR writer/reader geliştir.
- [ ] Bozuk/truncated/uyumsuz OIR dosyaları için güvenlik testleri ekle.
- [ ] C++ OIR2 ile özellik/format karşılaştırma tablosu oluştur.

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

Valgrind çalıştırma seçeneği ve CI doğrulaması ekle. Sonraki kapsam genişletmesi semantic allocation-failure ve scope/ownership testleri olsun.


## P0 — Modüler derleyici pipeline (mimari kabul edildi, uygulama bekliyor)

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
- [ ] Codegen modülünü ortak ABI'ye geçir.
- [ ] Lexer, parser ve semantic analyzer'ı ayrı DLL/SO/dylib modüllerine ayır.
- [ ] Bağımsız derleme işlerini paralel çalıştır; modül thread-safety/reentrancy sözleşmesini test et.
- [ ] Kuyruk/back-pressure, job cancellation ve final output atomic publish testlerini ekle.
- [ ] IR, optimizer pass, target validation, backend, assembler ve linker aşamalarını ayrı modüllere böl.

> Mimari karar ve tasarım belgesi eklendi. Bu görevler henüz kodlandı veya CI ile doğrulandı olarak işaretlenmemiştir.

## Sonraki tek iş

Validated OIR AST payload → frontend AST reader/materializer: validate-before-allocate, disconnected node/string allocation, then no-fail pointer-link pass; failure cleanup must not call ast_destroy() on a partial graph.


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


## OCC Dynamic Stage Loader — 31 Ekim 2026

- [x] Host-owned occ_module_loader bileşeni: Windows LoadLibrary/GetProcAddress veya POSIX dlopen/dlsym ile tam modül yolunu yükle.
- [x] Export sembolünü çöz, ABI v1 descriptor'ını doğrula; yükleme hatası, eksik sembol ve geçersiz descriptor durumlarını ayrı status kodlarıyla döndür.
- [x] Ortak invocation köprüsü: host diagnostic sink kur, OIR girdilerini ABI validator'dan geçir, NEED_OUTPUT_BUFFER yanıtında modülün istediği kesin kapasiteyle retry et.
- [x] Başarılı çıktıyı host tarafında tekrar OIR olarak aç; artifact kind, producer stage, job ID ve validity alanlarını descriptor/result ile karşılaştır. Başarısız modül çağrısında çıktı pointer'ı yayımlama.
- [x] Pass-through fixture testi: load/invoke/retry/callback/unload; eksik export ve bulunamayan dosya testleri.
- [x] Gerçek Parser DLL testi artık doğrudan process() çağırmak yerine OCC host loader üzerinden geçerli ve hatalı kaynakları çalıştırıyor; exact diagnostic span ve başarısızlıkta sıfır çıktı doğrulanıyor.
- [ ] CI'da yeni loader testlerini Windows/Ubuntu/macOS + ASan/UBSan/Valgrind matrisinde doğrula.
- [ ] main.c statik parser yolunu host loader + OIR handoff ile değiştir; semantic stage ve failure checkpoint'i gerçek Parser DLL çıktısına bağla.
- [ ] Modül başına çağrı serileştirmesini ve aynı job collector'a diagnostic callback kilidini OCC dispatcher seviyesinde uygula. Loader şu anda host çağrısı başına callback kurar; collector'ın kendisi thread-safe değildir.


## OCC CLI → Parser DLL OIR pipeline — 31 Ekim 2026

- [x] OCC host lexer çıktısını SOURCE_UNIT + TOKEN_STREAM OIR artifact'larına dönüştürür; lexer tanıları TOKEN_STREAM içindeki DIAGNOSTICS section'ında stable diagnostic ID'leriyle taşınır.
- [x] OCC parser stage DLL'sini executable yanındaki runtime modül dizininden yükler ve host-owned dynamic loader üzerinden çağırır.
- [x] Parser DLL'nin AST OIR çıktısını host tarafında yeniden doğrula ve AST materializer ile semantic frontend nesnelerine dönüştür.
- [x] Semantic analiz artık parser DLL'den gelen AST üzerinde çalışır; eski statik parser_run(ctx) ana akıştan kaldırıldı.
- [x] Parser DLL hatasında job diagnostic collector + orijinal kaynak baytları failure checkpoint'e yazılır. CLI CTest, checkpoint dosyasında kaynak baytlarının birebir bulunduğunu doğrular.
- [x] Lokalize OCC kopyasının yanında parser DLL'nin de deploy edilmesini sağla.
- [x] **CI run 41174447616:** Windows, Ubuntu ve macOS build/test başarılı; CTest 39/39; Ubuntu ASan/UBSan ve Valgrind başarılı.
- [ ] Lexer'ı bağımsız DLL stage'e ayır ve OCC'nin statik lexer çağrısını kaldır.
- [ ] Semantic stage için SEMANTIC_AST OIR schema/codec + DLL oluştur; host materialization'ı yalnızca legacy consumer gerekiyorsa kullan.
- [ ] Codegen'i ortak stage ABI'ye taşı; mevcut codegen_run yüklemesi henüz eski ayrı entry-point sözleşmesidir.
- [ ] Aynı job diagnostic collector'a erişen çağrılar için synchronized dispatcher/serialization policy ekle.
