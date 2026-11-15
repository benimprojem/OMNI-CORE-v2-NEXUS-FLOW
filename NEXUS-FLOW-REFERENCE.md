# NEXUS FLOW — Language Reference Manual

**Belge türü:** Normatif dil referansı — yalnızca sözdizimi ve kaynak dil kullanım kuralları  
**Dil:** NEXUS Flow  
**Durum:** İçerik indeksi / ilk taslak  
**Sürüm:** 0.1-draft  
**Dil standardı:** Tasarım ve uygulama kanıtlarıyla aşamalı olarak netleştirilecektir.

> Bu belge yalnızca Nexus Flow kaynak dilini tanımlar. OCC derleyici mimarisi, AST veri modeli, Semantic modülünün iç tasarımı, OIR, Verifier, Codegen, hedef işlemci mimarisi ve SDK/API belgeleri bu belgenin kapsamı dışındadır.
>
> Her dil özelliği; sözdizimi, biçimsel gramer, kullanım kuralları, kapsam/yaşam alanı, geçerli ve geçersiz örnekler, hata durumları ve varsa uyarı/öneriler ile açıklanacaktır. Bir özelliğin belgede tanımlanması, derleyicide tamamlandığı anlamına gelmez. Dil tasarımı ve uygulama olgunluğu ayrı ayrı gösterilir.
>
> Bu dosya yeni referansın başlangıç noktasıdır. Eski referans belgeleri silinmeyecek veya bu dosyaya otomatik olarak kopyalanmayacaktır; içerikleri bölüm bölüm gözden geçirilerek doğrulanacak ve yeniden yazılacaktır.

---

## 0. Belgenin kullanımı ve normatif dil

### 0.1 Amaç ve kapsam
### 0.2 Kapsam dışı konular
### 0.3 Normatif ifadeler
- MUST / ZORUNLUDUR
- MUST NOT / YASAKTIR
- SHOULD / ÖNERİLİR
- MAY / İSTEĞE BAĞLIDIR
### 0.4 Özellik olgunluk etiketleri
- **Tanımlı:** Dil kuralı kararlaştırılmış ve bu belgede açıklanmış.
- **Uygulama ile doğrulandı:** Sözdizimi ve örnekler mevcut derleyici testleriyle doğrulanmış.
- **Kısmen tanımlı:** Temel biçim belli, bazı kurallar veya sınırlar açık.
- **Tasarım aşamasında:** Karar henüz kesinleşmedi; geçici öneri normatif değildir.
- **Ertelendi:** Bilinçli olarak sonraki sürüme bırakıldı.
- **Desteklenmiyor:** Mevcut dil sürümünde geçerli değildir.
### 0.5 Örneklerde kullanılan gösterim ve adlandırma
### 0.6 Hata, uyarı ve öneri gösterimi
### 0.7 Sürümleme, geriye dönük uyumluluk ve değişiklik politikası

---

## Part I — Başlangıç

## 1. Nexus Flow'a giriş
### 1.1 Dilin genel yaklaşımı
### 1.2 En küçük geçerli program
### 1.3 Kaynak dosyası ve program yapısı
### 1.4 İlk değişken ve ifade
### 1.5 İlk fonksiyon
### 1.6 İlk koşul ve döngü
### 1.7 Örnek program: basitten biraz daha kapsamlıya

## 2. Kaynak dosyaları, karakterler ve biçim
### 2.1 Kaynak kodu karakter kümesi
### 2.2 Satır sonları ve dosya kodlaması
### 2.3 Boşluklar ve ayraçlar
### 2.4 Yorumlar
### 2.5 Satır ve blok sınırları
### 2.6 İfade sonlandırma ve noktalı virgül kuralları
### 2.7 Girintileme: anlamlı mı, biçimsel mi?
### 2.8 Sözdizimsel hata kurtarma sınırları

## 3. Tokenlar ve sözcüksel kurallar
### 3.1 Token sınıfları
### 3.2 Tanımlayıcılar
### 3.3 Anahtar sözcükler
### 3.4 Ayrılmış ve geleceğe ayrılmış sözcükler
### 3.5 Sayısal literal'lar
### 3.6 String literal'ları
### 3.7 Character literal'ları
### 3.8 Boolean ve özel literal'lar
### 3.9 Kaçış dizileri
### 3.10 Sayı ayraçları ve üs gösterimi
### 3.11 Geçersiz veya tamamlanmamış literal'lar
### 3.12 Token sınırlarının belirlenmesi

