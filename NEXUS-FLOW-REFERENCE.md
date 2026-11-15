# NEXUS FLOW — Language Reference Manual

**Belge türü:** Nexus Flow programlama dili kullanım ve başvuru kılavuzu  
**Durum:** İlk taslak — dil kullanıcılarına yönelik olarak düzenleniyor  
**Sürüm:** 0.2-draft  
**Çalışma dalı:** `refactor/c-core-roadmap`

> Bu kılavuz Nexus Flow ile program yazan kişiler içindir. Dilin sözdizimini, ifadelerin nasıl kullanılacağını, tipleri, kontrol yapılarını, bellek ve akış yapılarını örneklerle açıklar. Derleyicinin kaynak kodu nasıl işlediğini veya bu yapıları içeride nasıl temsil ettiğini anlatmaz.
>
> Henüz tasarlanmamış veya kullanıma sunulmamış özellikler geçerli dil özelliği gibi gösterilmez. Eksik tanımlanmış konular açıkça işaretlenir.
---

# Bölüm 0 — Belgenin kullanımı ve kapsam

## 0.1 Amaç

Bu manuel, Nexus Flow kaynak kodunu yazan kişinin dilin kabul edilen yazım biçimlerini, ifade ve bildirim kurallarını, blok/kapsam yapısını ve mevcut dil operatörlerini tek bir yerde bulabilmesi için hazırlanır.

## 0.2 Kapsam sınırı

Bu belgeye yalnızca aşağıdaki kanıt zincirini geçen sözdizimi alınır:

1. Kaynak biçimi sözcüksel çözümleme/dil tarafından tanınır.
2. derleyici bu biçim için mevcut iç temsil'de anlamlı bir düğüm veya açıkça tanımlanmış mevcut düğüm biçimi üretir.
3. Sözdizimi ile iç temsil eşlemesi kaynak denetiminde doğrulanabilir.

Aşağıdakiler bu manuelin konusu değildir:

- iç temsil node alanlarının, codec'in veya OIR ID'lerinin teknik açıklaması.
- Semantic analiz algoritmalarının iç işleyişi.
- OIR, Verifier, Codegen, optimizer ve backend.
- CPU, target, ABI, bellek topolojisi veya runtime uygulama ayrıntıları.
- Henüz dil iç yapısı karşılığı olmayan gelecekteki dil yapıları.

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
| **Tanımlı** | Aktif derleyici biçimi iç temsil'de temsil ediyor ve buna ilişkin örnek/test kanıtı var. |
| **Kısmen tanımlı** | Kaynak biçiminin bir kısmı biliniyor; gramer veya sınırlar henüz tamamlanmamış. |
| **Tasarım aşamasında** | Dil kararı kesinleşmemiş. Bu etiketli öneri geçerli sözdizimi değildir. |
| **Desteklenmiyor** | Mevcut derleyici sözleşmesinde kabul edilmiyor. |
| **Legacy** | Eski iç temsil/codec verileriyle uyumluluk için korunuyor; yeni kaynak kodda aktif sözdizimi değildir. |

**Önemli:** Bir sözcüksel öğe listesi girdisi veya iç temsil enum etiketi tek başına “Tanımlı” statüsü vermez. Bu statü için aktif derleyici yolu ve test kanıtı aranır.

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

Yeni bir sözdizimi, aktif derleyici ve iç temsil karşılığı doğrulanmadan bu manuelde geçerli özellik olarak yayımlanmaz. Macro sistemi henüz tasarlanmadığı için geçerli dil özellikleri arasında yer almaz; bu kılavuzun en sonunda gelecekteki özellik olarak tutulur. Eski biçimler kaldırılacaksa uyumluluk etkisi ve geçiş biçimi ayrıca belgelenir.

---

# Bölüm 1 — Nexus Flow'a giriş

## 1.1 Dilin kaynak kodu biçimi

Nexus Flow programı; bildirimler, fonksiyonlar, tip tanımları, ifadeler ve bloklardan oluşur. Programın kök yapısı iç temsil'de program düğümüyle, süslü parantezle çevrili kod blokları ise block düğümüyle temsil edilir.

## 1.2 En küçük program biçimi

Mevcut kaynak örneklerinde kullanılan temel giriş fonksiyonu biçimi:

```nxf
f:main() {
}
```

Bu örnek, fonksiyon bildirimi ve blok biçimini gösterir. Programın çalıştırılabilir giriş noktası olma şartları bu syntax belgesinin değil, ilgili program/target sözleşmesinin konusudur.

**Durum:** Fonksiyon ve blok kaynak biçimleri dil iç yapısı'de temsil edilir. Giriş noktası seçimi burada tanımlanmaz.

## 1.3 Değişken bildirimi ve ifade

```nxf
v:number = 42;
v:other!i32 = 7;
```

- `v:` değişken bildiriminin önekidir.
- `=` başlatıcı değerini bağlar.
- `!i32` açık tip belirtimini gösterir.
- Bildirim iç temsil'de değişken bildirimi olarak temsil edilir.

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

Aktif iç temsil/derleyici envanterinde koşul, döngü, return, break ve continue yapıları bulunur. Bunların kesin yazım biçimleri Bölüm 13–17'de verilir. Bu giriş bölümü ek bir kontrol yapısı veya yeni anahtar sözcük tanımlamaz.

---

# Bölüm 2 — Kaynak kodu ve biçim kuralları

## 2.1 Kaynak dosyası

Nexus Flow kaynak metni sözcüksel çözümleme'a girdi olarak verilir ve dil tarafından program/blok/declaration/statement biçimlerine ayrıştırılır.

Dosya uzantısı, kodlama standardı ve satır sonu normalizasyonu için mevcut kaynak denetiminde kesin bir normatif sözleşme doğrulanmadığından bu sürümde sabit bir değer ilan edilmez.

**Durum:** Kısmen tanımlı.

## 2.2 Boşluk ve satır sonları

Boşlukların ve satır sonlarının sözcüksel öğe sınırlarına etkisi sözcüksel çözümleme davranışına bağlıdır. Bu manuel, sözcüksel çözümleme kaynak kodu ve testleriyle doğrulanmayan biçimsel bir boşluk/indentation kuralı varsaymaz.

Girintileme için ayrı bir blok kapsamı anlamı tanımlanmamıştır; blokların iç temsil karşılığı süslü parantezli yapıdır.

**Durum:** Kısmen tanımlı; tam whitespace/indentation matrisi sözcüksel çözümleme denetimiyle tamamlanacak.

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

**Durum:** Tanımlı.

## 2.4 İfade sonlandırma

Mevcut kaynak örneklerinde bildirim ve birçok ifade `;` ile sonlandırılır. Ancak tüm statement türleri için noktalı virgül zorunluluğunu tek bir genel kurala dönüştürmeden önce derleyici üretimleri ve negatif örnek'lar birlikte denetlenmelidir.

**Geçici kural:** Bir bölüm belirli bir statement için açıkça farklı bir sonlandırma biçimi vermiyorsa, o statement'ın mevcut derleyici örneklerinde kullanılan biçimi izlenmelidir.

**Durum:** Kısmen tanımlı; tüm statement türleri için zorunluluk matrisi hazırlanacak.

## 2.5 Yorumlar

Eski syntax referansında `#` ile başlayan açıklama satırları kullanılmıştır. Bu biçimin aktif sözcüksel çözümleme tarafından hangi satır/blok sınırlarında kabul edildiği kaynak ve testlerle ayrıca doğrulanmadan çok satırlı yorum veya iç içe yorum kuralı ilan edilmez.

**Durum:** Kısmen tanımlı. Bu bölümde yalnızca kaynakta kullanılan `#` örnekleri kayıt altındadır.

