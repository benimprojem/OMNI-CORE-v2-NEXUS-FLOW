# NexusFlow Syntax & Parser Implementation Status
**Versiyon:** 1.0 (OIR v2 Parser Katmanı)

Bu belge, **Modül 1 (Parser DLL)** kapsamındaki C tabanlı Lexer, Parser ve AST altyapısının desteklediği Nexus Flow syntax (sözdizimi) yeteneklerini ve uygulama durumunu özetler.

---

## 1. Temel Tanımlayıcılar (Identifiers & Prefixes)
NexusFlow değişken ve yapı tanımlamalarında ön ekler (prefix) kullanır. Aşağıdaki yapılar tamamen desteklenmektedir:

- `v:` Değişkenler (Mutable)
- `c:` Derleme zamanı sabitleri (Compile-time Constant)
- `m:` Normal, değiştirilemez değer (Immutable Value; initializer zorunludur)
- `f:` Fonksiyonlar
- `exf:` Extern fonksiyonlar
- `r:` Rules (Kurallar)
- `nt:` NewType / Struct / Enum / Union bağlamaları

### Değiştirilemez Değerler — `m:`

`c:` derleme zamanı sabiti olarak tanımlanır. `m:` ise normal bir değer gibi initializer ile oluşturulur; değeri çalışma zamanı/normal ifade bağlamında kullanılabilir fakat isim üzerinden sonradan yeniden atanamaz. `m:` derleme zamanı sabiti değildir. Kaynak/modül yükleme ve import işlemi `use` ile yapılır.

```nxf
m:a = 5;
m:pi = 3.141592653589793;

f:main() {
    m:local_limit = 64;
    echo(a);
    echo(pi);
    echo(local_limit);
}
```

- `m:name = expression;` biçiminde initializer zorunludur.
- Değerin türü initializer ifadesinden Semantic aşamasında çıkarılır.
- `m:` ile tanımlanan isim okunabilir ve normal ifade/argümanlarda kullanılabilir.
- Tanımlandıktan sonra doğrudan yeniden atama, bileşik atama, artırma/azaltma yapılamaz.
- `m:` değişkenleri program ve blok kapsamlarında tanımlanabilir; normal kapsam ve isim çakışması kuralları uygulanır.
- `m:` ifadesi dosya varlığını kontrol etmez, kaynak yüklemez ve `use` yerine geçmez.

Bu tanımın Parser/AST temsili `AST_MUST_DECL` adını korur (append-only AST kimlik uyumluluğu); buradaki MUST adı modül bağımlılığı anlamına gelmez. Semantic bunu `IMMUTABLE` sembol bayrağıyla, `c:` sabitlerinden ayrı kaydeder. `c:` için derleme zamanı ifade değerlendirmesi ayrı bir Semantic yeteneğidir; mevcut sembol bayrağı tek başına constant folding tamamlandı anlamına gelmez.

**Açık Tip Zorlaması:** `!`, Nexus Flow'da türün açıkça zorlandığını/belirtildiğini gösterir. Değişken bildiriminde tür adı değişken adından sonra gelir:

```nxf
v:sayi!i32 = 10;
v:data!u8[];
v:flags!8bit;
v:raw!byte;
```

Burada `!` tür adının parçası değildir; `i32`, `u8`, `8bit` veya `byte` türünü açıkça seçer. Tür çıkarımı istenen bildirimlerde `!` kullanılmayabilir; ancak bit/byte gibi temsil odaklı türlerde açık tür belirtimi tercih edilir. Açık dönüşüm/zorlama ifadelerinin kaynak değer aralığı ve hedef genişliği bakımından Semantic doğrulaması ayrıca tamamlanacaktır.

---

## 2. Tipler (Primitive & Complex Types)
Tüm temel tip anahtar kelimeleri parser tarafından tanınmakta ve analizöre doğru tip OIR kodlarıyla aktarılmaktadır:
- **Sayısal:** `i8..i64`, `u8..u64`, `f32, f64`, `d32, d64`
- **İlkel:** `bit`, `byte`, `hex`, `char`, `str`, `bool`
- **Jenerik/Sanal:** `t` (generic), `fptr`
- **Oyun/Grafik (SIMD):** `Vec2`, `Vec3`, `Vec4`
- **Koleksiyonlar:** Dinamik Array `[]`, Statik Array `[N]`, Harita `map str, i32`