---

## Part II — Temel sözdizimi ve veri tanımları

## 4. Tanımlayıcılar, isimler ve isim alanları
### 4.1 İsimlendirme kuralları
### 4.2 Büyük/küçük harf duyarlılığı
### 4.3 İsim çakışmaları
### 4.4 Yerel ve üst kapsam isimleri
### 4.5 Alan, metot ve modül isimleri
### 4.6 Ayrılmış isimler ve geçersiz isimler

## 5. Bildirim biçimleri ve bağlama türleri
### 5.1 Bildirimlerin genel biçimi
### 5.2 `v:` — değiştirilebilir değer bildirimi
### 5.3 `c:` — derleme zamanı sabiti
### 5.4 `m:` — yeniden atanamayan normal değer
### 5.5 `nt:` — yeni tip / tip takma adı bildirimi
### 5.6 Ayrılmış bildirim önekleri
### 5.7 Başlatıcılı ve başlatıcısız bildirimler
### 5.8 Birden fazla bildirim ve bildirim sırası
### 5.9 Bildirimlerde tip çıkarımı
### 5.10 Geçersiz bildirimler ve tanı örnekleri

## 6. Tip sistemi — yazım ve temel kurallar
### 6.1 Tip adlarının yazımı
### 6.2 Açık tip belirtimi
### 6.3 `!` zorunlu tip belirtimi (hard type)
### 6.4 Varsayılan tip çıkarımı
### 6.5 Literal'dan tip çıkarımı
### 6.6 Tip uyumluluğu ve örtük dönüşüm sınırları
### 6.7 Tip takma adları
### 6.8 Fonksiyon imzalarında tip yer tutucusu `t`
### 6.9 Desteklenmeyen tip biçimleri

## 7. İlkel ve sayısal tipler
### 7.1 Boolean
### 7.2 İşaretli tamsayılar
### 7.3 İşaretsiz tamsayılar
### 7.4 Kayan noktalı tipler
### 7.5 Decimal tipleri
### 7.6 Geniş sayısal tipler
### 7.7 `bit` ve `byte`
### 7.8 `8bit`, `16bit`, `32bit`, `64bit`, `128bit`
### 7.9 Byte blok tipleri
### 7.10 `char` ve karakter temsili
### 7.11 Sayısal aralıklar ve literal sınırları
### 7.12 Henüz tanımlanmamış tip aileleri

## 8. Bileşik ve koleksiyon tipleri
### 8.1 Diziler
### 8.2 Sabit boyutlu diziler
### 8.3 Dinamik diziler
### 8.4 Çok boyutlu diziler
### 8.5 Dizi parametreleri
### 8.6 Tuple'lar
### 8.7 Map / dictionary
### 8.8 Vektör tipleri: Vec2, Vec3, Vec4
### 8.9 İç içe koleksiyonlar
### 8.10 Koleksiyon literal'ları ve tip çıkarımı
### 8.11 İndeksleme ve eleman erişimi
### 8.12 Geçersiz boyutlar, indeksler ve tip uyuşmazlıkları

## 9. Kullanıcı tanımlı veri tipleri
### 9.1 Struct bildirimi
### 9.2 Struct alanları
### 9.3 Struct literal'ları
### 9.4 Alan başlatma ve alan erişimi
### 9.5 İç içe struct'lar
### 9.6 Union bildirimi
### 9.7 Union üyeleri ve ortak depolama
### 9.8 Enum bildirimi
### 9.9 Enum üyeleri ve discriminant değerleri
### 9.10 İç içe ve bileşik kullanıcı tipleri
### 9.11 Eksik veya ertelenmiş aggregate kuralları

---

## Part III — İfadeler ve operatörler

