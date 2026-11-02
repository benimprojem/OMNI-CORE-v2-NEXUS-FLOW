# OmniCore SDK — Mimari Kararlar

Bu belge uygulama kararlarını ve henüz karara bağlanmamış tasarım konularını birbirinden ayırır.

## ADR-001 — C ana uygulama dili

- **Durum:** Kabul edildi
- **Karar:** OmniCore Compiler'ın ana kaynak uygulaması C olacaktır.
- **Gerekçe:** Daha yalın ABI, düşük seviye kontrol, küçük çalışma zamanı bağımlılığı ve sistem yazılımı hedefleriyle uyum.
- **Kapsam:** Lexer, parser, AST, semantik analiz, OIR altyapısı, optimizer ve codegen ana hatları.
- **C++ rolü:** Silinmeyecek. Parser, OIR writer/reader, analyzer ve IDE bileşenleri referans olarak tutulacak. Yararlı algoritmalar ancak davranışları testlerle doğrulandıktan sonra C tarafına aktarılacak.
- **Geçiş:** Kaynaklar ilk aşamada yerinde korunur. Build/test düzeni doğrulanmadan toplu taşıma yapılmaz.

## ADR-002 — Tek ve açık ara temsil sözleşmesi

- **Durum:** OIR v1 container yönü önerildi; binary layout ve payload semantiği henüz freeze edilmedi.
- C tarafındaki parser_save_ir() AST serileştirmesi ile C++ tarafındaki OIR2 ikili biçimi aynı format değildir. Hiçbiri doğrudan kanonik format ilan edilmeyecektir.
- Tüm compiler-stage handoff tek bir sürümlenmiş OIR container üzerinden yapılacaktır. Container, payload kind/schema version alanlarıyla SOURCE_UNIT, TOKEN_STREAM, AST, SEMANTIC_AST, OIR_MODULE, TARGET_IR, ASSEMBLY ve OBJECT gibi farklı aşama çıktılarını taşıyabilir.
- OIR container aynı zamanda in-memory opaque artifact handle sözleşmesinin kalıcı/checkpoint byte representation'ıdır.
- Container alanları fixed-width, little-endian, length-delimited ve pointer/host-struct bağımsız olacaktır.
- Malformed, truncated, uyumsuz sürümlü, aşırı büyük veya tutarsız artifact güvenli biçimde reddedilmelidir.
- Canonical OIR_MODULE semantiği (type system, opcodes, memory/ownership model, SSA policy) ayrı tasarlanacaktır; hedef mimariye özel register/assembly ayrıntıları canonical OIR içine gömülmeyecektir.
- Ayrıntılar: [OIR_FORMAT.md](OIR_FORMAT.md).

## ADR-003 — Taşınabilir frontend ve çoklu platform hedefleri

- **Durum:** Kabul edildi
- OmniCore, Windows, Linux ve macOS üzerinde çalışacak şekilde tasarlanacaktır.
- Derleyici frontend'i işletim sistemi ve CPU mimarisinden bağımsız NexusFlow semantiği üretir.
- Host platform (derleyicinin çalıştığı sistem) ile target platform (üretilen programın çalışacağı sistem) birbirinden ayrıdır.
- Kullanıcı target seçmezse host OS + host architecture varsayılan hedef olur. Açık target seçimi cross-compilation dahil host varsayımını geçersiz kılar.
- Belirli bir özel işlemci modeli seçimi zorunlu değildir; genel CPU mimarisi ve isteğe bağlı CPU feature seçimi desteklenmelidir.
- Özel işlemci hedefleri ileride aynı target/backend modeline eklenebilir; NEXSUS işlemcisinin varlığı veya kesinleşmesi beklenmez.

## ADR-004 — Target dosyası tek başına backend desteği anlamına gelmez

- **Durum:** Kabul edildi
- Bir .target dosyasının bulunması veya okunması, ilgili mimari için geçerli makine kodu üretildiğini kanıtlamaz.
- Her target için komut seçimi, ABI, register kullanımı, çağrı kuralları, object format ve testler ayrı doğrulanmalıdır.
- Backend, desteklemediği özelliklerde sessizce yanlış assembly üretmek yerine açık hata vermelidir.