### Bit ve byte türleri

Bit ve byte ailesi, sayısal tamsayı türlerinden ayrı tutulur. Tür adı değişken bildiriminde `!` ile açıkça belirtilir:

| Tür | Genişlik | Anlamı | Tipik kullanım |
|---|---:|---|---|
| `bit` | 1 bit | Tek bitlik veri değeri (0 veya 1) | Tek flag / ikili durum |
| `byte` | 8 bit | Ham byte saklama birimi | Bellek, dosya ve protokol verisi |
| `8bit` | 8 bit | Bit vektörü | 8 flag / maske |
| `16bit` | 16 bit | Bit vektörü | 16 flag / maske |
| `32bit` | 32 bit | Bit vektörü | 32 flag / maske |
| `64bit` | 64 bit | Bit vektörü | 64 flag / maske |
| `128bit` | 128 bit | Bit vektörü | 128 flag / maske |
| `1byte` | 1 byte | Ham veri bloğu | Tek byte veri |
| `2byte` | 2 byte | Ham veri bloğu | 2 byte veri |
| `4byte` | 4 byte | Ham veri bloğu | 4 byte veri |
| `8byte` | 8 byte | Ham veri bloğu | 8 byte veri |
| `16byte` | 16 byte | Ham veri bloğu | 16 byte veri |

`bit` ile `bool` aynı kavram değildir: `bool` mantıksal koşul/doğruluk değeridir; `bit` tek bitlik veri temsilidir. `byte` ham 8-bit saklama birimidir. `u8` ise aritmetik yapılabilen sayısal işaretsiz tamsayıdır. `byte`, `8bit` ve `u8` aynı genişlikte olabilseler de birbirlerinin yerine örtük olarak kullanılamaz. Benzer biçimde `1byte` ve `8bit` aynı fiziksel genişliğe sahiptir, ancak tip niyetleri farklıdır.

**Bellek yerleşimi ve bit numaralandırma (normatif):**

PC bellekleri byte adreslenebilir olduğundan tek başına kullanılan `bit` değişkeni bellekte 1 byte'lık bir slot kaplar. Bu slotun yalnızca **bit 0** alanı değeri taşır; bit 1–7 sıfır tutulur. Dolayısıyla `bit` için saklama kodlaması `0x00` veya `0x01` olur. Ayrı `bit` değişkenleri otomatik olarak aynı byte içinde paketlenmez. Paketli flag alanları için `8bit`, `16bit` vb. bit vektörleri kullanılır.

Bit vektörlerinde bit numarası **LSB-first** olarak tanımlanır:

- Bit 0, ilk byte'ın en düşük anlamlı bitidir (LSB, maske `0x01`).
- Bit 7, ilk byte'ın en yüksek anlamlı bitidir (MSB, maske `0x80`).
- Bit 8, ikinci byte'ın en düşük anlamlı bitidir.
- Genel eşleme: `byte_index = bit_index / 8`; `bit_offset = bit_index % 8`.
- Bu sıralama bit vektörünün bellek/serileştirme düzenidir; hedef CPU'nun sayısal tamsayı endianness ayarından bağımsızdır.

Örneğin `16bit` için bit 0–7 ilk byte'ta, bit 8–15 ikinci byte'tadır. `Nbit` türünün saklama boyutu `N / 8` byte'tır (tanımlı genişliklerin tamamı 8'in katıdır). `Nbyte` türü ise doğrudan N byte ham veri bloğudur.

**Açık tür bildirimi örnekleri:**

```nxf
f:main() {
    v:enabled!bit;
    v:packet_byte!byte;
    v:flags8!8bit;
    v:flags16!16bit;
    v:flags32!32bit;
    v:flags64!64bit;
    v:flags128!128bit;

    v:header!1byte;
    v:word!2byte;
    v:record!4byte;
    v:block!8byte;
    v:large_block!16byte;
}
```

