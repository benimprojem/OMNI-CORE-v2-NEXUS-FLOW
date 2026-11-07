# Nexus Flow — Dil Özellikleri, Karşılaştırma ve Açık Tasarım Alanları

**Durum:** 2026-11-07 kaynak denetimine göre ilk karşılaştırma  
**Kapsam:** Nexus Flow dil tasarımı ve aktif C Lexer/Parser/AST uygulaması  
**Normatif syntax kaynağı:** [syntax-reference.md](syntax-reference.md)

> Bu belge yeni bir syntax standardı değildir. Tasarımda bulunan, Parser tarafından kabul edilen, AST'de temsil edilen ve henüz yalnızca taslak olan özellikleri birbirinden ayırır. “Özgün” ifadesi başka hiçbir dilde benzeri yok anlamına gelmez; Nexus Flow'daki birleşimin ayırt edici olduğunu belirtir.

---

## 1. Nexus Flow'un ayırt edici özellikleri

### 1.1 Handler / masa tabanlı kaynak akışı

Temel biçimler:

- `(h) <- expression`: sonucu isimlendirilmiş handler/masa alanına yakalama.
- `(h) -> operation`: değeri sonraki işleme taşıma.
- `_>`: kaynak/context yerini değiştirme ve sahiplik transferi niyeti.
- `(resource)?;`: tekil kaynağı kapatma.
- `}?;`: blokta kalan sahipli kaynakları topluca kapatma.

**Neden ayırt edici?** Rust'ın ownership/borrowing modeli, Zig'in `defer` / `errdefer` mekanizması ve Go'nun `defer` ifadesi kaynak yaşam süresini yönetir; fakat Nexus Flow bunları handler isimleri, açık transfer operatörü ve tekil/blok kapsamlı Halt sözdizimiyle bir akış modeli içinde birleştirmeyi hedefler.

**Avantajları**
- Kaynak yaşam süresinin kaynak kodda görünür olması.
- Kaynak devrinin sıradan kopyalamadan ayrılması.
- Blok sonunda unutulmuş kaynakların toplu kapatılabilmesi.
- DLL, dosya, tablo, socket ve benzeri kaynak türlerini aynı soyutlamayla yönetebilme potansiyeli.

**Riskleri / dezavantajları**
- Her handler'ın sahiplik, ödünç alma, boş/dolu ve geçersiz durumlarının kesin kuralları olmalı.
- `_>` sonrasında eski adın kullanılması statik olarak engellenmeli.
- Hata veren cleanup işlemlerinin sırası ve hata birleştirme politikası belirlenmeli.
- Rust seviyesinde bir borrow/lifetime denetimi olmadan yalnızca sözdizimi kullanmak güvenlik garantisi sağlamaz.

**Uygulama sınırı:** AST'de capture, flow, relocate, single Halt ve block Halt düğümleri var. Sahiplik/effect kayıtları ve gerçek cleanup lowering tamamlanmış kabul edilmez.

### 1.2 Operatörlerle hata yönlendirme ve yeniden deneme

Öne çıkan biçimler:

- `?(count, delay ms)->target`: sınırlı yeniden deneme.
- `?->`: hata yoluna yönlendirme.
- `?=>`: fallback değer/işlem.
- `!->`: hatayı görmezden gelerek akışı sürdürme.
- `(e){...}`: hata işleme bloğu.

**Neden ayırt edici?** Go çoğunlukla açık hata değerleri ve kontrol ifadeleri kullanır; Rust `Result` ve `?` ile hata yayılımı sağlar; Zig `try`, `catch` ve `errdefer` sunar. Nexus Flow'un farkı, retry, catch, fallback ve ignore davranışlarını ayrı akış operatörleri olarak aynı ifade hattına yerleştirme hedefidir.

**Avantajları**
- Hata yolunun normal başarı yolundan görsel olarak ayrılması.
- Retry/fallback davranışının çağrı çevresinde açıkça yazılabilmesi.
- Pipeline benzeri kodda hata yönlendirmesinin görünür olması.

**Riskleri / dezavantajları**
- Operatör zincirlerinin önceliği ve birleşimi çok kesin tanımlanmalı.
- Retry'nin hangi hatalarda tekrarlanacağı, toplam süre, cancellation ve idempotency sözleşmeleri belirlenmeli.
- `!->` hatayı yok saydığı için uyarı/diagnostic zorunluluğu düşünülmeli.
- Flow operatörleri ile normal bitwise/shift operatörleri çakışmamalı.

**Uygulama sınırı:** Parser flow/pipe AST üretir ve rolling metadata'yı taşır. Runtime hata modeli, effect kuralları ve OIR/Codegen yürütme davranışı tamamlanmış değildir.

### 1.3 Multi-Rule Engine

