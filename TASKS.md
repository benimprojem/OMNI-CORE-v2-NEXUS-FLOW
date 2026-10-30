# OmniCore SDK — Güncel Görevler

**Ana uygulama dili:** C  
**C++:** Korunan referans uygulaması  
**Güncel plan:** ROADMAP.md  
**Durum raporu:** PROJECT_STATUS.md

> Görevler yalnızca kod yazıldığı için tamamlandı sayılmaz. Build ve test kanıtı gereklidir. Eski yüzde tamamlanma iddiaları bu listede kullanılmamaktadır.

## P0 — Temel güvenilirlik

> CMake/CI altyapısı mevcut. Windows, Ubuntu ve macOS'ta 13/13 CTest başarılı; Ubuntu GCC ASan/UBSan ve Valgrind koşuları da 13/13 geçti.

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
- [ ] Sürümlenmiş C module ABI'sini tanımla: descriptor, stage ID, ABI version, artifact tür/sürüm, process/result, diagnostics ve ownership.
- [ ] Derleme başına bağımsız context ve ortak diagnostic sink oluştur.
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

PIPELINE_MIGRATION_AUDIT.md içindeki ilk kod denetimine göre, önce job-owned diagnostic collector ve compilation context için C API taslağını çıkar; ardından lexer/parser/semantic'i bu ortak sink'e adapte et.