`!` işareti açık tür zorlamasıdır. Bit/byte ailesinde bu, kaynak değerin hedef genişliğe güvenli biçimde dönüştürülmesini ister; sessiz kesme/truncation yapılmaz. Derleyici şu durumları Semantic aşamasında doğrular:

- Sabit tamsayı değerleri hedef aralığına sığmalıdır. Örneğin `v:one!bit = 1;`, `v:flags!8bit = 255;` ve `v:raw!byte = 255;` geçerlidir; `bit = 2` veya `8bit = 256` hata verir.
- Kaynak unsigned tamsayının genişliği hedef genişliğe eşit veya küçükse dönüşüm güvenlidir (ör. `u8` → `8bit`).
- Bit/byte temsil türünde daha geniş kaynaktan daha dar hedefe dönüşüm, kaynak değerin aralığı kanıtlanamıyorsa reddedilir. Dinamik değerler için çalışma zamanı kontrolü henüz yoktur.
- Daha dar bit/byte temsil türünden daha geniş hedefe dönüşüm sıfır genişletme olarak kabul edilir.

Başarılı açık dönüşümler SemanticResult dönüşüm tablosuna kaynak/hedef tür kimlikleri, dönüşüm türü ve explicit işaretiyle kaydedilir; bu kayıtlar SEMANTIC_AST OIR artifact'ine taşınır. Bu metadata aktarımı test edilmiştir; henüz Codegen/runtime dönüşüm komutu üretildiği anlamına gelmez.

Bit vektörleri (`Nbit`) ve ham byte türleri (`byte`, `Nbyte`) üzerinde desteklenen bitwise işlemler:

- `&`, `|`, `^`, `~`: bitwise maskeleme, birleştirme, XOR ve complement.
- `==`, `!=`: aynı tip/genişlikte değer karşılaştırması.
- `&=`, `|=`, `^=`: bitwise bileşik atama.
- Tek bit ayarlama, temizleme ve tersleme maskelerle yapılır: `flags |= mask`, `flags &= ~mask`, `flags ^= mask`.
- Flag kontrolü `(flags & mask) != 0` kalıbıyla yapılır.
- İşlemlerde operand türleri aynı olmalıdır; `8bit | 16bit` ve `1byte | 2byte` tür/genişlik uyuşmazlığıdır. Byte türleri ham veri taşısa da desteklenen bitwise işlemlerle maske olarak kullanılabilir.

```nxf
f:update_flags() {
    v:flags!8bit;
    v:read_mask!8bit;
    v:inverse!8bit = ~flags;

    flags |= read_mask;   # Flag'i etkinleştir.
    flags &= ~read_mask;  # Flag'i temizle.
    flags ^= read_mask;   # Flag'i tersle.

    v:has_read!bool = (flags & read_mask) != 0;
}
```

Tek bit erişimi bit vektörlerinde köşeli parantez ile yapılır. `flags[0]` en düşük anlamlı bit, `flags[7]` ise `8bit` vektörünün en yüksek anlamlı bitidir. Okuma sonucu `bit`, yazma hedefi de `bit` türündedir. `Nbyte` bloklarında `data[0]` ilk byte'ı verir ve sonucu `byte` türündedir:

```nxf
f:inspect_flags() {
    v:flags!8bit;
    v:first!bit = flags[0];  # LSB
    v:last!bit = flags[7];   # MSB
    v:one!bit = 1;
    flags[0] = one;          # LSB'yi güncelle
}

f:read_packet() {
    v:packet!4byte;
    v:first_byte!byte = packet[0];
    v:last_byte!byte = packet[3];
}
```