## 2.6 Hatalı blok ve ayraç kullanımı

Aşağıdaki durumlar yapısal olarak hatalıdır:

```nxf
f:broken() {
    v:x = 1;
# Eksik kapanış ayracı
```

Beklenen: Açılan her blok için eşleşen bir kapanış ayracı bulunmalıdır.

Daha karmaşık hata kurtarma davranışı dilin tanı üretimi konusudur; dilin geçerli sözdizimi kuralı değildir.

---

# Bölüm 3 — Tokenlar ve sözcüksel öğeler

## 3.1 Token sınıfları

Mevcut sözcüksel öğe listesi'sinde şu genel gruplar bulunur:

- Tanımlayıcı ve literal sözcüksel öğeları.
- Anahtar sözcük sözcüksel öğeları.
- Bildirim önekleri.
- Tip sözcüksel öğeları.
- Aritmetik, karşılaştırma, mantık ve bit operatörleri.
- Nexus Flow akış sözcüksel öğeları.
- Noktalama ve blok ayraçları.
- Inline ASM ham gövde sözcüksel öğeı.

Registry, sembolik sözcüksel öğe envanteridir; liste'deki her sözcüksel öğeın her kaynak bağlamında aktif olarak üretildiği anlamına gelmez. Bu manuel yalnızca dil tarafından tüketildiği ve iç temsil'de temsil edildiği doğrulanan biçimleri normatif olarak açıklar.

## 3.2 Tanımlayıcılar

Tanımlayıcılar değişken, fonksiyon, tip, alan, Group, method ve modül adları gibi isim konumlarında kullanılır.

Örnekler:

```nxf
v:counter = 0;
f:calculate() {}
struct: Point { v:x!i32, v:y!i32 }
```

Büyük/küçük harf duyarlılığı, izin verilen Unicode karakterleri, alt çizgi ve rakam konumları için sözcüksel çözümleme kaynak/test denetimi tamamlanmadan daha ayrıntılı karakter grameri ilan edilmez.

**Durum:** Tanımlayıcı sözcüksel öğeı ve isim konumları mevcut; tam karakter grameri kısmen tanımlı.

## 3.3 Bildirim önekleri

Aktif iç temsil/derleyici eşlemesinde doğrulanması gereken temel bildirim aileleri:

| Yazım | Kaynak rolü | iç temsil temsili | Durum |
|---|---|---|---|
| `v:` | Değişken bildirimi | `iç temsil_VAR_DECL` | Aktif |
| `c:` | Sabit işaretli değişken bildirimi | `iç temsil_VAR_DECL` | dil iç yapısı eşlemesi mevcut; compile-time değerlendirme ayrı konudur |
| `m:` | Başlatıcılı, değiştirilemeyen değer bildirimi | `iç temsil_MUST_DECL` | dil iç yapısı mevcut |
| `nt:` | NewType bildirimi | `iç temsil_NEWTYPE_DECL` | Aktif |
| `f:` | Fonksiyon bildirimi | `iç temsil_FUNC_DECL` | Aktif |
| `struct:` | Struct bildirimi | `iç temsil_STRUCT_DECL` | Aktif |
| `enum:` | Enum bildirimi | `iç temsil_ENUM_DECL` | Aktif |
| `union:` | Union bildirimi | `iç temsil_UNION_DECL` | Aktif |
| `group:` | Group bildirimi | `iç temsil_GROUP_DECL` | Aktif |
| `exf:` | Harici fonksiyon bildirimi | `iç temsil_EXTERN_DECL` | Aktif |

`r:`, `t:` ve `o:` önekleri mevcut syntax kaynağında ayrılmış olarak gösterilir. Aktif iç temsil/derleyici dil özelliği olarak belgelenmez.

## 3.4 Literal sözcüksel öğeları

iç temsil literal düğümü ve dil iç yapısı karşılaştırma kaydında aşağıdaki literal sınıfları yer alır:

- Sayısal literal.
- String literal.
- Character literal.
- `true` ve `false`.
- `null`.

Sayısal biçimlerin tabanı, ayraçları ve üs yazımı sözcüksel çözümleme denetimiyle kesinleştirilecektir. Bu sürümde doğrulanmamış bir literal biçimi eklenmez.

**Durum:** Literal sınıfları dil iç yapısı'de temsil edilir; alt biçimlerin tam grameri kısmen tanımlı.

## 3.5 Tip sözcüksel öğeları ve tip ifadeleri

Registry'de temel sayısal/tip sözcüksel öğeları bulunur; ayrıca iç temsil'de recursive type expression düğümü tip biçimlerini taşır. Bu manuelde tiplerin kaynak yazımı Bölüm 6–9'da ele alınır.

Pointer, reference, array ve function-type biçimleri yalnızca mevcut dilin tanıdığı type-expression bağlamlarında açıklanır. Registry'de bulunmayan tip aileleri bu belgeye eklenmez.

## 3.6 Operatör ve noktalama sözcüksel öğeları

Operatörler ve ayraçlar Bölüm 10–12'de kaynak yazımlarıyla açıklanır. Özellikle Nexus Flow akış sözcüksel öğeları ile normal aritmetik/bit operatörleri birbirine karıştırılmamalıdır; aynı karakter ailesinin farklı sözcüksel öğe kimlikleri olabilir.

## 3.7 sözcüksel çözümleme tarafından reddedilen kaynak

sözcüksel çözümleme'ın tanımadığı karakter dizileri geçerli sözcüksel öğe değildir. Hatalı literal, tanınmayan karakter ve eksik operatör biçimleri için hata örnekleri, sözcüksel çözümleme test envanteri doğrulandıkça bu bölüme eklenecektir.

**Açık denetim:** Token liste 164 sembolik sözcüksel öğe türü içerir; tam lexer emission audit henüz tamamlanmış değildir. Bu nedenle liste'deki her öğe aktif kaynak sözdizimi olarak kabul edilmez.

---

# Part II — Bildirimler ve tipler

# 4. Tanımlayıcılar ve isim konumları

Tanımlayıcılar değişken, fonksiyon, tip, alan, enum seçeneği ve modül/Group isimlerini belirtir. sözcüksel çözümleme normal isimleri `TOKEN_IDENTIFIER` olarak üretir; bazı önekli isimler tek sözcüksel öğe olarak okunur.

```nxf
v:counter = 0;
f:calculate() {}
struct: Point { v:x!i32, v:y!i32 }
```

`v:counter` / `v: counter` ve `struct:Point` / `struct: Point` gibi bitişik ve ayrık önek biçimleri dil tarafından desteklenir.

| İsim konumu | iç temsil temsili |
|---|---|
| `v:name` / `c:name` | `iç temsil_VAR_DECL.name`; `c:` için `is_const` |
| `m:name` | `iç temsil_MUST_DECL.name` |
| `f:name` | `iç temsil_FUNC_DECL.name` |
| `struct: Name` / `union: Name` | `iç temsil_STRUCT_DECL` / `iç temsil_UNION_DECL` |
| `enum: Name` | `iç temsil_ENUM_DECL.name` |
| `nt:Name` | `iç temsil_NEWTYPE_DECL.name` |
| Struct alanı | `iç temsil_VAR_DECL.name` |
| Enum seçeneği | `iç temsil_ENUM_VARIANT.name` |

Tam identifier karakter grameri (Unicode, harf duyarlılığı, ilk karakter, alt çizgi) bu denetimde sözcüksel çözümleme testleriyle bütünüyle doğrulanmadığından kesin EBNF ilan edilmez. `r:`, `t:` ve `o:` sözcüksel öğeları tanınsa da bu manuelde aktif tamamlanmış bildirim biçimleri olarak sunulmaz.

