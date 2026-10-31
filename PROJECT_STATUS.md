# OmniCore SDK — Başlangıç Durum Raporu

**Değerlendirme tarihi:** 30 Ekim 2026  
**İncelenen dal:** main  
**Çalışma dalı:** refactor/c-core-roadmap  
**İnceleme türü:** GitHub kaynak ağacı ve metin dosyalarının statik incelemesi. Bu rapor derleme/test sonucu değildir.

## Karar

C ana kaynak olacaktır. C++ kaynakları korunacak ve referans olarak incelenecektir. OmniCore hedefi Windows, Linux ve macOS host/target platformları ile x86-64 ve AArch64 gibi genel CPU mimarilerini desteklemektir. Kullanıcı isterse CPU modeli ve ek yetenekleri seçebilir; hedef belirtilmezse host OS + host architecture varsayılan olur.

## Mevcut envanter

### C uygulaması

Konum: omnicore-sdk/src/c/

- main.c: CLI/derleyici orkestrasyonu.
- parser/: AST, lexer, parser ve semantic modülleri.
- codegen/: Codegen giriş noktası ve target yapılandırması.
- lang/: Dil mesaj dosyaları.
- tests/: CTest tarafından kullanılan .nxf girdileri ve C test programları.
- Eski DLL, EXE ve statik kütüphane çıktıları temp/build-artifacts/ altında arşivlendi.

**Değerlendirme:** C tarafında anlamlı bir frontend ve codegen iskeleti var. Dosyaların varlığı, tüm özelliklerin çalıştığını kanıtlamıyor. Otomatik çalışan bir build/test hattı bu incelemede doğrulanmadı.

### C++ uygulaması

Konum: temp/legacy-cpp/src/ (C++); omnicore-sdk/src/oce/ (IDE/editor)

- parser/: Lexer, NexusParser, OIR writer ve generator.
- analyzer/: Analyzer, OIR reader, symbol table ve type system.
- occ/: OCC ana programı.


**Değerlendirme:** Referans alınabilecek tasarım parçaları var. Ancak bu hat ana geliştirme hattı olmayacak. OIR writer/reader güvenlik ve veri bütünlüğü kontrolleri, ayrıca modül sınırları denetlenmeli.

IDE/editor kaynağı ayrı tutulmuştur: omnicore-sdk/src/oce/.

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

**Kalan doğrulama:** Parser test dosyası depoda bulunuyordu ancak CMake/CTest'e bağlı değildi. 31 Ekim'de test kendi bozuk fixture'larını üretecek şekilde genişletildi ve CTest'e eklendi. Yeni test Windows, Ubuntu ve macOS üzerinde geçti; her platformda 10/10 CTest başarılı.


## C build altyapısı — 30 Ekim 2026

Çalışma dalına C uygulaması için ilk tekrarlanabilir build altyapısı eklendi:

- `omnicore-sdk/CMakeLists.txt`: C11 projesi; frontend static library, codegen shared library ve OCC executable hedeflerini ayrı tanımlar.
- `omnicore-sdk/build_c.bat` ve `build_c.sh`: Windows ve Unix benzeri sistemler için CMake build/test sarmalayıcıları.
- `omnicore-sdk/BUILDING.md`: gereksinimler, komutlar, çıktı yolları ve C/C++ hatlarının ayrımı.
- `.github/workflows/c-build.yml`: Windows, Ubuntu ve macOS üzerinde derleme/CTest matrisi.
- C OCC'nin codegen yükleyicisi Windows, Linux ve macOS için ayrı dinamik kütüphane adlarını kullanacak şekilde düzenlendi. Modül yükleme hatası artık sessizce yalnızca IR üretip başarılı çıkmamalı.

**Kaynak seçimi:** C frontend için `ast.c`, `lexer.c`, `parser.c`, `semantic.c` derlenir. `parser/main.c` ikinci bir `main()` tanımladığı için ana OCC hedefinden özellikle çıkarılmıştır. C++ kaynakları `temp/legacy-cpp/src/` altında; eski C++ build betikleri `temp/legacy-cpp/scripts/` altında korunmuştur.

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


## AST_EXTERN_DECL round-trip düzeltmesi — 31 Ekim 2026

AST tür envanterinde AST_EXTERN_DECL için bir lifecycle uyumsuzluğu bulundu: düğüm boyutu ASTFuncDecl olarak ayrılıyordu ancak yazdırma, destroy, serialize ve deserialize switch dalları bu türü AST_FUNC_DECL ile aynı gruba almıyordu. Bu durum AST_EXTERN_DECL alanlarının serileştirmede kaybolmasına ve ayrılmış alanların temizlenmemesine yol açabilirdi.

- Dört AST lifecycle switch'ine AST_EXTERN_DECL eşlemesi eklendi.
- AST_EXTERN_DECL adı, dönüş tipi, flags, parametre ve body alanlarını kontrol eden round-trip regresyon testi eklendi.
- GitHub Actions son koşusu (commit 897a29fa57eb70aeb48af045d04dcea35a6087b2) Windows, Ubuntu ve macOS üzerinde başarılı tamamlandı.
- Her üç platformda CMake build başarılı; CTest **10/10** geçti. AST_EXTERN_DECL round-trip testi mevcut AST test programının içine dahil edildi.