Semantic artık `Nbit[index]` erişimini `bit`, `Nbyte[index]` erişimini `byte` olarak sınıflandırır, indeks ifadesinin tamsayı türünde olmasını ister ve sabit olarak hesaplanabilen negatif veya sınır dışı indeksleri reddeder. Çalışma anında hesaplanan indeksler için sınır kontrolü OIR/Verifier ve çalışma zamanı sözleşmesinde ayrıca tamamlanacaktır. Bit/byte ailesine açık `!` zorlaması; sabit tamsayı aralık kontrolü, güvenli unsigned genişlik dönüşümü ve temsil türlerinde sıfır genişletme ile uygulanır. Kanıtlanamayan dinamik daraltmalar reddedilir; genel çalışma zamanı dönüşüm kontrolleri ve maskelerin daha geniş literal biçimleri henüz tamamlanmamıştır.

`sizeof(value)` byte cinsinden saklama boyutunu sorgular; bit genişliği ise türün ayrı bir özelliğidir. Bu yerleşim kuralında `sizeof(bit)` ve `sizeof(byte)` 1 byte; `sizeof(8bit)` ve `sizeof(1byte)` de 1 byte'tır. `sizeof(16bit)` 2 byte, `sizeof(128bit)` 16 byte'tır. Hedefe bağlı hizalama/padding ve tüm tür ailesinin nihai ABI yerleşimi ayrıca doğrulanacaktır.

`<<` ve `>>` Nexus Flow'un mevcut stream/zone operatörleridir; bit kaydırma anlamı verilmez.

**Uygulama durumu:** Lexer `bit`, `byte`, `8bit`–`128bit` ve `1byte`–`16byte` yazımlarını ayrı tip tokenları olarak tanır. Parser yazımları AST type-name alanında korur ve açık tür işaretini ayrı saklar. Semantic türleri tanır, uyumlu bitwise işlemleri ve `~` operandını doğrular; temsil türlerine sayısal aritmetik izni vermez. `Nbit[index]` erişimini `bit`, `Nbyte[index]` erişimini `byte` olarak sınıflandırır ve indeks türünü tamsayıyla sınırlar. AST frontend OIR codec round-trip testi tüm on numeric-leading type spelling ve hard-type işaretini doğrular; SemanticResult primitive tür sınıflandırması günceldir. Bit/byte fiziksel yerleşim kuralı LSB-first olarak belgelenmiştir. OIR_MODULE 1.2 type map ve bağımsız Verifier, bit/byte tür kayıtlarını tanır ve genişliklerini doğrular. OIR opcode registry 1.1'de bitwise AND/OR/XOR/NOT kimlikleri eklendi; Verifier operand/sonuç tür kimliklerinin eşleşmesini doğrular. Semantic AST lowering bu opcode'ları henüz üretmez; Nbit/Nbyte indeksleme ve dinamik sınır denetimi, 128-bit immediate kodlaması ve nihai ABI hizalama/padding doğrulaması beklemektedir.

---

## 3. Akış ve Handler Operatörleri ("The 15 Keys")
Aşağıdaki tüm özel NexusFlow 15-Key operatörleri, lexer seviyesinde tokenleştirilir ve parser'da özel işlem görerek (veya `parseHandleExpression` içinde) derlenip OIR'a yansıtılır:

- `(h)`: Masa/Handler `(OP_REG_HANDLE)`
- `<-`: Yakala (Capture / Bind)
- `->`: Başarı/İleri sür (Pipe)
- `_>`: Taşı (Relocate)
- `?(n, ms)`: Rolling Retry (Zaman Ayarlı Tekrar)
- `?->`: Catch (Saptırıcı)
- `?=>`: Fallback (Alternatif)
- `!->`: Ignore (Sessiz devam)
- `<<`, `>>`: Zone Write ve Flow Feed
- `!!`: Panic
- `!!=`: Derleyici Direktifi
- `}?;`: Halt / Temizlik ve Sonlandırma Terminatorü

---

## 4. Değişken Atamaları ve Yapılar
```nxf
# Standart atama
v:x = 100;
v:pi!f64 = 3.14159;

# Pointer ve Ref (Kısmi sözdizimi ayrıştırma)
v:ptr!*i32;

# Dinamik ve Statik Diziler
v:sayi[];
v:sayilar[5]!i32 = [1, 2, 3, 4, 5];

# Çok boyutlu diziler desteklenmektedir
v:matris[3][3]!f32 = [ ... ];

# Maps (Haritalar)
v:notlar!map str, i32;
```