## 10. İfadeler
### 10.1 İfade ve statement ayrımı
### 10.2 Literal ifadeleri
### 10.3 İsim ve alan ifadeleri
### 10.4 Gruplama ve parantez
### 10.5 Dizi, tuple, map ve struct ifadeleri
### 10.6 Fonksiyon çağrısı ifadeleri
### 10.7 Koşullu ifadeler
### 10.8 İfade önceliği ve birleşme yönü
### 10.9 İfade içindeki atama
### 10.10 Geçersiz ifade biçimleri

## 11. Operatörler
### 11.1 Operatör öncelik tablosu
### 11.2 Aritmetik operatörler
### 11.3 Üs alma: `^^`
### 11.4 Karşılaştırma operatörleri
### 11.5 Mantıksal operatörler
### 11.6 Bit düzeyinde operatörler
### 11.7 Bit kaydırma operatörleri
### 11.8 Atama ve bileşik atama
### 11.9 Artırma ve azaltma
### 11.10 Unary operatörler
### 11.11 Pointer dereference ve adres alma
### 11.12 Aralık ve özel operatörler
### 11.13 Operatörlerin tip kuralları
### 11.14 Öncelik kaynaklı belirsizlikler ve parantez önerileri

## 12. Dönüşümler ve tip doğrulama ifadeleri
### 12.1 Açık tip dönüşümü
### 12.2 `cast` biçimi
### 12.3 Daraltma ve genişletme
### 12.4 Sayısal dönüşüm sınırları
### 12.5 Tip sorgulama ifadeleri
### 12.6 Boyut ve uzunluk ifadeleri
### 12.7 Geçersiz dönüşüm örnekleri

---

## Part IV — Akış kontrolü ve bloklar

## 13. Bloklar ve kapsamlar
### 13.1 Blok sözdizimi
### 13.2 Lexical scope
### 13.3 İç içe bloklar
### 13.4 İsim görünürlüğü
### 13.5 Shadowing ve yeniden bildirim
### 13.6 Bloktan çıkış
### 13.7 Blok sonu ve kapsam kapanışı
### 13.8 Kapsam hataları ve örnekler

## 14. Koşullu akış
### 14.1 `if` biçimi
### 14.2 `else` ve alternatif dallar
### 14.3 Koşul ifadelerinin kuralları
### 14.4 Koşullu atama ve `=?` ailesi
### 14.5 İç içe koşullar
### 14.6 Koşul bloklarında kapsam
### 14.7 Geçersiz koşul örnekleri

## 15. Döngüler ve tekrar
### 15.1 `loop` temel biçimi
### 15.2 Sayaçlı döngü biçimleri
### 15.3 Koşullu döngüler
### 15.4 Sonsuz döngü
### 15.5 Döngü kapsamı
### 15.6 Döngüden çıkış ve devam
### 15.7 İç içe döngüler
### 15.8 Döngü kontrol ifadeleri
### 15.9 Rolling / zaman kontrollü tekrar
### 15.10 Süre literal'ları ve birimler
### 15.11 Geçersiz döngü ve süre tanımları

## 16. Akış etiketleri ve yönlendirme
### 16.1 Etiket bildirimi
### 16.2 Etiketli blok
### 16.3 Etikete yönlendirme operatörleri
### 16.4 Tekrar akışı
### 16.5 Akış sonlandırma
### 16.6 Etiket kapsamı ve görünürlüğü
### 16.7 Geçersiz hedefler ve erişilemeyen etiketler

## 17. Return ve fonksiyon/blok sonlandırma
### 17.1 `ret` bildirimi
### 17.2 Değer döndüren ve döndürmeyen fonksiyonlar
### 17.3 Bloktan erken çıkış
### 17.4 `halt` ve tablo/blok sonlandırma biçimleri
### 17.5 Sonlandırma sonrası erişilemeyen kod
### 17.6 Geçersiz dönüş ve sonlandırma örnekleri

---

## Part V — Fonksiyonlar ve çağrılar

## 18. Fonksiyon bildirimi
### 18.1 Fonksiyon tanımının genel biçimi
### 18.2 Parametre bildirimi
### 18.3 Parametre tipleri
### 18.4 Dönüş tipi
### 18.5 Gövde ve blok kapsamı
### 18.6 İmzasız / çıkarımlı tip biçimleri
### 18.7 Fonksiyon bildirimi ile tanım ayrımı
### 18.8 Tekrarlı ve çakışan fonksiyon isimleri