**Kapsam sınırı:** Bu düzeltme tüm AST düğüm türlerinin eksiksiz test edildiği anlamına gelmez. Tam round-trip matrisi, serializer yazma hatalarının API üzerinden raporlanması ve sanitizer testleri hâlâ açık işlerdir.


## AST tag ve literal payload kontrolleri — 31 Ekim 2026

AST tür envanteri ve round-trip testleri genişletildi:

- AST enum'undaki tüm tag değerleri için allocation, node type, line/column ve sibling bağlantısını kapsayan smoke round-trip testi eklendi.
- AST_LITERAL yapısındaki token type alanının serializer/deserializer tarafından yazılıp okunmadığı tespit edildi. Semantic analiz bu alanı kullandığı için kayıp anlamlıydı.
- AST_LITERAL serializer/deserializer artık token type alanını da saklıyor. Regresyon testi token type, lit_type, int_value, float_value ve string_value alanlarını kontrol ediyor.
- AST_EXTERN_DECL alan testi ve özel AST_UNPACK/AST_NEXUS_HALT testleri korunuyor.
- Son doğrulanan kod koşusu (commit 8ea13a90526a7bd9fa80580c9c8aae0f1c4480b9) Windows, Ubuntu ve macOS üzerinde başarılı; üç platformda CMake build ve 10/10 CTest geçti.

**Kapsam sınırı:** Tüm düğüm ailelerinin serileştirilen alanları için field-level fixture matrisi artık eklendi. Sanitizer altında bellek yönetimi ve derinlik/sınır davranışları ayrıca doğrulanacak. AST stream biçimi sürümlenmiş bir genel OIR formatı değildir; AST_LITERAL alan dizilimi değiştiğinden eski dahili .oir dosyaları yeniden üretilmelidir.


## AST yazma hatası ve OCC hata akışı — 31 Ekim 2026

- ast_serialize() artık başarı/başarısızlık durumunu int olarak döndürüyor. ASTWriter, tüm scalar ve string yazımlarını ortak ast_write_exact() üzerinden izliyor; ilk fwrite hatasında akış başarısız duruma geçiyor.
- String writer, deserializer ile aynı 16 MiB üst sınırını uyguluyor.
- parser_save_ir() artık int döndürüyor; ast_serialize() sonucunu ve fclose() sonucunu kontrol ediyor. Başarısız kayıt durumunda kısmi çıktı dosyasını kaldırmayı deniyor.
- OCC main akışı parser_save_ir() başarısızsa "IR kaydedildi" mesajını yazmıyor ve codegen modülünü çalıştırmıyor.
- AST test programına var olmayan parent dizinine IR yazma denemesi eklendi; hata durumunun çağırana iletildiği doğrulanıyor.

Son kod doğrulaması commit 377e98486507b1156ac7307c8e6b5b59be32b2fe üzerinde tamamlandı: Windows, Ubuntu ve macOS build başarılı; her platformda CTest 10/10 geçti.

**Kalan sınır:** Her fwrite hatasını platformlar arasında zorlayarak test etmek henüz yapılmadı. Mevcut test, parser_save_ir dosya açma başarısızlığının ve API dönüş değerinin doğru iletilmesini doğruluyor; fclose hata yolu kodda kontrol ediliyor ancak özel bir arıza enjeksiyon testi yok.


## AST field-level round-trip matrisi — 31 Ekim 2026

- AST enum aralığındaki her tag için ayrı fixture oluşturuluyor.
- Her düğüm ailesinde serializer'ın yazdığı bütün alanlar anlamlı, sıfır olmayan veya metin içeren değerlerle dolduruluyor.
- Orijinal AST serialize ediliyor, deserialize ediliyor, tekrar serialize ediliyor; iki byte stream birebir karşılaştırılıyor.
- Böylece AST tag'lerinin yanı sıra alan sırası, scalar alanlar, string alanlar, child node bağlantıları ve nullable alanların stream düzeni de kontrol ediliyor.
- Son doğrulanan commit fcc53eff7ce8fa539361c03a81466c9821fda907 üzerinde Windows, Ubuntu ve macOS build/test başarılı; CTest her platformda 10/10 geçti.

**Kalan:** ASan/UBSan/Valgrind altında ownership, cleanup, derin ağaç ve hata durumları test edilmedi. Field-level round-trip kapsamı bu sanitizer doğrulamasının yerine geçmez.


## ASan/UBSan sonucu ve semantic scope leak düzeltmesi — 31 Ekim 2026

İlk sanitizer koşusu, normal CI'da görünmeyen iki adet 5 byte'lık sızıntı gösterdi. İki allocation da semantic.c içindeki add_slot() fonksiyonunda ayrılan Slot.type_name alanından geliyordu. exit_scope() yalnızca Slot.name ve Slot nesnesini serbest bırakıyor, type_name ve olası diğer Slot alt kaynaklarını bırakıyordu.