---

## 5. Fonksiyonlar ve Modüller

**Standart Fonksiyon:**
```nxf
f:topla(a!i32, b!i32)!i32 { 
    return a + b; 
}
```

**Lambda / Tek Satır Fonksiyonlar:**
```nxf
v:kare = f:(a)> a * a;
f:isim_getir(self)> self.ad;
```

**Modül İçe Aktarma (Imports):**
```nxf
use osmodule as os;
import "NetModule";
```

---

## 5.1 USE: alias ve namespace seçimi

Nexus Flow'da `use` bildirimi iki ayrı isimlendirme seçeneği sunar:

- `as` import edilen modülün/öğenin yerel **alias** adını belirler.
- `ns` seçilen Group'un üyelerinin hangi **namespace kökü** altında sunulacağını belirler.

Bu iki seçenek farklı görevler üstlenir; birbirinin yerine kullanılmaz. Kullanım sırası
sabittir: önce isteğe bağlı `as`, ardından isteğe bağlı `ns`.

```nxf
group db {
    group mysql {
        group sql {
            query => f:(text) { /* ... */ }
        }
    }
}

// Son Group seçilir; namespace kökü sql olur.
use db.mysql.sql ns sql;

// mysql Group'u seçilir; altındaki sql Group'u hiyerarşide kalır.
use db.mysql ns mysql;

// Alias ve namespace birlikte kullanılabilir.
use db.mysql.sql as mysql_sql ns sql;

// Alias olmadan yalnızca namespace seçilebilir.
use db.mysql ns mysql;

// ns _ namespace ön ekini kaldırır; üyeler doğrudan mevcut kapsamda aranır.
use db.mysql.sql ns _;
```

Yukarıdaki seçimlerin isim çözümleme hedefleri:

- `use db.mysql.sql ns sql;` → `sql.query(...)`
- `use db.mysql ns mysql;` → `mysql.sql.query(...)`
- `use db.mysql.sql as mysql_sql ns sql;` → import öğesinin alias'ı `mysql_sql`, namespace kökü `sql`
- `use db.mysql.sql ns _;` → `query(...)` biçiminde doğrudan kapsamdan erişim

Kurallar:

1. Dotted yol, seçilecek modül veya Group öğesini tanımlar.
2. `as name`, import öğesine yerel alias verir; namespace hiyerarşisini tek başına değiştirmez.
3. `ns name`, seçilen Group'un namespace kök adını belirler.
4. `ns _`, namespace ön ekini kaldırarak üyeleri mevcut kapsamda görünür kılma isteğini belirtir.
5. `as _` özel bir namespace kısaltması değildir; `as` sonrasında sıradan alias olarak değerlendirilir.
6. Bir `use` listesinde her import öğesi kendi `as` ve `ns` seçeneklerini taşıyabilir.
7. Namespace/alias çakışmaları, Group görünürlüğü ve gerçek sembol aktarımı Semantic import-resolution aşamasında doğrulanır.

Parser, `module_name`, `alias` ve `namespace_name` alanlarını ayrı AST payload'ları olarak
korur. AST stream formatı v6, namespace ve program modül-politikası alanlarını saklar; v1-v5 akışları geriye dönük
okunabilir. Parser/AST testleri sözdizimini, alanları, kaynak aralıklarını ve codec
round-trip davranışını doğrular. Bu testler Semantic namespace çözümlemesinin tamamlandığı
anlamına gelmez.


## 5.2 Varsayılan modüller: `coremodule` ve `osmodule`

- `coremodule`: işletim sistemi bağımsız genel fonksiyon kütüphanesi.
- `osmodule`: `file`, `print`, `echo`, `input` gibi OS servislerine bağlı standart fonksiyonlar.
- `osmodule` varsayılan olarak otomatik yüklenir.
- `no: osmodule;` program kapsamında bu otomatik yüklemeyi kapatır.
- `no:` genel modül kapatma mekanizması değildir; `no: coremodule;` geçersizdir.
- `coremodule` için varsayılan yükleme politikası ayrıca karara bağlanacaktır.

