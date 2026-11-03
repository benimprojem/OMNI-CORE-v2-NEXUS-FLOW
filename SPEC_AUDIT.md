# Nexus Flow Spec İnceleme Raporu

**Durum:** İlk inceleme / karar bekleyen noktalar  
**Tarih:** 2026-10-30  
**Kapsam:** `nexus docs/` ve `omnicore-sdk/docs/` altındaki dil, operatör, ownership, kural motoru ve OIR belgeleri  
**Not:** Bu belge bir denetim raporudur. Aşağıdaki belirsizlikler, dil tasarımı kararı verilmeden kesin davranış olarak kabul edilmemelidir.

## 1. Genel sonuç

Spec belgeleri önemli tasarım fikirlerini içeriyor; ancak bazı kavramlar yalnızca isim veya kısa açıklama düzeyinde kalmış. Bazı operatörlerin anlamları birden fazla yerde farklı yorumlanabiliyor. Aynı konuyu anlatan paralel belgeler de bulunuyor. Bu nedenle mevcut belgeler henüz tek ve çelişkisiz bir dil standardı olarak kullanılamaz.

Öncelik kodu değiştirmek değil; dilin normatif davranışını belirlemek, örnekleri bu davranışla uyumlu hâle getirmek ve sonra parser/AST/OIR uygulamasını bu standarda göre düzeltmektir.

## 2. Niyet sistemi — kritik eksik

### Mevcut durum

- `omnicore-sdk/docs/syntax.md` Bölüm 2.1 başlığı `Identifiers (Niyet Belirleyiciler)` olarak geçiyor.
- Bu bölümde `v:`, `c:`, `m:`, `nt:` gibi tanımlama önekleri listeleniyor. Bunlar niyet bildiriminden ziyade değişken, sabit, zorunlu bağımlılık ve yeni tip gibi bildirim kategorileri.
- Aynı dosyanın operatör tablosunda `@` yalnızca `Intent` olarak adlandırılıyor ve “derleyiciye niyet bildirimi” deniyor.
- `nexus docs/NEXUS FLOW OPERATÖRLERİ (THE 15 KEYS) — Formal Specification.md` de `@` için yalnızca “derleyiciye veya runtime’a işlem niyetini bildirir (optimization hints)” açıklamasını veriyor.
- İncelenen ana syntax belgesinde `@` için somut bir kullanım örneği veya tanımlı niyet kategorisi bulunamadı.

### Eksik tanımlar

1. Niyetin amacı: optimizasyon ipucu mu, programcı garantisi/varsayımı mı, yürütme tercihi mi, yoksa bunların ayrılmış alt türleri mi?
2. Sözdizimi: `@` hangi dil öğelerinin önünde/arkasında kullanılabilir? Parametre veya blok alır mı?
3. Kapsam: tek ifade, blok, fonksiyon, modül veya proje düzeyinde mi geçerlidir?
4. Zaman: yalnızca derleme anında mı değerlendirilir, runtime'a metadata taşır mı?
5. Güvenlik: derleyici niyeti yerine getiremiyorsa uyarı mı verir, hata mı verir, yoksa bildirimi yok sayabilir mi?
6. Çakışma: birbiriyle çelişen birden fazla niyet bildiriminin önceliği nedir?
7. Hedef bağımlılığı: CPU/OS/backend desteklemiyorsa davranış nedir?
8. Semantik sınır: niyet bildirimi programın doğruluğunu değiştirebilir mi, yoksa yalnızca performans/yerleşim gibi doğruluğu etkilemeyen tercihleri mi ifade eder?

**Güncel karar (2026-10-30):** Yalnızca kavramsal ayrım kaydedilmiştir: `!!=` açık ve daha katı derleyici talimatları için, `@` ise gelecekteki AI katmanına programcının niyetini/isteğini iletmek için ayrılmıştır. AI bileşeni henüz bulunmadığından Intent'in grameri, veri modeli, kapsamı, kategorileri ve AST/OIR temsili bilinçli olarak ertelenmiştir. Bu konu AI katmanının tasarım zamanı geldiğinde uçtan uca yeniden ele alınacaktır. Geçici not: [Intent System — Tasarım Notu](INTENT_SPEC.md). `syntax.md` Bölüm 2.1 başlığı “Tanımlama Önekleri” olarak düzeltilmiş, `@` açıklaması güncellenmiştir.

