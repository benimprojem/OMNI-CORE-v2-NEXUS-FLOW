# OmniCore SDK — Güncel Görevler

**Ana uygulama dili:** C  
**C++:** Korunan referans uygulaması  
**Güncel plan:** ROADMAP.md  
**Durum raporu:** PROJECT_STATUS.md

> Görevler yalnızca kod yazıldığı için tamamlandı sayılmaz. Build ve test kanıtı gereklidir. Eski yüzde tamamlanma iddiaları bu listede kullanılmamaktadır.

## P0 — Temel güvenilirlik

- [ ] C kaynaklarının temiz checkout üzerinde derlenmesini sağla.
- [ ] parser_build.bat (C++ derlemesi) ile C uygulamasının build akışını açıkça ayır.
- [ ] C OCC için Windows build hedefi ve Linux/macOS build hedefleri oluştur.
- [ ] Parser, semantic ve codegen için ayrı build hedefleri oluştur.
- [ ] Config okuyucuyu bölüm/satır sırasından bağımsız hale getir.
- [ ] CLI argüman ayrıştırmasını kaynak dosyası ve seçenek sırasına dayanıklı yap.
- [ ] Path işlemlerindeki NULL pointer karşılaştırmasını kaldır.
- [ ] Parser/codegen hata durumlarını OCC çıkış koduna aktar.
- [ ] Otomatik lexer/parser/semantic test iskeletini kur.
- [ ] AST ve semantic bellek sahipliği/free kurallarını doğrula.

## P0 — Dil ve semantik

- [ ] Token envanterini dil spesifikasyonuyla karşılaştır.
- [ ] The 15 Keys operatörlerinin token öncelik ve çakışma testlerini ekle.
- [ ] AST düğüm sözleşmesini ve source location bilgisini sabitle.
- [ ] Scope, type promotion, ownership ve control-flow kurallarını testlerle doğrula.
- [ ] Hata ve warning API'sini ortaklaştır.

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
- [ ] Target parser'da bounded string parsing ve alan uzunluğu doğrulaması uygula.
- [ ] Eksik target dosyası, bilinmeyen alan ve bilinmeyen feature için açık hata üret.
- [ ] Target parser testleri: truncated, uzun alan, eksik alan, duplicate key ve invalid enum.
- [ ] Windows DLL, Linux .so ve macOS .dylib codegen yükleme/bağlama stratejisini belirle.
- [ ] Codegen yüklenemediğinde derlemeyi başarılı göstermeyen hata davranışını uygula.
- [ ] Target listeleme ve target dosyası çözümlemesini platformdan bağımsız hale getir.
- [ ] Optimizer pass altyapısını kur.
- [ ] Her optimizer pass için davranış eşdeğerliği testleri ekle.

## P2 — SDK ve araçlar

- [ ] occ.ocf yapılandırma gramerini tanımla.
- [ ] .nxr build akışını tasarla ve uygula.
- [ ] stdlib/dpslib bağımlılık çözümlemesini doğrula.
- [ ] Tekrarlanabilir paketleme ve kurulum akışı oluştur.
- [ ] CI build/test hattı ekle.

## P2/P3 — IDE ve isteğe bağlı özel işlemci

- [ ] OCE IDE'yi kararlı compiler API'sine bağla.
- [ ] Debugger ve OIR/source-map inceleyicisi geliştir.
- [ ] Runtime/threading özelliklerini test et.
- [ ] Gerekirse ileride özel işlemci target/backend eklentisi ekle; genel derleyici yolunu buna bağımlı yapma.

## Klasör yapısı

Hedef yapı PROPOSED_LAYOUT.md içinde tanımlıdır. Fiziksel taşıma, build betikleri ve testler yeni yollarda doğrulanmadan yapılmayacaktır.

## Sonraki tek iş

Faz 0: C hattının tekrarlanabilir build sürecini oluşturmak. Ardından config/CLI/path ve hata kodu sorunlarını düzeltmek.