Düzeltme:
- Slot kaynaklarının tamamını temizleyen ortak destroy_slot() fonksiyonu eklendi.
- exit_scope() artık scope dışına çıkan her Slot için destroy_slot() kullanıyor.
- semantic_destroy() aynı ortak temizleme yolunu kullanıyor; param_types ve FieldInfo listeleri de tek noktadan serbest bırakılıyor.
- Null context kontrolü ve temizleme sonrası head/error listelerinin sıfırlanması eklendi.

Doğrulama:
- Son sanitizer koşusu commit 2815e6f847eb048db90f21428497fec4202fb99d üzerinde başarılı.
- Ubuntu GCC ASan + UBSan, LeakSanitizer açıkken CTest **10/10** geçti; önceki iki leak raporu tekrarlanmadı.
- Aynı commitin normal Windows, Ubuntu ve macOS build/test matrisi de başarılı.
- Valgrind henüz CI'ya eklenmedi.

Bu sonuç yalnızca mevcut 10 testin kapsamındaki yolların sanitizer altında temiz olduğunu gösterir; tüm semantik program şekillerinde bellek hatası bulunmadığını kanıtlamaz.


## Repository klasör düzenlemesi — 31 Ekim 2026

- Etkin C derleyici kaynakları omnicore-sdk/src/c/ altına taşındı; CMake C_SOURCE_DIR bu yolu kullanacak şekilde güncellendi.
- Eski C++ parser/analyzer/OCC kaynakları silinmeden temp/legacy-cpp/src/ altına taşındı; eski build betikleri temp/legacy-cpp/scripts/ altında tutuluyor.
- Eski DLL/EXE/static library/object/IR/assembly ürünleri temp/build-artifacts/ altında korundu; aktif CMake build bunları kullanmıyor.
- Eski bağımsız .nx test girdileri temp/legacy-tests/ altına alındı. CMake'in kullandığı test girdileri src/c/tests/ içinde kaldı.
- Byte-for-byte aynı olduğu doğrulanan belge kopyaları temp/duplicate-docs/ altında arşivlendi. Farklı içerikli benzer belgeler yerinde bırakıldı.
- omnicore-sdk/src/oce/ IDE/editor projesi bağımsız bileşen olduğu için taşınmadı.

Doğrulama: Yeni yol ile Ubuntu build ve CTest geçti; Ubuntu ASan/UBSan işi de başarılı. Windows, Ubuntu ve macOS build/CTest matrisinin tamamı geçti; Ubuntu ASan/UBSan ve Valgrind işleri de başarılı.


## Klasör düzenlemesi sonrası CI doğrulaması

Son tam doğrulanan kod ağacı commit 9bc6e20be9b8fa48d358f961d927f77992b8aa3d üzerinde: Windows, Ubuntu ve macOS build/CTest işleri başarılı ve her platformda 10/10 test geçti. Ubuntu GCC ASan/UBSan işi 10/10 geçti; Valgrind memcheck işi de 10/10 geçti. Valgrind yalnızca definite/indirect leak türlerini hata kabul edecek şekilde yapılandırıldı. Ardından gelen 7d84739aef8c8577e4686b6ad47d80a164798923 commitinde yalnızca tarihsel plan belgesi arşive taşındı ve yönlendirme bağlantıları güncellendi; derleme kaynakları değişmedi.


## Lexer / Parser / Semantic test ayrımı — 31 Ekim 2026

- test_lexer.c: declaration prefix, numeric literal, Nexus Flow operator token'ları ve satır/sütun konumu için izole test.
- test_parser.c: geçerli kaynak kabulü, bozuk syntax reddi ve parse hatasında partial AST'nin parser context tarafından sahiplenilip temizlenmesi.
- test_semantic.c: geçerli program ve undefined identifier semantik tanı testi.
- parser_run() artık AST kökünü parse başında context'e bağlar; syntax hatasıyla erken dönüldüğünde parser_destroy() partial tree'yi temizleyebilir.
- CI commit b231af5a0476d0c4aee6728361ab5725a1b5edbd üzerinde Windows, Ubuntu ve macOS build/test başarılı; CTest **13/13** geçti. Ubuntu ASan/UBSan ve Valgrind işleri de **13/13** geçti.

Bu testler temel regresyon iskeletidir; lexer token envanterinin tamamını, tüm parser grammar dallarını veya semantic ownership kurallarının tamamını kapsadığı anlamına gelmez.


## OIR ortak artifact formatı — 31 Ekim 2026

Kullanıcının onayıyla derleme aşamaları arasında ortak veri taşıma standardı olarak OIR yönü belirlendi. [OIR_FORMAT.md](OIR_FORMAT.md) taslağı eklendi.

