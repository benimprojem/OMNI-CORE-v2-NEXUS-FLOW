# OmniCore SDK — Başlangıç Durum Raporu

**Değerlendirme tarihi:** 30 Ekim 2026  
**İncelenen dal:** main  
**Çalışma dalı:** refactor/c-core-roadmap  
**İnceleme türü:** GitHub kaynak ağacı ve metin dosyalarının statik incelemesi. Bu rapor derleme/test sonucu değildir.

## Karar

C ana kaynak olacaktır. C++ kaynakları korunacak ve referans olarak incelenecektir. Kaynakların fiziksel taşınması, build yolları ve testler hazırlandıktan sonra yapılacaktır.

## Mevcut envanter

### C uygulaması

Konum: src - c - 28-01-26/

- main.c: CLI/derleyici orkestrasyonu.
- parser/: AST, lexer, parser ve semantic modülleri.
- codegen/: Codegen giriş noktası ve target yapılandırması.
- lang/: Dil mesaj dosyaları.
- tests/: NexusFlow örnekleri ve bazı OIR/assembly çıktıları.
- Aynı dizinde DLL, EXE ve statik kütüphane gibi derleme ürünleri de bulunuyor.

**Değerlendirme:** C tarafında anlamlı bir frontend ve codegen iskeleti var. Dosyaların varlığı, tüm özelliklerin çalıştığını kanıtlamıyor. Otomatik çalışan bir build/test hattı bu incelemede doğrulanmadı.

### C++ uygulaması

Konum: src/

- parser/: Lexer, NexusParser, OIR writer ve generator.
- analyzer/: Analyzer, OIR reader, symbol table ve type system.
- occ/: OCC ana programı.
- oce/: IDE/editor bileşenleri.

**Değerlendirme:** Referans alınabilecek tasarım parçaları var. Ancak bu hat ana geliştirme hattı olmayacak. OIR writer/reader güvenlik ve veri bütünlüğü kontrolleri, ayrıca modül sınırları denetlenmeli.

### Diğer SDK bileşenleri

- targets/: Hedef tanımları.
- stdlib/: NexusFlow standart kütüphane kaynakları.
- dpslib/: Bağımlılık kütüphaneleri.
- test/: SDK düzeyinde test girdileri.
- bin/, lib/: Derleme ürünleri.
- docs/: Çok sayıda dil/derleyici tasarım belgesi.
- impl_plan.md ve kodlama_plani.md aynı içerik ve aynı blob SHA değerine sahip mükerrer plan dosyalarıdır.

## Statik incelemede görülen somut riskler

1. **C config okuma:** get_config_language() içinde eşleşmeyen ilk satırda döngüden çıkılıyor. occ.ocf dosyası [package] ile başladığı için language alanına ulaşılmayabilir.
2. **C path işlemi:** main.c içinde slash ve backslash işaretçileri NULL olabilecek durumdayken doğrudan karşılaştırılıyor. NULL işaretçileri arasında ilişkisel karşılaştırma tanımlı değildir.
3. **C CLI:** Kaynak dosyası doğrudan argv[1] kabul ediliyor; seçeneklerin farklı sıralanması güvenli biçimde ele alınmıyor.
4. **C codegen:** Target yapılandırması okunsa da çıktı üretiminde x86-64/NASM varsayımları var. Target dosyası bulunması ARM64 desteğini kanıtlamaz.
5. **C hata kodları:** Bazı alt işlem hataları üst seviyeye taşınmıyor; başarısız codegen durumunda süreç yine başarılı çıkış kodu döndürebilir.
6. **C++ OIR:** C++ OIR2 reader/writer formatı, C AST serialization ile aynı değil. Reader'ın header/offset/length doğrulaması ve string sonlandırması ayrıca denetlenmeli.
7. **Build düzeni:** Build betikleri farklı kaynak yolları ve farklı uygulama hatları varsayıyor. Temiz checkout build'i henüz doğrulanmadı.
8. **Görev listesi:** C++ modülleri için yazılmış yüzde tamamlanma iddiaları test kanıtı ile desteklenmiyor. Bundan sonra yüzdeler yerine kanıt temelli durum kullanılmalı.

## Bu raporun kapsamı dışı

- Hiçbir C veya C++ binary'si burada çalıştırılmadı.
- Derleyici çıktılarının semantik doğruluğu ölçülmedi.
- Bellek sızıntısı iddiaları ASan/Valgrind veya eşdeğer testler çalıştırılmadan kesinleştirilmedi.
- ISA, ABI ve OIR v1 tasarımı nihai hale getirilmedi.

## Öncelikli sonuç

İlk iş yeni özellik eklemek değil; C hattını tekrarlanabilir biçimde derlemek, temel testleri otomatikleştirmek ve CLI/config/path sorunlarını kapatmaktır. Bundan sonra AST/semantic güvenilirliği ve kanonik OIR'a geçilecektir.