`rules`, bariyerler, trigger'lar, `always`, `default`, `apply rules` ve `on` yapıları bir dil özelliği olarak tasarlanmıştır.

**Ayırt edici tarafı:** Bariyerlerin ilk hatada durmak yerine toplu değerlendirilmesi, hataların biriktirilmesi ve başarılı değerlendirme sonrasında birden fazla trigger/handler'ın çalıştırılması.

**Avantajları**
- Validasyon kurallarının fonksiyon gövdesinden ayrılması.
- Birden çok hatanın tek seferde raporlanması.
- Olay/trigger odaklı uygulama kodunun daha okunabilir olması.

**Riskleri**
- Kural sırası, side-effect, tekrar değerlendirme ve hata biriktirme ABI'si kesin olmalı.
- Kural bloklarının saf mı yoksa yan etkili mi olduğu belirlenmeli.
- Normal return, RuleSet failure ve panic birbirinden ayrılmalı.

**Uygulama sınırı:** Parser/AST ve bazı Semantic kayıtları mevcut; yürütme ve lowering sözleşmesi tamamlanmış değil.

### 1.4 Unified loop ve derleme zamanı unroll

Nexus Flow tek `loop` ailesiyle koşullu, sayımlı, range, foreach ve sonsuz döngü biçimlerini hedefler. `unroll(count) -> loop ...` derleme zamanı açılımı için tasarlanmıştır.

**Avantajları**
- Döngü ailelerinin tek bir anahtar kelimede toplanması.
- Basit döngülerin sözdizimsel çeşitliliğinin azaltılması.
- Sabit sayılı küçük döngüler için unroll niyetinin görünür olması.

**Dezavantajları**
- Tek gramer çok farklı kontrol akışlarını kapsadığı için AST normalizasyonu ve Semantic doğrulaması zorlaşır.
- Range, foreach ve klasik sayaç döngülerinin iterator/indeks davranışı açık olmalı.
- Unroll sayısı ile gerçek iteration sayısı uyuşmazsa davranış tanımlanmalı.

### 1.5 SELECT ve SCAN ayrımı

- `select(expr)`: değer/pattern karşılaştırması.
- `scan expr`: tür/varyant/struct pattern incelemesi ve alan binding'i.

Bu ayrım Rust `match` ve Kotlin `when` gibi pattern tabanlı seçimlerle aynı problem alanına girer; Nexus Flow'da değer seçimi ile yapı/varyant incelemesini iki ayrı üst yapı olarak ifade etmesi farklıdır.

**Avantajları:** Niyetin açık olması ve value matching ile payload binding'in ayrılması.

**Dezavantajları:** Exhaustiveness, guard sonrası sonraki kola geçiş, array scan davranışı ve pattern binding tür çıkarımı kesinleşmeli.

**Uygulama sınırı:** Parser/AST ve temel arm scope yapıları mevcut; subject-aware enum payload type inference ve exhaustiveness tamamlanmamış.

### 1.6 Group / method düzeni

`group Name { member => f:(...) {...} }` biçimi, fonksiyonları bir namespace/servis alanında toplar. Aynı adlı struct/group ilişkisinden nesne metodu ve statik benzeri çağrı tasarlanır.

**Benzerleri:** Go package, Rust module/impl, Kotlin object/class ve extension/member fonksiyonları.

**Farkı:** Nexus Flow'un `group` yapısı hem fonksiyon kümesi hem iç içe grup hem de veri yapısına bağlı method organizasyonu olarak düşünülür.

**Risk:** Statik üyeler, instance receiver, görünürlük, iç struct erişimi ve overload çözümlemesi birbirinden ayrılmalı.

### 1.7 FASTEXEC / inline ASM

`fastexec` içinde `asm:LABEL {...}`, `asmcall` ve `asmjmp` ile derleyici kontrollü düşük seviye blok hedeflenir.

**Avantaj:** Donanım/ABI seviyesinde optimizasyon ve register/ASM etkileşimi için doğrudan alan.

**Dezavantaj:** Yanlış ASM; register clobber, stack, ABI, memory ordering ve kontrol akışı açısından güvenli değildir. Derleyici bu alanı normal koddan daha yoğun doğrulamalıdır.

**Uygulama sınırı:** Raw ASM gövdesi ve AST taşıması var; register/clobber/ABI/target doğrulaması tamamlanmamış.

### 1.8 AREA / ZONE bellek niyeti

`area` ve `zone` fiziksel cache seviyelerini doğrudan garanti etmek yerine hedef/runtime tarafından eşlenecek mantıksal bellek yerleşim niyetleri olarak tasarlanmıştır.

**Avantaj:** CPU/GPU/NPU gibi farklı hedeflere aynı kaynak dil niyetiyle yaklaşma potansiyeli.

