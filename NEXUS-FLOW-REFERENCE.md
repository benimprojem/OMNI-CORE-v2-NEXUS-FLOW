# NEXUS FLOW — Language Reference Manual

**Belge türü:** Normatif dil referansı — yalnızca Nexus Flow kaynak sözdizimi  
**Durum:** AST sınırına göre yeniden düzenleniyor  
**Sürüm:** 0.2-draft  
**Çalışma dalı:** `refactor/c-core-roadmap`

> Bu belge yalnızca Nexus Flow dilinin kaynak sözdizimini ve doğrudan bu sözdizimine ait kullanım kurallarını tanımlar. OCC mimarisi, AST'nin iç veri yapıları, Semantic modülünün algoritmaları, OIR, Verifier, Codegen, target/ABI ve SDK konuları kapsam dışıdır.
>
> **Kapsam kapısı:** Bu manuelde bir dil yapısı ancak aktif Parser tarafından kabul edilip mevcut AST modelinde temsil edilebiliyorsa normatif özellik olarak belgelenebilir. Sadece token/enum/legacy AST etiketi bulunması yeterli kanıt değildir. Parser/AST karşılığı olmayan gelecek fikirleri bu manuelin özellik listesine eklenmez.
>
> Bir yapının AST'de temsil edilmesi, onun tüm Semantic veya runtime davranışlarının tamamlandığı anlamına gelmez. Bu belge yalnızca kaynak biçimini ve doğrulanmış dil seviyesindeki sınırları açıklar.

---

# Bölüm 0 — Belgenin kullanımı ve kapsam

## 0.1 Amaç

Bu manuel, Nexus Flow kaynak kodunu yazan kişinin dilin kabul edilen yazım biçimlerini, ifade ve bildirim kurallarını, blok/kapsam yapısını ve mevcut dil operatörlerini tek bir yerde bulabilmesi için hazırlanır.

## 0.2 Kapsam sınırı

Bu belgeye yalnızca aşağıdaki kanıt zincirini geçen sözdizimi alınır:

1. Kaynak biçimi Lexer/Parser tarafından tanınır.
2. Parser bu biçim için mevcut AST'de anlamlı bir düğüm veya açıkça tanımlanmış mevcut düğüm biçimi üretir.
3. Sözdizimi ile AST eşlemesi kaynak denetiminde doğrulanabilir.

Aşağıdakiler bu manuelin konusu değildir:

- AST node alanlarının, codec'in veya OIR ID'lerinin teknik açıklaması.
- Semantic analiz algoritmalarının iç işleyişi.
- OIR, Verifier, Codegen, optimizer ve backend.
- CPU, target, ABI, bellek topolojisi veya runtime uygulama ayrıntıları.
- Henüz Parser/AST karşılığı olmayan gelecekteki dil yapıları.

## 0.3 Normatif ifadeler

- **ZORUNLUDUR (MUST):** Geçerli kaynak kod bu kurala uymalıdır.
- **YASAKTIR (MUST NOT):** Bu biçim mevcut dil sözleşmesinde kabul edilmez.
- **ÖNERİLİR (SHOULD):** Kodun açık ve taşınabilir olması için tercih edilmelidir.
- **OLABİLİR (MAY):** Dilin izin verdiği isteğe bağlı biçimdir.

Bu ifadeler yalnızca ilgili bölümdeki kanıtlanmış dil kuralları için kullanılır. Tasarım önerisi normatif kural gibi yazılmaz.

## 0.4 Özellik durum etiketleri

| Etiket | Anlam |
|---|---|
| **Tanımlı** | Kaynak biçimi ve kural bu belgede açıkça tanımlanmış. |
| **Parser/AST doğrulandı** | Aktif Parser biçimi AST'de temsil ediyor ve buna ilişkin fixture/test kanıtı var. |
| **Kısmen tanımlı** | Kaynak biçiminin bir kısmı biliniyor; gramer veya sınırlar henüz tamamlanmamış. |
| **Tasarım aşamasında** | Dil kararı kesinleşmemiş. Bu etiketli öneri geçerli sözdizimi değildir. |
| **Desteklenmiyor** | Mevcut Parser sözleşmesinde kabul edilmiyor. |
| **Legacy** | Eski AST/codec verileriyle uyumluluk için korunuyor; yeni kaynak kodda aktif sözdizimi değildir. |

