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