## 19. Fonksiyon çağrıları
### 19.1 Konumsal argümanlar
### 19.2 İsimli argümanlar
### 19.3 Karma argüman kullanımı
### 19.4 Varsayılan argümanlar
### 19.5 Değişken sayıda argüman
### 19.6 Argüman sırası ve eşleştirme
### 19.7 Argüman tipi ve sayısı hataları
### 19.8 İç içe fonksiyon çağrıları

## 20. Fonksiyon tip çıkarımı ve genel biçimler
### 20.1 `t` tip yer tutucusu
### 20.2 Parametreler arası tip eşleştirme
### 20.3 Dönüş tipi çıkarımı
### 20.4 Tip tutarsızlığı örnekleri
### 20.5 İç içe generic biçimler — tasarım durumu

## 21. Lambda ve callable ifadeler
### 21.1 Lambda biçimi
### 21.2 Parametre ve dönüş tipi
### 21.3 Yakalama sözdizimi
### 21.4 Lambda dışı blok/işlev aktarımı
### 21.5 Callable değerlerin çağrılması
### 21.6 Desteklenmeyen callable biçimleri

## 22. Group ve method sözdizimi
### 22.1 Group bildirimi
### 22.2 Group örneği oluşturma
### 22.3 Method tanımı
### 22.4 `self` parametresi ve alıcı sözdizimi
### 22.5 Instance method çağrısı
### 22.6 Method argüman eşleştirmesi
### 22.7 Group kapsamı ve üye görünürlüğü
### 22.8 Geçersiz method çağrıları

---

## Part VI — Pointer, reference ve sahiplik ile ilgili kaynak sözdizimi

## 23. Pointer tipleri ve işlemleri
### 23.1 Pointer tip yazımı: `*T`
### 23.2 Çok katmanlı pointer: `**T`
### 23.3 Pointer bildirimi
### 23.4 Adres alma
### 23.5 Dereference
### 23.6 Pointer ataması
### 23.7 Pointer aritmetiği
### 23.8 Null / geçersiz pointer gösterimi
### 23.9 Raw pointer riskleri ve açık sınırlar
### 23.10 Geçersiz pointer ifadeleri

## 24. Reference ve borrow sözdizimi
### 24.1 Shared reference: `&T`
### 24.2 Mutable reference: `&mut T`
### 24.3 Reference oluşturma
### 24.4 Reference üzerinden erişim
### 24.5 Reference parametreleri
### 24.6 Reference döndürme
### 24.7 Reference yeniden bağlama
### 24.8 Reference ve aggregate alanları
### 24.9 Geçici borrow ifadeleri
### 24.10 Geçersiz reference biçimleri

## 25. Borrow, ownership ve yaşam alanı kuralları
### 25.1 Owner ve kaynak değer
### 25.2 Shared / exclusive borrow
### 25.3 Borrow çakışması
### 25.4 Projection: struct alanları
### 25.5 Projection: array indeksleri
### 25.6 Dinamik indeks ve bilinmeyen erişim yolu
### 25.7 Scope ve lifetime sınırları
### 25.8 Fonksiyon çağrısında geçici borrow
### 25.9 Parametre kökeni ve dönüş kökeni
### 25.10 Raw pointer ile safe reference ayrımı
### 25.11 Tanı örnekleri ve düzeltme önerileri
### 25.12 Açık tasarım konuları

---

## Part VII — Nexus Flow özgün akış yapıları

## 26. Table / masa modeli
### 26.1 Masa kavramı ve kaynak sözdizimi
### 26.2 Masa oluşturma
### 26.3 Masa adı ve kapsamı
### 26.4 Masa içindeki ifadeler
### 26.5 Masa ömrü ve blok kapanışı
### 26.6 Masa üzerinden veri aktarımı
### 26.7 Masa ile fonksiyon/metot etkileşimi
### 26.8 Geçersiz masa kullanımları

## 27. Veri akışı ve sahiplik aktarımı
### 27.1 Veri akış operatörleri
### 27.2 `<-` ile alma/bağlama
### 27.3 `->` ile gönderme/çağrı
### 27.4 `_>` ile relocate / ownership handoff
### 27.5 Taşıma ile kopyalama ayrımı
### 27.6 Kaynak kullanımının sona ermesi
### 27.7 Akış zincirleri
### 27.8 Hatalı aktarım ve sahiplik örnekleri