**Önemli:** Bir token registry girdisi veya AST enum etiketi tek başına “Parser/AST doğrulandı” statüsü vermez. Bu statü için aktif Parser yolu ve test kanıtı aranır.

## 0.5 Örneklerin okunması

- Kod örnekleri Nexus Flow kaynak kodudur.
- Açıklama satırları örneğin parçası değilse ayrıca belirtilir.
- Geçersiz örnekler, hangi dil kuralını ihlal ettikleriyle birlikte gösterilir.
- Bir örneğin sözdizimsel olarak parse edilebilmesi, Semantic olarak geçerli olduğu anlamına gelmeyebilir; ayrım açıkça yazılır.

## 0.6 Hata açıklama biçimi

Bir sözdizimi kuralının hata örneklerinde mümkün olduğunda şu bilgiler verilir:

1. Hatalı kaynak bölümü.
2. Hatanın bulunduğu konum veya ifade.
3. İhlal edilen sözdizimi kuralı.
4. Beklenen biçim.
5. Düzeltilmiş örnek.

Bu bölüm hata mesajlarının OCC iç uygulamasını değil, dil referansındaki açıklama standardını belirler.

## 0.7 Sürümleme ve değişiklik

Yeni bir sözdizimi, aktif Parser ve AST karşılığı doğrulanmadan bu manuelde geçerli özellik olarak yayımlanmaz. Eski biçimler kaldırılacaksa uyumluluk etkisi ve geçiş biçimi ayrıca belgelenir.

---

# Bölüm 1 — Nexus Flow'a giriş

## 1.1 Dilin kaynak kodu biçimi

Nexus Flow programı; bildirimler, fonksiyonlar, tip tanımları, ifadeler ve bloklardan oluşur. Programın kök yapısı AST'de program düğümüyle, süslü parantezle çevrili kod blokları ise block düğümüyle temsil edilir.

## 1.2 En küçük program biçimi

Mevcut kaynak örneklerinde kullanılan temel giriş fonksiyonu biçimi:

```nxf
f:main() {
}
```

Bu örnek, fonksiyon bildirimi ve blok biçimini gösterir. Programın çalıştırılabilir giriş noktası olma şartları bu syntax belgesinin değil, ilgili program/target sözleşmesinin konusudur.

**Durum:** Fonksiyon ve blok kaynak biçimleri Parser/AST'de temsil edilir. Giriş noktası seçimi burada tanımlanmaz.

## 1.3 Değişken bildirimi ve ifade

```nxf
v:number = 42;
v:other!i32 = 7;
```

- `v:` değişken bildiriminin önekidir.
- `=` başlatıcı değerini bağlar.
- `!i32` açık tip belirtimini gösterir.
- Bildirim AST'de değişken bildirimi olarak temsil edilir.

Tip çıkarımı ve tip uyumluluğunun ayrıntıları Bölüm 5–7'de ele alınır.

## 1.4 Fonksiyon bildirimi ve çağrısı

```nxf
f:add(a!i32, b!i32)!i32 {
    ret a + b;
}

f:main() {
    v:result = add(10, 20);
}
```

Bu örnek fonksiyon bildirimi, parametreler, dönüş tipi, return ve fonksiyon çağrısı biçimlerini gösterir. Parametre eşleştirme ve dönüş tipi kuralları ilgili bölümlerde ayrıca tanımlanır.

## 1.5 Kontrol akışına kısa bakış

Aktif AST/Parser envanterinde koşul, döngü, return, break ve continue yapıları bulunur. Bunların kesin yazım biçimleri Bölüm 13–17'de verilir. Bu giriş bölümü ek bir kontrol yapısı veya yeni anahtar sözcük tanımlamaz.

---

# Bölüm 2 — Kaynak kodu ve biçim kuralları

## 2.1 Kaynak dosyası

Nexus Flow kaynak metni Lexer'a girdi olarak verilir ve Parser tarafından program/blok/declaration/statement biçimlerine ayrıştırılır.

Dosya uzantısı, kodlama standardı ve satır sonu normalizasyonu için mevcut kaynak denetiminde kesin bir normatif sözleşme doğrulanmadığından bu sürümde sabit bir değer ilan edilmez.

**Durum:** Kısmen tanımlı.

## 2.2 Boşluk ve satır sonları