**Dezavantaj:** Host ve bare-metal davranışı, paylaşım kapsamı, senkronizasyon, tutarlılık ve allocation failure kuralları tanımlanmalı. L1/L2 veya L3/L4 eşlemesi garanti gibi sunulmamalı.

---

### 1.9 Expression lambda

`f:(a, b) > a * b` biçimi expression-body lambda'dır. Dönüş değeri son ifadedir; AST'de body doğrudan ifade düğümü olarak tutulur, sentetik `ret` eklenmez.

**Avantaj:** Kısa fonksiyon değerleri ve dönüş ifadesi tek yerde görünür; AST gövdesi gereksiz bir Return/Block katmanıyla şişmez.

**Dezavantaj / açık alan:** Lambda capture listesi, capture-by-value/reference, closure lifetime ve function-type uyumu henüz tamamlanmış değildir. Kotlin ve Rust closure'ları dış scope değerlerini yakalayabilir; Kotlin ayrıca function types'ı değişken ve parametre türü olarak açıkça tanımlar. Nexus Flow bu yeteneklerin sözdizimini değil, semantik ve tip modelini henüz tamamlamalıdır.

## 2. Başka dillerde zaten bulunan / ortak özellikler

Aşağıdaki yapılar Nexus Flow'da değerli olabilir; ancak tek başlarına özgün değildir:

| Nexus Flow yapısı | Yaygın karşılıklar | Nexus Flow için önemli fark |
|---|---|---|
| `struct`, `union`, enum payload | C, Rust, Zig, Go, Swift | Layout/ABI, tagged payload ve packed davranış |
| `nt:` NewType | C typedef, Rust newtype, Zig alias/wrapper | Nominal mi structural mı olduğu netleşmeli |
| Lambda / expression body | Kotlin lambda, Rust closure, Go function literal | Capture ve function type modeli henüz eksik |
| Generic fonksiyon | Go, Rust, Kotlin, Zig compile-time generic yaklaşımı | Type parameter/constraint grameri henüz normatif değil |
| `defer` | Go, Zig, Swift | Scope exit sırası, hata/panic/jump yolları |
| `select` / `scan` | Rust match, Kotlin when, Go switch/type switch | Exhaustiveness ve subject-aware binding |
| Pointer / reference | C pointer, Rust reference, Go pointer, Zig pointer | Nexus Flow'da tip grameri ve ownership bağlantısı tamamlanmalı |
| Module import / extern | C headers/linker, Go packages, Rust modules, Zig imports | `use`, MUST ve extern ABI sözleşmesi |

---

## 3. Nexus Flow'un diğer dillere göre avantaj potansiyeli

1. **Kaynak yaşam süresinin görünürlüğü:** Handler ve Halt sözdizimiyle kaynakların nerede taşındığı/kapatıldığı kaynakta görülebilir.
2. **Hata akışının ifade edilmesi:** Retry/catch/fallback/ignore tek bir kontrol akış modelinde yazılabilir.
3. **Kural odaklı validasyon:** Çoklu bariyer hatalarını biriktirme, kuralları fonksiyon gövdesinden ayırabilir.
4. **Hedefe yakın programlama:** FASTEXEC, raw ASM, register ve area/zone niyeti yüksek seviye dil içinde yer alabilir.
5. **Tek döngü ailesi:** Loop biçimlerini tek anahtar kelime çevresinde toplama ve unroll niyetini açık gösterme.

Bu avantajlar henüz tasarım/Parser seviyesindeki avantaj potansiyelidir. Semantic, OIR, Verifier ve runtime tamamlanmadan performans veya güvenlik üstünlüğü iddiası yapılmamalıdır.

## 4. Eksik veya diğer dillerde daha olgun olan yapılar

### P0 — Syntax / AST kapanışı için gerekli

| Eksik alan | Neden önemli | Önerilen yön |
|---|---|---|
| Pointer/reference type declarator | C ABI, extern, düşük seviye bellek erişimi | `*T`, `**T`, `&T` gramerini AST type metadata ile temsil et |
| Function type / callback | Callback API, DLL/extern arayüzü, fonksiyon değerleri | Parser artık `f:(name!T)>!R` imzasını kabul edip AST type-name alanında saklıyor; dedicated type node, compatibility ve ABI lowering açık |
| Generic type parameter | Generic örnekteki `t` şu anda gerçek type parameter olarak tanımlı değil | Açık type-parameter listesi ve daha sonra constraint sistemi |
| Null / optional değer | Flow fallback açıklamalarında null var, fakat gerçek literal/type sözleşmesi eksik | `null` literal ile Option/nullable type kavramını ayır |
| Numeric literal standardı | Hex, binary, octal ve scientific notation low-level/numeric kodda önemlidir | Radix/exponent grammar ve overflow kuralları |
| Shift / Flow operator ayrımı | `<<` / `>>` iki anlam için çakışıyor | Açık bir syntax disambiguation; token heuristiği kullanma |
| Directive / macro grameri | `!!=[target...]` ve `:macro` örnekleri core Parser tarafından tam karşılanmıyor | Preprocessor/target directive katmanını core expression gramerinden ayır |
| Source-span parity | Her production'ın gerçek kaynak aralığı AST'de korunmalı | Her production için start/end test matrisi |