## 3. Operatör semantiği — kritik tutarsızlıklar

### 3.1 `->` birden fazla anlama geliyor

Operatör spec'inde `->` başarı akışı ve veriyi sonraki işleme taşıma olarak anlatılıyor. Handler bölümünde ise `(h) -> write()` için kopyalama değil sahiplik transferi ve kaynak masanın null edilmesi iddia ediliyor. Multi-Rule Engine belgesinde aynı `->` koşullu trigger tanımlamak için kullanılıyor.

Bunlar aynı operatörün bağlama göre ayrışan anlamları olabilir; fakat gramer, AST düğüm türü ve semantik kuralları tanımlanmamış. Üç kullanımın aynı token ile nasıl ayrıştırılacağı ve hangi bağlamda geçerli olduğu açıklanmalı.

### 3.2 `<<` ve `>>` aşırı genellenmiş

Operatör listesi bunları atomik Zone Write ve Flow Feed olarak tanımlıyor. Genel syntax referansında ise `<<` ve `>>` bit kaydırma operatörleri olarak da listeleniyor. Token çakışması, operand türleri ve bağlama göre ayrıştırma kuralları belirtilmeli. Ayrıca her zone/flow yazma veya besleme işleminin otomatik olarak atomik olduğu varsayılmamalı; atomiklik, bellek modeli ve desteklenen tiplerle tanımlanmalı.

### 3.3 Hata akışı ve sonuç değerleri

`?->` açıklaması başarısızlığı `false`, `null`, `none` veya `unknown` durumlarıyla eş tutuyor. Bunların aynı hata kategorisi olup olmadığı, normal boolean false değerinin neden hata sayılacağı, hata değerinin nasıl taşındığı ve `?=>` fallback ile hangi sırada etkileştiği tanımlı değil.

### 3.4 Sonlandırma yazımı

Belgelerde hem `?;` hem `}?;` kullanılıyor. İlki operatör listesinde Halt/sonlandırma; ikincisi blok sonu cleanup olarak anlatılmış. Bunlar iki ayrı gramer öğesi mi, yoksa yazım tutarsızlığı mı, açıklığa kavuşturulmalı.

### 3.5 Öncelik tablosu

“15 Keys” tablosundaki sayılar operatör önceliği olarak sunuluyor. Ancak bunların leksik öncelik, parser precedence, değerlendirme sırası veya yürütme aşaması sırası olduğu tanımlanmamış. Handler'ın “her zaman ilk sırada” olması da ifade/operatör önceliğiyle aynı kavram değil.

## 4. Ownership ve Handler yaşam döngüsü

`ownership.md`, `syntax.md` ve operatör spec'i `(h)` için blok kapsamı, otomatik cleanup ve `_>` ile sahiplik transferi öngörüyor. Ancak şu temel noktalar normatif değil:

- Hangi değerler sahiplik taşır? Her değişken mi, yalnızca handle/pointer/resource türleri mi?
- Atama kopyalama mı, taşıma mı, ödünç alma mı yapar?
- Birden fazla immutable referansa izin var mı? Mutable erişim nasıl sınırlandırılır?
- Fonksiyon parametresine geçişte varsayılan davranış nedir?
- Dönüş değerinde sahiplik nasıl aktarılır?
- Hata, erken return, panic ve cleanup sırasında kaynakların durumu nedir?
- `<-` operatörünün capture, atama ve move davranışları arasındaki ayrım nedir?
- “Ownership her zaman son bloktadır” ifadesinin kesin anlamı nedir?

Özellikle `<-` operatörünün bazı örneklerde değişkeni boşalttığı, başka yerlerde yalnızca değer/adres yakaladığı görülüyor. Kopyalama ve taşıma ayrımı açıkça yazılmalı.

## 5. Multi-Rule Engine

`nexus docs/NEXUSFLOW Çoklu Kural İşleyici...` ile `omnicore-sdk/docs/Multi-Rule Engine Formal Spec.md` aynı konuya ait paralel sürümlerdir. İçerik büyük ölçüde benzer olsa da örneklerde `r:` öneki farklı kullanılıyor.