- Her modül bir OIR artifact alacak ve tanımlı kind/schema version ile yeni bir OIR artifact üretecek.
- Aynı artifact, pipeline içinde opaque in-memory handle olarak aktarılabilecek; checkpoint istendiğinde aynı container biçimi `.oir` dosyasına yazılacak.
- Container ile payload ayrıldı: SOURCE_UNIT, TOKEN_STREAM, AST, SEMANTIC_AST, OIR_MODULE, TARGET_IR, ASSEMBLY ve OBJECT ayrı payload türleri olacak.
- Container fixed-width little-endian alanlar, length-delimited sections ve host pointer/struct bağımsız serileştirme hedefliyor.
- Mevcut C AST serializer native int/enum gösterimi yazdığı ve artifact kind/schema metadata taşımadığı için OIR v1 olarak kabul edilmedi. Eski C++ OIR2 de ayrı legacy format olarak kalıyor.
- Canonical OIR_MODULE opcode/type/memory/ownership/SSA semantiği henüz tasarlanmadı.
- OIR v1.0 container header offset'leri ve başlangıç numeric ID'leri tanımlandı; container reader/writer C kodu eklendi. Payload schema field layout'ları henüz tasarlanmadı.

**Sıradaki teknik adım:** OIR container'ın kesin binary layout'unu ve C reader/writer API'sini belirlemek; ardından malformed/truncated/oversized artifact testlerini eklemek.


## OIR v1 container implementation — 31 October 2026

İlk OIR container C uygulaması eklendi:
- `omnicore-sdk/src/c/oir/oir.h` / `oir.c`: 44-byte fixed header, 16-byte section headers, little-endian field encoding, length-delimited sections, in-memory writer, validating reader and read-only section views.
- `omnicore-sdk/src/c/tests/test_oir.c`: writer/reader round-trip and malformed input checks.
- `omnicore-sdk/CMakeLists.txt`: `occ_oir` utility library and `oir_container_roundtrip_and_validation` CTest target.
- `OIR_FORMAT.md`: initial byte offsets and stable IDs documented.

Current scope is deliberately limited to the container. Payload schemas (SOURCE_UNIT/TOKEN_STREAM/AST/SEMANTIC_AST/OIR_MODULE), filesystem checkpoint helpers, stage ABI and stage DLL integration are not implemented yet. GitHub Actions run 41143050717 completed successfully: Windows, Ubuntu and macOS build/CTest passed **14/14**; Ubuntu ASan/UBSan and Valgrind also passed **14/14**.


## OIR SOURCE_UNIT / TOKEN_STREAM schema taslağı — 31 Ekim 2026

- `OIR_TOKEN_REGISTRY.md`: Mevcut C lexer enum'unda 160 token türü bulundu. OIR için 1–160 arası ayrı ve kalıcı token ID listesi oluşturuldu. OIR ID'leri C enum ordinal değerlerine bağlanmayacak; adaptör sembolik eşleme yapacak.
- `OIR_PAYLOAD_SCHEMAS.md`: SOURCE_UNIT schema 1.0 ve TOKEN_STREAM schema 1.0 için ilk payload düzeni yazıldı.
- SOURCE_UNIT, kaynak baytlarını UTF-8/path metadata ile orijinal byte dizisi olarak saklar.
- TOKEN_STREAM, lexeme metnini çoğaltmak yerine SOURCE_UNIT içindeki byte offset/length alanlarına referans verir. Token kaydı 32 byte; token ID, diagnostic reference, source span ve line/column alanları içerir.
- Mevcut C Token struct henüz source byte offset veya diagnostic ID taşımıyor. Lexer error token'ı source buffer yerine mesaj metnine işaret edebildiği için pointer subtraction ile offset üretmek güvenli değil.
- Lexer'da multiline string line tracking, unterminated block comment davranışı ve enum tokenlarının gerçek emission coverage'ı OIR token adaptöründen önce denetlenmeli.

Bu bölüm payload şema taslağıdır; SOURCE_UNIT/TOKEN_STREAM encode/decode kodu henüz yazılmadı.


## SOURCE_UNIT payload codec — 31 Ekim 2026

- `omnicore-sdk/src/c/oir/oir_payload.h/.c`: SOURCE_UNIT schema 1.0 için in-memory writer ve zero-copy validated view eklendi.
- Payload 24-byte header, UTF-8 path metadata ve orijinal kaynak byte dizisinden oluşuyor.
- Kaynak ve path için strict UTF-8 doğrulaması; embedded NUL, overlong/invalid encoding, yanlış source ID, kesilmiş payload, tutarsız uzunluk ve küçük buffer reddi eklendi.
- `test_oir_source_unit.c` CTest'e bağlandı; OIR container testine ek olarak toplam test sayısı 15 oldu.
- GitHub Actions run 41143616273: Windows, Ubuntu ve macOS build/CTest **15/15** başarılı; Ubuntu ASan/UBSan ve Valgrind **15/15** başarılı.

TOKEN_STREAM payload şeması belgelendi ancak codec henüz yazılmadı. Lexer token'larının source byte offset ve diagnostic ID bilgisi taşıması gerektiği için önce lexer token metadata uyarlaması ve 160 token enum değerinin OIR stable ID eşlemesi yapılacak.


## OIR token ID adapter — 31 Ekim 2026