```nxf
v: = 10;       # İsim eksik.
f:() {}        # Fonksiyon adı eksik.
struct: { }    # Struct adı eksik.
nt:UserId;     # Hedef tip eksik.
```

**Durum:** İsim konumları Tanımlı; karakter grameri kısmen tanımlı.

---

# 5. Değer bildirimleri

## 5.1 Bildirim aileleri

| Önek | Rol | iç temsil | Kural |
|---|---|---|---|
| `v:` | Değişken | `iç temsil_VAR_DECL` | Başlatıcı isteğe bağlı; `!Type` ve array suffix kullanılabilir. |
| `c:` | Sabit işaretli değişken | `iç temsil_VAR_DECL` | `v:` ile aynı gramer; `is_const` işaretlenir. |
| `m:` | Immutable değer | `iç temsil_MUST_DECL` | `= expression` zorunludur. |

`c:` ile `m:` aynı değildir. `c:` değişken bildirim düğümünün sabit işaretli biçimidir; `m:` ayrı iç temsil düğümü oluşturur. Compile-time değerlendirme ve değiştirilemezliğin tüm Semantic kuralları syntax tanımından ayrıdır.

## 5.2 `v:` ve `c:` bildirimleri

```ebnf
variable-declaration = ("v:" | "c:") identifier { "[" [expression] "]" } ["!" type] ["=" expression] ";" ;
```

```nxf
v:count = 10;
v:message!str = "hello";
v:buffer[256]!u8;
c:maximum = 100;
c:version!u16 = 3;
c:table[4]!u32 = [16, 32, 64, 128];
v: separated!i32 = 7;
```

İsim zorunludur. `!Type` isteğe bağlıdır. Başlatıcı isteğe bağlıdır. Array boyutları isimden sonra, açık tipten önce yazılır. `c:` dil tarafından `iç temsil_VAR_DECL.is_const` işaretli oluşturulur.

```nxf
v: = 1;          # İsim eksik.
v:value! = 1;    # Tip adı eksik.
v:items[4 = 1;   # ] eksik.
v:value = ;      # Başlatıcı expression eksik.
```

## 5.3 `m:` immutable değer bildirimi

```ebnf
immutable-declaration = "m:" identifier "=" expression ";" ;
```

```nxf
m:limit = 64;
m:welcome = "Welcome";
m:ratio = 1.25;
```

İsim, `=` ve başlatıcı expression zorunludur. Bu dil kuralınde `!Type` veya array suffix tanımlı değildir.

```nxf
m:limit;       # = ve initializer eksik.
m:limit = ;    # Expression eksik.
m: = 10;       # İsim eksik.
```

## 5.4 Alan bildirimi ve kapsam

dilin Struct/Union gövde üretimi yalnızca `v:` alan bildirimi tüketir; `c:` ve `m:` alanları bu üretimde yoktur. Normal bildirimler statement bağlamında, örneğin fonksiyon/blok gövdesinde kullanılabilir.

**Durum:** `v:`, `c:` ve `m:` kaynak biçimleri Tanımlı.

---

# 6. Tip yazımı

## 6.1 İsimlendirilmiş ve açık tipler

Tip konumunda sözcüksel çözümleme'ın tanıdığı tip sözcüksel öğeı veya dilin kabul ettiği identifier kullanılabilir. `!` açık tip işaretidir:

```nxf
v:count!i32 = 10;
v:label!str = "Nexus";
v:entity!Entity;
```

`!` tip adının parçası değildir. `v:`/`c:` bildirimlerinde tip atlanırsa başlatıcı expression saklanır; tip çıkarımı Semantic konusudur.

## 6.2 Pointer ve reference tipleri

derleyici tip adından önce en fazla iki `*` önekini veya `&` / `&mut` önekini tanır:

```nxf
v:ptr!*i32;
v:ptrptr!**u8;
v:shared!&i32;
v:mutable!&mut i32;
```

Daha derin pointer zincirleri bu üretimde doğrulanmış değildir. Pointer/reference oluşturma, dereference, borrow ve yaşam süresi kuralları Bölüm 23–25'tedir.

## 6.3 Array declarator

Array boyutları tip adının sonuna değil, değişken adına eklenir:

```nxf
v:dynamic[]!u8;
v:fixed[64]!u8;
v:matrix[4][8]!f32;
```

Her `[...]` içinde boş veya bir expression bulunabilir. Birden fazla suffix sırasıyla ayrı boyutlar oluşturur; iç temsil karşılığı `iç temsil_ARRAY_TYPE` / `iç temsil_ARRAY_DIMENSION` düğümleridir. `v:bytes!u8[16]` kanonik değildir; doğru sıralama `v:bytes[16]!u8` şeklindedir.

## 6.4 Function type ve doğrulanmamış biçimler

derleyici, tip konumunda `f:` ile başlayan function-type imzası için özel üretime sahiptir; tüm parametre/dönüş varyantları bağımsız örnek matrisiyle doğrulanmadığından ayrıntılı EBNF kısmen tanımlıdır. Dotted/qualified tip adları, generic parametre listeleri ve `map key, value` birleşik biçimi bu denetimde kesin derleyici grameri olarak doğrulanmamıştır; normatif gramer olarak sunulmaz.

```nxf
v:value! = 1;   # Tip adı eksik.
v:ptr!*;        # Pointer hedef tipi eksik.
v:ref!&;        # Reference hedef tipi eksik.
v:array!u8[8];  # Array suffix yanlış konumda.
```

**Durum:** İsimlendirilmiş, pointer, reference ve array declarator Tanımlı; function type kısmen tanımlı.

---

# 7. İlkel tipler ve literal'lar

sözcüksel çözümleme/derleyici kaynaklarında görülen tip aileleri:

- İşaretli tamsayı: `i8`, `i16`, `i32`, `i64`.
- İşaretsiz tamsayı: `u8`, `u16`, `u32`, `u64`.
- Kayan nokta: `f32`, `f64`.
- Geniş hassasiyet: `d32`, `d64`.
- Mantıksal/karakter/metin: `bool`, `char`, `str`.
- Temsil odaklı: `bit`, `byte`, `hex` ve sözcüksel çözümleme'da tanınan bit/byte genişlik tipleri.

Bu liste tip konumunda tanınan adları özetler; tüm işlemlerin, ABI yerleşiminin veya runtime davranışlarının tamamlandığı anlamına gelmez.

```nxf
v:enabled = true;
v:disabled = false;
v:empty = null;
v:positive = 42;
v:negative = -42;
v:ratio = 3.5;
v:greeting = "hello";
v:marker = 'A';
v:flag!bit;
v:raw!byte;
v:mask!8bit;
v:wide_mask!128bit;
```

`true`, `false` ve `null` özel literal sözcüksel öğelarıdır. Sayısal literal `TOKEN_NUMBER`; string ve character literal'ları ayrı sözcüksel öğe türleri olarak `iç temsil_LITERAL` düğümüne aktarılır. Negatif işaret ayrı unary operatördür.

Ondalık/üs/hex/binary biçimler, digit separator, escape dizileri, çok satırlı string ve Unicode kodlama ayrıntıları için tam sözcüksel çözümleme matrisi henüz normatif olarak tamamlanmamıştır. `bit`, `byte`, `hex` ve genişlik türlerinin fiziksel yerleşimi/işlem kuralları yalnızca syntax'tan çıkarılamaz.

**Durum:** Literal sınıfları Tanımlı; alt literal gramerleri kısmen tanımlı.

---

# 8. Array ve tuple