Boşlukların ve satır sonlarının token sınırlarına etkisi Lexer davranışına bağlıdır. Bu manuel, Lexer kaynak kodu ve testleriyle doğrulanmayan biçimsel bir boşluk/indentation kuralı varsaymaz.

Girintileme için ayrı bir blok kapsamı anlamı tanımlanmamıştır; blokların AST karşılığı süslü parantezli yapıdır.

**Durum:** Kısmen tanımlı; tam whitespace/indentation matrisi Lexer denetimiyle tamamlanacak.

## 2.3 Blok ayraçları

Bloklar süslü parantezlerle yazılır:

```nxf
f:work() {
    v:value = 1;
}
```

- Açılış ayracı: `{`
- Kapanış ayracı: `}`
- İç içe bloklar ayrı block yapıları olarak temsil edilir.
- Program kökü ile fonksiyon/koşul/döngü gövdeleri aynı kaynak ayraçlarını kullanabilir; hangi bağlamda geçerli oldukları ilgili gramer kuralıyla belirlenir.

**Durum:** Parser/AST doğrulandı.

## 2.4 İfade sonlandırma

Mevcut kaynak örneklerinde bildirim ve birçok ifade `;` ile sonlandırılır. Ancak tüm statement türleri için noktalı virgül zorunluluğunu tek bir genel kurala dönüştürmeden önce Parser üretimleri ve negatif fixture'lar birlikte denetlenmelidir.

**Geçici kural:** Bir bölüm belirli bir statement için açıkça farklı bir sonlandırma biçimi vermiyorsa, o statement'ın mevcut Parser örneklerinde kullanılan biçimi izlenmelidir.

**Durum:** Kısmen tanımlı; tüm statement türleri için zorunluluk matrisi hazırlanacak.

## 2.5 Yorumlar

Eski syntax referansında `#` ile başlayan açıklama satırları kullanılmıştır. Bu biçimin aktif Lexer tarafından hangi satır/blok sınırlarında kabul edildiği kaynak ve testlerle ayrıca doğrulanmadan çok satırlı yorum veya iç içe yorum kuralı ilan edilmez.

**Durum:** Kısmen tanımlı. Bu bölümde yalnızca kaynakta kullanılan `#` örnekleri kayıt altındadır.

## 2.6 Hatalı blok ve ayraç kullanımı

Aşağıdaki durumlar yapısal olarak hatalıdır:

```nxf
f:broken() {
    v:x = 1;
# Eksik kapanış ayracı
```

Beklenen: Açılan her blok için eşleşen bir kapanış ayracı bulunmalıdır.

Daha karmaşık hata kurtarma davranışı Parser'ın tanı üretimi konusudur; dilin geçerli sözdizimi kuralı değildir.

---

# Bölüm 3 — Tokenlar ve sözcüksel öğeler

## 3.1 Token sınıfları

Mevcut token registry'sinde şu genel gruplar bulunur:

- Tanımlayıcı ve literal tokenları.
- Anahtar sözcük tokenları.
- Bildirim önekleri.
- Tip tokenları.
- Aritmetik, karşılaştırma, mantık ve bit operatörleri.
- Nexus Flow akış tokenları.
- Noktalama ve blok ayraçları.
- Inline ASM ham gövde tokenı.

Registry, sembolik token envanteridir; registry'deki her tokenın her kaynak bağlamında aktif olarak üretildiği anlamına gelmez. Bu manuel yalnızca Parser tarafından tüketildiği ve AST'de temsil edildiği doğrulanan biçimleri normatif olarak açıklar.

## 3.2 Tanımlayıcılar

Tanımlayıcılar değişken, fonksiyon, tip, alan, Group, method ve modül adları gibi isim konumlarında kullanılır.

Örnekler:

```nxf
v:counter = 0;
f:calculate() {}
struct: Point { v:x!i32, v:y!i32 }
```

Büyük/küçük harf duyarlılığı, izin verilen Unicode karakterleri, alt çizgi ve rakam konumları için Lexer kaynak/test denetimi tamamlanmadan daha ayrıntılı karakter grameri ilan edilmez.

**Durum:** Tanımlayıcı tokenı ve isim konumları mevcut; tam karakter grameri kısmen tanımlı.

## 3.3 Bildirim önekleri

Aktif AST/Parser eşlemesinde doğrulanması gereken temel bildirim aileleri:

| Yazım | Kaynak rolü | AST temsili | Durum |
|---|---|---|---|
| `v:` | Değişken bildirimi | `AST_VAR_DECL` | Aktif |
| `c:` | Sabit işaretli değişken bildirimi | `AST_VAR_DECL` | Parser/AST eşlemesi mevcut; compile-time değerlendirme ayrı konudur |
| `m:` | Başlatıcılı, değiştirilemeyen değer bildirimi | `AST_MUST_DECL` | Parser/AST mevcut |
| `nt:` | NewType bildirimi | `AST_NEWTYPE_DECL` | Aktif |
| `f:` | Fonksiyon bildirimi | `AST_FUNC_DECL` | Aktif |
| `struct:` | Struct bildirimi | `AST_STRUCT_DECL` | Aktif |
| `enum:` | Enum bildirimi | `AST_ENUM_DECL` | Aktif |
| `union:` | Union bildirimi | `AST_UNION_DECL` | Aktif |
| `group:` | Group bildirimi | `AST_GROUP_DECL` | Aktif |
| `exf:` | Harici fonksiyon bildirimi | `AST_EXTERN_DECL` | Aktif |

`r:`, `t:` ve `o:` önekleri mevcut syntax kaynağında ayrılmış olarak gösterilir. Aktif AST/Parser dil özelliği olarak belgelenmez.

## 3.4 Literal tokenları

AST literal düğümü ve Parser/AST karşılaştırma kaydında aşağıdaki literal sınıfları yer alır:

- Sayısal literal.
- String literal.
- Character literal.
- `true` ve `false`.
- `null`.

Sayısal biçimlerin tabanı, ayraçları ve üs yazımı Lexer denetimiyle kesinleştirilecektir. Bu sürümde doğrulanmamış bir literal biçimi eklenmez.

**Durum:** Literal sınıfları Parser/AST'de temsil edilir; alt biçimlerin tam grameri kısmen tanımlı.

## 3.5 Tip tokenları ve tip ifadeleri

Registry'de temel sayısal/tip tokenları bulunur; ayrıca AST'de recursive type expression düğümü tip biçimlerini taşır. Bu manuelde tiplerin kaynak yazımı Bölüm 6–9'da ele alınır.

Pointer, reference, array ve function-type biçimleri yalnızca mevcut Parser'ın tanıdığı type-expression bağlamlarında açıklanır. Registry'de bulunmayan tip aileleri bu belgeye eklenmez.

## 3.6 Operatör ve noktalama tokenları

Operatörler ve ayraçlar Bölüm 10–12'de kaynak yazımlarıyla açıklanır. Özellikle Nexus Flow akış tokenları ile normal aritmetik/bit operatörleri birbirine karıştırılmamalıdır; aynı karakter ailesinin farklı token kimlikleri olabilir.

## 3.7 Lexer tarafından reddedilen kaynak

Lexer'ın tanımadığı karakter dizileri geçerli token değildir. Hatalı literal, tanınmayan karakter ve eksik operatör biçimleri için hata örnekleri, Lexer test envanteri doğrulandıkça bu bölüme eklenecektir.

**Açık denetim:** Token registry 164 sembolik token türü içerir; tam lexer emission audit henüz tamamlanmış değildir. Bu nedenle registry'deki her öğe aktif kaynak sözdizimi olarak kabul edilmez.

---

# Part II — Bildirimler ve tipler

## 4. Tanımlayıcılar ve isim konumları
### 4.1 Değişken isimleri
### 4.2 Fonksiyon isimleri
### 4.3 Tip, alan ve üye isimleri
### 4.4 Group ve method isimleri
### 4.5 Modül isimleri ve alias biçimi
### 4.6 Ayrılmış önekler ve isimler

## 5. Değer bildirimleri
### 5.1 `v:` değişken bildirimi
### 5.2 `c:` sabit işaretli bildirim
### 5.3 `m:` immutable değer bildirimi
### 5.4 Başlatıcılı ve başlatıcısız bildirim
### 5.5 Açık tip belirtimi ve `!`
### 5.6 Bildirimlerin blok içindeki konumu
### 5.7 Geçersiz bildirim örnekleri