## ADR-005 — Build çıktıları kaynak koddan ayrılacak

- **Durum:** Kabul edildi
- bin/, lib/ ve ara dosyalar derleme çıktısıdır; ana kaynak dosyaları değildir.
- Mevcut binary dosyalar, hangi kaynak ve build komutuyla üretildikleri doğrulanmadan silinmeyecektir.
- Tekrarlanabilir build sağlandıktan sonra çıktılar için temizleme ve Git ignore politikası uygulanacaktır.

## Açık kararlar

1. OIR v1 binary layout'unun freeze edilmesi ve canonical OIR_MODULE semantiğinin tasarlanması.
2. AST düğüm türleri ve tip/ownership bilgisinin hangi aşamada ekleneceği.
3. NexusFlow tip yükseltme ve dönüşüm kurallarının kesin tablosu.
4. Handler/masa yaşam süresi ve move/copy/borrow benzeri kuralların kesin semantiği.
5. Hata kodu, warning ve diagnostic formatının ortak sözleşmesi.
6. ARM64 ve x86-64 backend kapsamı; assembler/linker bağımlılıkları.
7. NEXSUS ISA kararı verildiğinde özel backend'in nasıl ekleneceği.


## ADR-006 — Target, OS, architecture ve CPU feature ayrımı

- **Durum:** Kabul edildi
- Target tanımı en az OS, architecture, ABI, object format ve toolchain alanlarını kapsar.
- CPU model ve CPU features isteğe bağlı ek seçimlerdir; genel mimari hedefi tek başına geçerli bir target olabilir.
- Kullanıcı bir genel mimari seçip ilave CPU yeteneklerini seçebilir. Seçilen her feature, backend capability tablosunda doğrulanır.
- Host varsayılanı yalnızca açık target seçimi olmadığında kullanılır.
- Windows, Linux ve macOS aynı CPU mimarisini paylaşsa bile ABI, object format, sistem çağrıları ve linker farkları nedeniyle ayrı platform target profilleridir.
- Target descriptor bir yapılandırma/veri sözleşmesidir; gerçek backend desteği test edilmeden ilan edilemez.
- Desteklenmeyen target/feature kombinasyonları sessizce en yakın hedefe düşürülmez; anlaşılır hata ile reddedilir.


## ADR-007 — Aşama başına yüklenebilir derleyici modülleri ve boru hattı

- **Durum:** Kabul edildi; uygulama aşamalı olarak yapılacak.
- **Karar:** Her derleme aşaması kendi bağımsız, platforma uygun shared library modülü olacaktır: Windows DLL, Linux SO, macOS dylib. OCC executable bu modülleri yükleyen, aşamaları yöneten ve ara çıktıları aktaran orkestratördür.
- **Aşama sınırı:** Lexer, parser, semantic analyzer, IR lowering/validation, optimizer pass'leri, target/capability doğrulama, backend/codegen, assembler ve linker/packager birbirinden ayrılacaktır.
- **ABI:** Modüller sürümlenmiş C ABI ile haberleşir. Modül kimliği, ABI sürümü, girdi/çıktı artifact türleri, yetenekler, işlem girişi, sonuç, diagnostic aktarımı ve bellek sahipliği sözleşmesi tanımlanacaktır.
- **Durum izolasyonu:** Her derleme işi bağımsız context ve diagnostic koleksiyonu taşır. Modül-global mutable derleme durumu kullanılmaz; paralel kullanım ancak modül reentrant/thread-safe olarak ilan edilmişse yapılır.
- **Devam politikası:** Kaynak hataları ve uyarıları mümkün olduğunca toplanır; güvenli olduğu sürece sonraki tanı aşamaları çalışır. Kısmi artifact, geçerlilik durumu belirtilerek üretilebilir. Bozuk artifact güvenliymiş gibi sonraki aşamaya verilemez.
- **Çıktı kapısı:** Varsayılan katı politikada hata veya uyarı varsa final çıktı yayımlanmaz. Ara çıktılar ve toplu diagnostic raporu üretilebilir. Uyarıları kabul ederek final çıktı üretmek ileride açık kullanıcı seçeneği olarak tasarlanabilir.
- **Checkpoint:** Kullanıcı herhangi bir tamamlanmış aşamadan sonra durdurup o aşamanın artifact'ini alabilir. Checkpoint final program çıktısı değildir; format, sürüm, diagnostic ve geçerlilik bilgisini taşır.
- **Çoklu derleme:** Modüller tekrar kullanılabilir; derleme işleri bağımsız context'lerle paralel/kuyruklu boru hattında çalışabilir. Back-pressure ve iş bazında iptal desteklenecektir.
- **Gerekçe:** Hata ayıklamayı, modül testini, aşamalar arası değişiklikleri, paralel derlemeyi ve IDE entegrasyonunu kolaylaştırmak.
- **Detay:** Modül ABI'si, artifact sözleşmesi, diagnostic modeli ve geçiş sırası [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md) içinde tanımlanmıştır.