## 8.1 Array literal ve declarator

```nxf
v:values = [1, 2, 3];
v:matrix = [[1, 2], [3, 4]];
v:empty = [];
v:items[10]!i32;
v:dynamic_items[]!str;
v:grid[3][3]!f32;
```

Array literal `iç temsil_ARRAY_LITERAL` üretir ve iç içe literal desteklenir. Array declarator ise bildirimde isimden sonra gelen boyut suffix'leridir; boyut içeriği boş veya expression olabilir. İkisi farklı yapılardır.

```ebnf
array-literal = "[" [ expression { "," expression } ] "]" ;
array-suffix = { "[" [expression] "]" } ;
```

## 8.2 İndeks erişimi

```nxf
v:first = values[0];
v:cell = matrix[1][2];
```

İndeks erişimi postfix expression biçimidir. İndeks türü ve sınır kontrolü Semantic/runtime kurallarıdır.

## 8.3 Tuple expression

```nxf
v:point = (10, 20);
v:record = (1, "ready", true);
```

derleyici `iç temsil_TUPLE` üretir; dil kullanım testlerinde `(1,2)` biçimi doğrulanmıştır. Tek öğeli tuple ayrımı ve tuple type declarator bu sürümde kesin gramer olarak ilan edilmez.

```nxf
v:bad = [1, 2;  # ] eksik.
v:bad = (1, 2;  # ) eksik.
v:bad[4!i32;    # Array boyutunun ] ayracı eksik.
```

**Durum:** Array literal, declarator, indeks erişimi ve tuple expression Tanımlı; tuple type ve tüm trailing-comma varyantları kısmen tanımlı.

---

# 9. Kullanıcı tanımlı tipler

## 9.1 Struct ve Union

```nxf
struct: Point {
    v:x!i32,
    v:y!i32
}

union: Register32 {
    v:full!u32,
    v:low!u16
}
```

Struct `iç temsil_STRUCT_DECL`, Union ayrı `iç temsil_UNION_DECL` üretir. Her iki gövdede de alanlar `v:` ile başlar. derleyici virgül tüketebilir; `;` zorunlu alan ayıracı değildir.

```nxf
struct: Invalid {
    x!i32
}
```

Çıplak alan adı geçersizdir; derleyici `v:` alan önekini bekler.

## 9.2 Enum ve payload

```nxf
enum: ProcessState {
    Idle,
    Running(pid!u32),
    Failed(code!i32, message!str)
}
```

derleyici `iç temsil_ENUM_DECL` ve `iç temsil_ENUM_VARIANT` üretir. Variant payload alanları isim ve zorunlu `!Type` ile yazılır; alanlar `iç temsil_VAR_DECL` olarak saklanır.

```nxf
enum: Invalid {
    Running(pid)
}
```

Payload alanında `!Type` eksiktir.

## 9.3 Enum discriminant

```nxf
enum: StatusCode {
    Ok = 0,
    NotFound = 404,
    Failed = 500
}
```

derleyici `=` sonrasındaki expression'ı `discriminant_expr` alanına aktarır. Integer constant expression olma şartı Semantic doğrulamasıdır. `=` sonrasında expression bulunmaması derleyici hatasıdır.

## 9.4 NewType ve aggregate varyantı

```nxf
nt:UserId = u64;
nt:FileHandle = i32;
```

Temel NewType biçimi `nt:Name = Type;` olup `iç temsil_NEWTYPE_DECL` üretir. derleyici ayrıca `nt:` sözcüksel öğeı içinde `struct:`, `enum:` veya `union:` devamı gördüğünde ilgili aggregate parser yoluna yönlendirir; tüm birleşik yazım varyantları için örnek matrisi tamamlanmadığından bu özel biçimler kısmen tanımlıdır.

## 9.5 Struct literal

```nxf
v:point = Point {
    x = 10,
    y = 20
};
```

derleyici `iç temsil_STRUCT_LITERAL` ve alan başlatmaları için `iç temsil_STRUCT_FIELD_INIT` üretir. Literal alan başlatıcısı `=` kullanır; declaration alanı ise `v:field!Type` biçimindedir.

```nxf
v:point = Point { x: 10 };
```

Geçersizdir: Struct literal alan başlatıcısı `:` değil `=` bekler.

| Yapı | iç temsil | Durum |
|---|---|---|
| Struct / alan | `iç temsil_STRUCT_DECL` / `iç temsil_VAR_DECL` | Tanımlı |
| Union | `iç temsil_UNION_DECL` | Tanımlı |
| Enum / variant | `iç temsil_ENUM_DECL` / `iç temsil_ENUM_VARIANT` | Tanımlı |
| Enum payload | `iç temsil_VAR_DECL` | Tanımlı |
| Enum discriminant | `discriminant_expr` | derleyici expression'ı saklar; sabit tamsayı kuralı Semantic |
| NewType | `iç temsil_NEWTYPE_DECL` | Tanımlı |
| Struct literal | `iç temsil_STRUCT_LITERAL` | Tanımlı |
| `nt:` aggregate varyantları | İlgili aggregate iç temsil | Kısmen tanımlı |

# Part III — İfadeler ve operatörler

# 10. İfadeler

## 10.1 Amaç ve iç temsil temsili

Expression; literal, isim, çağrı, erişim, operatör veya bunların birleşiminden oluşur. derleyici temel biçimleri `iç temsil_LITERAL`, `iç temsil_IDENTIFIER`, `iç temsil_BINARY_EXPR`, `iç temsil_UNARY_EXPR`, `iç temsil_FUNC_CALL` ve özel expression düğümleriyle temsil eder.

## 10.2 Primary expression

dilin tanıdığı temel biçimler:

| Biçim | Örnek | iç temsil |
|---|---|---|
| Sayısal/string/char literal | `42`, `"text"`, `'A'` | `iç temsil_LITERAL` |
| Boolean/null | `true`, `false`, `null` | `iç temsil_LITERAL` |
| İsim | `value` | `iç temsil_IDENTIFIER` |
| Parantezli expression | `(a + b)` | İç expression |
| Tuple | `(a, b)` | `iç temsil_TUPLE` |
| Array literal | `[a, b]` | `iç temsil_ARRAY_LITERAL` |
| Struct literal | `Point { x = 1 }` | `iç temsil_STRUCT_LITERAL` |
| Lambda | `f:(x) > x` veya block-body biçimi | `iç temsil_LAMBDA` |
| Loop expression | `loop { ... }` | `iç temsil_LOOP` |

Parantez içinde tek expression varsa gruplama yapılır; virgülle ayrılmış öğeler varsa tuple oluşturulur. Boş tuple için derleyici'da `{}` yolu da vardır; kullanım sınırları ayrıca netleştirilecektir.

## 10.3 Unary ve binary expression

```nxf
v:a = !ready;
v:b = -value;
v:c = ~mask;
v:d = *pointer;
v:r = &value;
v:mr = &mut value;

v:sum = a + b;
v:equal = a == b;
v:valid = ready && connected;
v:mask = flags & mask;
v:power = 2 ^^ 8;
```

Unary operatörler `!`, `-`, `~`, `*`, `&` ve `&mut` biçimindedir ve `iç temsil_UNARY_EXPR` üretir. Binary ifadeler `iç temsil_BINARY_EXPR` üretir. Operandların tip uyumu Semantic kuralıdır.

## 10.4 Postfix expression zinciri

derleyici şu postfix biçimlerini destekler:
- Çağrı: `call(args)`
- Üye erişimi: `object.member`
- İndeks erişimi: `array[index]`
- Postfix artırma/azaltma: `value++` / `value--`