## 28. Hata saptırmalı akış
### 28.1 `?` hata saptırma işareti
### 28.2 Hata işleme bloğu
### 28.3 Hata değerinin bağlanması
### 28.4 Başarılı ve hatalı akış yolları
### 28.5 Zincirleme hata saptırma
### 28.6 Hata bloğu kapsamı
### 28.7 İşlenmeyen hata durumu
### 28.8 Geçersiz hata saptırma örnekleri

## 29. Asenkron ve eşzamanlı akış
### 29.1 Asenkron ifade biçimleri
### 29.2 Bekleme ve devam sözdizimi
### 29.3 Task / iş parçacığı ile ilgili dil yapıları
### 29.4 `spawn`, `listen`, `send`, `receive`, `done` kullanımı
### 29.5 Paylaşılan kaynak ve aktarım biçimleri
### 29.6 Zamanlama sözdizimi
### 29.7 Tasarım aşamasındaki concurrency kuralları

---

## Part VIII — Modüller ve kaynak organizasyonu

## 30. Modül kullanımı
### 30.1 `use` bildirimi
### 30.2 Modül adı ve yol biçimleri
### 30.3 Birden fazla modül kullanımı
### 30.4 Standart / otomatik yüklenen modül adları
### 30.5 `no:` ile modül davranışını değiştirme
### 30.6 Modül bildirimi sırası
### 30.7 Geçersiz modül bildirimleri

## 31. Harici kaynak ve bağlantı bildirimi
### 31.1 Harici modül sözdizimi
### 31.2 Harici sembol bildirimi
### 31.3 `.nxf` kaynak kullanımı
### 31.4 `.exf` harici nesne bildirimi
### 31.5 Harici fonksiyon imzaları
### 31.6 Dil seviyesindeki sınırlar ve açık konular

---

## Part IX — İleri sözdizimi

## 32. Pattern ve destructuring
### 32.1 Tuple açılımı
### 32.2 Array açılımı
### 32.3 Struct alanı eşleme
### 32.4 Enum eşleme
### 32.5 Pattern kapsamı ve tip kuralları
### 32.6 Tasarım durumu

## 33. Attribute ve metadata sözdizimi
### 33.1 Attribute biçimi
### 33.2 Fonksiyon attribute'ları
### 33.3 Tip ve alan attribute'ları
### 33.4 Derleme yönergeleri
### 33.5 Ayrılmış metadata biçimleri
### 33.6 Tasarım durumu

## 34. Macro ve metaprogramlama
### 34.1 Macro ihtiyacı ve kapsam
### 34.2 Olası macro çağrı biçimleri
### 34.3 Token / syntax genişletme
### 34.4 Compile-time değerlendirme sınırları
### 34.5 Tasarım aşamasında olan kurallar

## 35. Unsafe ve düşük seviyeli sözdizimi
### 35.1 Unsafe bölge ihtiyacı
### 35.2 Raw memory erişim ifadeleri
### 35.3 Donanım/CPU'ya yönelik dil ifadeleri
### 35.4 Güvenlik sınırları
### 35.5 Henüz tanımlanmamış biçimler

---

## Part X — Hata ve tanı referansı

## 36. Sözdizimi hataları
### 36.1 Beklenmeyen token
### 36.2 Eksik ayraç veya kapanış
### 36.3 Geçersiz ifade
### 36.4 Geçersiz bildirim
### 36.5 Geçersiz tip yazımı
### 36.6 Hatalı operatör kullanımı
### 36.7 Hata konumu ve kaynak aralığı

## 37. Dil kuralı ihlalleri
### 37.1 İsim ve kapsam ihlalleri
### 37.2 Tip uyuşmazlığı
### 37.3 Fonksiyon argüman hataları
### 37.4 Borrow ve lifetime ihlalleri
### 37.5 Akış ve sahiplik aktarımı hataları
### 37.6 Modül bildirimi hataları
### 37.7 Uyarı ve öneri sınıfları