### 4.1 Önerilen syntax adayları (henüz normatif değil)

Bu adaylar yaygın dil tasarımlarından yararlanır; mevcut Parser desteği olarak okunmamalıdır.

**Function type / callback:**

```nexus
nt:Callback = f:(id!u32)>!bool;
f:apply(callback!Callback, id!u32)!bool {
    ret callback(id);
}
```

İmza grameri `f:(parametreler)>!DönüşTipi` olarak tanımlandı ve Parser tarafından kabul ediliyor. Mevcut AST bu imzayı type-name string alanında korur; ileride type checking/ABI gereksinimi büyürse ayrı bir function-type AST node'a yükseltilebilir. Sıradan lambda expression ile karıştırılmamalıdır. Go ve Kotlin de fonksiyon tiplerini parametre ve sonuç tipleriyle tanımlar; Kotlin fonksiyon değerlerini `f(x)` biçiminde çağırabilir.

**Generic function:**

```nexus
f:sum<T>(a!T, b!T)!T {
    ret a + b;
}
```

İlk aşamada type parameter listesi fonksiyon adından sonra, parametre listesinden önce gelir. Constraint syntax'ı (ör. `T:Numeric`) daha sonra ayrıca tanımlanmalıdır. Type parameter listesi AST'de isim/constraint taşıyan ayrı düğümler olmalıdır; yalnızca fonksiyon adının içine metin olarak eklenmemelidir.

**Null / optional:** `null` bir literal, `none` ise varsa Option/Maybe türünün boş varyantı olarak ayrı kavramlar olmalıdır. Bunları tek değerde birleştirmek pointer nullability ile optional-value semantiğini karıştırır. Nexus Flow'da henüz bu karar verilmemiştir.

### P1 — Semantic/type sistemi

- Generic type inference ve type constraints.
- Callback/function value invocation, closure capture ve capture lifetime.
- Pointer/reference nullability, mutability, aliasing ve lifetime/ownership kuralları.
- Option/Result veya eşdeğer standart hata/değer türleri.
- Enum/scan exhaustiveness ve pattern payload type inference.
- Map key/value type policy ve generic map constraints.
- Vector/SIMD type dimension ve promotion kuralları.
- Function overloading, method receiver ve ambiguity resolution.
- String interpolation, Unicode escape ve character encoding semantics.

### P2 — Runtime / OIR / target sözleşmesi

- Flow operator retry/cancel/idempotency/error aggregation.
- Defer/Halt cleanup sırası ve cleanup hata birleştirme.
- Thread/spawn, channel, synchronization ve memory ordering.
- AREA/ZONE host/bare-metal allocation ve coherence.
- FASTEXEC register/clobber/ABI/target capability validation.
- Macro expansion, target conditional compilation ve external linker metadata.
- Generator/iterator protocol ve SCAN array iteration semantics.

Bu liste özelliklerin mutlaka eklenmesi gerektiği anlamına gelmez. Her madde dilin hedefleriyle karşılaştırılmalı; gereksiz olanlar bilinçli olarak kapsam dışı bırakılmalıdır.

---

## 5. Önerilen karar sırası

1. Pointer/reference syntax Parser üretimi tamamlandı; type metadata/ownership Semantic kuralları açık.
2. Function type/callback AST temsili.
3. Generic type parameter grameri ve AST temsili.
4. Null/optional modelinin dil seviyesinde kararı.
5. Numeric literal biçimleri.
6. `<<` / `>>` çakışması için açık gramer.
7. Directive/macro'nun core Parser dışındaki katmanının kesinleştirilmesi.
8. Syntax → Lexer → Parser → AST → Codec → Validator → tests matrisi.
9. AST kapanış denetimi; ardından Semantic aşamasına geçiş.

---

## 6. Karşılaştırma için resmi kaynaklar

- [Go Language Specification](https://go.dev/ref/spec) — pointer/function types, generics, goto, range ve defer.
- [Rust Reference: References](https://doc.rust-lang.org/core/primitive.reference.html) — `&T` / `&mut T` borrow semantics.
- [Zig Language Reference](https://ziglang.org/documentation/) — `defer`, `errdefer` ve compile-time behavior.
- [Kotlin: Higher-order functions and lambdas](https://kotlinlang.org/docs/lambdas.html) — function types, lambdas, closures ve function values.
- [Kotlin: Sealed classes and interfaces](https://kotlinlang.org/docs/sealed-classes.html) — kapalı varyant kümeleri ve exhaustive `when`.