Belgedeki açık sorular:
- `:=>` barrier kuralının koşul doğru olduğunda mı, yanlış olduğunda mı engelleme yaptığı kesin değil.
- Barrier, trigger, always ve default kurallarının sırası kısmen anlatılmış; birden fazla trigger eşleştiğinde tümü mü çalışır, ilk eşleşen mi çalışır belirtilmemiş.
- “Sıralı ve bağımsız” kuralların bağımsızlık koşulu ve yan etkilerle ilişkisi tanımlı değil.
- `always` ve `default` birlikteyse kesin yürütme sırası açıklanmalı.
- Kural parametrelerinin tipi, kapsamı, yan etkisi ve hata davranışı belirtilmemiş.
- `->` token'ının genel akış operatörüyle aynı token olması gramer düzeyinde açıklanmalı.

## 6. OIR belgeleri

`omnicore-sdk/docs/ir v2.md` ile `nexus docs/OCC Intermediate Representation (OIR) Specification.md` farklı kapsam ve ayrıntı düzeyinde iki OIR anlatımı sunuyor. Mevcut C parser'ın AST serileştirmesi de kanonik OIR formatı değildir.

Özellikle:
- OIR v2 belgesinde 16 byte sabit opcode kaydı denirken alanların toplamı 16 byte olarak verilmiş olsa da format sürümleme, endian, alignment, string/section uzunlukları ve doğrulama kuralları tam değil.
- Symbol Table alanları ve opcode alanları tanımlanmış; ancak referansların geçerliliği, section sınırları, bozuk/truncated dosya davranışı ve uyumluluk politikası eksik.
- Target örneği Windows x64 ABI ve PE çıktısına özgü. Bu örnek genel target tanımı gibi algılanmamalı.
- OIR'nin frontend AST'sinden nasıl üretildiği ve analiz/optimizasyon aşamalarının hangi alanları değiştirebildiği açık bir sözleşmeye bağlanmalı.

**Karar:** C AST serileştirmesi, sürümlü ve platform bağımsız OIR v1 sözleşmesi tamamlanmadan kanonik format olarak adlandırılmamalı.

## 7. Belge çoğalması ve kaynak hiyerarşisi

Aynı veya benzer konular iki ayrı yerde bulunuyor:
- `nexus docs/`
- `omnicore-sdk/docs/`

İlk karşılaştırmada operatör, Multi-Rule Engine ve sistem/derleyici spec'lerinin paralel kopyaları bulundu. Bazı dosyalar birebir aynı görünse de küçük sözdizimi ve örnek farklılıkları var. Bunlar tek bir kanonik kaynak seçilmeden birleştirilmemeli.

Önerilen hiyerarşi:
1. Dilin normatif kuralları: kanonik Language Specification.
2. Konu bazlı normatif ekler: ownership, intent, error model, rule engine, memory model.
3. Compiler/IR/backend sözleşmeleri: dil semantiğinden ayrı teknik belgeler.
4. Eski taslaklar: tarihsel referans olarak arşiv.

## 8. Önerilen düzeltme sırası

1. **Niyet modeli (`@`)** — kavram ve kapsamı tanımla; tanımlama öneklerinden ayır.
2. **Operatör sözlüğü ve gramer** — token, precedence, associativity, bağlam ve AST anlamını tanımla.
3. **Ownership / Handler** — copy, move, borrow, lifetime, cleanup ve hata yollarını tanımla.
4. **Error / Result modeli** — false/null/none/unknown ayrımı, `?->`, `?=>`, `!!`, `!->` davranışı.
5. **Multi-Rule Engine** — barrier/trigger/always/default yürütme sırası ve yan etkiler.
6. **Memory model** — area/zone, atomiklik, senkronizasyon ve adres/context transferi.
7. **Kanonik OIR** — dil semantiği kararlı hâle geldikten sonra frontend/IR sözleşmesi.
8. **Örneklerin doğrulanması** — her normatif kural için pozitif/negatif örnek ve test.

## 9. Bu raporun sınırları

Bu ilk tur, depo içindeki temel belgelerin ve ilgili bölümlerin incelemesidir; henüz bütün spec dosyalarının satır satır doğrulaması değildir. Buradaki bulgular tasarım hatası olduğu kesinleşmiş maddeler değil, çözülmesi gereken çelişki/eksiklik kayıtlarıdır. Kod ve parser davranışı, dil kararı verilene kadar spec yerine kanıt olarak kullanılmamalıdır.