## 6. Tip yazımı
### 6.1 İsimlendirilmiş tipler
### 6.2 Açık tip zorlaması: `!Type`
### 6.3 Tip çıkarımlı bildirimler
### 6.4 Pointer tipleri: `*T`, `**T`
### 6.5 Reference tipleri: `&T`, `&mut T`
### 6.6 Array tipleri: `T[]`, `T[n]`, çok boyutlu biçimler
### 6.7 Function type ifadeleri
### 6.8 Qualified type biçimleri
### 6.9 Desteklenmeyen/henüz doğrulanmamış tip biçimleri

## 7. İlkel tipler ve literal'lar
### 7.1 Tamsayı tipleri
### 7.2 Kayan noktalı tipler
### 7.3 `bool`, `char`, `str`
### 7.4 `bit`, `byte`, `hex`
### 7.5 Sayısal literal biçimleri
### 7.6 String ve character literal'ları
### 7.7 Boolean ve null literal'ları
### 7.8 Literal-tip uyumu için kaynak yazımı

## 8. Array ve tuple
### 8.1 Array literal: `[a, b, ...]`
### 8.2 Boş ve iç içe array literal'ları
### 8.3 Array type declarator
### 8.4 Sabit ve dinamik boyut yazımı
### 8.5 Çok boyutlu array yazımı
### 8.6 Tuple ifadeleri
### 8.7 İndeks erişimi
### 8.8 Geçersiz array/tuple biçimleri

## 9. Kullanıcı tanımlı tipler
### 9.1 Struct bildirimi
### 9.2 Struct alanları
### 9.3 Struct literal ve alan başlatma
### 9.4 Enum ve variant bildirimi
### 9.5 Enum payload ve discriminant yazımı
### 9.6 Union bildirimi
### 9.7 NewType: `nt:`
### 9.8 İç içe tip bildirimleri ve sınırlar

---

# Part III — İfadeler ve operatörler

## 10. İfadeler
### 10.1 Literal ve identifier ifadeleri
### 10.2 Unary ifadeler
### 10.3 Binary ifadeler
### 10.4 Postfix ifadeler
### 10.5 Fonksiyon çağrısı
### 10.6 Member erişimi
### 10.7 Index erişimi
### 10.8 Tuple ve array ifadeleri
### 10.9 Struct literal ifadeleri
### 10.10 Cast ifadesi
### 10.11 Named argument yazımı
### 10.12 İfade gruplama

## 11. Operatörler
### 11.1 Aritmetik operatörler
### 11.2 Üs alma: `^^`
### 11.3 Karşılaştırma operatörleri
### 11.4 Mantıksal operatörler
### 11.5 Bit düzeyinde operatörler
### 11.6 Sayısal bit kaydırma operatörleri
### 11.7 Atama ve bileşik atama
### 11.8 Artırma ve azaltma
### 11.9 Unary pointer dereference
### 11.10 Operatör önceliği ve birleşme yönü
### 11.11 Nexus Flow operatörleriyle ayrım

## 12. Yerleşik ifade biçimleri
### 12.1 `cast(...)`
### 12.2 `typeof(...)`
### 12.3 `sizeof(...)`
### 12.4 `len(...)`
### 12.5 `is_ok`, `is_err`, `is_some`, `is_none`, `is_null`
### 12.6 `area` ve `zone` kaynak biçimleri
### 12.7 `addr` ve bellek erişim ifadeleri
### 12.8 Tarih, saat ve clock ifadeleri
### 12.9 `swap`, `unpack` ve diğer AST-backed ifadeler
### 12.10 Echo ve diğer I/O çağrı biçimleri

---

# Part IV — Bloklar ve kontrol akışı

## 13. Bloklar ve kapsam
### 13.1 Program ve block biçimi
### 13.2 İç içe bloklar
### 13.3 Blok içinde bildirimler
### 13.4 Thread block kaynak biçimi
### 13.5 FASTEXEC block kaynak biçimi
### 13.6 Blok sonlandırma: `}?;`
### 13.7 Blok içinde geçersiz yapı örnekleri

## 14. Koşullu akış
### 14.1 `if`
### 14.2 `elsif`
### 14.3 `els`
### 14.4 Koşullu kontrol biçimi
### 14.5 Koşul gövdeleri ve iç içe koşullar
### 14.6 Geçersiz koşul biçimleri

