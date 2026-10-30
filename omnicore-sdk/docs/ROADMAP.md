# OmniCore SDK — Geliştirme Yol Haritası

**Yön:** C ana kaynak, C++ referans uygulama  
**Belge türü:** Canlı plan; her fazın çıkışı testlerle doğrulandıkça güncellenecek.

## Hedef

NexusFlow'u tek bir işlemciye kilitlemeden derleyebilen; dil kurallarını doğru denetleyen; sabit bir ara temsil üzerinden farklı backend'lere bağlanabilen bir araç zinciri oluşturmak.

## Şu an neredeyiz?

- **C hattı:** Lexer, parser, AST, semantik analiz ve sınırlı NASM/x86-64 odaklı codegen kaynakları mevcut.
- **C++ hattı:** Lexer/parser, OIR writer, analyzer ve OCE/IDE kaynakları mevcut.
- **Ortak sözleşme:** İki hattın kullandığı tek, doğrulanmış OIR sözleşmesi henüz yok.
- **Build/test:** Kaynak yolları, betikler, DLL yükleme yolları ve test dosyaları tek, tekrarlanabilir bir akışta birleşmiş değil.
- **Backend:** C codegen target yapılandırması okusa da çıktı tarafında x86-64/NASM varsayımları bulunuyor; çoklu target desteği tamamlanmış değil.
- **Dil doğruluğu:** Dokümanlardaki tüm özelliklerin C uygulamasında desteklendiğini gösteren kapsamlı test matrisi yok.

Proje şu an **prototip / entegrasyon aşamasında**. Yüzde tamamlanma yerine modül bazlı doğrulama kullanılacak.

## Öncelik sırası

~~~text
0. Kaynak envanteri ve tekrarlanabilir build
1. C frontend: lexer + parser + AST
2. Semantik: type / scope / ownership / control-flow
3. Kanonik OIR v1: writer + reader + doğrulama
4. Optimizer altyapısı
5. Backend sözleşmesi ve ARM64
6. x86-64 backend (Windows ABI ve test ortamı)
7. Linker / paketleme / SDK dağıtımı
8. IDE, runtime, debugger ve NEXSUS özel backend
~~~

## Faz 0 — Envanter ve tekrarlanabilir build

**Öncelik:** P0

- [ ] C kaynak dosyalarının bağımlılıklarını ve include yollarını çıkar.
- [ ] Parser, semantic ve codegen için ayrı build komutları oluştur.
- [ ] Debug ve release build profilleri ekle.
- [ ] Build betiklerinde alt işlem çıkış kodlarını kontrol et.
- [ ] Kaynak, test girdisi, test çıktısı ve binary dosyalarını ayır.
- [ ] Temiz checkout üzerinden build doğrulaması yap.
- [ ] Build sürümü/derleyici sürümünü logla.

**Çıkış ölçütü:** Temiz checkout'tan tekrarlanabilir build; hata durumunda sıfır olmayan çıkış kodu.

## Faz 1 — C frontend'i sabitle

**Öncelik:** P0

- [ ] Token envanterini nfsyntax.md ve lexer ile karşılaştır.
- [ ] The 15 Keys ve bileşik operatörler için çakışma/öncelik testleri yaz.
- [ ] String, escape, sayı, yorum, Unicode ve satır/sütun konumlarını test et.
- [ ] AST düğüm türlerini ve sahiplik/free kurallarını belgeleyip uygula.
- [ ] Parser hata toparlama ve diagnostic konumlarını test et.
- [ ] Her syntax özelliği için geçerli ve geçersiz örnekler ekle.

**Çıkış ölçütü:** Tanımlı syntax kapsamı için otomatik testlerin geçmesi; AST'nin deterministik üretilmesi ve çökme/sızıntı olmaması.

## Faz 2 — Semantik analiz

**Öncelik:** P0

- [ ] Scope ve symbol table yaşam döngüsünü netleştir.
- [ ] Tip sistemi, numeric promotion ve dönüşüm kurallarını tek tabloda tanımla.
- [ ] Handler/masa sahipliği, transfer ve erişim kurallarını formelleştir.
- [ ] ->, >>, <-, _> ve benzeri operatörlerin AST/semantik anlamlarını ayır.
- [ ] Kontrol akışı, return ve erişilemeyen kod analizini ekle.
- [ ] Hata ve uyarıları tek diagnostic API üzerinden üret.