```nxf
f:main() {
    print("Hello World");
    echo("Nexus Flow");
    input();
}
```

OS bulunmayan bare-metal hedefte:

```nxf
no: osmodule;

f:main() {
    # Kernel başlangıç kodu
}
```

Parser bu bildirimi yalnızca program seviyesinde kabul eder; ASTProgram üzerinde boolean politika alanı ve tam kaynak aralığı taşır. Bu alan otomatik modül ekleme kararının host/pipeline katmanına aktarılması içindir. Module Resolver'ın otomatik osmodule ekleme entegrasyonu henüz tamamlanmış değildir.

## 6. Döngüler (Unified Loop)
Tek bir `loop` anahtar kelimesi altında 5 farklı varyasyon parser tarafından ayrıştırılır:
```nxf
# 1. Range Loop
loop(1..100) { ... }

# 2. For Loop
loop(i, a < b, i++) { ... }

# 3. Foreach Loop
loop(v:item in list) { ... }

# 4. While Loop
loop(a < b) { ... }

# 5. Infinite Loop
loop { ... }
```
Ayrıca `break;` ve `continue;` ifadeleri döngü içinde OIR loop sonlandırma opcode'larına başarıyla çevrilir.

---

## 7. Dallanma (If / Elsif / Els)
Geleneksel kontrol akışı Nexus kurallarıyla %100 entegre edilmiştir. İçiçe veya art arda bloklar parser tarafından tanınır:

```nxf
if (a == b) {
    (h) <- echo("Esit");
} elsif (a > b) {
    (h) <- echo("Buyuk");
} els {
    (h) <- echo("Kucuk");
}
```
*(Parantezsiz veya blok süslü parantez olmadan tek satır ifadelere de destek sunulmuştur)*

---

## 8. Veri Kalıpları (Struct, Enum, Union, Group)

`struct`, `!struct`, `enum`, `union` tiplerinin ve bunlarla ilişkili metotları gruplandıran `group` yapısının ayrıştırması (parseGroup vb. metotlarla) eksiksizdir:

```nxf
group Oyuncu {
    # Tip tanımı
    nt:struct:Karakter {
        v:ad!str,
        v:puan!i32
    };

    # Group İçi Metot (Anonim binding)
    yeni => f:(isim) { ... }

    # Alt group
    canta => group {
        ac => f:() { ... }
    }
    
    # Default handler
    default { echo("Bilinmeyen çağrı"); }
}
```

---

## 9. Kurallar (Rules ve Multi-Rule Engine)
`rules` blokları, içerisindeki özel `:=>` (Bariyer), `->` (Yönlendirme), `always ->` ve `default ->` durumlarıyla hatasız parse edilebilmekte, sembol ve opcode tablolarına aktarabilmektedir. `apply` anahtar kelimesiyle enjeksiyonlar parser'da ele alınır.

```nxf
rules SafeCheck(w, h) {
    w > 2000 :=> "Sinir asildi";
    (w == h) -> SquareMode;
    always -> Log;
    default -> Normal;
}
```

---

## 10. Inline Assembly ve Derleyici Direktifleri
**Derleyici Direktifi:**
```nxf
!!= target = "Linux64"
!!= link "opengl32"
```

**Gelişmiş Assembly Blokları:**
```nxf
fastexec {
    asm: LABEL_1 {
        mov rax, %a
        add rax, %b
    }
}
asmcall(LABEL_1);
```

---

### Modül 1 Çıktısı (OIR v2.0)
Tüm yukarıdaki kodlar hatasız şekilde binary `*.oir` dosyası olarak sentezlenmektedir. Oluşturulan AST (Abstract Syntax Tree) doğrudan lineer bir Instruction dizilimine (OIR) yapılandırılmakta; bilinmeyen tanımlayıcılar veya yanlış blok sınırlandırmalarında `ErrorList` devreye girerek (Panic-mode recovery destekli) süreci raporlamaktadır.