## 15. Döngüler
### 15.1 `loop` temel biçimi
### 15.2 Sonsuz döngü
### 15.3 Koşullu döngü
### 15.4 Range/count biçimleri
### 15.5 Foreach ve `in`
### 15.6 `break` ve `continue`
### 15.7 `unroll(count)->loop`
### 15.8 Rolling retry kaynak biçimi
### 15.9 Döngü sözdizimi hata örnekleri

## 16. Select ve Scan
### 16.1 `select` temel biçimi
### 16.2 Select arm
### 16.3 Default arm
### 16.4 `scan` temel biçimi
### 16.5 Binding pattern
### 16.6 Constructor pattern
### 16.7 `when` guard
### 16.8 Scan catch-all arm
### 16.9 İç içe select/scan
### 16.10 Pattern biçimlerinin sınırları

## 17. Return, break, continue ve sonlandırma
### 17.1 `ret`
### 17.2 `break`
### 17.3 `continue`
### 17.4 `panic`
### 17.5 `exit`
### 17.6 Tekil Halt biçimi
### 17.7 Blok Halt biçimi
### 17.8 Sonlandırma ifadelerinin kaynak konumu

---

# Part V — Fonksiyonlar, Group ve çağrılar

## 18. Fonksiyon bildirimi
### 18.1 `f:name(...)` biçimi
### 18.2 Parametre bildirimi
### 18.3 Parametre tipleri
### 18.4 Dönüş tipi
### 18.5 Fonksiyon gövdesi
### 18.6 Default parameter biçimi
### 18.7 Function type / callback tip yazımı
### 18.8 Variadic parametre: AST/Parser doğrulama durumu

## 19. Fonksiyon çağrısı
### 19.1 Konumsal argümanlar
### 19.2 Named argument: `name:value`
### 19.3 Konumsal ve named argüman ayrımı
### 19.4 İç içe çağrılar
### 19.5 Argüman listesi ve ayraçlar
### 19.6 Geçersiz çağrı biçimleri

## 20. Lambda ve callable ifadeler
### 20.1 Expression-body lambda
### 20.2 Block-body lambda
### 20.3 Lambda parametreleri
### 20.4 Lambda'nın ifade içindeki yeri
### 20.5 Function type ile lambda ayrımı
### 20.6 Henüz tanımlanmamış capture biçimleri

## 21. Group ve method
### 21.1 Group bildirimi
### 21.2 Group alanları
### 21.3 Method tanımı
### 21.4 Instance method ve receiver biçimi
### 21.5 Static method biçimi
### 21.6 Method çağrısı
### 21.7 Group içinde tip ve method kullanımı

## 22. Harici fonksiyon bildirimi
### 22.1 `exf:` bildirimi
### 22.2 Harici fonksiyon imzası
### 22.3 Link name biçimi
### 22.4 Parametre ve dönüş tipi yazımı
### 22.5 Geçersiz extern bildirimleri

---

# Part VI — Pointer, reference ve kaynak akışı

## 23. Pointer sözdizimi
### 23.1 Pointer tip yazımı: `*T`
### 23.2 Çok katmanlı pointer: `**T`
### 23.3 Pointer bildirimi
### 23.4 Dereference: `*value`
### 23.5 Pointer ile atama
### 23.6 Pointer tipinin fonksiyon imzasında kullanımı
### 23.7 Raw pointer kaynak biçiminin sınırları

## 24. Reference sözdizimi
### 24.1 Shared reference: `&T`
### 24.2 Mutable reference: `&mut T`
### 24.3 Reference oluşturma
### 24.4 Reference değişkeni bildirimi
### 24.5 Reference parametresi
### 24.6 Reference dönüş tipi
### 24.7 Reference ve alan/indeks ifadeleri
### 24.8 Geçersiz reference biçimleri

## 25. Borrow ve ownership ile ilişkili kaynak kuralları
### 25.1 Borrow oluşturma yazımı
### 25.2 Shared ve mutable biçimlerin ayrımı
### 25.3 Reference kullanım kapsamı
### 25.4 Alan ve indeks üzerinden borrow örnekleri
### 25.5 Fonksiyon çağrısında borrow argümanları
### 25.6 Kaynak aktarımı ile borrow ayrımı
### 25.7 Sözdizimi ile Semantic kuralın ayrıldığı noktalar

---

# Part VII — Nexus Flow akış yapıları

