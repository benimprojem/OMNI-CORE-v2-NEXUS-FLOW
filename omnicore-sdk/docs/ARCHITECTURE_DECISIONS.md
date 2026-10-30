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

- **Durum:** Tasarım kararı gerekli
- C tarafındaki parser_save_ir() AST serileştirmesi ile C++ tarafındaki OIR2 ikili biçimi aynı format değildir.
- Bunlardan birini sessizce diğerinin yerine kullanmak yasaktır.
- Kanonik OIR; sürüm, byte order, alan genişlikleri, offset sınırları, string encoding, opcode/payload modeli, source map, target metadata ve hata davranışını tanımlamalıdır.
- OIR okuyucu/yazıcısı malformed, truncated, uyumsuz sürümlü ve taşma oluşturan girdileri güvenli biçimde reddetmelidir.
- İlk sürümde hedef mimariye özel register ve assembly ayrıntıları OIR içine gömülmemelidir.

## ADR-003 — Dil semantiği ISA'dan bağımsız kalacak

- **Durum:** Kabul edildi
- NexusFlow dil kuralları, henüz kesinleşmemiş NEXSUS CPU talimat setine bağlanmayacaktır.
- Handler/masa sahipliği, akış, hata yönlendirme, tip sistemi, scope ve kontrol akışı önce dil düzeyinde tanımlanacaktır.
- ISA kesinleştiğinde backend eşlemesi yapılacaktır; frontend'in yeniden yazılması gerekmemelidir.

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

1. Kanonik OIR'ın ilk sürümü ve ABI'si.
2. AST düğüm türleri ve tip/ownership bilgisinin hangi aşamada ekleneceği.
3. NexusFlow tip yükseltme ve dönüşüm kurallarının kesin tablosu.
4. Handler/masa yaşam süresi ve move/copy/borrow benzeri kuralların kesin semantiği.
5. Hata kodu, warning ve diagnostic formatının ortak sözleşmesi.
6. ARM64 ve x86-64 backend kapsamı; assembler/linker bağımlılıkları.
7. NEXSUS ISA kararı verildiğinde özel backend'in nasıl ekleneceği.