```nxf
v:result = service.calculate(input);
v:first = values[0];
v:width = image.width;
counter++;
counter--;
```

Postfix zinciri birden çok erişim/çağrı içerebilir; derleyici zincir derinliğini 32 adımla sınırlar. Prefix artırma/azaltma biçimleri `++value` ve `--value` mevcut dil kullanım testlerinde reddedilir.

## 10.5 Fonksiyon çağrısı

```ebnf
call = expression "(" [ argument { "," argument } ] ")" ;
argument = expression | identifier ":" expression ;
```

```nxf
v:result = add(10, 20);
v:width = image.get_width();
```

Normal çağrı postfix expression olarak `iç temsil_FUNC_CALL` üretir. Intrinsic anahtar sözcükler kendi özel iç temsil düğümlerine yönlendirilebilir.

## 10.6 Üye erişimi

```nxf
v:x = point.x;
v:width = engine.renderer.width;
```

Nokta sonrasında bir identifier zorunludur. Üye erişimi `iç temsil_MEMBER_ACCESS` üretir; üyenin gerçekten var olup olmadığı Semantic kontrolüdür.

```nxf
v:x = point.;
```

Nokta sonrasında üye adı eksiktir.

## 10.7 İndeks erişimi

```nxf
v:first = values[0];
v:cell = matrix[row][column];
```

Her indeks suffix'i `iç temsil_INDEX_ACCESS` üretir ve bir expression içermelidir. Kapanış `]` zorunludur.

```nxf
v:x = values[];
v:y = values[0;
```

İlk örnekte indeks expression'ı, ikincide kapanış ayracı eksiktir.

## 10.8 Array ve tuple expression

Array literal `[expression, ...]`; tuple expression `(expression, expression, ...)` biçimindedir. Array/tuple yazım kuralları Bölüm 8'de açıklanır.

## 10.9 Struct literal

```nxf
v:point = Point {
    x = 10,
    y = 20
};
```

Struct literal alanları `identifier = expression` biçimindedir. derleyici `iç temsil_STRUCT_LITERAL` ve `iç temsil_STRUCT_FIELD_INIT` üretir. Bu biçim Struct declaration içindeki `v:name!Type` alan yazımından farklıdır.

## 10.10 Cast expression

```nxf
v:value = cast(source_value, i32);
v:small = cast(counter, u8);
```

derleyici `cast` sonrasında `(expression, Type)` bekler. Hedef tip identifier veya tip sözcüksel öğeı olmalıdır. iç temsil karşılığı `iç temsil_Ciç temsil` olup kaynak expression ve hedef tip ayrı saklanır.

```nxf
v:bad = cast(value);
v:bad = cast(value,);
```

İlk örnekte virgül/hedef tip, ikincide hedef tip eksiktir.

## 10.11 Named argument

```nxf
configure(width: 800, height: 600);
open(path, mode: read);
```

Named argüman `name: expression` biçimindedir. Konumsal argümanlar named argümanlardan önce gelebilir; named argümandan sonra positional argüman gelemez. Aynı çağrıda aynı named label iki kez kullanılamaz. derleyici bu kuralları normal ve intrinsic çağrılarda uygular; düğüm `iç temsil_NAMED_ARG` biçimindedir.

```nxf
configure(width: 800, 600);        # Named argümandan sonra positional argüman.
configure(width: 800, width: 900); # Aynı label iki kez.
```

## 10.12 Gruplama ve legacy sınırı

Parantez tek expression'ı gruplar; virgül içeren biçim tuple oluşturur. Legacy unpack operatörü `...>` için negatif derleyici testi vardır ve bu biçim desteklenmez.

**Durum:** Temel expression, call, member/index, tuple/array, struct literal, cast ve named argument biçimleri Tanımlı.

---

# 11. Operatörler

## 11.1 Öncelik ve birleşme yönü

Aşağıdaki sıra derleyici expression zincirinden çıkarılmıştır; üstteki operatör daha yüksek önceliklidir.

| Öncelik | Operatörler | Açıklama |
|---:|---|---|
| 1 | Çağrı `()`, üye `.`, indeks `[]`, postfix `++ --` | Postfix |
| 2 | `^^` | Üs alma; sağ birleşimli |
| 3 | Unary `! - ~ * & &mut` | Unary |
| 4 | `* / %` | Çarpma/bölme/kalan |
| 5 | `+ -` | Toplama/çıkarma |
| 6 | `<<< >>>` | Sayısal bit kaydırma |
| 7 | `&` | Bitwise AND |
| 8 | `^` | Bitwise XOR |
| 9 | `|` | Bitwise OR |
| 10 | `< <= > >=` | İlişkisel |
| 11 | `== != === ==!` | Eşitlik |
| 12 | `..` | Range |
| 13 | `&&` | Mantıksal AND |
| 14 | `||` | Mantıksal OR |
| 15 | `-> << >> ?-> ?=> !-> _> ?(…)` | Nexus Flow |
| 16 | `<- = += -= *= /= &= |= ^= .=` | Capture/assignment |

`<<` ve `>>` sayısal shift değildir; Nexus Flow pipe/zone operatörleridir. Sayısal kaydırma `<<<` ve `>>>` ile yapılır.

## 11.2 Aritmetik ve üs alma

```nxf
v:add = a + b;
v:sub = a - b;
v:mul = a * b;
v:div = a / b;
v:rem = a % b;
v:pow = 2 ^^ 8;
```

`^^` sağ birleşimlidir. `2 ^^ 3 ^^ 2` sağdaki üs alma önce gruplanacak şekilde parse edilir. Unary eksi üs alma işleminden daha düşük önceliklidir.

## 11.3 Karşılaştırma ve eşitlik

```nxf
v:a = x < y;
v:b = x <= y;
v:c = x > y;
v:d = x >= y;
v:e = x == y;
v:f = x != y;
v:g = x === y;
v:h = x ==! y;
```

sözcüksel çözümleme/derleyici `==`, `!=`, `===` ve `==!` sözcüksel öğelarını ayrı tutar. Bu operatörlerin farklı Semantic anlamları bu syntax belgesinde yeniden tanımlanmaz.

## 11.4 Mantıksal ve bitwise operatörler

```nxf
v:ready = connected && authenticated;
v:available = local || remote;
v:and_mask = flags & mask;
v:xor_mask = flags ^ mask;
v:or_mask = flags | mask;
v:inverted = ~flags;
```

`&&` / `||` mantıksal AND/OR; `&` / `^` / `|` bitwise AND/XOR/OR; `~` unary bitwise NOT'tur. `&` unary konumda borrow/reference operatörü olarak da kullanılır.

## 11.5 Sayısal shift ve Nexus Flow ayrımı

```nxf
v:left = value <<< amount;
v:right = value >>> amount;
v:streamed = source << destination;
v:fed = source >> destination;
```

- `<<<` → `TOKEN_BIT_SHL`, sayısal bitwise left shift.
- `>>>` → `TOKEN_BIT_SHR`, sayısal bitwise right shift.
- `<<` / `>>` → `TOKEN_LSHIFT` / `TOKEN_RSHIFT`, Nexus Flow pipe/zone operatörleri.

Bu ayrım dil kullanım testlerinde iç temsil operatör kimlikleriyle doğrulanmıştır.

## 11.6 Atama ve bileşik atama

```nxf
value = next;
value += amount;
value -= amount;
value *= factor;
value /= divisor;
flags &= mask;
flags |= mask;
flags ^= mask;
text .= suffix;
```

Bu dil kuralınde desteklenen bileşik atamalar `+=`, `-=`, `*=`, `/=`, `&=`, `|=`, `^=` ve `.=` biçimleridir. `%=` desteklenen listede yoktur. `!!=` ayrıca directive/force-assignment bağlamında ele alınır; directive kullanımı Bölüm 34'tedir.