## 26. Nexus Flow akış operatörleri
### 26.1 Akış operatörleri genel bakış
### 26.2 Capture biçimi
### 26.3 Pipe / flow biçimi
### 26.4 Error handler biçimi
### 26.5 Fallback biçimi
### 26.6 Ignore biçimi
### 26.7 Shift ve numeric shift ayrımı
### 26.8 Operatör zinciri ve parantezleme

## 27. Masa ve kaynak bağlama
### 27.1 Masa/capture kaynak biçimi
### 27.2 Masa adı ve bağlama
### 27.3 Masa üzerinden akış
### 27.4 Tekil Halt
### 27.5 Blok Halt
### 27.6 Masa ile function çağrısı
### 27.7 AST/Parser sınırında olmayan masa fikirleri kapsam dışı

## 28. Relocate
### 28.1 `_>` operatörü
### 28.2 Kaynak ve hedef ifadeleri
### 28.3 Relocate ifadesinin statement bağlamı
### 28.4 Geçersiz relocate biçimleri
### 28.5 Kaynak biçimi ile ownership sonucu ayrımı

## 29. Label ve jump
### 29.1 Label bildirimi: `label:>`
### 29.2 Operator jump biçimi
### 29.3 `jump(label)` biçimi
### 29.4 Hedef yazımı
### 29.5 Geçersiz label/jump örnekleri

## 30. Hata saptırmalı akış ve retry
### 30.1 Error-handler operatörü
### 30.2 Error binding block
### 30.3 Fallback operatörü
### 30.4 Ignore operatörü
### 30.5 Rolling retry: `?(count, delay)->target`
### 30.6 Retry delay yazımı
### 30.7 Akış operatörlerinin birleşimi
### 30.8 Sözdizimsel ve çalışma zamanı sınırları

---

# Part VIII — Ruleset ve event biçimleri

## 31. Ruleset bildirimi
### 31.1 Ruleset adı ve parametreleri
### 31.2 Barrier kuralı
### 31.3 Trigger kuralı
### 31.4 Always kuralı
### 31.5 Default kuralı
### 31.6 Kural gövdesi ve sıralama

## 32. Ruleset kullanımı
### 32.1 `apply rules(...)`
### 32.2 Argüman yazımı
### 32.3 Event handler: `on Rule.Event`
### 32.4 Handler gövdesi
### 32.5 Ruleset ve event kaynak biçimi sınırları

---

# Part IX — Modüller ve düşük seviyeli kaynak yapıları

## 33. Modül kullanımı
### 33.1 `use` bildirimi
### 33.2 Birden fazla modül adı
### 33.3 Dotted module path
### 33.4 Alias: `as`
### 33.5 Modül bildirimi örnekleri

## 34. Inline assembly kaynak biçimi
### 34.1 `fastexec` block
### 34.2 `asm: LABEL { ... }`
### 34.3 Ham ASM gövdesi
### 34.4 ASM label
### 34.5 `asmcall` ve `asmjmp` çağrı biçimleri
### 34.6 Parser kapsam sınırları

## 35. Donanım ve sistem intrinsic kaynak biçimleri
### 35.1 Port I/O biçimleri
### 35.2 Memory peek/poke biçimleri
### 35.3 IRQ ve register biçimleri
### 35.4 Print/write biçimleri
### 35.5 Date/time/clock biçimleri
### 35.6 Area/zone biçimleri
### 35.7 Kaynak bırakma: `free` ve `defer` biçimleri
### 35.8 Zone sync, view ve lock biçimleri
### 35.9 Kaynak çağrı biçimi ile platform davranışının ayrımı

---

# Part X — Hatalı kaynak örnekleri

## 36. Lexer kaynak hataları
### 36.1 Tanınmayan karakter
### 36.2 Hatalı literal
### 36.3 Eksik operatör
### 36.4 Token sınırı hataları

## 37. Parser kaynak hataları
### 37.1 Eksik ayraç
### 37.2 Hatalı bildirim
### 37.3 Hatalı fonksiyon imzası
### 37.4 Hatalı ifade
### 37.5 Hatalı blok
### 37.6 Hatalı akış operatörü kullanımı

## 38. Dil kuralı hataları
### 38.1 Yanlış argüman biçimi
### 38.2 Yanlış tip yazımı
### 38.3 Yanlış pointer/reference biçimi
### 38.4 Yanlış kapsamda kontrol ifadesi
### 38.5 Hatalı label/jump
### 38.6 Hatalı select/scan arm

