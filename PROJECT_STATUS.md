# OmniCore SDK — Başlangıç Durum Raporu

**Değerlendirme tarihi:** 30 Ekim 2026  
**İncelenen dal:** main  
**Çalışma dalı:** refactor/c-core-roadmap  
**İnceleme türü:** GitHub kaynak ağacı ve metin dosyalarının statik incelemesi. Bu rapor derleme/test sonucu değildir.

## Karar

C ana kaynak olacaktır. C++ kaynakları korunacak ve referans olarak incelenecektir. OmniCore hedefi Windows, Linux ve macOS host/target platformları ile x86-64 ve AArch64 gibi genel CPU mimarilerini desteklemektir. Kullanıcı isterse CPU modeli ve ek yetenekleri seçebilir; hedef belirtilmezse host OS + host architecture varsayılan olur.

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

## İlk statik incelemeden kalan riskler ve güncel durum

1. **C config okuma — ilk katman çözüldü:** `get_config_language()` artık config dosyasının tüm satırlarını tarıyor. Config dosyası da executable konumundan bulunuyor.
2. **C path işlemi — ilk katman çözüldü:** NULL slash/backslash karşılaştırması kaldırıldı. Runtime asset yolları executable konumuna göre kuruluyor.
3. **C CLI — ilk katman çözüldü:** Kaynak ve `--target` sıralaması serbest; bilinmeyen seçenek, eksik target değeri ve birden fazla kaynak reddediliyor. CI testleriyle doğrulandı.
4. **C codegen — açık:** Çıktı üretimi hâlâ x86-64/NASM varsayımlarına dayanıyor. Target dosyasının varlığı ARM64 desteğini kanıtlamaz.
5. **C hata kodları — kısmen doğrulandı:** Normal compile yolu ve CLI hata yolları test edildi. Codegen modülünün yüklenememesi/bozuk sembol gibi hata enjeksiyon testleri henüz yok.
6. **C++ OIR — açık:** C++ OIR2 reader/writer formatı C AST serialization ile aynı değil. Reader'ın header/offset/length doğrulaması ve string sonlandırması ayrıca denetlenmeli.
7. **Build düzeni — ilk katman çözüldü:** CMake build, Windows/Ubuntu/macOS CI matrisi ve dokuz CTest kontrolü mevcut (sonraki target-parser testi eklenmeden önceki durum).
8. **Görev listesi:** İlerleme yüzdeleri yerine test veya kod kanıtı olan tamamlanma durumu kullanılmaya devam edilecek.

## Bu raporun kapsamı dışı

- Assistant çalışma ortamında yerel binary çalıştırılmadı; build ve testler GitHub Actions runner'larında yürütüldü.
- Derleyici çıktılarının semantik doğruluğu tüm dil özellikleri için ölçülmedi.
- ASan/UBSan/Valgrind bellek doğrulaması henüz yapılmadı.
- ISA, ABI ve kanonik OIR v1 tasarımı nihai hale getirilmedi.

## Öncelikli sonuç

İlk build, temel AST girdi güvenliği ve CLI/path katmanı artık üç platformda CI ile doğrulanıyor. Sonraki adımlar: runtime yol davranışının test kapsamını genişletmek, target descriptor parser'ını mevcut dosya biçimiyle uyumlu ve güvenli hale getirmek, ardından AST/OIR formatını sürümleme ve doğrulama kurallarıyla ele almak.


## Çalışma dalındaki ilk düzeltmeler

Bu raporun ardından C ana programında aşağıdaki değişiklikler kodlandı:

- Config içindeki language anahtarını ilk satırda eşleşme yok diye okumayı bırakma davranışı düzeltildi.
- Config dil tamponuna güvenli sonlandırma eklendi.
- Dil mesaj dosyası yolu, C uygulamasının kendi lang/ dizinine yönlendirildi.
- CLI kaynak dosyası ve --target seçeneklerini farklı sıralarda kabul edecek şekilde düzenlendi; bilinmeyen seçenekler ve birden fazla kaynak dosyası reddediliyor.
- Windows ve POSIX yol ayırıcıları NULL pointer karşılaştırması yapmadan ele alınıyor.
- Parser, semantic ve codegen başarısızlıklarının süreç çıkış koduna taşınması eklendi.

**Doğrulama durumu:** Bu ilk düzeltmeler CMake/CTest hattına alındı. CLI seçenek sıralaması ve hata durumları üç platformda CI ile doğrulandı. Runtime asset yollarının executable konumuna taşınması da sekiz testlik son CI koşusunda Windows, Ubuntu ve macOS üzerinde başarılı oldu.