## 11.7 Artırma ve azaltma

```nxf
counter++;
counter--;
```

Postfix biçimler doğrulanmıştır. Prefix `++counter` ve `--counter` dil kullanım testlerinde reddedilir.

## 11.8 Unary pointer/borrow ve range

`*value` dereference, `&value` shared borrow/reference, `&mut value` mutable borrow biçimidir. Ayrıntılar Bölüm 23–25'tedir.

Range ifadesi `start..end` biçimindedir ve unparenthesized expression içinde tek bir `..` kabul edilir:

```nxf
v:valid = 1..10;
v:invalid = 1..10..20;  # derleyici reddeder.
```

Aritmetik, karşılaştırma, eşitlik, bitwise ve mantıksal zincirler derleyici testlerinde sol birleşimli; üs alma sağ birleşimli iç temsil olarak doğrulanmıştır.

**Durum:** Öncelik/associativity ve shift ayrımı mevcut dil kullanım testleriyle doğrulanmıştır. Operatörlerin tip bazında geçerliliği Semantic aşamasına aittir.

---

# 12. Yerleşik ifade biçimleri

## 12.1 Genel kural

Aşağıdaki adlar sözcüksel çözümleme/dil tarafından özel intrinsic sözcüksel öğeı olarak tanınır ve ilgili iç temsil düğümüne yönlendirilir. Bu bölüm kaynak çağrı biçimini tanımlar; parametre sayısı, tür uyumu, donanım yetkisi ve runtime davranışı ayrıca değerlendirilir.

## 12.2 Tip ve boyut sorguları

```nxf
v:type = typeof(value);
v:size = sizeof(value);
v:length = len(items);
```

`typeof` → `iç temsil_TYPEOF`, `sizeof` → `iç temsil_SIZEOF`, `len` → `iç temsil_LEN`.

## 12.3 Durum sorguları

```nxf
v:a = is_ok(result);
v:b = is_err(result);
v:c = is_some(option);
v:d = is_none(option);
v:e = is_null(pointer);
```

Bu çağrılar `iç temsil_IS_OK`, `iç temsil_IS_ERR`, `iç temsil_IS_SOME`, `iç temsil_IS_NONE` ve `iç temsil_IS_NULL` üretir.

## 12.4 Cast ve swap

```nxf
v:converted = cast(value, i32);
swap(left, right);
```

`cast(value, Type)` → `iç temsil_Ciç temsil`; `swap(...)` → `iç temsil_SWAP`.

## 12.5 Echo, print ve write ailesi

```nxf
echo("hello");
prnt(value);
prmt(value);
write(value);
fwrite(value);
```

derleyici eşlemesi: `echo` → `iç temsil_ECHO`; `prnt` → `iç temsil_PRNT`; `prmt` → `iç temsil_PRMT`; `write` → `iç temsil_WRITE`; `fwrite` → `iç temsil_FWRITE`. OS bağımlılığı ve çıktı davranışı syntax tanımının kapsamı dışındadır.

## 12.6 Adres, bellek ve port I/O

```nxf
v:address = addr(value);
v:read_value = peek(address);
poke(address, value);
v:port_value = inb(port);
outb(port, value);
```

`addr` → `iç temsil_ADDR`; `peek` / `poke` → `iç temsil_PEEK` / `iç temsil_POKE`; `inb` / `outb` → `iç temsil_INB` / `iç temsil_OUTB`.

## 12.7 IRQ, interrupt, register, area ve zone

```nxf
irq(1);
intr(1);
reg.r(0);
v:a = area(1);
v:z = zone(1);
```

Bunlar `iç temsil_IRQ`, `iç temsil_INTR`, `iç temsil_REG`, `iç temsil_AREA` ve `iç temsil_ZONE` özel çağrı düğümlerini üretir. `reg` için derleyici `reg.member(...)` biçiminde bir üye adı bekler.

## 12.8 Tarih, saat ve kaynak bırakma

```nxf
v:today = date();
v:now = datenow();
v:time_value = time();
v:clock_value = clock();
free(resource);
```

`date` → `iç temsil_DATE`; `datenow` → `iç temsil_DATENOW`; `time` → `iç temsil_TIME`; `clock` → `iç temsil_CLOCK`; `free` → `iç temsil_FREE`.

## 12.9 Panic ve exit

```nxf
panic("fatal error");
exit(0);
```

`panic("message")` `iç temsil_PANIC` üretir ve derleyici mesaj olarak string literal bekler. `exit(...)` `iç temsil_EXIT` üretir. Sonlandırma davranışı bu bölümde tanımlanmaz.

## 12.10 Thread API çağrıları ve normal çağrılardan ayrım

```nxf
spawn(task);
listen(channel);
send(message);
receive(channel);
done();
```

derleyici bu adları özel intrinsic iç temsil düğümü olarak değil normal identifier/call biçimine yönlendirir. Bunlar thread API çağrılarıdır; özel intrinsic ailesiyle karıştırılmamalıdır.

## 12.11 Desteklenmeyen eski expression biçimleri

Legacy unpack yazımı `...>` için negatif derleyici testi vardır; bu biçim desteklenmez. `unpack` bu sürümde yerleşik expression olarak belgelenmez.

**Durum:** Bu bölümde listelenen intrinsic aileleri dil iç yapısı testinde kaynak biçimi ve iç temsil eşlemesiyle doğrulanmıştır. Semantic parametre/tip sözleşmeleri ve runtime davranışları syntax manuelinin kapsamı dışındadır.

# Part IV — Bloklar ve kontrol akışı

# 13. Bloklar ve kapsam

## 13.1 Normal block

Blok süslü parantezlerle açılır ve kapanır. dil içindeki statement'ları sırayla iç temsil_BLOCK altında saklar; iç içe bloklar desteklenir.

    f:main() {
        v:value = 10;
        if (value > 0) {
            echo(value);
        }
    }

Blokların isim görünürlüğü, shadowing ve yaşam süresi Semantic kapsamındadır; burada yalnızca kaynak sınırları tanımlanır.

## 13.2 Statement sonlandırma

| Statement | Sonlandırma |
|---|---|
| v: / c: bildirimi | ; zorunlu |
| m: bildirimi | ; zorunlu |
| ret | ; zorunlu |
| break / continue | ; zorunlu |
| Expression statement | ; zorunlu |
| Function/Struct/Enum/Union/Group gövdesi | Kapanış } |
| If/loop/select/scan gövdesi | Kendi block/statement sınırı |
| Boş statement | Tek başına ; kabul edilir |

Satır sonu genel bir statement terminator değildir.

## 13.3 Named thread block

derleyici (name)>{...} biçimini özel thread block olarak tanır:

    (worker)>{
        process();
    }

Bu biçim iç temsil_THREAD_BLOCK üretir ve thread adını saklar. Parantez içinde tek identifier ve hemen ardından >{ sözcüksel öğeı beklenir. Thread yürütme ve iletişim davranışı syntax kapsamı dışındadır.

## 13.4 Fiç temsilEXEC block

    fastexec {
        asm: ENTRY {
            nop
        }
    }

fastexec bir block açar ve iç temsil_Fiç temsilEXEC_BLOCK üretir. Inline ASM declaration yalnızca bu block kapsamı içinde kabul edilir. Fiç temsilEXEC ve ASM kaynak ayrıntıları Bölüm 34'tedir.

## 13.5 Block halt eki

Normal veya Fiç temsilEXEC block kapanışından hemen sonra }?; yazılabilir:

    f:main() {
        prepare();
    }?;

