# NEXUS FLOW — Dil Referans Kılavuzu

**Belge türü:** Programcılar için dil kullanım ve başvuru kılavuzu  
**Sürüm:** 0.3-draft  
**Dal:** refactor/c-core-roadmap

Bu belge Nexus Flow ile program yazan kişiler içindir. Dilin yazım biçimini, kurallarını, kapsamını ve kullanımını örneklerle açıklar. Derleyicinin iç işleyişini, veri yapılarını veya kod üretimini anlatmaz.

Bu kılavuzda yalnızca mevcut Nexus Flow dilinde kullanılabilen özellikler geçerli özellik olarak açıklanır. Henüz tasarlanmamış özellikler geçerli sözdizimi gibi sunulmaz. Eksik tanımlanmış konular açıkça işaretlenir.

---

# Bölüm 0 — Bu kılavuz nasıl kullanılır?

## 0.1 Amaç

Kılavuz, Nexus Flow öğrenen veya program yazarken dil kurallarına başvurmak isteyen kullanıcılar için hazırlanmıştır. Konular basitten karmaşığa doğru sıralanır.

## 0.2 Kapsam

Bu belge yalnızca Nexus Flow dilini kapsar: bildirimler, tipler, ifadeler, operatörler, bloklar, kontrol akışı, fonksiyonlar, pointer/reference, akış yapıları, modüller ve düşük seviyeli dil kullanımları. Derleyici mimarisi ve geliştirme ayrıntıları kapsam dışıdır.

## 0.3 Kural ifadeleri

- **Zorunludur:** Geçerli program bu kurala uymalıdır.
- **Yasaktır:** Bu kullanım geçerli değildir.
- **Önerilir:** Daha açık ve güvenilir kod için tercih edilmelidir.
- **İsteğe bağlıdır:** Kullanıcı ihtiyacına göre kullanılabilir.

## 0.4 Özellik durumu

- **Tanımlı:** Yazım ve kullanım kuralı bu kılavuzda açıklanmıştır.
- **Kısmen tanımlı:** Temel biçim bilinir, bazı ayrıntılar henüz kesinleşmemiştir.
- **Tasarım aşamasında:** Henüz kullanılabilir bir dil özelliği değildir.
- **Desteklenmiyor:** Bu sürümde geçerli kullanım değildir.
- **Eski kullanım:** Önceki sürümlerden kalmıştır; yeni kodda kullanılmamalıdır.

## 0.5 Örnekler

Kod blokları Nexus Flow kaynak kodudur. Hatalı örneklerin yanında hata nedeni belirtilir; mümkün olduğunda doğru kullanım da gösterilir. Bir yazım biçiminin doğru olması, her tip veya çalışma ortamında kullanılabileceği anlamına gelmez.

## 0.6 Eksik tanımlar

Henüz kesinleşmemiş ayrıntılar tahminle doldurulmaz. İlgili konu kısmen tanımlı olarak işaretlenir.

## 0.7 Gelecekteki özellikler

Yeni özellikler kullanılabilir hale geldiklerinde ilgili bölümlere eklenir. Macro sistemi henüz tasarlanmadığından bu kılavuzun en sonunda gelecekte eklenecek özellik olarak tutulur.

---

# Bölüm 1 — Nexus Flow'a giriş

## 1.1 Programın temel yapısı

Nexus Flow programları bildirimlerden, fonksiyonlardan, tip tanımlarından, ifadelerden ve bloklardan oluşur. Kod blokları süslü parantezlerle yazılır.

## 1.2 En küçük program

~~~nxf
f:main() {
}
~~~

Bu örnek parametresiz bir main fonksiyonu tanımlar. Programın başlangıç noktası olarak kullanılması program türüne ve hedef ortama ilişkin kurallara bağlıdır.

## 1.3 Değişken ve tip

~~~nxf
v:number = 42;
v:other!i32 = 7;
~~~

- v: bir değişken tanımlar.
- = başlangıç değerini verir.
- !i32 açık tip belirtir.
- Tipin açıkça belirtilmediği durumlar Bölüm 5–7'de açıklanır.

## 1.4 Fonksiyon tanımı ve çağrısı

~~~nxf
f:add(a!i32, b!i32)!i32 {
    ret a + b;
}

f:main() {
    v:result = add(10, 20);
}
~~~

Fonksiyonlar parametre alabilir, dönüş tipi belirtebilir ve başka fonksiyonlardan çağrılabilir.

## 1.5 Kontrol akışı

Koşullar if, elsif ve els; döngüler loop ile yazılır. Fonksiyonlardan ret, döngülerden break ve continue ile çıkılır.

---

# Bölüm 2 — Kaynak kodu ve biçim kuralları

## 2.1 Kaynak dosyası

Kaynak dosyasının uzantısı, karakter kodlaması ve satır sonu standardı bu sürümde kesin olarak belgelenmemiştir.

**Durum:** Kısmen tanımlı.

## 2.2 Boşluk ve satır sonları

Girintileme kodun okunmasını kolaylaştırır ancak blok sınırlarını belirlemez. Blokları süslü parantezler belirler. Boşluk ve satır sonlarının bütün sözcüksel öğelere etkisi henüz eksiksiz belgelenmemiştir.

**Durum:** Kısmen tanımlı.

## 2.3 Blok ayraçları

~~~nxf
f:work() {
    v:value = 1;
}
~~~

- { bir blok başlatır.
- } bloğu bitirir.
- Bloklar iç içe kullanılabilir.

## 2.4 Noktalı virgül

Değişken ve değer bildirimleri, ret, break, continue ve expression statement'ları noktalı virgülle sonlandırılır. Fonksiyon, koşul ve döngü bloklarının kapanışından sonra genel olarak ayrıca noktalı virgül yazılmaz. Özel blok sonlandırma biçimleri ilgili bölümlerde açıklanır.

## 2.5 Yorumlar

# ile başlayan satır yorumları kullanılır:

~~~nxf
v:count = 10; # başlangıç değeri
~~~

Çok satırlı yorum ve yorumların iç içe kullanımı henüz kesin olarak tanımlanmamıştır.

## 2.6 Eksik blok