---

# Part XI — Örnekler

## 39. Başlangıç örnekleri
### 39.1 En küçük fonksiyon
### 39.2 Değişken ve tip
### 39.3 Fonksiyon çağrısı
### 39.4 Koşul ve döngü

## 40. Veri tipi örnekleri
### 40.1 Array ve tuple
### 40.2 Struct
### 40.3 Enum
### 40.4 Union
### 40.5 Group ve method

## 41. Akış ve bellek örnekleri
### 41.1 Pointer
### 41.2 Reference
### 41.3 Borrow
### 41.4 Capture ve flow
### 41.5 Relocate ve Halt
### 41.6 Error handler ve retry

## 42. İleri kaynak örnekleri
### 42.1 Select ve scan
### 42.2 Ruleset ve event
### 42.3 Modül kullanımı
### 42.4 Extern fonksiyon
### 42.5 FASTEXEC ve ASM
### 42.6 Donanım intrinsic çağrıları

---

# Ekler

## A. Aktif kaynak biçimleri ve AST eşleme indeksi
## B. Token sınıfları özeti
## C. Anahtar sözcük ve bildirim önekleri
## D. Operatör tablosu
## E. Tip yazımı tablosu
## F. Literal biçimleri tablosu
## G. Nexus Flow operatörleri tablosu
## H. Pointer/reference yazım tablosu
## I. Parser tarafından desteklenmeyen/legacy biçimler
## J. Özellik olgunluk matrisi
## K. Değişiklik günlüğü
## L. Terimler sözlüğü
## M. Eski referanslar ve kaynak belgeler

---

# Bölüm yazım standardı

Her özellik bölümü aşağıdaki yapıyla tamamlanır. Bir başlık o özellik için geçerli değilse “Uygulanmaz” denir; tasarım aşamasındaki kurallar geçerliymiş gibi yazılmaz.

1. **Amaç:** Kaynak yapısının ne ifade ettiği.
2. **Sözdizimi:** Kabul edilen kanonik yazım.
3. **Gramer:** Mevcut Parser üretiminden çıkarılabilen biçim; kesin değilse açıkça belirtilir.
4. **Kurallar:** Kabul edilen ve reddedilen kaynak biçimleri.
5. **Kapsam:** Yapının hangi kaynak bağlamında kullanılabildiği.
6. **Örnekler:** En az bir geçerli örnek.
7. **Geçersiz örnekler:** Hatalı biçim ve reddedilme nedeni.
8. **Tanı:** Hata konumu ve açıklanabilir hata nedeni.
9. **Sınırlar:** Parser/AST ile henüz temsil edilmeyen durumlar.
10. **Olgunluk:** Tanımlı / Parser-AST doğrulandı / Kısmen tanımlı / Tasarım aşamasında / Desteklenmiyor / Legacy.
11. **İlgili bölümler:** Bağlantılı kaynak kuralları.

---

# Kaynaklar ve kapsam denetimi

Yeni manuelin hazırlanmasında aşağıdaki dosyalar kaynak denetimi için kullanılır; bu dosyalar yeni manuelin yerine geçmez:

- [Önceki syntax referansı](docs/language/syntax-reference.md)
- [AST node registry](OIR_AST_NODE_REGISTRY.md)
- [Syntax ↔ AST karşılaştırma matrisi](NEXSUS_AST_SYNTAX_MATRIX.md)
- [Token registry](OIR_TOKEN_REGISTRY.md)
- [OCC pipeline ilerleme tablosu](OCC_PIPELINE_PROGRESS.md)

**Kapsam denetimi kuralı:** Registry'de bulunup aktif Parser tarafından üretilmeyen legacy/unsupported düğümler yeni bir kaynak yapısı olarak tanıtılmaz. Parser/AST kanıtı eksik olan bir biçim, kesin gramer olarak yazılmaz. Yeni bir dil özelliği ancak AST kapsamı genişletilip Parser bağlantısı doğrulandıktan sonra bu manuelin ilgili bölümüne alınır.

**İlk yazım sırası:** Bölüm 0–3 → 4–9 → 10–12 → 13–17 → 18–22 → 23–30 → 31–35 → 36–42 → ekler.