**Çıkış ölçütü:** Pozitif/negatif semantik testleri; hatalı programların codegen'e geçmemesi.

## Faz 3 — Kanonik OIR v1

**Öncelik:** P0

- [ ] OIR v1 format belgesini onayla.
- [ ] AST serialization ile OIR'ı ayrı isimlendir ve ayrı API'ler kullan.
- [ ] C OIR writer/reader geliştir.
- [ ] Header, offset, length, count, alignment ve integer overflow kontrolleri ekle.
- [ ] Truncated, bozuk, bilinmeyen opcode ve sürüm testleri yaz.
- [ ] Source map ve debug/release farkını tanımla.
- [ ] C++ OIR2'yi uyumluluk/özellik karşılaştırması için kullan; doğrulanmadan ana formata dönüştürme.

**Çıkış ölçütü:** Writer-reader round-trip ve bozuk dosya testleri; OIR sürüm uyumluluğu açıkça tanımlı.

## Faz 4 — Optimizer

**Öncelik:** P1

- [ ] OIR doğrulayıcı ve pass altyapısı.
- [ ] Constant folding.
- [ ] Dead code elimination.
- [ ] Basit common subexpression elimination.
- [ ] Her pass için önce/sonra eşdeğerlik testleri.
- [ ] Ownership ve side-effect kurallarını ihlal etmeme.

## Faz 5 — Backend sözleşmesi ve ARM64

**Öncelik:** P1

- [ ] Target tanımını mimari, ABI, object format ve özellikler olarak ayrıştır.
- [ ] ARM64 register sınıfları ve çağrı kurallarını tanımla.
- [ ] Stack frame, argüman, return ve branch üretimini uygula.
- [ ] İlk desteklenen instruction subset'ini açıkça listele.
- [ ] Assembler ile küçük programları derle ve çalıştır.
- [ ] Desteklenmeyen OIR operasyonunda açık hata üret.

**Çıkış ölçütü:** Küçük test programları için ARM64 assembly/object üretimi ve doğrulanmış çalışma.

## Faz 6 — x86-64 backend

**Öncelik:** P1

- [ ] Microsoft x64 ABI: argüman yazmaçları, shadow space, alignment, callee-saved.
- [ ] Windows object format ve linker entegrasyonu.
- [ ] Linux/System V ABI ayrı profil olarak.
- [ ] Register allocation ve spill stratejisi.
- [ ] Debug symbol/source map.
- [ ] ARM64 ile ortak backend test sözleşmesi.

## Faz 7 — SDK paketleme ve araçlar

**Öncelik:** P2

- [ ] occ CLI argüman ayrıştırma ve config okuma.
- [ ] .ocf ve .nxr dosyalarının resmi grameri.
- [ ] Bağımlılık çözümleme ve dpslib entegrasyonu.
- [ ] Runtime/stdlib sınırları.
- [ ] Kurulum, örnek proje ve temizleme komutları.
- [ ] CI üzerinde test/build.

## Faz 8 — IDE, debugger ve özel donanım hedefi

**Öncelik:** P2/P3

- [ ] IDE'nin compiler API'sine bağlanması.
- [ ] Debugger, source map ve OIR inceleyici.
- [ ] Runtime/threading doğrulaması.
- [ ] NEXSUS ISA kararı netleşince NEXSUS backend.
- [ ] FPGA/simulator test yolu.

## İlk uygulanacak işler

1. C build komutlarını temiz checkout üzerinde çalıştırılabilir hale getir.
2. C main.c içindeki config okuma, path ve hata kodu sorunlarını düzelt.
3. Parser ve codegen için küçük otomatik testler kur.
4. C AST/semantic bellek sahipliğini denetle.
5. OIR v1 tasarımını mevcut AST serialization ve C++ OIR2 ile karşılaştırarak hazırla.

## İlerleme raporu formatı

Her faz için şu alanlar tutulacak:

- **Durum:** Planlandı / Devam ediyor / Kodlandı / Test edildi / Tamamlandı
- **Kanıt:** Build logu, test adı veya örnek çıktı
- **Bilinen sınırlamalar:** Henüz desteklenmeyen özellikler
- **Sonraki adım:** Tek ve ölçülebilir iş

Kod yazılmış olması tek başına "Tamamlandı" sayılmayacaktır.