~~~nxf
f:broken() {
    v:x = 1;
# kapanış } eksik
~~~

Her açılan blok kapatılmalıdır.

**Durum:** Blok ve temel sonlandırma biçimleri tanımlı; dosya biçimi ve tüm boşluk/yorum ayrıntıları kısmen tanımlı.

---

# Bölüm 3 — Sözcüksel öğeler

## 3.1 İsimler ve özel sözcükler

Nexus Flow'da değişken, fonksiyon, tip, alan ve modül adları kullanılır. Bazı sözcükler belirli dil yapılarında özel anlam taşır. İsimlerin izin verilen karakterleri ve büyük/küçük harf duyarlılığı henüz tam olarak belgelenmemiştir.

## 3.2 Bildirim önekleri

| Yazım | Kullanım |
|---|---|
| v: | Değişken bildirimi |
| c: | Sabit işaretli değişken bildirimi |
| m: | Değiştirilemez değer bildirimi |
| nt: | Yeni tip bildirimi |
| f: | Fonksiyon bildirimi |
| struct: | Struct tipi |
| enum: | Enum tipi |
| union: | Union tipi |
| group: | Group bildirimi |
| exf: | Harici fonksiyon bildirimi |

## 3.3 Literal değerler

~~~nxf
v:count = 42;
v:ratio = 3.5;
v:title = "Nexus Flow";
v:letter = 'N';
v:enabled = true;
v:disabled = false;
v:empty = null;
~~~

Kaynak kodunda sayısal, metin, karakter, mantıksal ve null değerleri kullanılabilir. Sayı tabanları, kaçış dizileri ve literal biçimlerinin ayrıntıları Bölüm 7'de açıklanır.

## 3.4 Operatörler ve ayraçlar

Operatörler değerler üzerinde işlem yapar veya Nexus Flow akış yapılarını kurar. Nokta, virgül, iki nokta, noktalı virgül, parantezler ve süslü parantezler kaynak yapısını düzenler.

---

# Bölüm II — Bildirimler ve tipler

# 4. İsimler ve tanımlayıcılar

Tanımlayıcılar değişkenleri, fonksiyonları, tipleri, alanları, enum seçeneklerini ve modülleri adlandırır.

~~~nxf
v:counter = 0;
f:calculate() {}
struct: Point {
    v:x!i32,
    v:y!i32
}
~~~

Bildirim önekleri isimle bitişik veya aralarında boşluk olacak şekilde yazılabilir:

~~~nxf
v:counter = 0;
v: counter = 0;
struct:Point {}
struct: Point {}
~~~

İsim zorunludur. Boş isim kullanılamaz:

~~~nxf
v: = 10;       # Değişken adı eksik.
f:() {}        # Fonksiyon adı eksik.
struct: { }    # Tip adı eksik.
~~~

Tanımlayıcıların izin verilen karakter kümesi ve Unicode kuralları henüz tam olarak belgelenmemiştir.

**Durum:** Bildirimlerde isim kullanımı tanımlı; karakter kümesinin tüm ayrıntıları kısmen tanımlı.

---

# 5. Değer bildirimleri

## 5.1 Bildirim türleri

| Yazım | Kullanım |
|---|---|
| v:name | Değişken |
| c:name | Sabit işaretli değişken |
| m:name | Değiştirilemez değer |

c: ve m: aynı anlama gelmez. c: değişken bildirim biçiminin sabit işaretli halidir; m: ayrı bir değer bildirimidir.

## 5.2 Değişken bildirimi: v:

~~~nxf
v:count = 10;
v:message!str = "hello";
v:buffer[256]!u8;
v:table[4]!u32 = [16, 32, 64, 128];
~~~

Genel yazım:

~~~text
v:name[optional_dimensions]!optional_type = optional_expression;
~~~

- İsim zorunludur.
- Tip !Type ile açıkça belirtilebilir.
- Başlangıç değeri = ile verilir.
- Array boyutları isimden sonra, açık tipten önce yazılır.
- Başlangıç değeri verilmeden tanımlanan değişkenlerin kullanım koşulları tip ve yaşam süresi kurallarına bağlıdır.

Hatalı örnekler:

~~~nxf
v: = 1;          # İsim eksik.
v:value! = 1;    # Tip adı eksik.
v:items[4 = 1;   # ] eksik.
v:value = ;      # Başlangıç ifadesi eksik.
~~~

## 5.3 Sabit işaretli değişken: c:

~~~nxf
c:maximum = 100;
c:version!u16 = 3;
c:table[4]!u32 = [16, 32, 64, 128];
~~~

c: değişken bildirimiyle aynı temel yazım biçimini kullanır; bildirime sabit niteliği verir.

## 5.4 Değiştirilemez değer: m:

~~~nxf
m:limit = 64;
m:welcome = "Welcome";
m:ratio = 1.25;
~~~

Genel yazım:

~~~text
m:name = expression;
~~~

İsim ve başlangıç ifadesi zorunludur. Bu bildirim biçiminde açık tip ve array suffix kullanımı tanımlı değildir.

~~~nxf
m:limit;       # Başlangıç değeri eksik.
m:limit = ;    # İfade eksik.
m: = 10;       # İsim eksik.
~~~

## 5.5 Alan bildirimi

Struct ve Union alanları v: ile bildirilir. Alan bildirimleri Bölüm 9'da açıklanır.

**Durum:** v:, c: ve m: bildirimlerinin temel yazım biçimleri tanımlı.

---

# 6. Tip yazımı

## 6.1 Açık tip belirtimi

! işareti değişkenin tipini açıkça belirtir:

~~~nxf
v:count!i32 = 10;
v:label!str = "Nexus";
v:entity!Entity;
~~~

Tip adı ! işaretinden sonra yazılır. Tip belirtilmediğinde tipin nasıl çıkarılacağı kullanılan başlangıç değerine ve tip kurallarına bağlıdır.

## 6.2 Pointer ve reference tipleri

~~~nxf
v:ptr!*i32;
v:ptrptr!**u8;
v:shared!&i32;
v:mutable!&mut i32;
~~~

Pointer ve reference tipleri Bölüm 23–25'te ayrıntılı olarak açıklanır.

## 6.3 Array tip bildirimi

Array boyutları tip adından sonra değil, değişken adından sonra yazılır:

~~~nxf
v:dynamic[]!u8;
v:fixed[64]!u8;
v:matrix[4][8]!f32;
~~~

Her köşeli parantez bir boyut belirtir. Boyut boş bırakılabilir veya bir ifade içerebilir.

~~~nxf
v:bytes!u8[16];  # Boyut yanlış konumda.
~~~

Doğru biçim:

~~~nxf
v:bytes[16]!u8;
~~~

## 6.4 Fonksiyon tipleri

Fonksiyon tiplerinin f: ile başlayan yazımı bulunur. Parametre ve dönüş tipi varyantlarının tamamı henüz tek bir kesin gramer halinde belgelenmemiştir.

**Durum:** İsimlendirilmiş tip, pointer, reference ve array yazımı tanımlı; fonksiyon tipi ayrıntıları kısmen tanımlı.

---

# 7. İlkel tipler ve literal değerler

## 7.1 Sayısal tip aileleri

Mevcut tip adları arasında şunlar bulunur:

- İşaretli tamsayılar: i8, i16, i32, i64.
- İşaretsiz tamsayılar: u8, u16, u32, u64.
- Kayan nokta: f32, f64.
- Geniş hassasiyet: d32, d64.
- Mantıksal, karakter ve metin: bool, char, str.
- Temsil odaklı tipler: bit, byte, hex ve genişlik belirten bit/byte tipleri.

Tip adının mevcut olması, her işlem veya hedef ortamda aynı davranışın garanti edildiği anlamına gelmez. Geniş tiplerin ayrıntılı işlem ve yerleşim kuralları ayrıca tanımlanacaktır.

## 7.2 Literal örnekleri

~~~nxf
v:positive = 42;
v:negative = -42;
v:ratio = 3.5;
v:greeting = "hello";
v:marker = 'A';
v:enabled = true;
v:disabled = false;
v:empty = null;
v:flag!bit;
v:raw!byte;
v:mask!8bit;
v:wide_mask!128bit;
~~~

## 7.3 Henüz tam tanımlanmamış literal ayrıntıları

Sayı tabanları, üs gösterimi, basamak ayırıcıları, escape dizileri, çok satırlı string ve Unicode ayrıntıları bu sürümde tam olarak belgelenmemiştir.

**Durum:** Temel literal sınıfları tanımlı; alt yazım kuralları kısmen tanımlı.

---

# 8. Array ve tuple

## 8.1 Array oluşturma

~~~nxf
v:values = [1, 2, 3];
v:matrix = [[1, 2], [3, 4]];
v:empty = [];
~~~

Array literal köşeli parantez içinde virgülle ayrılmış ifadelerden oluşur. İç içe array kullanılabilir.

## 8.2 Array bildirimi

~~~nxf
v:items[10]!i32;
v:dynamic_items[]!str;
v:grid[3][3]!f32;
~~~

Boyut suffix'leri değişken adından sonra gelir. Birden fazla suffix çok boyutlu bildirim oluşturur.

## 8.3 İndeks erişimi

~~~nxf
v:first = values[0];
v:cell = matrix[1][2];
~~~

İndeks erişimi köşeli parantezle yapılır. İndeks ifadesi boş bırakılamaz ve her açılan köşeli parantez kapatılmalıdır.

## 8.4 Tuple

~~~nxf
v:point = (10, 20);
v:record = (1, "ready", true);
~~~

Tuple, parantez içinde virgülle ayrılmış öğelerden oluşur. Tek öğeli tuple ve tuple tip bildiriminin ayrıntıları kısmen tanımlıdır.

~~~nxf
v:bad = [1, 2;  # ] eksik.
v:bad = (1, 2;  # ) eksik.
~~~

**Durum:** Array literal, array bildirimi, indeks erişimi ve temel tuple kullanımı tanımlı.

---

# 9. Kullanıcı tanımlı tipler

## 9.1 Struct

~~~nxf
struct: Point {
    v:x!i32,
    v:y!i32
}
~~~

Struct, birden fazla isimlendirilmiş alanı tek bir tip altında toplar. Alanlar v: ile bildirilir.

Hatalı örnek:

~~~nxf
struct: Invalid {
    x!i32
}
~~~

Alan bildirimi v: önekiyle yapılmalıdır.

## 9.2 Union

~~~nxf
union: Register32 {
    v:full!u32,
    v:low!u16
}
~~~

Union, aynı bellek alanını paylaşan alternatif alanları tanımlar. Alanlar v: ile bildirilir. Bellek yerleşimi ve alan erişim kuralları ilgili tip kurallarına bağlıdır.

## 9.3 Enum

~~~nxf
enum: ProcessState {
    Idle,
    Running(pid!u32),
    Failed(code!i32, message!str)
}
~~~

Enum, isimlendirilmiş seçeneklerden oluşur. Seçenekler payload içerebilir; payload alanları isim ve tip ile yazılır.

~~~nxf
enum: Invalid {
    Running(pid)
}
~~~

Payload alanında tip belirtimi eksiktir.

## 9.4 Enum değerleri

~~~nxf
enum: StatusCode {
    Ok = 0,
    NotFound = 404,
    Failed = 500
}
~~~

Enum seçeneklerine açık değer atanabilir. Atanan değerin sabit ve uygun tipte olması gerekir.

## 9.5 NewType

~~~nxf
nt:UserId = u64;
nt:FileHandle = i32;
~~~

NewType mevcut bir temel tipten yeni bir isimlendirilmiş tip oluşturur. Aggregate tabanlı NewType biçimlerinin tüm varyantları henüz tam belgelenmemiştir.

## 9.6 Struct literal

~~~nxf
v:point = Point {
    x = 10,
    y = 20
};
~~~

Struct değeri oluşturulurken alanlar alan_adı = ifade biçiminde başlatılır. Struct tanımındaki alan bildirimiyle literal alan başlatımı birbirinden farklıdır.

**Durum:** Struct, Union, Enum, temel NewType ve Struct literal biçimleri tanımlı; bazı aggregate NewType varyantları kısmen tanımlı.

---

# Bölüm III — İfadeler ve operatörler

# 10. İfadeler

Bir ifade değer üretir veya bir işlem tanımlar. Değişken başlangıçlarında, fonksiyon argümanlarında, koşullarda ve başka ifadelerin içinde kullanılabilir.

## 10.1 Temel ifadeler

~~~nxf
v:number = 42;
v:name = "Nexus";
v:enabled = true;
v:empty = null;
v:result = calculate();
~~~

## 10.2 Aritmetik ve mantıksal ifadeler

~~~nxf
v:sum = a + b;
v:valid = ready && connected;
v:mask = flags & mask;
v:power = 2 ^^ 8;
~~~

## 10.3 Fonksiyon çağrısı

~~~nxf
v:result = add(10, 20);
v:width = image.get_width();
~~~

Fonksiyon çağrısında argümanlar parantez içinde virgülle ayrılır. Argümanlar konumsal veya isimli olabilir.

## 10.4 Üye erişimi

~~~nxf
v:x = point.x;
v:width = engine.renderer.width;
~~~

Nokta operatörü bir değerin üyesine erişir. Birden fazla nokta kullanılarak erişim zinciri kurulabilir.

~~~nxf
v:x = point.; # Üye adı eksik.
~~~

Nokta sonrasında üye adı yazılmalıdır.

## 10.5 İndeks erişimi

~~~nxf
v:first = values[0];
v:cell = matrix[row][column];
~~~

Köşeli parantezler array veya indekslenebilir bir değerin elemanına erişir. İndeks ifadesi boş bırakılamaz.

## 10.6 Tuple ve array ifadeleri

~~~nxf
v:point = (10, 20);
v:values = [1, 2, 3];
~~~

Tuple ve array oluşturma biçimleri Bölüm 8'de açıklanır.

## 10.7 Struct oluşturma

~~~nxf
v:point = Point {
    x = 10,
    y = 20
};
~~~

Struct literal alanları alan_adı = ifade biçiminde başlatılır.

## 10.8 Tür dönüştürme

~~~nxf
v:value = cast(source_value, i32);
v:small = cast(counter, u8);
~~~

cast(ifade, Tip) bir değeri belirtilen tipe dönüştürme isteğini ifade eder. Hedef tip yazılmalıdır.

~~~nxf
v:bad = cast(value);   # Hedef tip eksik.
v:bad = cast(value,);  # Hedef tip eksik.
~~~

## 10.9 İsimli argümanlar

~~~nxf
configure(width: 800, height: 600);
open(path, mode: read);
~~~

İsimli argüman isim: ifade biçimindedir. Konumsal argümanlar isimli argümanlardan önce yazılmalıdır. İsimli argümanlardan sonra yeni bir konumsal argüman eklenemez; aynı argüman adı bir çağrıda iki kez kullanılamaz.

~~~nxf
configure(width: 800, 600);        # İsimli argümandan sonra konumsal argüman.
configure(width: 800, width: 900); # Aynı argüman adı iki kez.
~~~

## 10.10 Gruplama

Parantezler bir ifadeyi gruplayabilir. Virgülle ayrılmış birden fazla öğe tuple oluşturur.

~~~nxf
v:value = (a + b) * c;
v:pair = (a, b);
~~~

Eski unpack yazımı ...> bu sürümde geçerli bir ifade biçimi değildir.

**Durum:** Temel ifade, çağrı, üye/indeks erişimi, tuple/array, struct literal, cast ve isimli argüman kullanımları tanımlı.

---

# 11. Operatörler

Operatörler değerler üzerinde işlem yapar. Nexus Flow'da aritmetik ve mantıksal işlemlerin yanında akış ve kaynak aktarım operatörleri de bulunur.

## 11.1 Operatör önceliği

Üst sıralar daha yüksek önceliğe sahiptir. Karmaşık ifadelerde parantez kullanılması önerilir.

| Öncelik | Operatörler | Kullanım |
|---:|---|---|
| 1 | (), ., [], postfix ++ -- | Çağrı ve erişim |
| 2 | ^^ | Üs alma |
| 3 | Unary ! - ~ * & &mut | Tek operandlı işlemler |
| 4 | * / % | Çarpma, bölme, kalan |
| 5 | + - | Toplama, çıkarma |
| 6 | <<< >>> | Sayısal bit kaydırma |
| 7 | & | Bitwise AND |
| 8 | ^ | Bitwise XOR |
| 9 | \| | Bitwise OR |
| 10 | < <= > >= | Karşılaştırma |
| 11 | == != === ==! | Eşitlik karşılaştırmaları |
| 12 | .. | Aralık |
| 13 | && | Mantıksal AND |
| 14 | \|\| | Mantıksal OR |
| 15 | -> << >> ?-> ?=> !-> _> ?(...) | Nexus Flow akış işlemleri |
| 16 | <- = += -= *= /= &= \|= ^= .= | Capture ve atama |

Üs alma ^^ sağdan birleşir. Diğer ikili işlem gruplarında soldan sağa değerlendirme kullanılır. Anlamın karışabileceği ifadelerde parantez tercih edilmelidir.

## 11.2 Aritmetik

~~~nxf
v:add = a + b;
v:sub = a - b;
v:mul = a * b;
v:div = a / b;
v:rem = a % b;
v:pow = 2 ^^ 8;
~~~

## 11.3 Karşılaştırma

~~~nxf
v:a = x < y;
v:b = x <= y;
v:c = x > y;
v:d = x >= y;
v:e = x == y;
v:f = x != y;
v:g = x === y;
v:h = x ==! y;
~~~

==, !=, === ve ==! ayrı karşılaştırma operatörleridir. Anlamları birbirinin yerine kullanılabilecek şekilde varsayılmamalıdır.

## 11.4 Mantıksal ve bit düzeyinde işlemler

~~~nxf
v:ready = connected && authenticated;
v:available = local || remote;
v:and_mask = flags & mask;
v:xor_mask = flags ^ mask;
v:or_mask = flags | mask;
v:inverted = ~flags;
~~~

- && ve || mantıksal işlemlerdir.
- &, ^ ve | bit düzeyinde işlemlerdir.
- ~ bit düzeyinde tersleme yapar.
- & ayrıca unary konumda reference/borrow oluşturmak için kullanılır.

## 11.5 Bit kaydırma ve akış operatörleri

~~~nxf
v:left = value <<< amount;
v:right = value >>> amount;

source << destination;
source >> destination;
~~~

<<< ve >>> sayısal bit kaydırma operatörleridir. << ve >> Nexus Flow akış/zone işlemlerinde kullanılır; birbirlerinin yerine yazılmazlar.

## 11.6 Atama

~~~nxf
value = next;
value += amount;
value -= amount;
value *= factor;
value /= divisor;
flags &= mask;
flags |= mask;
flags ^= mask;
text .= suffix;
~~~

Bileşik atama, işlem sonucunu sol taraftaki değere geri yazar. %= bu sürümde desteklenen bileşik atama biçimleri arasında değildir.

## 11.7 Artırma ve azaltma

~~~nxf
counter++;
counter--;
~~~

Artırma ve azaltma postfix biçimde yazılır. ++counter ve --counter biçimleri geçerli değildir.

## 11.8 Pointer, reference ve range operatörleri

*value pointer dereference; &value shared reference/borrow; &mut value mutable reference/borrow biçimidir. Ayrıntılar Bölüm 23–25'tedir.

Aralık ifadesi başlangıç..bitiş biçimindedir:

~~~nxf
v:valid = 1..10;
~~~

Bir aralık ifadesinde birden fazla .. operatörü kullanılması geçerli değildir.

**Durum:** Operatörlerin yazımı ve öncelik tablosu tanımlı. Tip bazındaki geçerlilik ilgili tip ve işlem kurallarına bağlıdır.

---

# 12. Yerleşik işlemler

Nexus Flow, sık kullanılan bazı işlemler için özel adlar sunar. Bu işlemler çağrı biçiminde yazılır.

## 12.1 Tip, boyut ve uzunluk sorguları

~~~nxf
v:type = typeof(value);
v:size = sizeof(value);
v:length = len(items);
~~~

- typeof(value) bir değerin tipini sorgular.
- sizeof(value) bir değerin veya tipin boyutunu sorgular.
- len(value) bir koleksiyonun veya metnin uzunluğunu sorgular.

## 12.2 Sonuç ve null sorguları

~~~nxf
v:a = is_ok(result);
v:b = is_err(result);
v:c = is_some(option);
v:d = is_none(option);
v:e = is_null(pointer);
~~~

Bu işlemler sonuç, option ve null durumlarını sınamak için kullanılır.

## 12.3 Dönüştürme ve değer değiştirme

~~~nxf
v:converted = cast(value, i32);
swap(left, right);
~~~

cast(value, Type) tip dönüşümü ister. swap(a, b) iki değerin yerini değiştirmek için kullanılır.

## 12.4 Çıktı işlemleri

~~~nxf
echo("hello");
prnt(value);
prmt(value);
write(value);
fwrite(value);
~~~

Bu adlar çıktı/yazma işlemlerini ifade eder. Kullanılabilirlikleri ve çıktı hedefleri çalışma ortamına bağlı olabilir.

## 12.5 Adres ve bellek işlemleri

~~~nxf
v:address = addr(value);
v:read_value = peek(address);
poke(address, value);
~~~

addr bir değerin adresini alır. peek adresteki değeri okur; poke adrese değer yazar. Geçerli adres, erişim genişliği ve bellek güvenliği ayrıca dikkate alınmalıdır.

## 12.6 Port, IRQ ve register işlemleri

~~~nxf
v:port_value = inb(port);
outb(port, value);
irq(1);
intr(1);
reg.r(0);
~~~

Bunlar düşük seviyeli sistem işlemleridir. Kullanılabilirlikleri hedef donanım ve çalışma ortamına bağlıdır.

## 12.7 Area ve zone

~~~nxf
v:a = area(1);
v:z = zone(1);
~~~

area ve zone Nexus Flow'un alan/zone kavramlarıyla birlikte kullanılır. Ayrıntılı akış kuralları Bölüm 26–27'de ele alınır.

## 12.8 Tarih ve zaman

~~~nxf
v:today = date();
v:now = datenow();
v:time_value = time();
v:clock_value = clock();
~~~

Bu çağrılar tarih, güncel zaman veya saat sayacı bilgisi almak için kullanılır. Birim ve platform davranışı ilgili çalışma ortamına bağlıdır.

## 12.9 Kaynak bırakma ve sonlandırma

~~~nxf
free(resource);
panic("fatal error");
exit(0);
~~~

free bir kaynağı serbest bırakma isteğini ifade eder. panic hata nedeniyle yürütmeyi durdurma; exit programdan çıkma amacıyla kullanılır.

## 12.10 Thread işlemleri

~~~nxf
spawn(task);
listen(channel);
send(message);
receive(channel);
done();
~~~

Bu adlar thread ve iletişim API'leri olarak kullanılır. İlgili thread kütüphanesi ve kullanım kuralları geçerli olmalıdır.

**Durum:** Çağrı biçimleri tanımlı. Parametrelerin ayrıntılı tip sözleşmeleri ve hedefe bağlı davranışlar ilgili API/çalışma ortamı belgeleriyle tamamlanacaktır.

---

# Bölüm IV — Bloklar ve kontrol akışı

# 13. Bloklar ve kapsam

## 13.1 Normal blok

Blok süslü parantezlerle açılır ve kapanır. Bloklar iç içe kullanılabilir.

~~~nxf
f:main() {
    v:value = 10;
    if (value > 0) {
        echo(value);
    }
}
~~~

Bir blok içinde tanımlanan isimlerin görünürlüğü ve yaşam süresi kapsam kurallarına bağlıdır.

## 13.2 Statement sonlandırma

Bildirimler, ret, break, continue ve expression statement'ları noktalı virgülle biter. Fonksiyon, struct, enum, union, group, if ve loop gövdeleri kendi kapanış ayraçlarıyla biter.

## 13.3 Named thread block

~~~nxf
(worker)>{
    process();
}
~~~

Parantez içindeki isim thread bloğunu adlandırır. Thread'in çalıştırılma ve iletişim kuralları ilgili thread özellikleriyle birlikte değerlendirilir.

## 13.4 FASTEXEC block

~~~nxf
fastexec {
    asm: ENTRY {
        nop
    }
}
~~~

fastexec düşük seviyeli hızlı yürütme/assembly içeriği için ayrılmış bir blok biçimidir. Ayrıntılar Bölüm 34'tedir.

## 13.5 Block halt eki

~~~nxf
f:main() {
    prepare();
}?;
~~~

}?; bir blok kapanışına eklenerek o blok için halt davranışı belirtir. Normal statement biçimindeki ?; ile aynı kullanım değildir.

## 13.6 Hatalı blok

~~~nxf
f:main() {
    if (ready) {
        echo("ready");
~~~

Açılan bloklar kapatılmalıdır. Eksik kapanış ayracı ilgili blok yapısını tamamlanmamış bırakır.

**Durum:** Normal blok, named thread block, FASTEXEC block ve block-halt yazımları tanımlı.

---

# 14. Koşullu akış

## 14.1 if, elsif ve els

~~~nxf
if (score >= 90) {
    echo("A");
}
elsif (score >= 70) {
    echo("B");
}
els {
    echo("C");
}
~~~

if ve elsif sonrasında parantezli koşul yazılır. els koşulsuz alternatif daldır. Her dal bir blok veya tek statement olabilir.

## 14.2 Tek statement dalı

~~~nxf
if (ready) start();
elsif (waiting) wait();
els stop();
~~~

Tek statement kullanıldığında statement'ın kendi sonlandırma kuralı geçerlidir.

## 14.3 Koşullu kontrol biçimi

~~~nxf
(condition) ? break;
(condition) ? continue;
(condition) ? ret 1;
(condition) ? process(value);
(condition) ? { process_a(); } : { process_b(); }
~~~

Koşullu kontrol biçiminde parantezli koşuldan sonra ? ve çalıştırılacak işlem yazılır. İsteğe bağlı alternatif dal : ile ayrılır. Bu kullanım değer seçen ternary ifade değildir; koşullu statement biçimidir.

~~~nxf
(a == b) -> break;
~~~

Bu örnek koşullu kontrol biçimi değildir. Koşullu kontrol için ? kullanılır.

## 14.4 Hatalı koşul biçimleri

~~~nxf
if ready { start(); }          # Koşul parantezleri eksik.
if (ready)                     # Dal eksik.
if (ready) {} elsif (false)    # elsif dalı eksik.
if (ready) {} els {} els {}    # Yinelenen els.
(condition) ? ;                # Koşullu işlem eksik.
~~~

**Durum:** if/elsif/els ve koşullu kontrol biçimleri tanımlı.

---

# 15. Döngüler

## 15.1 Sonsuz loop

~~~nxf
loop {
    process();
}
~~~

Koşul belirtilmeyen loop sürekli yinelenen döngüdür. Döngüden çıkmak için break veya uygun bir kontrol akışı kullanılmalıdır.

## 15.2 Koşullu loop

~~~nxf
loop(keep_running) {
    process();
}
~~~

Parantez içindeki ifade döngünün koşulunu belirtir.

## 15.3 Üç parçalı loop

~~~nxf
loop(init, condition, increment) {
    process();
}
~~~

Bu biçimde ilk ifade başlangıç, ikinci ifade koşul, üçüncü ifade artış/güncelleme bölümüdür. Bölümler virgülle ayrılır.

## 15.4 Collection loop

~~~nxf
loop(item in items) {
    process(item);
}
~~~

in biçimi bir koleksiyonun öğeleri üzerinde yineleme yapar. Döngü değişkeni her adımda ilgili öğeyi temsil eder.

## 15.5 Unroll

~~~nxf
unroll(3) -> loop(item in items) {
    process(item);
}
~~~

unroll parantez içindeki sayı kadar açılım/tekrar isteğini belirtir ve ardından loop biçimi gelmelidir.

~~~nxf
unroll(3) -> process();   # Hedef loop değil.
unroll(3) loop { }        # -> eksik.
~~~

## 15.6 break ve continue

~~~nxf
loop {
    if (finished) break;
    if (skip_item) continue;
    process();
}
~~~

break geçerli döngüden çıkar. continue mevcut yinelemenin kalanını atlayıp sonraki yinelemeye geçer.

## 15.7 Rolling retry

Rolling retry, bir işlemi belirli sayıda yeniden denemek için kullanılan akış biçimidir. Ayrıntılı yazım Bölüm 30'da açıklanır.

**Durum:** Sonsuz, koşullu, üç parçalı ve collection loop biçimleri tanımlı; tüm sınır durumları kısmen tanımlı.

---

# 16. Select ve Scan

## 16.1 Genel kullanım

select ve scan bir değere veya duruma göre birden fazla pattern kolundan uygun olanı seçmek için kullanılır. Her kolun pattern bölümünden sonra -> yazılır.

## 16.2 Select

~~~nxf
select(state) {
    Ready -> start();
    Running when active -> process();
    default -> recover();
}
~~~

select bir değeri olası seçeneklerle karşılaştırır. default hiçbir özel kol eşleşmediğinde kullanılan varsayılan daldır. default en fazla bir kez yazılabilir ve when koşulu taşımaz.

## 16.3 Scan

~~~nxf
scan(value) {
    Range(start, end) when start < end && end > 0 -> {
        process(start, end);
    }
    item -> process(item);
    else -> recover();
}
~~~

scan pattern eşleştirme ve koşullu guard kullanımına izin verir. else son ve en fazla bir kez kullanılabilen fallback dalıdır.

## 16.4 Pattern biçimleri

- İsim bağlama: item
- Constructor pattern: Range(start, end)
- Literal pattern: 0, "none", true, null

Constructor pattern içinde en az bir binding bulunmalıdır.

## 16.5 when guard

~~~nxf
scan(value) {
    item when item > 0 -> process(item);
    else -> recover();
}
~~~

when, pattern eşleşmesine ek bir koşul getirir. default ve else fallback dallarında when kullanılamaz.

## 16.6 Hatalı select/scan örnekleri

~~~nxf
select(x) {
    default when true -> {}
}
~~~

default guard taşıyamaz.

~~~nxf
select(x) {
    default -> {}
    default -> {}
}
~~~

Select içinde ikinci default yasaktır.

~~~nxf
scan(x) {
    Range() -> {}
}
~~~

Constructor pattern boş binding listesi içeremez.

~~~nxf
scan(x) {
    else -> {}
    item -> {}
}
~~~

Scan else dalı son sırada olmalıdır.

**Durum:** Select/Scan, pattern, guard ve fallback biçimleri tanımlı.

---

# 17. Return, break, continue ve sonlandırma

## 17.1 Return

~~~nxf
ret;
ret value;
ret calculate();
~~~

ret fonksiyondan çıkmak için kullanılır. İsteğe bağlı bir dönüş değeri taşıyabilir. Her iki biçimde de noktalı virgül zorunludur.

## 17.2 Break ve continue

~~~nxf
break;
continue;
~~~

break döngüyü bitirir. continue geçerli döngünün sonraki yinelemesine geçer. Bu ifadeler döngü bağlamında kullanılmalıdır.

## 17.3 Panic

~~~nxf
panic("fatal error");
!! "critical failure";
~~~

panic hata nedeniyle yürütmeyi durdurmak için kullanılır. !! biçimi mesajlı veya mesajsız panic statement biçimidir; noktalı virgülle sonlandırılır.

## 17.4 Exit

~~~nxf
exit(0);
~~~

exit programdan çıkmak için kullanılır. Parametrenin anlamı çalışma ortamına bağlıdır.

## 17.5 Nexus Halt

~~~nxf
?;
~~~

?; Nexus Flow halt biçimidir. Blok kapanışına eklenen }?; ile karıştırılmamalıdır; block-halt biçimi Bölüm 13.5'te açıklanır.

## 17.6 Defer

~~~nxf
defer cleanup();
defer {
    release(resource);
}
~~~

defer bir statement veya blok biçimindeki işlemi ertelenmiş işlem olarak tanımlar. İşlemin hangi koşulda ve hangi sırada yürütüleceği kaynak yaşam süresi kurallarıyla birlikte değerlendirilir.

**Durum:** ret, break, continue, panic, exit, halt ve defer yazımları tanımlı.

---

# Bölüm V — Fonksiyonlar, Group ve çağrılar

## 18. Fonksiyon bildirimi

Fonksiyonlar `f:` önekiyle tanımlanır. Fonksiyon adı, parametre listesi ve isteğe bağlı dönüş tipi imzasını oluşturur; gövde süslü parantez içinde yazılır.

## 18.1 Temel biçim

~~~nxf
f:add(a!i32, b!i32)!i32 {
    ret a + b;
}

f:log(message!str) {
    echo(message);
}
~~~

Genel biçim:

~~~text
f:name(parameter_list)!return_type {
    statements
}
~~~

- `f:` fonksiyon bildirimini başlatır.
- Parametreler parantez içinde virgülle ayrılır.
- Parametre tipi `!Type` ile açıkça yazılabilir.
- `!ReturnType` dönüş tipini belirtir.
- Dönüş tipi yazılmadığında fonksiyonun dönüş sözleşmesi ayrıca değerlendirilir; değer döndüren fonksiyonlarda dönüş tipini açık yazmak önerilir.

## 18.2 Parametre bildirimi

Parametre adı ve tipi şu biçimde yazılır:

~~~nxf
f:scale(value!f32, factor!f32)!f32 {
    ret value * factor;
}
~~~

Parametre adı zorunludur. Aynı parametre listesinde isimler benzersiz olmalıdır.

~~~nxf
f:invalid(value!i32, value!i32) {}
~~~

Bu örnekte `value` iki kez tanımlandığı için parametre adı çakışır.

## 18.3 Parametre tipleri

Parametreler ilkel tip, kullanıcı tanımlı tip, pointer veya reference tipi taşıyabilir:

~~~nxf
f:print_point(point!Point) {
    echo(point.x);
}

f:read_value(ptr!*i32)!i32 {
    ret *ptr;
}
~~~

Pointer ve reference parametrelerinin erişim, geçerlilik ve yaşam süresi kuralları Bölüm 23–25'te açıklanır. Parametreye tip yazmak, çağrı sırasında verilen argümanın bu tipe uygun olmasını gerektirir.

## 18.4 Dönüş tipi

Dönüş tipi fonksiyon imzasında parametre listesinden sonra `!Type` ile belirtilir:

~~~nxf
f:sum(a!i32, b!i32)!i32 {
    ret a + b;
}
~~~

Değer döndüren fonksiyon `ret expression;` kullanır. Değer döndürmeyen fonksiyon `ret;` ile sonlandırılabilir.

~~~nxf
f:reset() {
    clear_state();
    ret;
}
~~~

Dönüş tipi ile `ret` ifadesinin uyumlu olması gerekir. Her yürütme yolunda dönüş değeri gerektiren bir fonksiyonun uygun değer döndürmesi gerekir.

## 18.5 Fonksiyon gövdesi

Fonksiyon gövdesi normal blok kurallarını izler. Yerel bildirimler, koşullar, döngüler ve başka fonksiyon çağrıları kullanılabilir:

~~~nxf
f:calculate(input!i32)!i32 {
    v:result = input * 2;
    if (result > 100) {
        ret 100;
    }
    ret result;
}
~~~

Fonksiyon içindeki yerel isimler fonksiyonun ve iç blokların kapsam kurallarına tabidir.

## 18.6 Varsayılan parametreler

Varsayılan parametre değerlerinin tam yazım biçimi ve değerlendirme kuralları bu sürümde kesinleştirilmemiştir.

**Durum:** Kısmen tanımlı. Kesinleşmemiş bir default-parametre biçimi geçerli Nexus Flow sözdizimi olarak kullanılmamalıdır.

## 18.7 Fonksiyon tipi ve callback

Fonksiyon tipi/callback kullanımının tüm imza, atama ve dönüş varyantları henüz tek bir kesin sözleşme halinde belgelenmemiştir. Fonksiyon adıyla çağrı yapmak ile callable bir değeri çağırmak aynı şey varsayılmamalıdır.

**Durum:** Kısmen tanımlı.

## 18.8 Variadic parametreler

Variadic fonksiyon parametrelerinin yazımı ve argüman toplama kuralları henüz tanımlanmamıştır.

**Durum:** Tasarım ayrıntısı bekleniyor.

---

# 19. Fonksiyon çağrısı

Fonksiyon, adı ve ardından parantez içindeki argüman listesiyle çağrılır.

## 19.1 Konumsal argümanlar

~~~nxf
v:result = add(10, 20);
log("started");
~~~

Konumsal argümanlar parametre sırasına göre eşleşir. Argüman sayısı ve tipleri fonksiyon imzasıyla uyumlu olmalıdır.

## 19.2 İsimli argümanlar

~~~nxf
configure(width: 800, height: 600);
open(path, mode: read);
~~~

İsimli argüman `name: expression` biçimindedir. İsim, fonksiyonun parametre adıyla eşleşmelidir.

## 19.3 Argüman sırası

Konumsal argümanlar isimli argümanlardan önce yazılır. İsimli argüman başladıktan sonra konumsal argüman yazılmaz.

~~~nxf
configure(800, height: 600);       # Geçerli sıra
configure(width: 800, 600);         # Hatalı: isimli argümandan sonra konumsal
~~~

Aynı isimli parametre bir çağrıda iki kez verilemez:

~~~nxf
configure(width: 800, width: 1024); # Hatalı: width yineleniyor
~~~

## 19.4 İç içe çağrılar

~~~nxf
v:result = add(read_left(), read_right());
print(format(result));
~~~

Bir argüman başka bir fonksiyon çağrısı olabilir. Parantezlerin ve virgüllerin doğru eşleştirilmesi gerekir.

## 19.5 Argüman ayraçları

Argümanlar virgülle ayrılır. Son argümandan sonra fazladan virgül kullanımı bu sürümde geçerli biçim olarak belgelenmemiştir.

~~~nxf
add(1, 2);      # Doğru
add(1 2);       # Hatalı: virgül eksik
add(, 2);       # Hatalı: ilk argüman eksik
add(1,);        # Hatalı/henüz desteklenmiyor: boş son argüman
~~~

## 19.6 Çağrı hataları

~~~nxf
add(1);                 # Parametre eksik
add(1, 2, 3);           # Fazla argüman
add(a: 1);              # İsim imzada yoksa geçersiz
add(value: 1, value: 2);# Parametre adı yineleniyor
~~~

Hata mesajı mümkün olduğunda çağrı konumunu, beklenen parametreyi ve verilen argümanı belirtmelidir. İsimli argümanlarda parametre adıyla eşleşme kontrol edilir.

**Durum:** Konumsal/isimli çağrı ve argüman sırası tanımlı.

---

# 20. Lambda ve callable ifadeler

Lambda, bir fonksiyon gövdesini değer olarak ifade etmeye yarar. Nexus Flow'da lambda ile normal isimli fonksiyon bildirimi birbirinden farklıdır: lambda bir ifade bağlamında yazılır, `f:name(...)` ise isimli fonksiyon tanımlar.

## 20.1 Expression-body lambda

~~~nxf
v:double = (x!i32) => x * 2;
~~~

Expression-body biçiminde `=>` sonrasında tek bir ifade bulunur. Bu ifadenin sonucu lambda sonucudur.

## 20.2 Block-body lambda

~~~nxf
v:handler = (value!i32) => {
    v:result = value + 1;
    ret result;
};
~~~

Block-body lambda süslü parantezle gövde oluşturur. Gövde içinde statement'lar ve `ret` kullanılabilir.

## 20.3 Parametreler

~~~nxf
v:combine = (left!i32, right!i32) => left + right;
~~~

Lambda parametreleri normal fonksiyon parametreleri gibi isim ve tip taşıyabilir. Parametre isimleri benzersiz olmalıdır.

## 20.4 Lambda kullanımı

~~~nxf
v:transform = (value!i32) => value * 3;
v:result = transform(10);
~~~

Lambda bir değişkene bağlandıktan sonra callable olarak kullanılabilir olduğu durumda çağrı biçimi normal fonksiyon çağrısına benzer. Callable değerlerin tüm tip çıkarımı ve atama kuralları henüz eksiksiz belgelenmemiştir.

## 20.5 Fonksiyon ile lambda ayrımı

~~~nxf
f:triple(value!i32)!i32 {
    ret value * 3;
}

v:triple_value = (value!i32) => value * 3;
~~~

İlk biçim isimli fonksiyon tanımlar. İkinci biçim lambda ifadesi oluşturur. İsimli fonksiyonun gövdesi bildirim içinde; lambda gövdesi bir ifade içinde yer alır.

## 20.6 Capture

Lambda dış kapsamdan değer veya referans yakalama biçimlerinin kesin sözdizimi ve yaşam süresi kuralları henüz tamamlanmamıştır. Capture listesi veya örtük capture biçimi tahmin edilerek kullanılmamalıdır.

**Durum:** Temel lambda yazımı tanımlı; callable tip uyumu ve capture ayrıntıları kısmen tanımlı.

---

# 21. Group ve method

Group, alanları ve bu alanlar üzerinde çalışan method'ları bir arada tanımlamak için kullanılır. Group içindeki method'lar receiver üzerinden group verisine erişebilir.

## 21.1 Group bildirimi

~~~nxf
group:Counter {
    v:value!i32;

    f:increment() {
        self.value += 1;
    }

    f:get()!i32 {
        ret self.value;
    }
}
~~~

Group adı zorunludur. Alanlar `v:` ile, method'lar `f:` ile tanımlanır. Group gövdesi süslü parantez içinde yazılır.

## 21.2 Group alanları

~~~nxf
group:Point {
    v:x!f32;
    v:y!f32;
}
~~~

Group alanları isim ve tip bildirimi taşır. Alan adları aynı group içinde benzersiz olmalıdır. Alanların başlangıç değerleri ve otomatik oluşturma kuralları, kullanılan group oluşturma biçimine bağlıdır.

## 21.3 Method tanımı

~~~nxf
group:Counter {
    v:value!i32;

    f:add(amount!i32) {
        self.value += amount;
    }
}
~~~

Method, group gövdesinde normal `f:` bildirimiyle yazılır. Method parametreleri normal fonksiyon parametre kurallarını izler.

## 21.4 Receiver ve self

Group method içinde `self` mevcut group örneğini/receiver'ı ifade eder:

~~~nxf
f:set(value!i32) {
    self.value = value;
}
~~~

`self` method'un receiver erişimidir; normal bir kullanıcı parametresi olarak yeniden tanımlanmaz. Method çağrısında receiver ayrıca aktarılır ve method'un argüman sayısına kullanıcı tarafından yazılan parametreler esas alınır.

## 21.5 Method çağrısı

~~~nxf
counter.increment();
v:value = counter.get();
counter.add(5);
~~~

Method çağrısı receiver, nokta ve method adıyla yazılır. Parametreler method adından sonra parantez içinde verilir.

## 21.6 Receiver ile fonksiyon biçimi

Receiver'ı açıkça bir parametre/argüman akışı içinde aktarmak gerektiğinde normal fonksiyon çağrısı biçimi kullanılabilir:

~~~nxf
(counter) -> process(counter);
~~~

Bu biçim lambda zorunluluğu değildir; receiver veya değer aktarımını ifade eden akış biçimidir. Kullanımın ayrıntıları Bölüm 26'da ele alınır.

## 21.7 Group kullanım sınırları

Group oluşturma, alanların başlangıç değerleri, kopyalama/taşıma ve method receiver yaşam süresi kuralları bu kılavuzun ilgili type ve ownership bölümleriyle birlikte değerlendirilmelidir.

**Durum:** Group alanları, method, `self` ve method çağrısı tanımlı. Oluşturma ve yaşam süresi ayrıntıları ilgili bölümlerde tamamlanacaktır.

---

# 22. Harici fonksiyon bildirimi

Harici fonksiyonlar `exf:` önekiyle bildirilir. Bu bildirim, program içinde çağrılacak ancak gövdesi Nexus Flow kaynak dosyasında bulunmayan bir fonksiyonun imzasını tanımlar.

## 22.1 Temel biçim

~~~nxf
exf:external_name(arg!i32)!i32;
~~~

Harici fonksiyon bildirimi bir imza tanımlar ve noktalı virgülle biter; fonksiyon gövdesi içermez.

## 22.2 Parametre ve dönüş tipi

~~~nxf
exf:read_data(buffer!*u8, length!u32)!i32;
~~~

Parametreler normal fonksiyon parametreleri gibi yazılır. Dönüş tipi varsa parametre listesinden sonra `!Type` ile belirtilir.

## 22.3 Harici isim ve bağlantı

Harici fonksiyonun gerçek sembol adı, bağlantı sistemi ve platform ABI'si ile uyumlu olmalıdır. Nexus Flow bildiriminin tek başına harici sembolün varlığını veya bağlantının başarılı olacağını garanti etmediği unutulmamalıdır.

## 22.4 Çağrı

~~~nxf
v:status = external_name( buffer, length );
~~~

Harici fonksiyon çağrısı normal fonksiyon çağrısı biçimindedir. Argüman tipleri ve ABI sözleşmesi harici fonksiyonla uyumlu olmalıdır.

## 22.5 Hatalı bildirimler

~~~nxf
exf:read_data(buffer!u8) { }  # Harici bildirim gövde taşımaz.
exf:read_data(buffer!u8)      # İmza sonlandırıcısı eksik.
exf:read_data(!i32);          # Parametre adı eksik.
~~~

**Durum:** Temel `exf:` imza bildirimi tanımlı; harici sembol adı eşleme, calling convention ve ABI ayrıntıları kısmen tanımlı.

---

# 23. Pointer
### 23.1 Pointer tipi: *T
### 23.2 Çok katmanlı pointer: **T
### 23.3 Pointer bildirimi
### 23.4 Dereference: *value
### 23.5 Pointer ile atama
### 23.6 Pointer'ın fonksiyon imzasında kullanımı
### 23.7 Raw pointer sınırları

## 24. Reference
### 24.1 Shared reference: &T
### 24.2 Mutable reference: &mut T
### 24.3 Reference oluşturma
### 24.4 Reference değişkeni
### 24.5 Reference parametresi
### 24.6 Reference dönüş tipi
### 24.7 Alan ve indeks referansları
### 24.8 Hatalı reference biçimleri

## 25. Borrow ve ownership
### 25.1 Borrow oluşturma
### 25.2 Shared ve mutable borrow
### 25.3 Reference kullanım kapsamı
### 25.4 Alan ve indeks üzerinden borrow
### 25.5 Fonksiyon çağrısında borrow argümanları
### 25.6 Kaynak aktarımı ve borrow farkı
### 25.7 Kullanım örnekleri ve hatalar

---

# Bölüm VII — Nexus Flow akış yapıları

## 26. Akış operatörleri
### 26.1 Genel bakış
### 26.2 Capture
### 26.3 Pipe ve flow
### 26.4 Error handler
### 26.5 Fallback
### 26.6 Ignore
### 26.7 Shift ve sayısal shift farkı
### 26.8 Zincirleme ve parantezleme

## 27. Masa ve kaynak bağlama
### 27.1 Masa oluşturma ve capture
### 27.2 Masa adı ve bağlama
### 27.3 Masa üzerinden akış
### 27.4 Tekil Halt
### 27.5 Blok Halt
### 27.6 Masa ile fonksiyon çağrısı

## 28. Relocate
### 28.1 _> operatörü
### 28.2 Kaynak ve hedef ifadeleri
### 28.3 Relocate kullanım kapsamı
### 28.4 Hatalı relocate biçimleri
### 28.5 Relocate ve ownership ilişkisi

## 29. Label ve jump
### 29.1 Label bildirimi: label:>
### 29.2 Operator jump
### 29.3 jump(label)
### 29.4 Hedef yazımı
### 29.5 Hatalı label/jump örnekleri

## 30. Hata saptırmalı akış ve retry
### 30.1 Error-handler operatörü
### 30.2 Error binding block
### 30.3 Fallback
### 30.4 Ignore
### 30.5 Rolling retry: ?(count, delay)->target
### 30.6 Retry delay yazımı
### 30.7 Akış operatörlerinin birleşimi
### 30.8 Örnekler ve hatalar

---

# Bölüm VIII — Ruleset ve event

## 31. Ruleset bildirimi
### 31.1 Ruleset adı ve parametreleri
### 31.2 Barrier
### 31.3 Trigger
### 31.4 Always
### 31.5 Default
### 31.6 Kural gövdesi ve sıralama

## 32. Ruleset kullanımı
### 32.1 apply rules(...)
### 32.2 Argüman yazımı
### 32.3 on Rule.Event
### 32.4 Handler gövdesi
### 32.5 Örnekler ve hatalar

---

# Bölüm IX — Modüller ve düşük seviyeli kaynak yapıları

## 33. Modül kullanımı
### 33.1 use bildirimi
### 33.2 Birden fazla modül adı
### 33.3 Dotted module path
### 33.4 Alias: as
### 33.5 Modül örnekleri

### 33.6 no: osmodule;

no: osmodule; varsayılan olarak yüklenen osmodule'ün otomatik yüklenmesini engeller. Bare-metal ve OS servisi bulunmayan programlarda kullanılır. Bu genel bir modül kapatma mekanizması değildir; yalnızca osmodule için tanımlıdır.

~~~nxf
no: osmodule;

f:main() {
    # Bare-metal başlangıç kodu
}
~~~

Kurallar:
- Programın en üst seviyesinde yazılır.
- Fonksiyon veya iç içe blok içinde kullanılamaz.
- Yalnızca osmodule hedeflenebilir.
- Bir programda en fazla bir kez yazılır.
- Noktalı virgül zorunludur.

Hatalı örnekler:

~~~nxf
no: coremodule;       # Desteklenmeyen hedef.
no: osmodule          # Noktalı virgül eksik.
no: osmodule;
no: osmodule;         # Yinelenen bildirim.

f:main() {
    no: osmodule;     # Yanlış kapsam.
}
~~~

**Durum:** Tanımlı.

## 34. Inline assembly
### 34.1 fastexec bloğu
### 34.2 asm: LABEL { ... }
### 34.3 Ham assembly gövdesi
### 34.4 Assembly label
### 34.5 asmcall ve asmjmp
### 34.6 Kullanım örnekleri ve sınırlar

## 35. Donanım ve sistem işlemleri
### 35.1 Port I/O
### 35.2 Memory peek/poke
### 35.3 IRQ ve register
### 35.4 Print ve write
### 35.5 Date, time ve clock
### 35.6 Area ve zone
### 35.7 free ve defer
### 35.8 Zone sync, view ve lock
### 35.9 Hedefe bağlı kullanım kuralları

---

# Bölüm X — Hatalı kaynak örnekleri

## 36. Sözcüksel hatalar
### 36.1 Tanınmayan karakter
### 36.2 Hatalı literal
### 36.3 Eksik operatör
### 36.4 İsim ve ayraç hataları

## 37. Sözdizimi hataları
### 37.1 Eksik ayraç
### 37.2 Hatalı bildirim
### 37.3 Hatalı fonksiyon imzası
### 37.4 Hatalı ifade
### 37.5 Hatalı blok
### 37.6 Hatalı akış operatörü kullanımı

## 38. Dil kullanım hataları
### 38.1 Yanlış argüman biçimi
### 38.2 Yanlış tip yazımı
### 38.3 Yanlış pointer/reference kullanımı
### 38.4 Yanlış kapsamda kontrol ifadesi
### 38.5 Hatalı label/jump
### 38.6 Hatalı select/scan kolu

---

# Bölüm XI — Örnek programlar

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

## 42. İleri örnekler
### 42.1 Select ve scan
### 42.2 Ruleset ve event
### 42.3 Modül kullanımı
### 42.4 Harici fonksiyon
### 42.5 FASTEXEC ve assembly
### 42.6 Donanım işlemleri

---

# Ekler

## A. Dil yapıları özeti
## B. Bildirim önekleri ve özel sözcükler
## C. Operatör tablosu
## D. Tip yazımı tablosu
## E. Literal biçimleri
## F. Nexus Flow akış operatörleri
## G. Pointer ve reference özeti
## H. Desteklenmeyen ve eski yazım biçimleri
## I. Özellik durumu tablosu
## J. Değişiklik günlüğü
## K. Terimler sözlüğü

---

# Bölüm yazım standardı

Her özellik bölümü programcıya yönelik olarak şu soruları yanıtlamalıdır:

1. Bu özellik ne işe yarar?
2. Nasıl yazılır?
3. Hangi kurallar geçerlidir?
4. Hangi kapsamda kullanılabilir?
5. Doğru kullanım örnekleri nelerdir?
6. Hangi yazımlar geçersizdir ve neden?
7. Hata nasıl düzeltilir?
8. Hangi ayrıntılar henüz tanımlanmamıştır?

Derleyici iç yapıları, modül isimleri, veri yapıları ve uygulama ayrıntıları bu bölümlere eklenmez.

---

# Gelecekte eklenecek özellikler

## Macro sistemi

Macro sistemi henüz tasarlanıp kullanıma sunulmadığından mevcut Nexus Flow dilinin parçası değildir. Tasarımı tamamlandığında bu kılavuzun en sonunda ayrı bir bölüm olarak eklenecektir.