- `oir_token_map.h/.c`: C frontend OmniTokenType ile OIR registry 1.0 arasına açık, sembolik forward/reverse mapping eklendi. OIR ID'leri C enum numeric değerleriyle aynı kabul edilmiyor.
- `test_oir_token_map.c`: 160 token türünün tamamı için OIR ID → C enum → OIR ID round-trip; geçersiz enum, geçersiz ID ve NULL output pointer reddi.
- CMake'de adapter, container library'den ayrı `occ_oir_lexer_adapter` static hedefi olarak tutuldu. Böylece temel OIR container/parser tiplerine bağımlı değil.
- GitHub Actions run 41143848197: Windows, Ubuntu ve macOS CMake build/CTest **16/16** başarılı; Ubuntu ASan/UBSan ve Valgrind işleri de başarılı.

Bu adapter yalnızca enum kimlik eşlemesini sağlar. Lexer'ın tüm 160 token türünü gerçekten ürettiği anlamına gelmez; emission audit ve token stream serialization hâlâ açık.


## Lexer token coverage ve source span metadata — 31 Ekim 2026

İlk envanter karşılaştırmasında lexer.h içindeki 160 token enum değerinden 13'ünün lexer.c içinde hiçbir eşlemesi olmadığı bulundu. Mevcut enum adlarına göre eksikler tamamlandı:
- Anahtar kelimeler: asm (uzunluk koşulu hatalıydı), fastexec, break, echo, send, receive.
- Tip adları: f32, f64, bool, bit, byte, hex, void, any.
- is_err kontrolündeki length==5 iken 6 byte memcmp yapan hatalı dal kaldırıldı; doğru length==6 kontrolü korundu.

Token metadata:
- Token artık source_offset ve source_length alanlarını lexeme/message pointer ve length alanlarından ayrı taşır.
- ParserContext lexer token başlangıç byte offset, line ve column bilgilerini tutar.
- Token konumu tarama bitişinden geriye hesaplanmak yerine başlangıçta yakalanır.
- String içindeki newline iki kez sayılmaktan çıkarıldı.
- test_lexer.c yeni keyword/type eşleşmelerini ve çok satırlı string sonrası source offset/line/column değerlerini kontrol ediyor.

Doğrulama: GitHub Actions run 41144498647 tamamlandı. Windows, Ubuntu ve macOS build/CTest **16/16** başarılı; Ubuntu ASan/UBSan ve Valgrind de **16/16** başarılı.

Kapanmayan block comment için de TOKEN_ERROR/source span testi eklendi. Sonraki GitHub Actions run 41144698187 ile Windows, Ubuntu ve macOS build/CTest **16/16**, Ubuntu ASan/UBSan ve Valgrind **16/16** başarılı.


## Job-owned diagnostic collector — 31 Ekim 2026

- `omnicore-sdk/src/c/core/diagnostics.h/.c`: bağımsız C collector eklendi.
- Diagnostic kaydı code, severity, stage ID, message ID ve optional source span taşıyor.
- Message/hint metinleri collector-owned arena'ya kopyalanıyor; modüller arası caller pointer'ı saklanmıyor.
- Severity sayımları ve strict output gate eklendi: NOTE tek başına final çıktıyı engellemez; WARNING/ERROR/FATAL engeller.
- Allocation failure veya collector yokluğu fail-closed davranışıyla final output'u engelliyor.
- Embedded NUL, geçersiz severity/code ve hatalı source span reddediliyor.
- `test_diagnostics.c` CTest'e bağlandı. Windows, Ubuntu ve macOS build/CTest **17/17** başarılı; Ubuntu ASan/UBSan ve Valgrind de **17/17** başarılı.

Bu collector henüz ParserContext/SemanticContext'e bağlanmadı ve OIR DIAGNOSTICS section codec'i yok. İlk sürüm aynı collector'a eşzamanlı çoklu thread yazımını desteklemez; aynı job içindeki paralel raporlama için orchestrator serialization veya mutex wrapper gerekir.


## Per-job CompilationContext — 31 Ekim 2026

- `omnicore-sdk/src/c/core/compilation.h/.c`: her derleme işi için bağımsız context lifecycle eklendi.
- Context job ID, copied target name, optimization level, option flags, stop-after stage ID ve kendi diagnostic collector'ını tutuyor.
- State geçişleri CREATED → RUNNING → COMPLETED / FAILED / CANCELLED olarak kontrol ediliyor.
- Source diagnostic birikmesi RUNNING durumunu FAILED yapmıyor; pipeline aşamaları tamamlanabiliyor.
- Final output yalnızca COMPLETED state ve strict diagnostic gate birlikte izin verirse yayımlanabilir.
- Failed/cancelled job final output üretemez.
- `test_compilation.c` CTest'e bağlandı; test yeni job state ve output gate davranışlarını doğruluyor.

Bu ilk context iskeletidir. Source manager, artifact registry, module invocation view, cancellation propagation ve checkpoint metadata henüz eklenmedi. Ortak DLL ABI'si olarak freeze edilmedi.