derleyici kapanış brace sonrasında HALT sözcüksel öğeını görürse block statement listesine iç temsil_BLOCK_HALT ekler. Bu biçim normal statement içindeki ?; ile aynı iç temsil düğümü değildir.

## 13.6 Hatalı block

    f:main() {
        if (ready) {
            echo("ready");
    # Kapanış brace eksik.

derleyici eksik kapanış brace konumunu raporlamalıdır.

Durum: Normal block, thread block, Fiç temsilEXEC block ve block-halt kaynak biçimleri dil iç yapısı tarafından temsil edilir.

---

# 14. Koşullu akış

## 14.1 If / elsif / els

    if (score >= 90) {
        echo("A");
    }
    elsif (score >= 70) {
        echo("B");
    }
    els {
        echo("C");
    }

if ve elsif sonrasında parantezli koşul zorunludur. Gövde block veya tek statement olabilir. els koşulsuz alternatif branch'tir. derleyici iç temsil_IF düğümlerini else-if zinciri olarak bağlar.

## 14.2 Tek statement branch

    if (ready) start();
    elsif (waiting) wait();
    els stop();

Tek statement'ın kendi terminator kuralı korunur; örneğin çağrı statement'ı ; ile biter.

## 14.3 Conditional-control shorthand

    (condition) ? break;
    (condition) ? continue;
    (condition) ? ret 1;
    (condition) ? process(value);
    (condition) ? { process_a(); } : { process_b(); }

Bu bir ternary value expression değil, iç temsil_IF üreten statement biçimidir. Alternatif branch : ile ayrılır ve block veya statement olabilir. derleyici lookahead ile bu biçimi rolling retry suffix'inden ayırır.

    (a == b) -> break;

Bu conditional-control biçimi değildir; -> Nexus Flow pipe sözcüksel öğeıdır. Koşullu action için ? kullanılır.

## 14.4 Hatalı koşul biçimleri

    if ready { start(); }          # Koşul parantezleri eksik.
    if (ready)                     # Branch eksik.
    if (ready) {} elsif (false)    # elsif branch eksik.
    if (ready) {} els {} els {}    # Yinelenen els.
    (condition) ? ;                # Conditional action eksik.

Durum: If/elsif/els ve conditional-control biçimleri dil iç yapısı testleriyle doğrulanmıştır.

---

# 15. Döngüler

## 15.1 Sonsuz loop

    loop {
        process();
    }

Bu canonical sonsuz loop biçimidir ve iç temsil_LOOP içinde loop_type 0 olarak temsil edilir.

## 15.2 Koşullu loop

    loop(keep_running) {
        process();
    }

Koşul expression'ı loop parantezleri içinde bulunur. Gövde öncesinde isteğe bağlı -> sözcüksel öğeı tüketilebilir. Koşulun runtime değerlendirme sıklığı syntax konusu değildir.

## 15.3 Üç parçalı loop

    loop(init, condition, increment) {
        process();
    }

İlk ifade init, ikinci condition, üçüncü increment alanına aktarılır. Ayırıcı virgüller zorunludur; C tarzı noktalı virgül kullanan for-loop yazımı bu gramer değildir.

## 15.4 Collection loop

    loop(item in items) {
        process(item);
    }

İlk ifade sonrasında contextual identifier in gelirse collection loop biçimi seçilir. İlk kısım loop variable, ikinci expression collection olarak saklanır. derleyici v: veya c: ile başlayan bildirim biçimini de ilk bölümde kabul eder.

## 15.5 Unroll

    unroll(3) -> loop(item in items) {
        process(item);
    }

unroll contextual identifier olarak tanınır. Count sonrasında -> zorunludur ve sağ taraf iç temsil_LOOP olmalıdır. Başka expression veya normal block hedefi kabul edilmez; iç temsil_UNROLL üretilir.

    unroll(3) -> process();   # Hedef loop değil.
    unroll(3) loop { }        # -> eksik.

## 15.6 Loop kontrolü

    loop {
        if (finished) break;
        if (skip_item) continue;
        process();
    }

break; ve continue; ayrı iç temsil_BREAK / iç temsil_CONTINUE statement'larıdır. Hangi loop'a etki ettikleri ve loop dışında geçerlilikleri Semantic kontrolüdür.

## 15.7 Loop sınırları

derleyici sonsuz, koşullu, üç parçalı ve collection loop biçimlerini ayırt eder. Bütün varyantların sınır kombinasyonları için test matrisi tamamlanmadığından tüm loop grameri kısmen tanımlı işaretlenir; sonsuz loop ve unroll örnekleri açık testlerle doğrulanmıştır.

Durum: dil iç yapısı üretimleri mevcut; tüm loop varyantlarının test kapsamı kısmen tanımlı.

---

# 16. Select ve Scan

## 16.1 Ortak yapı

select ve scan sözcüksel çözümleme keyword'ü değildir; derleyici belirli identifier + parantez bağlamında bunları contextual sözcük olarak tanır.

    select-expression = "select" "(" expression ")" "{" arms "}" ;
    scan-expression   = "scan" "(" expression ")" "{" arms "}" ;

Her arm pattern sonrasında -> ister. Arm gövdesi block veya tek statement olabilir. Arm'lar virgül veya noktalı virgülle ayrılabilir.

## 16.2 Select

    select(state) {
        Ready -> start();
        Running when active -> process();
        default -> recover();
    }

Pattern literal, identifier binding veya constructor pattern olabilir. default en fazla bir kez kullanılabilir ve when guard taşıyamaz. derleyici iç temsil_SELECT / iç temsil_SELECT_ARM üretir.

## 16.3 Scan

    scan(value) {
        Range(start, end) when start < end && end > 0 -> {
            process(start, end);
        }
        item -> process(item);
        else -> recover();
    }

scan subject üzerinde pattern arm'ları oluşturur. else fallback arm'ıdır; en fazla bir kez bulunabilir ve son arm olmak zorundadır. else üzerinde when guard kullanılamaz. derleyici iç temsil_SCAN / iç temsil_SCAN_ARM üretir.

## 16.4 Pattern biçimleri

| Pattern | Örnek | iç temsil |
|---|---|---|
| Identifier binding | item | iç temsil_PATTERN_BINDING |
| Constructor + binding list | Range(start, end) | iç temsil_PATTERN_CONSTRUCTOR |
| Literal | 0, "none", true, null | Literal iç temsil |

Constructor pattern en az bir binding içermelidir; Range() reddedilir.

## 16.5 Guard

    scan(value) {
        item when item > 0 -> process(item);
        else -> recover();
    }

Normal pattern arm'ına when expression guard eklenebilir. default ve else fallback arm'larında guard yasaktır.

## 16.6 Hatalı select/scan örnekleri

    select(x) {
        default when true -> {}
    }

default guard taşıyamaz.

    select(x) {
        default -> {}
        default -> {}
    }

Select içinde ikinci default yasaktır.

    scan(x) {
        Range() -> {}
    }

Constructor pattern boş binding listesi içeremez.

    scan(x) {
        else -> {}
        item -> {}
    }

Scan else arm'ı son sırada olmalıdır. derleyici ayrıca ikinci scan else, eksik -> ve kapanış/arm ayraç hatalarını raporlar.

Durum: Select/Scan, pattern, constructor binding, guard ve fallback kuralları dil iç yapısı testleriyle doğrulanmıştır.

---

# 17. Return, break, continue ve sonlandırma

## 17.1 Return

    ret;
    ret value;
    ret calculate();

ret tek başına veya bir expression ile kullanılabilir; her iki biçimde de ; zorunludur. derleyici iç temsil_RETURN üretir ve operandı isteğe bağlı saklar.

## 17.2 Break ve continue

    break;
    continue;

Her ikisi de ; ile biter ve iç temsil_BREAK / iç temsil_CONTINUE üretir. Geçerli loop kapsamı Semantic kontrolüdür.

## 17.3 Panic

    panic("fatal error");
    !! "critical failure";

panic("message") intrinsic biçimidir. !! statement biçiminde string mesaj isteğe bağlıdır; mesaj yoksa derleyici varsayılan metin atar ve ; zorunludur. Her iki yol da iç temsil_PANIC ile ilişkilidir.

## 17.4 Exit

    exit(0);

exit(...) özel intrinsic çağrısıdır ve iç temsil_EXIT üretir. Parametre sözleşmesi ve sürecin sonlandırılması runtime davranışıdır.

## 17.5 Nexus Halt

    ?;

?; tek HALT sözcüksel öğeıdır ve normal statement bağlamında iç temsil_NEXUS_HALT üretir. Hedefsiz kullanımda iç temsil operandı boştur. Bu biçim block kapanışındaki }?; ekiyle aynı değildir; o biçim iç temsil_BLOCK_HALT oluşturur (Bölüm 13.5).

## 17.6 Defer

    defer cleanup();
    defer {
        release(resource);
    }

defer sonrasında derleyici bir block veya statement okur ve iç temsil_DEFER üretir. Defer body'nin ne zaman çalıştırılacağı runtime/semantic davranışıdır; burada kaynak biçimi tanımlanır.

## 17.7 Sonlandırma sınırları

ret, break, continue, panic, exit ve ?; farklı iç temsil/derleyici yollarına sahiptir; birbirinin eş anlamlısı değildir. Label/jump biçimleri Bölüm 29'da ayrıca tanımlanır.

Durum: Return, break/continue, panic, exit, halt ve defer kaynak biçimleri dil iç yapısı tarafından temsil edilir. Kontrol akışının geçerliliği ve çalışma zamanı etkisi Semantic/runtime kapsamındadır.

# Part V — Fonksiyonlar, Group ve çağrılar

## 18. Fonksiyon bildirimi
### 18.1 `f:name(...)` biçimi
### 18.2 Parametre bildirimi
### 18.3 Parametre tipleri
### 18.4 Dönüş tipi
### 18.5 Fonksiyon gövdesi
### 18.6 Default parameter biçimi
### 18.7 Function type / callback tip yazımı
### 18.8 Variadic parametre: iç temsil/derleyici doğrulama durumu

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
### 27.7 iç temsil/derleyici sınırında olmayan masa fikirleri kapsam dışı

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

### 33.6 `no: osmodule;` — otomatik OS modülü yüklemesini kapatma

`no:` genel bir modül kapatma mekanizması değildir. derleyici yalnızca program seviyesinde tek bir `no: osmodule;` bildirimini kabul eder. Bu bildirim varsayılan olarak otomatik yüklenen `osmodule`'ün yüklenmesini engeller; OS servisi bulunmayan bare-metal programlar için kullanılır.

```nxf
no: osmodule;

f:main() {
    # Bare-metal başlangıç kodu
}
```

Kurallar:
- Program kökünde bulunmalıdır; fonksiyon veya iç içe blokta kullanılamaz.
- Yalnızca `osmodule` hedefi desteklenir; `no: coremodule;` geçersizdir.
- Bir programda en fazla bir kez yazılabilir.
- `;` zorunludur.
- Ayrı statement düğümü üretmez; `iç temsilProgram.disable_osmodule` politika alanı ve kaynak aralığı kaydedilir.
- Otomatik modül yükleme entegrasyonu bu syntax kuralından ayrıdır.

```nxf
no: coremodule;       # Desteklenmeyen hedef.
no: osmodule          # ; eksik.
no: osmodule;
no: osmodule;         # Yinelenen bildirim.

f:main() {
    no: osmodule;     # Yanlış kapsam.
}
```

derleyici bu dört hatayı desteklenmeyen hedef, eksik sonlandırıcı, yinelenen bildirim ve yanlış kapsam olarak ayırır.

**Durum:** Tanımlı. derleyici testi program-kökü biçimini ve negatif durumları kontrol eder.

---

## 34. Inline assembly kaynak biçimi
### 34.1 `fastexec` block
### 34.2 `asm: LABEL { ... }`
### 34.3 Ham ASM gövdesi
### 34.4 ASM label
### 34.5 `asmcall` ve `asmjmp` çağrı biçimleri
### 34.6 derleyici kapsam sınırları

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

## 36. sözcüksel çözümleme kaynak hataları
### 36.1 Tanınmayan karakter
### 36.2 Hatalı literal
### 36.3 Eksik operatör
### 36.4 Token sınırı hataları

## 37. derleyici kaynak hataları
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
### 42.5 Fiç temsilEXEC ve ASM
### 42.6 Donanım intrinsic çağrıları

---

# Ekler

## A. Dil yapıları ve kullanım özeti
## B. Sözcüksel öğeler özeti
## C. Anahtar sözcük ve bildirim önekleri
## D. Operatör tablosu
## E. Tip yazımı tablosu
## F. Literal biçimleri tablosu
## G. Nexus Flow operatörleri tablosu
## H. Pointer/reference yazım tablosu
## I. dil tarafından desteklenmeyen/legacy biçimler
## J. Özellik olgunluk matrisi
## K. Değişiklik günlüğü
## L. Terimler sözlüğü
## M. Eski referanslar ve kaynak belgeler

---

# Bölüm yazım standardı

Her özellik bölümü aşağıdaki yapıyla tamamlanır. Bir başlık o özellik için geçerli değilse “Uygulanmaz” denir; tasarım aşamasındaki kurallar geçerliymiş gibi yazılmaz.

1. **Amaç:** Kaynak yapısının ne ifade ettiği.
2. **Sözdizimi:** Kabul edilen kanonik yazım.
3. **Gramer:** Mevcut dil kuralınden çıkarılabilen biçim; kesin değilse açıkça belirtilir.
4. **Kurallar:** Kabul edilen ve reddedilen kaynak biçimleri.
5. **Kapsam:** Yapının hangi kaynak bağlamında kullanılabildiği.
6. **Örnekler:** En az bir geçerli örnek.
7. **Geçersiz örnekler:** Hatalı biçim ve reddedilme nedeni.
8. **Tanı:** Hata konumu ve açıklanabilir hata nedeni.
9. **Sınırlar:** dil iç yapısı ile henüz temsil edilmeyen durumlar.
10. **Olgunluk:** Tanımlı / derleyici-iç temsil doğrulandı / Kısmen tanımlı / Tasarım aşamasında / Desteklenmiyor / Legacy.
11. **İlgili bölümler:** Bağlantılı kaynak kuralları.

---

# Kaynaklar ve değişiklikler

Bu kılavuzun hazırlanmasında önceki Nexus Flow dil notları ve güncel dil örnekleri karşılaştırmalı olarak kullanılır. Bu kaynaklar okuyucu için zorunlu değildir; kılavuz kendi başına kullanılabilecek şekilde yazılır.

---

## N. İleride eklenecek dil özellikleri

### N.1 Macro sözdizimi

Macro sistemi henüz Nexus Flow dil iç yapısı tarafından desteklenen bir kaynak özelliği değildir. Bu nedenle Bölüm 0–42 arasındaki normatif gramerin parçası değildir. Macro tasarımı ve uygulanması tamamlandıktan, iç temsil/derleyici karşılığı ve testleri doğrulandıktan sonra bu manuelin en sonunda ayrı bir bölüm olarak ele alınacaktır.