## Hedef platform kararı

NEXSUS işlemcisi mevcut bir bağımlılık veya ön koşul değildir. Derleyici genel amaçlıdır. Planlanan platform ailesi Windows, Linux ve macOS; işlemci mimarileri ise ilk etapta x86-64 ve AArch64 olarak ele alınacaktır. Her OS/architecture kombinasyonu ABI, object format, assembler/linker ve testleriyle ayrı doğrulanacaktır. CPU model/feature seçimi target descriptor üzerinden yapılabilir; gerçek destek backend capability testleriyle belgelenecektir.


## Target modeli açısından ek bulgular

Target modeli için ayrı [TARGET_MODEL.md](TARGET_MODEL.md) sözleşmesi oluşturuldu. Mevcut C kodu bu hedefle henüz aynı noktada değil:

1. **Default target:** `main.c` şu an `BareMetal64` descriptor'ını varsayılan seçiyor. Host OS/architecture otomatik tespiti henüz yok; bu nedenle host-default target kararı henüz uygulanmadı.
2. **Target yolu — ilk katman çözüldü:** Target dizini executable konumundan `../targets` olarak kuruluyor. Varsayılan `BareMetal64` dosyasıyla mevcut descriptor biçimi eşleşiyor.
3. **Target listesi — ilk katman çözüldü:** `list_targets()` Windows FindFirstFile ve POSIX opendir/readdir yollarını destekliyor.
4. **Codegen yüklemesi — ilk katman çözüldü:** Windows `LoadLibrary`, Linux/macOS `dlopen` kullanıyor; shared library executable yanından mutlak yol ile yükleniyor.
5. **Codegen hata kodu:** Modül veya `codegen_run` sembolü bulunamadığında ve codegen hata döndürdüğünde OCC başarısız çıkış kodu üretir. Başarı yolu test edildi; failure injection testi henüz yok.
6. `parser_build.bat` dosyası MinGW `g++` ile C++ `OIR_Generator.cpp` derliyor; C ana uygulamasının build sistemi değildir. Yeni `CMakeLists.txt` C hattını bundan ayrı tanımlar.

C derleyicisinin Windows, Linux ve macOS host sistemlerinde derlenmesi ve temel compile yolu CI ile doğrulanmıştır. Bu, codegen'in her target OS/architecture için doğru makine kodu ürettiği anlamına gelmez.


## Codegen target parser — mevcut durum

İlk statik incelemede kaydedilen parser riskleri güncel C kodunda giderilmiş görünüyor:

- target_config_load() dosya açılamadığında hata döndürüyor; varsayılanlarla sessizce devam etmiyor.
- String alanları hedef tampon boyutuna göre doğrulanıyor; satır uzunluğu, section/key uzunluğu ve toplam key sayısı sınırlandırılıyor.
- Bilinmeyen section/key, yinelenen key, eksik zorunlu alan, desteklenmeyen enum/SIMD ve hatalı sayı/boolean/array değerleri reddediliyor.
- target_backend_check() descriptor'ın parse edilebilmesini gerçek backend desteğinden ayırıyor.
- Codegen hâlâ x86-64 NASM şablonu üretir; ancak desteklenmeyen OS/ABI/format/mimari kombinasyonlarını backend kontrolü reddetmelidir.

**Kalan doğrulama:** Parser test dosyası depoda bulunuyordu ancak CMake/CTest'e bağlı değildi. 31 Ekim'de test kendi bozuk fixture'larını üretecek şekilde genişletildi ve CTest'e eklendi. Yeni testin üç platformlu CI sonucu henüz alınmadı.


## C build altyapısı — 30 Ekim 2026

Çalışma dalına C uygulaması için ilk tekrarlanabilir build altyapısı eklendi:

- `omnicore-sdk/CMakeLists.txt`: C11 projesi; frontend static library, codegen shared library ve OCC executable hedeflerini ayrı tanımlar.
- `omnicore-sdk/build_c.bat` ve `build_c.sh`: Windows ve Unix benzeri sistemler için CMake build/test sarmalayıcıları.
- `omnicore-sdk/BUILDING.md`: gereksinimler, komutlar, çıktı yolları ve C/C++ hatlarının ayrımı.
- `.github/workflows/c-build.yml`: Windows, Ubuntu ve macOS üzerinde derleme/CTest matrisi.
- C OCC'nin codegen yükleyicisi Windows, Linux ve macOS için ayrı dinamik kütüphane adlarını kullanacak şekilde düzenlendi. Modül yükleme hatası artık sessizce yalnızca IR üretip başarılı çıkmamalı.