## ADR-008 — OIR ortak artifact taşıma standardı

- **Durum:** Tasarım yönü kabul edildi; v1 binary layout henüz freeze edilmedi.
- **Karar:** Her derleme modülü, bir OIR artifact alır ve tanımlı artifact kind/schema version ile yeni bir OIR artifact üretir. Artifact, modüller arasında bellek içinde opaque handle olarak aktarılabilir; diske yazıldığında aynı OIR container serileştirmesi kullanılır.
- **Gerekçe:** Bir aşamanın çıktısının doğrudan sonraki aşamanın girdisi olması; modüllerin birbirlerinin özel C struct'larına, allocator'larına veya pointer'larına bağımlı kalmaması.
- **Container/payload ayrımı:** OIR container ortak başlık, metadata ve length-delimited section yapısını sağlar. Token stream, AST, semantic AST, canonical OIR, assembly ve object gibi payload'lar kendi tip ve schema sürümleriyle tanımlanır.
- **Geçerlilik:** Artifact envelope, payload kind/version ve validity durumu downstream çağrısından önce doğrulanır. PARTIAL yalnızca bunu kabul ettiğini bildiren modüle verilir.
- **Eski formatlar:** Mevcut C native-layout AST stream ve eski C++ OIR2 birbirinden farklı legacy biçimlerdir; OIR v1 olarak yorumlanamazlar.
- **Detay:** [OIR_FORMAT.md](OIR_FORMAT.md).


## ADR-009 — Independent OIR Verifier and optional optimization profiles

- **Durum:** Kabul edildi; ilk Verifier stage uygulandı, optimizer CLI/profile entegrasyonu planlandı.
- **Verifier:** OIR Lowering sonrasında ve Codegen öncesinde bağımsız doğrulama zorunludur. Optimizer çalışırsa ürettiği OIR, Codegen'e geçmeden önce aynı bağımsız Verifier tarafından yeniden doğrulanır.
- **ABI:** Verifier ayrı bir shared stage modülüdür ancak özel bir public ABI oluşturmaz. Ortak `OccStageModuleV1` sözleşmesini ve OCC diagnostic sink'ini kullanır.
- **Değişmezlik:** Verifier OIR'ı onarmaz veya değiştirmez; doğrulanmış artifact'i verifier producer kimliğiyle yeniden yayımlar.
- **Optimizer varsayılanı:** OCC komut satırında optimizasyon seviyesi belirtilmezse Optimizer aşaması atlanır. `--o0` zorunlu veya gerekli değildir.
- **Optimizer profilleri:** `--o1` temel, `--o2` standart, `--o3` ileri, `--o4` agresif optimizasyon profilidir. Tek derlemede yalnızca bir seviye seçilebilir.
- **Güvenlik kapısı:** Geçersiz OIR Optimizer veya Codegen'e verilemez. Her optimizer çıktısı yeniden doğrulanır.
- **Detay:** [OIR_VERIFIER_SPEC.md](OIR_VERIFIER_SPEC.md) ve [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md).