Doğrulama: GitHub Actions run 41145426190 ile Windows, Ubuntu ve macOS build/CTest **18/18** başarılı; Ubuntu ASan/UBSan ve Valgrind **18/18** başarılı.


## Ortak diagnostics entegrasyonu ve OIR DIAGNOSTICS codec — 31 Ekim 2026

- `OccDiagnosticStage` ile lexer, parser ve semantic için sabit stage ID değerleri tanımlandı.
- `ParserContext` ve `SemanticContext` artık isteğe bağlı job-owned `OccDiagnosticCollector` bağlantısı kabul ediyor. Mevcut linked `Error` listeleri korunurken tanılar ortak collector'a da kopyalanıyor.
- Parser/lexer ve semantic hata aktarımı için CTest kontrolleri eklendi. Bu ilk adaptör line/column bilgisini taşır; byte offset/end span henüz AST/diagnostic çağrılarına kadar yayılmıyor.
- ASan derlemesinde statik diagnostics kütüphanesinin shared codegen modülüne bağlanması için `POSITION_INDEPENDENT_CODE` etkinleştirildi.
- `omnicore-sdk/src/c/oir/oir_diagnostics.h/.c`: OIR DIAGNOSTICS schema 1.0 için 16-byte header, 68-byte kayıtlar, UTF-8 string-table referansları, little-endian writer ve validated zero-copy reader eklendi.
- Geçersiz record size/severity/message offset, truncated payload ve küçük output buffer testleri eklendi. Yeni test toplam CTest sayısını 19'a çıkardı.
- `OIR_PAYLOAD_SCHEMAS.md` DIAGNOSTICS schema 1.0 alan tablosu ve validation kurallarıyla güncellendi.

**Doğrulama:** GitHub Actions run 41147629609 tamamlandı. Windows, Ubuntu ve macOS build/CTest **19/19** başarılı; Ubuntu ASan/UBSan ve Valgrind işleri de başarılı. Çalışma dalı `refactor/c-core-roadmap`; merge/PR yapılmadı.

**Sıradaki adım:** TOKEN_STREAM schema 1.0 codec'i; ardından lexer-to-OIR ve OIR-to-parser adapter'ları. DLL/SO modül ayrımı, ortak ABI ve gerçek pipeline runner bundan sonra gelecek.


## OIR TOKEN_STREAM codec ve lexer diagnostic association — 31 Ekim 2026

- Token yapısına diagnostic_id alanı eklendi. ParserContext ortak diagnostic collector'a bağlıyken lexer error token'ı raporlandığında 1-based collector kayıt indeksi token üzerinde saklanıyor.
- oir_token_stream.h/.c: TOKEN_STREAM schema 1.0 için bounded writer, validating zero-copy reader ve record getter eklendi.
- Codec; registry'de olmayan token ID'lerini, sıfır/bozuk line-column değerlerini, source ID uyuşmazlığını, source dışına taşan veya çakışan span'leri, terminal EOF eksikliğini ve diagnostic kaydı olmayan TOKEN_ERROR girdilerini reddediyor.
- Frontend Token → OIR record adapter'ı; normal token lexeme span'lerini ve hata token'ının mesaj pointer'ından ayrı kaynak span/diagnostic ID alanlarını dönüştürüyor.
- Parser testine geçersiz UTF-8 byte fixture'ı eklendi; lexer kaynaklı tanının ortak collector'da OCC_STAGE_LEXER ve source location ile bulunduğu doğrulanıyor.
- test_oir_token_stream.c round-trip, source identity, truncation, small buffer, reserved bytes, unknown token ID, overlapping spans, terminal EOF ve diagnostic ID kontrollerini kapsıyor.
- GitHub Actions run 41148385330: Windows, Ubuntu ve macOS build/CTest 20/20 başarılı; Ubuntu ASan/UBSan ve Valgrind işleri de başarılı.

**Sınır:** Bu codec ve Token → OIR dönüşümü mevcut; OIR token kayıtlarını yeniden Token dizisine çevirip parser'a besleyen ikinci yön henüz yok. Parser hâlâ kendi lexer girişini kullanıyor. Sıradaki iş OIR-to-parser handoff adapter'ı.


## OIR TOKEN_STREAM → parser handoff — 31 Ekim 2026

- ParserContext artık borrowed pre-tokenized Token dizisi kabul ediyor. parser_run(), bu modda lexer_next_token() çağırmadan token dizisini tüketiyor; lookahead fonksiyonları token-stream index'ini geri alarak mevcut parser davranışını koruyor.
- Parser API son token'ın EOF olmasını ve daha önce EOF bulunmamasını doğruluyor. Token dizisinin sahipliği çağıranda; parser_run() bitene kadar Token dizisi ve bağlı source/diagnostic metinleri canlı kalmalı.
- OIR adapter, validated record + SOURCE_UNIT bytes + optional DIAGNOSTICS message görünümünden frontend Token oluşturuyor. Normal tokenlarda Token.start kaynak dilimine; TOKEN_ERROR için diagnostic message'a işaret ediyor. Kaynak span ve diagnostic_id ayrı alanlarda korunuyor.
- OIR'den gelen diagnostic_id mevcut job collector'da zaten kayıtlıysa parser aynı lexer hatasını ikinci kez eklemiyor.
- test_parser_oir_handoff.c gerçek test1.nxf dosyasını lexer → TOKEN_STREAM encode → TOKEN_STREAM validate/decode → frontend Token array → parser_run zincirinden geçiriyor.
- GitHub Actions run 41148803455: Windows, Ubuntu ve macOS build/CTest **21/21** başarılı; Ubuntu ASan/UBSan ve Valgrind işleri de başarılı.