**Kaynak seçimi:** C frontend için `ast.c`, `lexer.c`, `parser.c`, `semantic.c` derlenir. `parser/main.c` ikinci bir `main()` tanımladığı için ana OCC hedefinden özellikle çıkarılmıştır. C++ `src/` ve eski C++ build betikleri korunmuştur.

**Doğrulama durumu:** GitHub Actions temiz checkout matrisi Windows, Ubuntu ve macOS üzerinde başarılı tamamlandı. Her platformda CMake configure, C derlemesi ve CTest aşamaları geçti. Son doğrulanan koşuda her platformda **9/9 test başarılıdır**:

1. Bozuk/truncated AST akışlarını reddetme; AST_UNPACK ve AST_NEXUS_HALT round-trip; 700 sibling node listesi.
2. OCC help çıktısı.
3. Bilinmeyen CLI seçeneğini reddetme.
4. Eksik kaynak dosyasını reddetme.
5. `--target` seçeneğini kaynak dosyasından önce kabul etme.
6. Değersiz `--target` seçeneğini reddetme.
7. Birden fazla kaynak dosyasını reddetme.
8. `tests/test1.nxf` dosyasını lexer→parser→semantic→AST serialization/deserialization→shared codegen yolundan geçirme.
9. Ayrı executable klasöründeki Türkçe config ayarının semantic hata mesajına yansımasını doğrulama.

Son doğrulanan CI koşusunda üç platformda da derleyici uyarısı kalmadı. Bu testler ilk güvenilirlik katmanını doğrular; dilin tüm özelliklerinin veya tüm AST düğüm türlerinin doğrulandığı anlamına gelmez. Runtime config, language, target ve codegen dosyaları executable konumundan çözülür; son CI testleri farklı çalışma dizininden başlatmayı doğrular.


## AST ve kaynak dosyası okuma güvenliği — ilk katman

- AST deserializer tüm sabit boyutlu alan okumalarında tam okuma kontrolü yapacak şekilde düzenlendi.
- Node türü, string uzunluğu, toplam node sayısı ve iç içe AST derinliği sınırlandırıldı.
- Sibling node listeleri iteratif okunuyor; uzun düz listeler iç içe derinlik sınırını tüketmiyor.
- AST_UNPACK ve AST_NEXUS_HALT için yanlış struct yerleşimi kullanımı düzeltildi.
- Deserialize işlemi sonunda beklenmeyen trailing byte varsa dosya reddediliyor.
- Kaynak dosyası okuyucusunda fseek, ftell ve fread sonuçları kontrol ediliyor.
- Bozuk AST, geçersiz node türü, negatif/aşırı string uzunluğu, özel node round-trip ve uzun sibling listesi testleri eklendi.

**Kalan işler:** AST düğüm türlerinin tamamı için round-trip test matrisi; AST serializer yazma tarafında fwrite hata raporlaması; ASan/UBSan/Valgrind ile bellek güvenliği doğrulaması.


## Hâlâ açık target/codegen işleri

- Host OS + host architecture otomatik tespiti ve host-default target çözümlemesi uygulanmadı.
- C backend yalnızca x86-64/64-bit/little-endian/bare-metal/raw profili destekliyor; Linux/Windows/macOS ABI'leri ve AArch64/RISC-V için codegen henüz yok.
- Codegen'in assembly şablonu target'tan bağımsız genel bir backend mimarisine ayrılmadı; mevcut destekli profilin dışına çıkılmamalı.
- Target parser testinin yeni CTest entegrasyonu Windows/Linux/macOS üzerinde henüz doğrulanmadı.

Windows/Linux/macOS üzerinde C uygulamasının derlenmesi, bu işletim sistemleri veya mimarileri için doğru makine kodu üretildiği anlamına gelmez.


## Target parser test entegrasyonu — 31 Ekim 2026

- Depoda bulunan test_target_parser.c CMake/CTest hattına bağlandı.
- Test; mevcut beş target descriptor'ını parse eder ve backend destek durumunu ayrı kontrol eder.
- Test ayrıca eksik dosya, unknown section/key, duplicate key, aşırı uzun identifier/satır, eksik zorunlu alan ve bilinmeyen architecture/SIMD profilini reddetmeyi sınar.
- CTest tanımındaki test sayısı 9'dan 10'a çıktı.
- **Durum:** Kod ve CTest tanımı branch'e işlendi. Yeni 10 testlik matrisin Windows, Ubuntu ve macOS CI sonucu bekleniyor; bu rapor yeni testin geçtiğini iddia etmez.