## 38. Hata mesajı yazım standardı
### 38.1 Hata başlığı ve kodu
### 38.2 Hatanın yeri
### 38.3 Hatanın nedeni
### 38.4 İlgili kaynak ifadesi
### 38.5 Düzeltme önerisi
### 38.6 İlgili kural/bölüm bağlantısı
### 38.7 Birden fazla tanının sıralanması

---

## Part XI — Tam örnekler

## 39. Başlangıç örnekleri
### 39.1 Hello World
### 39.2 Değişkenler ve tipler
### 39.3 Koşul ve döngü
### 39.4 Fonksiyon çağrısı

## 40. Orta seviye örnekler
### 40.1 Diziler ve map
### 40.2 Struct ve enum
### 40.3 Group ve method
### 40.4 Modül kullanımı
### 40.5 Hata saptırmalı akış

## 41. İleri seviye örnekler
### 41.1 Pointer ve reference
### 41.2 Borrow ve lifetime
### 41.3 Masa ve ownership handoff
### 41.4 Asenkron veri akışı
### 41.5 Birden çok özelliği birleştiren örnek program

## 42. Geçersiz örnekler kataloğu
### 42.1 Lexer tarafından reddedilen örnekler
### 42.2 Parser tarafından reddedilen örnekler
### 42.3 Dil kuralı ihlali örnekleri
### 42.4 Hatalı örneklerin düzeltilmiş halleri

---

## Appendices

## A. Tam sözdizimi özeti
## B. Anahtar sözcükler ve ayrılmış isimler
## C. Operatör öncelik tablosu
## D. Tip adları ve tip yazım tablosu
## E. Literal biçimleri tablosu
## F. Bildirim önekleri tablosu
## G. Akış operatörleri tablosu
## H. Pointer ve reference yazım tablosu
## I. Hata ve uyarı kataloğu
## J. Özellik olgunluk matrisi
## K. Sürüm ve değişiklik günlüğü
## L. Terimler sözlüğü
## M. Kaynaklar ve eski referans belgeleri

---

## Bölüm yazım standardı

Her özellik bölümü aşağıdaki alt başlıkları kullanacaktır. Uygulanmayan başlıklar gerekçesiyle belirtilecek; boşluklar sessizce atlanmayacaktır.

1. **Amaç:** Özelliğin ne için bulunduğu.
2. **Sözdizimi:** Kanonik yazım ve mümkünse EBNF/gramer.
3. **Kurallar:** Kabul edilen ve reddedilen biçimler.
4. **Kapsam:** İsim görünürlüğü, blok kapsamı ve etki alanı.
5. **Yaşam süresi:** Değer, kaynak veya bağlamın geçerlilik süresi; ilgiliyse.
6. **Tip kuralları:** Operand, parametre ve sonuç tipleri; ilgiliyse.
7. **Örnekler:** En az bir temel ve bir gerçekçi kullanım.
8. **Geçersiz örnekler:** Hatalı kullanım ve neden reddedildiği.
9. **Hata / uyarı / öneri:** Beklenen tanı ve düzeltme önerisi.
10. **Sınırlar:** Desteklenmeyen veya belirsiz durumlar.
11. **Olgunluk:** Tanım ve uygulama durumu.
12. **İlgili bölümler:** Bağımlı veya bağlantılı kurallar.

## Eski referanslar ve başlangıç kaynakları

Aşağıdaki dosyalar yeni manuelin yerine geçmez. İçerikleri bölüm bölüm incelenecek, çelişkiler çözülecek ve doğrulanmış kurallar yeni belgeye taşınacaktır.

- [Eski geniş syntax referansı](docs/language/syntax-reference.md)
- [Eski Core Reference Guide](omnicore-sdk/docs/Core%20Reference%20Guide%20zero.md)
- [Nexus Flow dil karşılaştırması](docs/language/NEXUS-FLOW-LANGUAGE-COMPARISON.md)
- [OCC pipeline ilerleme tablosu](OCC_PIPELINE_PROGRESS.md)

**İlk çalışma sırası:** Bölüm 0–3 → 4–9 → 10–12 → 13–17 → 18–22 → 23–25 → 26–31 → 32–38 → 39–42 → ekler.