**Sınır:** Bu, testlerle doğrulanmış in-memory stage handoff'tur. OCC'nin ana compile komutu henüz OIR dosyaları üzerinden aşamaları yürütmüyor; DLL/SO/dylib modül ABI'si ve orchestrator hâlâ sonraki işlerdir. Bir sonraki OIR tasarım işi AST payload schema 1.0'dır.


## OIR AST payload schema 1.0 — tasarım taslağı, 31 Ekim 2026

- Mevcut C AST envanteri 80 node kind olarak çıkarıldı; OIR için C enum ordinal'lerinden bağımsız 1–80 stable ID registry taslağı OIR_AST_NODE_REGISTRY.md dosyasına yazıldı.
- Her node kind için field ID ve wire value type eşlemesi tanımlandı. Shared C struct kullanan farklı node türleri (ör. AST_FUNC_DECL / AST_EXTERN_DECL) OIR'de ayrı node-kind ID olarak kalıyor.
- OIR_PAYLOAD_SCHEMAS.md içine 32-byte payload header, 32-byte node record ve typed TLV field düzeni eklendi. Strings ayrı UTF-8 string table referansları; child pointers stable node IDs; operator/literal token kinds OIR_TOKEN_REGISTRY IDs.
- Null ve empty string ayrımı, optional TLV compatibility, little-endian fixed-width scalars, exact length checks, deterministic node numbering, acyclic single-owner tree invariants ve 1,000,000 node / 512 depth / 16 MiB string limitleri tanımlandı.
- Mevcut ast_serialize()/ast_deserialize() native C stream'inin OIR AST formatı olmadığı ve otomatik yeniden yorumlanmaması gerektiği açıkça belirtildi.

**Durum:** AST payload tasarım sözleşmesi ve stable node/field registry taslak olarak hazır. AST codec, parser checkpoint migration ve AST source byte-span propagation henüz uygulanmadı.


## AST → OIR stable node ID adapter — 31 Ekim 2026

- `omnicore-sdk/src/c/oir/oir_ast_map.h/.c` eklendi. Frontend `ASTNodeType` ile OIR registry'deki 1–80 arası sabit node-kind ID'leri açık tablo üzerinden eşleniyor; C enum ordinal'i wire ID olarak kullanılmıyor.
- `oir_ast_node_id_from_frontend()` ve `oir_ast_node_id_to_frontend()` API'leri eklendi. Geçersiz tag/ID ve null çıktı pointer'ı reddediliyor.
- `test_oir_ast_map.c` tüm 80 ID'nin iki yönlü birebir eşleşmesini, ID aralığını ve geçersiz değerleri kontrol ediyor.
- CMake'e bağımsız `occ_oir_ast_map` static utility hedefi ve `oir_ast_node_registry_mapping_roundtrip` CTest kaydı eklendi.
- **Durum:** Kaynak ve testler branch'e işlendi. GitHub Actions build/test, ASan/UBSan ve Valgrind koşuları başlatıldı; henüz sonuç alınmadı. Bu nedenle adapter şu aşamada kodlandı, CI ile doğrulanması bekleniyor.
- **Kapsam sınırı:** Bu yalnızca node-kind kimlik katmanıdır; AST field TLV writer/reader, graph validation, round-trip payload codec ve parser checkpoint migration henüz uygulanmadı.


## OIR AST codec implementation contract — 31 Ekim 2026

AST payload schema 1.0 ve frontend AST yapıları birlikte incelendi. Codec uygulamasından önce bellek sahipliği ve graph kuralları ayrı bir sözleşmede toplandı: [OIR_AST_CODEC_IMPLEMENTATION.md](OIR_AST_CODEC_IMPLEMENTATION.md).

Denetimde netleşen noktalar:
- `ASTNode.next` kardeş zinciri bağlantısıdır; node-specific TLV alanı değildir.
- Mevcut AST cleanup kodu child ve sibling bağlantılarını sahip olunan ağaç olarak temizler. Bu nedenle OIR reader, pointer bağlantılarını kurmadan önce cycle, duplicate-owner, orphan ve depth kontrollerini tamamlamalıdır.
- Reader önce tüm payload'ı doğrulamalı, sonra node/string allocation yapmalı, en son no-fail linking pass ile pointer'ları bağlamalıdır. Kısmen bağlanmış graf üzerinde mevcut `ast_destroy()` çağrılmamalıdır.
- Token/operator alanları stable OIR token map üzerinden çevrilmeli; C enum değerleri doğrudan yazılmamalıdır.
- Shared C struct kullanan farklı AST türleri OIR'de ayrı node kind olarak kalır.

**CI doğrulaması:** AST node map commit'i 41155747645; TASKS güncellemesi 41155765694; PROJECT_STATUS güncellemesi 41155769032 çalışmaları başarılı tamamlandı. Sonraki adım hâlâ bounded AST payload writer/reader ve graph/reference validation kodudur. Codec tamamlanmadan parser checkpoint migration veya stage DLL ABI freeze yapılmayacaktır.


## OIR AST TLV + node table foundations — 31 Ekim 2026

- `oir_ast_tlv.h/.c`: bounded TLV writer/reader; little-endian header, canonical field order, fixed-width type checks, canonical BOOL, token ID range and malformed-input tests.
- `oir_ast_node_table.h/.c`: 32-byte AST payload header and 32-byte node record writer/reader. Record IDs, root kind, node-kind range, flags/reserved fields, source coordinates, sibling IDs and packed field-data slices are checked.
- Node field TLV streams are validated in place. NODE_REF values are range-checked; STRING_REF offsets/lengths are bounded by the string table and UTF-8/NUL rules.
- CMake targets: `occ_oir_ast_tlv`, `occ_oir_ast_node_table`; CTest names: `oir_ast_tlv_primitives_roundtrip_and_validation`, `oir_ast_node_table_roundtrip_and_validation`.
- Node table writer uses caller-owned buffers and validates before emission; no hidden allocation. Reader exposes zero-copy views.
- **Not yet implemented:** graph ownership/cycle/orphan/depth validation, per-node required/optional field schema enforcement, frontend AST serialization/deserialization, parser checkpoint migration and stage DLL ABI freeze.
- TLV primitive CI runs 41156342338, 41156339751, 41156324270 and 41156312919 completed successfully. Node-table CI is pending.


## OIR AST graph validation — 31 Ekim 2026

- `oir_ast_graph.h/.c` eklendi. Doğrulanmış node-table view üzerinde NODE_REF ve next_node_id kenarlarının incoming-owner sayısını çıkarıyor.
- Root ID 1 için sıfır incoming edge, her diğer node için tam bir incoming edge şartı uygulanıyor.
- Caller-provided incoming-count, visited ve DFS stack dizileri kullanılıyor; validator içinde bellek allocation yok.
- Root'tan erişilebilirlik, tekrar ziyaret/cycle reddi ve 512 maksimum derinlik denetleniyor.
- `test_oir_ast_graph.c`: geçerli child+sibling ağacı, duplicate-owner/cyclic sibling, erişilemeyen self-cycle/orphan ve 513 seviyeli child zinciri senaryolarını kapsıyor.
- CMake hedefi `occ_oir_ast_graph`; CTest `oir_ast_graph_single_owner_reachability_and_depth`.
- **Kalan AST codec işleri:** node-kind başına required field/type schema validation, bilinmeyen optional field politikası, frontend AST writer/reader ve gerçek AST round-trip fixture'ları. DLL ABI ve parser checkpoint migration beklemede.
- **CI doğrulaması:** CMake entegrasyonu run 41157040260; Windows, Ubuntu, macOS, ASan/UBSan ve Valgrind işleri başarılı tamamlandı.


## OIR AST field schema validator — 31 Ekim 2026

- `oir_ast_field_schema.h/.c` eklendi. Her node kind için registry v1 field count ve wire type kontrolü yapılıyor.
- 80 node kind'in tümü için test payload'ları üretildi; required field eksikliği, bilinen field için yanlış wire type, unknown required field reddi ve unknown optional field kabulü kontrol ediliyor.
- Nullable string alanlarında NULL kabul edilir; diğer wire type'lar exact registry türüne uymalıdır.
- CMake hedefi `occ_oir_ast_field_schema`; CTest `oir_ast_field_schema_all_node_kinds_and_compatibility`.
- **CI doğrulaması:** Run 41157314066; Windows, Ubuntu, macOS, ASan/UBSan ve Valgrind işleri başarılı tamamlandı. Ubuntu CTest 26/26 geçti.
- **Sıradaki iş:** Frontend AST → OIR bounded writer; önce iki geçişli graph index ve deterministic DFS node ID ataması, sonra TLV/string emission. Reader materialization bundan sonra ele alınacak.


## OIR AST field schema CI confirmation — 31 Ekim 2026

- 80 node kind için field schema validator testleri düzeltildi ve aktif assertions ile çalıştırıldı.
- Test, her node kind için registry'ye uygun alan dizisi üretip doğruluyor; eksik zorunlu alan, bilinen alanda yanlış wire type, unknown required field ve unknown optional field davranışını sınanıyor.
- CI run 41157314066: Windows, Ubuntu ve macOS build/test başarılı; Ubuntu ASan/UBSan ve Valgrind başarılı. Ubuntu CTest 26/26.
- Sonraki uygulama işi: frontend AST → OIR bounded writer; caller-owned scratch ve deterministik DFS node ID atamasıyla.
