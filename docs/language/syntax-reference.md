# NEXUS FLOW — Syntax Reference Guide

**Language:** NEXUS Flow  
**Compiler project:** OMNI-CORE Compiler (OCC)  
**Document type:** Language syntax and semantics reference  
**Version:** 1.1 (gama; canonical reference reconciled with the active C compiler branch)

> Bu belge NEXUS Flow dilinin normatif sözdizimi ve dil-semantik başvuru kaynağıdır. Derleyici mimarisi, modül sorumlulukları ve geliştirme durumu kök [README](../../README.md) ile OCC ilerleme belgelerinde tutulur.
>
> **Olgunluk notu:** Dil tasarımında tanımlanmış olmak, özelliğin Lexer, Parser, AST, Semantic ve OIR katmanlarında uygulandığı anlamına gelmez. Mevcut uygulama sınırları ve açık semantik kararlar açıkça belirtilir. Kod örnekleri tek başına uygulama garantisi değildir.
>
> **Normatif konum:** Bu dosya OCC deposundaki tek normatif Nexus Flow syntax referansıdır. Derleyici uygulama durumu ve açık işler `OCC_PIPELINE_PROGRESS.md` ile AST/syntax matrisinde izlenir.
>
> Dil özelliklerinin diğer dillerle karşılaştırması ve eksik özellik envanteri için [NEXUS-FLOW-LANGUAGE-COMPARISON.md](NEXUS-FLOW-LANGUAGE-COMPARISON.md) belgesine bakın.

---

## Bölüm 1. TASARIM FELSEFESİ: MİNİMALİZM VE AKIŞ

1.  **Handler-Oriented (Masa):** Değişkenler sadece veri değil, birer "istasyon" (Handler) olarak görülür. `(h)` boşsa veri akar, doluysa kilitlenir.
2.  **Deterministik Bellek:** `area` ve `zone`, OS bağımsız mantıksal bellek alanlarını ifade eder. Fiziksel bellek/cache eşlemesi hedef topolojisine bırakılır; sahiplik (Ownership) ayrıca tanımlanır.

### 1.1 `area` ve `zone` bellek alanları

- `area`: çekirdeğe veya çekirdek grubuna yakın/yerel bellek alanı niyeti.
- `zone`: birden fazla çekirdeğin erişebildiği daha geniş paylaşım alanı niyeti.
- Bunlar doğrudan L1/L2 ve L3/L4 garantisi değildir. Cache paylaşım topolojisi işlemciye göre değiştiğinden fiziksel eşleme target/runtime katmanında yapılır.
- Bare-metal/OS bağımsız hedeflerde özel runtime; OS üzerinde çalışan hedeflerde OS adapter aynı dil kontratını uygular.
- Hangi fiziksel havuzdan tahsis yapılacağını seçen yerleşim politikası henüz tasarım aşamasındadır; mevcut sözdizimine yeni bir ayar eklenmiş değildir.

Ayrıntı ve açık kararlar: [AREA_ZONE_MEMORY_MODEL.md](../../AREA_ZONE_MEMORY_MODEL.md).

---

---

## Bölüm 2. TANIMLAYICILAR VE TİPLEME SİSTEMİ**

###  2.1 Identifiers (Niyet Belirleyiciler)**
---
| Önek | Adı | Açıklama |
| --- | --- | --- |
| `v:` | **VAR** | Değiştirilebilir değişken. Sahiplik son bloktadır. |
| `c:` | **CONST** | Derleme anı sabitleri (Değiştirilemez). |
| `m:` | **MUST** | Normal değer bildirimi; initializer zorunludur ve bağlama değiştirilemez. Modül yüklemez. |
||||
| `nt:`| **NewType**| Tip takma adları (Typedef-like). |
||||
| `r:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
| `t:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
| `o:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
||||
| `*`  | **Pointer**| Ham bellek adresi. |
| `**` | **P2P** | Pointer to Pointer. |
| `&`  | **Ref** | Bellek referansı. |
| `&&` | **RESERVE** | Referansın referansı için henüz tanımlı değildir; kullanılmaz. |

 > `&&T` mevcut Nexus Flow type syntax'ında tanımlı değildir ve reserved kabul edilir. `**T`, pointer katmanlarının iç içe geçmesini ifade eder; `&T` referans türüdür.
 > Pointer/reference türleri `*T`, `**T` ve `&T` biçimlerinde Parser tarafından local variable, function parameter, return type ve `nt:` hedef tipi bağlamlarında kabul edilir. SemanticResult bunları katman başına ayrı type ID ve `base_type_id` ilişkisiyle temsil eder; `**T` iki Pointer kaydı, `&T` Reference kaydı üretir. Bu yapısal kayıt pointer validity, mutability, nullability, aliasing ve borrow/lifetime kurallarının tamamlandığı anlamına gelmez.
---

###  2.2 Tipler **

- v:x = 100;        # default i32
- v:x!u8 = 100;     # u8 zorlama


- **Hard-Lock (`!`):** `v:sayi!u64 = 50`. Veri "50" 8 bit olsa bile 64-bit yer ayrılır.

**Tipler:** `bit`, `byte` (u8), `hex`, `char`, `f32`, `f64`, `d32`, `d64`, `i8..i64`, `u8..u64`, `bool`. `d32` ve `d64`, Decimal (ondalık) numeric tipleridir; Semantic numeric işlem ve compound assignment kontrollerinde tanınırlar. `d128`, `i128`, `u128`, `f128` gibi daha geniş tipler ileride eklenebilir; henüz desteklenmez.

**Bit/byte temsil türleri (C compiler branch):** `bit` tek bitlik veri, `byte` ham 8-bit saklama birimidir. `bool` mantıksal doğruluk türü; `u8` ise aritmetik işaretsiz tamsayıdır. Aynı fiziksel genişlik, türlerin örtük olarak birbirine dönüştürülebileceği anlamına gelmez.

| Tür ailesi | Desteklenen yazımlar | Anlam |
|---|---|---|
| Tek bit | `bit` | Tek 0/1 veri biti |
| Ham byte | `byte` | 8-bit ham saklama birimi |
| Bit vektörü | `8bit`, `16bit`, `32bit`, `64bit`, `128bit` | N bitlik maske/flag vektörü |
| Byte bloğu | `1byte`, `2byte`, `4byte`, `8byte`, `16byte` | N byte ham veri bloğu |

Bu türler değişken bildiriminde açık tür işaretiyle yazılır; `!` tür adının parçası değil, zorlamanın işaretidir:

```nxf
v:enabled!bit = 1;
v:packet_type!byte = 0x2A;
v:flags!16bit = 0x0101;
v:header!4byte;
```

PC bellekleri byte adreslenebilir olduğundan tekil `bit` değeri bir byte'lık slot kullanır. Yalnız slotun bit 0 alanı değer taşır: `0x00` false, `0x01` true; bit 1–7 sıfır/rezervedir. Ayrı `bit` değişkenleri otomatik paketlenmez. Paketli alanlar `Nbit` ile ifade edilir.

Bit vektörleri LSB-first sıralanır: `flags[0]` ilk byte'ın bit 0'ı (maske `0x01`), `flags[7]` ilk byte'ın bit 7'si (maske `0x80`), `flags[8]` ikinci byte'ın bit 0'ıdır. Genel eşleme `byte_index = bit_index / 8`, `bit_offset = bit_index % 8` biçimindedir. Bu, bit vektörünün kanonik byte sıralamasıdır ve CPU integer endianness'inden bağımsızdır. `Nbit` saklama boyutu N/8 byte, `Nbyte` saklama boyutu N byte'tır. `sizeof(value)` byte cinsinden boyut döndürür.

`!Type` açık zorlamadır. Bit/byte hedeflerine sabit tamsayı atanırken değer hedef aralığına sığmalıdır; örneğin `1` → `bit`, `255` → `byte` veya `8bit` geçerlidir, `2` → `bit` ve `256` → `8bit` geçersizdir. Daha dar unsigned/bit temsil türünden daha geniş hedefe sıfır genişletme güvenlidir. Kanıtlanamayan dinamik daraltma sessizce kesilmez ve Semantic tarafından reddedilir. Başarılı dönüşümler SemanticResult/OIR metadata'sında açık dönüşüm olarak saklanır; bu kayıt tek başına runtime komutunun üretildiği anlamına gelmez.
**Oyun/Grafik:** `Vec2`, `Vec3`, `Vec4` (SIMD Destekli).

---

###  2.3 Değişkenler**  Veri çeşitleri
* Değişkenler
```
v:sayi = 100;   # i32
                    
v:liste = [1, 2, 3]; # listenin içindeki en büyük elemana göre tipi ayarlar.

# Derleme zamanı sabiti
c:PI = 3.14159;

# Normal, fakat yeniden atanamayan değer
m:runtime_limit = get_limit();

# Ondalık tam sayı literal
v:val = 10E3; # Scientific notation

# Üslü sayılar.
v:vall= 10^^15;

`c:` derleme zamanı sabitidir; derleme zamanı değerlendirilebilen ifadelerde kullanılmak üzere tasarlanmıştır. `m:` ise normal bir değer bildirir: initializer çalıştırılır/değerlendirilir, değer okunabilir, ancak isim üzerinden sonradan yeniden atama veya güncelleme yapılamaz. `m:` türünü initializer ifadesinden çıkarır ve program/blok kapsamında kullanılabilir.

`use` modül/kaynak yükleme ve import işlemidir; `m:` dosya varlığı kontrolü veya dependency bildirimi değildir.

```nxf
use osmodule;
m:connection_limit = 8;
```

## Tip zorlama. '!' Eğer programcı zorlamak isterse:
v:pi!f64 = 3.1415; # '!' operatörüyle tip mühürlenir (Hard-lock).

loop(i, i < 100, i++) -> {
    v:sonuc = i * 2.5; 
    # 'sonuc' burada 'float' olur, bir sonraki adımda gerekirse 'int'e evrilir.
}

f:hassas_toplam(a:!f64, b:!f64)!f64 {ret a + b }
# Burada, içeri giren her veriyi f64'e zorlar.

f:topla(a, b) {ret a + b }
# Kullanım: topla(5, 10) veya topla(a:5, b:10)

v:sonuc = topla(a:10, b:20);
```
	 
### 2.3.1 Literal ve Intrinsic Sözdizimi (Mevcut Lexer/Parser sınırı)

**Uygulanan literal kuralları:**

- Sayısal literal: şu an ondalık tam sayı ve ondalık kesir biçimleri desteklenir (ör. `12`, `3.14`).
- String literal çift tırnakla yazılır: `"metin"`.
- Karakter literal tek tırnakla yazılır: `'A'`.
- String içinde ters eğik çizgi (`\`) kendisinden sonraki karakterin kapanış tırnağı olarak yorumlanmasını engeller. Örneğin `"a\\\"b"` tek bir string tokenıdır.
- Karakter literalinde ters eğik çizgi, sonraki karakteri kaçışlı karakter olarak tüketir. Eksik literal veya EOF ile biten kaçış Lexer hatasıdır.
- Lexer şu aşamada literal içeriğini ham yazımıyla AST'ye aktarır; kaçış dizilerinin çalışma zamanı değerine dönüştürülmesi bu sözdizimi kuralından ayrı bir aşamadır.

**Intrinsic biçimleri:**

```nxf
v:converted = cast(value, i32);
panic("critical failure");
```

- `cast(value, type)`: İlk argüman ifade, ikinci argüman tür adıdır. Tür adı primitive tür tokenı veya tanımlayıcı olabilir.
- `panic("message")`: Mevcut Parser uygulamasında mesaj string literalidir. Değişken veya genel ifade mesajı henüz desteklenmiş kabul edilmez.

**Desteklenen sayısal yazımlar:** Ondalık tam sayı/ondalık kesir (`12`, `3.14`), hexadecimal (`0xFF`), binary (`0b1010`), octal (`0o17`), bilimsel gösterim (`10E3`, `1e-3`) ve rakam gruplama ayıracı (`1_000`, `1.5_25e-2`). Ayraç yalnızca iki geçerli rakam arasında kullanılabilir. Tür ekleri (`12u8`, `1.5f32`) desteklenmez ve tek bir `TOKEN_ERROR` ile reddedilir. Rolling delay için tanımlı `500ms` biçimi istisnadır: Lexer bunu `TOKEN_NUMBER` + `ms` identifier tokenlarına ayırır; birim Parser tarafından rolling metadata bağlamında tüketilir. `^^` sayısal literal biçimi değil, ayrı bir üs alma operatörüdür. Parser `^^` operatörünü right-associative olarak ayrıştırır; `a^^b^^c`, `a^^(b^^c)` biçimindedir. Üs alma, unary eksi işaretinden daha sıkı bağlanır.

---

* Diziler (Arrays)
```
v:sayi[]; # Dinamik array Tanımlama
  sayi = [10, 20, 30, 40, 50]; # Atama. Otomatik u8 olur.
  

v:sayilar[5]!i32 = [1, 2, 3, 4, 5]; #Statik Tanımlama ve atama.
v:buffer[2*8+1]!i32; # Derleme zamanında hesaplanan pozitif sabit boyut.

# Boyut ifadeleri tamsayı literal'ları ve +, -, *, /, % işlemlerini destekler.
# % yalnızca integer operand'larla geçerlidir; sonuç pozitif olmalı, taşma ve sıfıra bölme kabul edilmez.

v:matris[3][3]!f32 = [
    [1.2, 0.3, 0.6],
    [0.5, 1.0, 0.1],
    [0.4, 0.2, 1.1]
];


# Bir fonksiyona parametre tipi olarak array vermek için `a![]`, ya da tip zorlama bir array `a!i32[]`

# Normal
f:listele(a![]) { ... }

# Tip zorlama
f:liste(a!i32[], ...){ ... }

# Çok boyutlu parametre ve imza uyumluluğu
f:matrisAl(veri:f64[2][2]) { ... }

# Çağrı sırasında eleman türü, boyut sayısı ve sabit boyutlar denetlenir.
# Parametredeki [] ilgili boyutta herhangi bir uzunluğu kabul eder.
# Örneğin i32[] parametresi i32[5] dizisini kabul eder; i32[4] kabul etmez.

```

* Haritalar (Maps / Dictionaries) normal bir türdür; fonksiyon parametresinde `a:map` biçimi tercih edilir. `a!map` biçimi mevcut Parser tarafından hard-type parametre olarak da kabul edilir, ancak map için zorunlu değildir. Yerel değişken bildiriminin mevcut grameri `!Type` biçiminde olduğundan `v:notlar!map;` bugün için kullanılan bildirim biçimidir; buradaki `!` map'e özgü bir key/value kuralı değil, değişken bildiriminin hard-type işaretidir.

Map tek bir sabit key/value tür çiftiyle sınırlandırılmak zorunda değildir. Anahtarlar çoğunlukla string olabilir; değerlerin türleri map içindeki kayıt/öğe bazında tutulabilir. Bu nedenle `map<str, i32>` biçimini zorunlu kılmıyoruz. Bu generic yazımın gelecekte isteğe bağlı bir kısıtlama olarak desteklenip desteklenmeyeceği henüz kararlaştırılmamıştır; mevcut Parser bu yazımı henüz desteklemez.

```nxf
v:notlar!map;
  notlar["Ali"] = 90;
  notlar["Veli"] = 85;
  
v:notu = notlar["Ali"]; # 90  
# Yukarıdaki v:notu , notlar["Ali"] içeriğinin kopyasıdır. 

notu = notu +5; # notu değeri 95 oldu.
# notlar["Ali"] değeri yeni bir değer kaydedilene kadar değişmez.

notlar["Ali"]<-notu; # Artık "notlar["Ali"]"nin değeri 90 değil 95 dir. 
# notu içerisi boşaltıldığı için 0 dır, tekrar kullanılmıyorsa blok bittiğinde silinir.

# Normal map parametresi
f:listele(a:map, ...) { ... }

# Eski / açık hard-type parametre yazımı; map için gerekli değildir
f:listele_legacy(a!map, ...) { ... }

```

* Matematiksel Vektörler - Oyun ve grafik programlama için SIMD destekli tipler.
*   `Vec2`: `[x, y]` 
*   `Vec3`: `[x, y, z]` 
*   `Vec4`: `[x, y, z, w]` 

```nxf
v:a!Vec2 = [1.0, 2.0];
v:b!Vec2 = [3.0, 4.0];
v:sum2 = a + b;             # [4.0, 6.0]
v:diff2 = a - b;            # [-2.0, -2.0]

v:c!Vec3 = [1.0, 2.0, 3.0];
v:d!Vec3 = [2.0, 3.0, 4.0];
v:product3 = c * d;         # [2.0, 6.0, 12.0]
v:negative3 = -c;            # [-1.0, -2.0, -3.0]
c += d;                      # [3.0, 5.0, 7.0]

v:e!Vec4 = [1.0, 2.0, 3.0, 4.0];
v:f!Vec4 = [2.0, 2.0, 2.0, 2.0];
v:quotient4 = e / f;         # [0.5, 1.0, 1.5, 2.0]

f:vec_process(a!Vec3, ...) { ... }
```

**Vektör aritmetiği (Semantic sözleşmesi):**
- `Vec2`, `Vec3` ve `Vec4` aynı boyuttaki iki vektör arasında bileşen bazında `+`, `-`, `*`, `/`, `%` ve `^^` işlemlerini destekler.
- Unary `-` vektörün her bileşeninin işaretini değiştirir.
- `+=`, `-=`, `*=`, `/=` bileşik atamaları aynı boyuttaki vektörlerde desteklenir.
- Operand boyutları eşleşmelidir. `Vec2` ile `Vec3` gibi farklı boyutlar Semantic hatasıdır.
- Vektör ile skalerin karıştırılması şu an desteklenmez; skaler yayılımı/broadcast için dil kuralı henüz tanımlanmamıştır.
- Başlatıcı tam olarak vektör boyutu kadar sayısal bileşen içermelidir. Örneğin `Vec3` üç bileşen ister.
- Bu maddeler Semantic kabul sözleşmesidir; SIMD kod üretimi ve çalışma zamanı davranışı Codegen/backend aşamasına aittir.

---

### 2.4 TAM OPARATÖR REFERANSI

####  2.4.1 Aritmetik ve Atama (Bellek Odaklı)**
Bu operatörler, işlem sonucunu her zaman en küçük sığabileceği tipe (u8, i16 vb.) otomatik dökümler.

* `+`, `-`, `*`, `/`, `%` (Modülo)
* `^^` (Üs alma)

> Uygulama durumu (2026-11-03): Lexer `^^` tokenını üretir; Parser `parse_power()` ile sağ birleşimli AST üretir. Semantic matching numeric operand kontrolü vardır. Promotion/overflow ve OIR opcode/lowering kapsamı açık kalır.
* `+=`, `-=`, `*=`, `/=` (Bileşik atama)
* `++`, `--` (Artırma/Azaltma)


#### 2.5 Karşılaştırma ve Mantık**

//is → type identity

* `==` (Eşit mi?)
* `!=` (Eşit değil mi?)
* `===` (Tam eşitlik: Hem değer hem de Tip/Bellek boyutu aynı mı?)
* `==!` (Zorlamalı eşitlik: Tipleri görmezden gel, sadece bit dizilimine bak)
* `>`, `<`, `>=`, `<=`
* `&&` (VE), `||` (VEYA), `!` (DEĞİL)

**Mevcut ifade önceliği (yüksekten düşüğe; parantezler her zaman önceliklidir):**

| Sıra | Operatör grubu | Birleşme |
|---:|---|---|
| 1 | Çağrı, üye erişimi, indeks, postfix `++ --` | postfix |
| 2 | Power `^^` | sağdan; unary eksi operandından daha sıkı |
| 3 | Unary `!`, `~`, `-` | sağdan |
| 4 | `*`, `/`, `%` | soldan |
| 5 | `+`, `-` | soldan |
| 6 | Bitwise `&` | soldan |
| 7 | Bitwise `^` | soldan |
| 8 | Bitwise `|` | soldan |
| 9 | `<`, `<=`, `>`, `>=` | soldan |
| 10 | `==`, `!=`, `===`, `==!` | soldan |
| 11 | `..` | tek aralık |
| 12 | `&&` | soldan |
| 13 | `||` | soldan |
| 14 | Nexus Flow akış operatörleri | akış grameri |
| 15 | `=`, `!!=`, capture `<-` | sağdan / capture grameri |

Bu tablo mevcut C Parser üretim sırasını yansıtır. `&&` `||`'dan daha yüksek önceliklidir. Kısa devre yürütme davranışı ayrıca OIR/lowering sözleşmesinde doğrulanmalıdır.

#### 2.6 Bit düzeyinde işlemler ve indeks erişimi

Bitwise işlemler `bit`, `byte`, sabit genişlikli `Nbit` ve `Nbyte` temsil türlerinde; ayrıca Semantic'in izin verdiği tamsayı türlerinde uygulanır. İki operandlı işlemlerde operand türleri aynı olmalı; genişlik veya temsil türü farklıysa örtük dönüşüm yapılmaz. Gerekli dönüşüm açık `!` zorlamasıyla yazılır.

| Operatör | İşlem | Kural |
|---|---|---|
| `&` | Bitwise AND | İki eş tür operand |
| `|` | Bitwise OR | İki eş tür operand |
| `^` | Bitwise XOR | İki eş tür operand |
| `~` | Bitwise NOT | Tek bitwise-capable operand; aynı tür/genişlikte sonuç |
| `&=`, `|=`, `^=` | Bitwise bileşik atama | Lvalue ve sağ operand uyumlu/eş tür olmalı |
| `==`, `!=` | Eşitlik | Aynı temsil türü ve genişlikte karşılaştırma |

```nxf
f:update_flags() {
    v:flags!8bit = 0;
    v:mask!8bit = 0x05;

    flags |= mask;       # bitleri etkinleştir
    flags &= ~mask;      # maskede 1 olan bitleri temizle
    flags ^= mask;       # maskede 1 olan bitleri tersle

    v:has_flag!bool = (flags & mask) != 0;
}
```

`Nbit[index]` erişimi tek bir `bit` okur veya yazar. İndeks 0 tabanlı ve LSB-first'tir. `Nbyte[index]` erişimi bir `byte` okur veya yazar; indeks 0 ilk byte'tır.

```nxf
f:inspect() {
    v:flags!16bit = 0;
    v:enabled!bit = 1;
    flags[0] = enabled;        # ilk byte, LSB
    v:high!bit = flags[15];    # ikinci byte, MSB

    v:data!4byte;
    v:first!byte = data[0];
    v:last!byte = data[3];
}
```

Sabit olarak hesaplanabilen negatif veya sınır dışı indeks Semantic hatasıdır. Dinamik indeks için çalışma zamanı sınır kontrolü henüz OIR/Verifier/runtime sözleşmesinde tamamlanmamıştır. Bit/byte indeksleme OIR lowering kapsamına henüz alınmamıştır.

**Önemli:** `<<` ve `>>` Nexus Flow'un Zone Write / Flow Feed akış operatörleridir; bit kaydırma işlemi değildir. Bu işaretlere sayısal shift anlamı yüklenmez.

#### 2.6.1 Bit kaydırma operatörleri

Nexus Flow'da `<<` ve `>>` Zone Write / Flow Feed anlamlarını korur. Bit kaydırma için ayrı operatörler kullanılır:

| Operatör | Anlam |
|---|---|
| `<<<` | Bit dizisini sola kaydır |
| `>>>` | Bit dizisini sağa kaydır |

```nexus
v: u8 = 1;
v:left!u8 = v <<< 2;    // 00000001 -> 00000100
v:right!u8 = left >>> 1; // 00000100 -> 00000010
```

Kaydırma işaretli türlerde de yalnızca bit dizisini işler; işaret biti özel korunmaz. Boşalan bitler sıfırla doldurulur, genişlik dışına çıkan bitler atılır. Kaydırma miktarı tür genişliğine eşit veya büyükse sonuç sıfırdır. Sonuç türü sol operandın türüdür. İki operand da integer olmalıdır; kaydırma miktarının integer türü sol operanddan farklı olabilir. Sabit negatif miktar Semantic hatasıdır; dinamik negatif miktar çalışma zamanında trap üretir. Bu aşamadaki Semantic ve OIR lowering kapsamı integer türleridir; Nbit üzerinde kaydırma, bit-vektör OIR işlemleri tamamlandığında ayrıca açılacaktır. Nbyte ham bloklarında bit kaydırma doğrudan tanımlı değildir.

OIR opcode karşılıkları `bit.shl` ve `bit.shr` olarak tanımlanmıştır. Bağımsız Verifier, sol operand/sonuç türlerinin aynı integer türü olmasını ve kaydırma miktarının integer olmasını doğrular. İlk lowering dilimi tek yapılandırılmış scalar türle sınırlıdır; farklı genişlikte sağ operandın genel lowering desteği henüz tamamlanmamıştır.



####  2.7 Özel Operatörler**

* `.(nokta)` :  Grup/Namespace çözümleyici.Üye erişimi (Metot veya özellik).
* `..` : Aralık (Range) belirtme; iki uç da dahildir (`1..10` = 1, 2, …, 10; toplam 10 değer). Tek bir aralık ifadesinde ikinci `..` kullanılamaz; aralık zincirleme yazımı geçersizdir.
* `...` : Fonksiyonlar için çoklu prametre
* `.=` : String birleştirme operatörü. Mevcut string'e yeni string ekler.

**Legacy unpack notu:** Eski `(data) ...> (h)` biçimi aktif Nexus Flow operatörü değildir ve Core Parser tarafından reddedilir. Aynı amaç gerekirse özel sembol yerine normal bir modül fonksiyonu olarak tasarlanacaktır (önerilen ad: `coremodule.unpack(data)`). Bu fonksiyonun veri biçimi, dönüş türü ve hata sözleşmesi tanımlanmadan standart API kabul edilmeyecektir. Dosya açma/okuma, veri çözme ve normalizasyon ayrı sorumluluklardır. `AST_UNPACK` kimliği codec uyumluluğu için ayrılmıştır; Parser bu düğümü üretmez.

**C Parser uygulama sınırı (2026-11-11):** Parser `^^` (sağ birleşimli), binary bitwise `& | ^` (AND > XOR > OR), sayısal bit kaydırma `<<< >>>`, Flow Feed / Zone Write `<< >>`, `+=`, `-=`, `*=`, `/=`, `&=`, `|=`, `^=` ve `.=` operatörlerini ilgili AST biçimlerinde ayrıştırır. Sayısal shift `AST_BINARY_EXPR` ve OIR `bit.shl` / `bit.shr` opcode'larını (26/27) kullanır; Flow operatörleri `AST_NEXUS_PIPE` / `AST_NEXUS_FLOW` yolunda kalır. Shift'in iki operandı integer olmalıdır; sağ operandın genişliği sol operanddan farklı olabilir. Verifier sol operand/sonuç türü eşitliğini ve integer shift-count türünü denetler; mixed-width RHS için bağımsız Verifier testi vardır. Semantic sabit negatif miktarı reddetmeli, dinamik negatif miktar trap sözleşmesini sağlamalıdır; scalar lowering kapsamı ve Nbit/bit-vector desteği henüz tam değildir. Logical `&&` / `||` Semantic bool kontrollerine sahiptir; kısa devre yürütme OIR/lowering aşamasında ayrıca doğrulanmalıdır. Select/scan pattern kollarındaki `when condition` guard'ı Parser tarafından kabul edilir; AST'de kol gövdesi `AST_IF(condition, body)` biçiminde kanonikleştirilir. Scan içindeki `else` catch-all kolu da Parser tarafından kabul edilir; AST'de `AST_SCAN_ARM.pattern == NULL` olarak temsil edilir ve son kol olmak zorundadır. Guard, pattern binding'leri tanımlandıktan sonra aynı kol scope'u içinde Semantic tarafından bool koşulu olarak denetlenir. Constructor içindeki binding'lerin subject/enum alan tiplerine bağlanması henüz tamamlanmadığından, guard içindeki sayısal karşılaştırmalar için tip çıkarımı şu an garanti edilmez. Ayrıntılı uygulama matrisi: [NEXSUS_AST_SYNTAX_MATRIX.md](../../NEXSUS_AST_SYNTAX_MATRIX.md).

---


---

## Bölüm 3. NEXUS AKIŞ OPERATÖRLERİ (THE 15 KEYS)**

| Öncelik | Operatör | Adı | İşlevi / Açıklaması |
| --- | --- | --- | --- |
| **1** | `(h)` | **Handler (Masa)** | Bellek adresini rezerve eder. İşlem öncesi masa hazır olmalıdır. |
| **2** | `<-` | **Capture (Yakala)**| Değeri veya sonucu masaya kilitler. |
| **3** | `_>` | **Relocate** | Masayı/Context'i başka bir bölgeye taşır (Örn: GPU'ya). |
| **4** | `?(n,ms)`| **Rolling** | Hata durumunda n kez ms milisaniye bekleyerek tekrar dener. |
| **5** | `?->` | **Catch (Saptırıcı)**| deneme başarısızsa akışı hata bloğuna sapıtır. |
| **6** | `?=>` | **Fallback** | Hata saptırıcı yoksa veya veri `null` ise alternatif veri enjekte eder. |
| **7** | `->`  | **Success (Başarı)**| Veriyi bir sonraki işleme/fonksiyona taşır. |
| **8** | `!->` | **Ignore (Sessiz)** | Hata olsa bile akışı zorla devam ettirir. |
| **9** | `<<`  | **Zone Write** | Veriyi atomik olarak paylaşımlı belleğe yazar. |
| **10** | `>>`  | **Flow Feed** | Veriyi atomik olarak akışa besler. |
| **11** | `(e){}`| **Error Block** | Saptırılan hatanın işlendiği son durak. |
| **12** | `?;` / `}?;` | **Halt** | `?;` tek kaynağı/masayı kapatır; blok sonundaki `}?;` blokta kalan kaynakları topluca kapatır. |
| **13** | `!!`  | **Panic** | Kritik durdurma. |
| **14** | `@`   | **Reserved / Deferred** | Intent tasarımı etkinleştirilene kadar Core Parser tarafından kabul edilmez. |
| **15** | `!!=` | **Directive** | .target sistemi ve derleyici talimatları. |

> `!!=` tokenı mevcut Parser'da infix assignment düğümü olarak da tüketilir. `!!=[target = ...] { ... }` biçimindeki prefix target-directive örnekleri normal Parser'ın tamamlanmış grameri değildir; Directive/Target katmanı sözleşmesi ayrıca kapanmalıdır.
>
> `??` AI-logic tokenı/AST ailesi olarak ayrılmıştır; sorgu payload'ı, bağlamı ve Semantic davranışı normatif olarak tanımlanmamıştır.
>
> `=?>` ve `<--` eski Flow token yazımlarıdır; güncel Core Parser gramerinde yer almaz. Koşullu statement kontrolü `(koşul) ? ...` biçimini, foreach döngüsü ise `loop(item in collection)` biçimini kullanır.

---


## Bölüm 4. MASA (HANDLER) VE SAHİPLİK MEKANİZMASI

### 4.1 Masa Kuralları
- **Table Occupied:** `(h)` doluysa ve `<-` ile yeni veri yazılmaya çalışılırsa derleyici hata verir. Masa boşaltılmalıdır.
- **Move-on-Flow:** `(h) -> write()` işlemi veriyi kopyalamaz, **taşır**. İşlem sonunda `(h)` sıfırlanır (Nulled).
- **Zero-Cost Handover:** Masalar arası transfer sadece adres etiketinin değişimidir.


### 4.1.1 `(h)` Masa (Handler) Yaşam Döngüsü

* **Scope:** `(h)` sadece tanımlandığı blokta geçerlidir. Başka bloklardan doğrudan erişilemez.
* **Ömür (Lifetime):** `?;` herhangi bir uygun noktada belirtilen tek kaynak/masayı kapatır. `}?;` blok kapanışına eklenir ve o blokta açılmış, hâlâ o bloğa ait tüm açık kaynakları topluca kapatır.
* **Transfer:** Başka bloğa taşımak için `_>` kullanılır. Taşındığında orijinal masa **silinir**.
* **Güvenlik Etkisi:**

  * Ömrü garanti altındadır → dangling pointer riskini minimize eder.
  * Başka bloklar `(h)`'yi okuyamaz → izinsiz erişim yok.


---

### 4.1.2 Halt: Tekil Kaynak ve Blok Kapsamlı Temizlik

Halt iki ayrı operatör değildir; aynı `?;` işleminin hedef kapsamı farklıdır:

- **Tekil Halt — `(resource)?;`:** Herhangi bir uygun statement noktasında kullanılabilir. İlgili kaynak/masa kapatılır veya temizlenir. Örnekler: açık dosyayı kapatmak, geçici tabloyu temizlemek, yüklenmiş DLL'yi kaldırmak.
- **Blok Halt — `}?;`:** Bir bloğun kapanışına eklenir. O blok içinde açılmış ve hâlâ o bloğun sahipliğinde bulunan tüm açık kaynakları tek tek kapatma gerektirmeden topluca sonlandırır.

Örnekler:

```nexus
(file) <- file.open("data.bin", r);
file.read(file);
(file)?;                 # Yalnızca bu dosya kaynağını kapat.

(table) <- table.create();
table.insert(table, row);
(table)?;                # Yalnızca bu tabloyu temizle.

(Optimizer) <- dll.load("optimizer.dll");
Optimizer.run(Optimizer);
(Optimizer)?;                  # Yüklenen DLL kaynağını kaldır.
```

Blok kapsamlı kapatma:

```nexus
{
    (input)  <- file.open("input.bin", r);
    (output) <- file.open("output.bin", w);
    (socket) <- net.connect(endpoint);
    process(input, output, socket);
}?; # Blokta kalan üç kaynağın tamamını kapat.
```

**Sahiplik sınırları:**
- Blok Halt yalnızca o blokta oluşturulmuş ve hâlâ o bloğa ait kaynakları kapatır.
- `_>` ile başka kapsama taşınmış kaynak artık eski bloğun temizleme listesinde değildir.
- Dış kapsamdan ödünç alınmış kaynak, açık bir sahiplik devri olmadan iç blok tarafından kapatılamaz.
- Tekil Halt sonrası kaynak/handler geçersiz duruma geçer; sonraki kullanım semantik hata olmalıdır.
- Temizleme deterministik olmalı; her kaynak en fazla bir kez sonlandırılmalıdır.
- Kaynak kapatma hatalarının cleanup sırası ve hata birleştirme davranışı runtime sözleşmesinde ayrıca tanımlanmalıdır.

Bu bölüm hedeflenen dil semantiğidir. C Parser tekil kaynak Halt için `AST_NEXUS_HALT`, blok kapanışındaki `}?;` için ayrı `AST_BLOCK_HALT` düğümü üretir. `AST_BLOCK_HALT` yalnızca ait olduğu bloğun son çocuğu olabilir. Kaynakların sahiplik listesine kaydı, cleanup sırası ve hata politikası Semantic/OIR lowering aşamalarında ayrıca uygulanmalıdır; bu AST temsili tek başına çalıştırılabilir cleanup garantisi vermez.

---

### 4.2 Standart Masa Takma Adları (Shims)
- `(h)` : Handle/Header (Dosya/Sistem kaynakları).
- `(e)` : Handle/Header (Hata işleme).
- `(ok)`: Boolean/Result (Kontrol mekanizmaları).
- `(v)` : Value (Ham veriler).
- `(v, val)` : Value (Ham veriler).
- `(it)`: Iterator (Döngü akışları).

---

### 4.3  Nexus Flow Mantığı
>* Kural: `(h)<- fn() ?->{}` operatörü. `(false, null, none, unknown)` hata/boşluk durumlarını anlatır.
>  Ternary kontrole benzer: Başarılı ise sola, başarısız ise akış sağa kayar.
>  Bu listede geçen `null` artık ayrı bir literal tokenıdır ve null pointer değerini ifade eder. `none` henüz literal değildir; Option/Maybe türünün boş varyantı ayrıca tanımlanmalıdır. `null` değerinin yalnızca nullable pointer hedeflerine atanması Semantic doğrulamasında zorunlu kılınacaktır.
```
# Hata Saptırmalı Akış,  Hatayı işlemek için Hata işleme bloğuna iletir. 
(h) <- file.open("a.txt",r) ?-> (e){ echo(e); };
(h) -> process(h);

```

**ÖRNEK AKIŞ (SAMPLE FLOW)**

```
# İki dosyayı aç, veriyi taşı ve kapat
(in)  <- file.open("input.txt",r)  ?-> !!("Giriş dosyası yok");
(out) <- file.open("output.txt",w) ?-> !!("Çıkış dosyası yok");

(in, out) -> {
    v:data <- file.read(in);
    file.write(out, data);
}?; # Akış biter, masalar imha edilir.

```




## Bölüm 5. KONTROL YAPILARI (CONTROL FLOW)

### 5.1 Unified Loop (Tekil Döngü)
Tüm döngü varyasyonları `(loop)` çatısı altındadır.

```
# Range Loop
loop(1..100){ echo(it) } // 1 ve 100 dahil

# For Loop
loop(i, a < b, i++){... kodlar...}

# Foreach
loop(v:item in list){ echo(item) }
                         
# Koşullu loop (while-benzeri; ayrı while anahtar kelimesi yok)
loop(a < b){ ....  a++;}
loop(a < b, a++){ .... }

# Loop (infinity)
loop{
    process();
    (a == EOF) ? break;
}

# Rolling Loop (Zaman Ayarlı) Hata saptırma.. Hata saptırmak yerine bir etiketede atlayabilir. ?(5, 500ms)->(newhttp);
# Aşağıdaki kod http.open() başarılı olursa kontrol sola akar. 
# Başarısız olursa toplamda 5 kez tekrar dener her denemede 500ms bekler. Hala hata alırsa kontrol sağa kayar.
(h) <- http.open("www.indir.net?545.zip")?(5, 500ms)->(e){echo("hata: {e} /n")};
(h) -> ......

# hata üretmesin alternatifi işlesin.
(h) <- translate("google",data")?(3, 500ms)?=> (data); # hata durumunda datayı orjinal halinde (h) iletir.
(h) -> ......


# unroll(c) ->loop... Döngüyü derleme anında düzler (Zero-jump).
f:fast_process() {
    v:data = [10, 20, 30]

    # Normalde (loop) ile dönerdik ama sayı sabitse:
    unroll(3) -> loop(item in data) {
        # Derleyici burayı 3 kez kopyalar
        core.io.print(data[item]) 
    }
}

f:send_packets() {
    v:ports = [80, 443, 8080]

    # Derleyici burayı 3 bağımsız fonksiyon çağrısı gibi aşağı doğru açar.
    unroll(3) -> loop(p in ports) {
        # Artık 'p' bir değişken değil, o adımın sabit değeridir.
        core.net.open_port(p)
    }
}

```

### 5.2 - 6: Dallanma (If / Elsif / Els)

#### Koşullu Kontrol Operatörü `? :`

Koşulun sonucuna göre tek bir kontrol veya işlem seçilir. Bu yapı ternary kontrol mantığıyla çalışır; değer döndürmek zorunda değildir.

- `(koşul) ? işlem;` — koşul doğruysa işlem çalışır. Yanlışsa normal akış devam eder.
- `(koşul) ? işlem_A : işlem_B;` — doğruysa A, yanlışsa B çalışır.
- `:` ve alternatif kol zorunlu değildir.
- Her iki kolda da normal ifadeler, bloklar veya kontrol aktarımı bulunabilir.
- `break` ve `continue` yalnızca geçerli bir loop kapsamı içindeyken kullanılabilir.
- Bu operatör, Bölüm 3'teki veri/işlem akışı için kullanılan `->` operatöründen ayrıdır.
- Tek satırlı kollar bir statement olmalı ve `;` ile biter. Blok kolları `{ ... }` ile biter.
- Koşul parantezli yazılır; böylece `?` işareti Rolling (`?(n,ms)`) ve Halt (`?;`) kullanımlarından parser tarafından ayırt edilir.
- Lexer, `?->`, `?=>` ve `?;` gibi mevcut çok karakterli operatörleri tek token olarak önce tanır. Koşullu kontrolün `?` token'ı yalnızca tamamlanmış `(koşul)` sonrasında parser tarafından kabul edilir.

Dilbilgisi:

```text
ConditionalControl := "(" Expression ")" "?" ControlArm [ ":" ControlArm ]
ControlArm         := Statement | Block
```

Bu yapı statement seviyesinde kontrol seçimidir; kendi başına ternary değer üretmez.

Örnekler:

```nexus
(a == EOF) ? break;
(a == 5) ? break : continue;

(a > 10) ? process(a);
(a > 10) ? process(a) : process(b);

(a == 0) ? {
    echo("zero");
} : {
    echo("non-zero");
}
```

Koşullu kontrol, OIR aşamasında iki yönlü CFG dallanmasına dönüştürülür. Alternatif kol yoksa FALSE yolu doğrudan devam block'una bağlanır. Operatörün kendisi bir değer üretmez; seçilen kolun sonucu normal ifade/statement kurallarına tabidir.

```
# Standart Karşılaştırma
A:  if(a == b){ ... }
    els { ... }

B:  if (a == b){ ... }  
    elsif(a >= 1){ ... }  
    els { ... }  
    
C:  if (a == b) status = 1; 
    els status = -1;
    
D:  (a == b) ? break;

E:  (a == b) ? break : continue;

F:  (a == b) ? process(a);

G:  (a == b) ? process(a) : process(b);

```



### 5.3 Etiketli atlama: `label:>`, `?>(label)` ve `jump(label)`

> `?>(label);` or `jump(label);` targets a label declared with `label:>`.
| **Atlama (Jump)** | Akışı tamamen başka bir etiketli `label:>` noktaya transfer eder. |

```nexus
jump(out);
# İleri veya geri tanımlanmış bir label hedeflenebilir.

out:> {
    process();
}
```


**Önemli kontrol akışı kuralı:** `jump(label)` aynı basic block içinde bir konuma atlamakla sınırlı değildir. Hedef etiket, fonksiyon içindeki başka bir basic block'un giriş noktası olabilir. Jump, bulunduğu block'u sonlandırır ve kontrolü hedef block'a aktarır; aradaki kaynak satırları yürütülmez. Hedef block kaynak kodda ileri veya geri konumda bulunabilir. Bu nedenle `jump`, fonksiyon içindeki farklı kontrol akışı bölgelerine doğrudan geçiş yapmak için kullanılan genel etiketli aktarım komutudur. Fonksiyon kapsamı ve lexical-scope güvenliği ayrıca denetlenir.

**Çoklu giriş ve ölçeklenebilirlik:** Aynı hedef label'a fonksiyon içindeki birden fazla farklı block'tan `jump(label)` ile ulaşılabilir. Bir hedefin predecessor sayısı veya fonksiyon içindeki toplam jump sayısı için dil seviyesinde keyfî sabit bir sınır tanımlanmaz. Gerçek sınırlar OIR formatının temsil kapasitesi, kullanılabilir bellek ve uygulama güvenlik limitleridir; bunlara ulaşıldığında derleme açık hata vermeli, atlama/kenarları sessizce kaybetmemelidir. Her basic block tek bir terminator ile sona erer; birden fazla çıkış gerekiyorsa ilgili koşullu veya çok seçenekli kontrol yapısı ayrı CFG kenarları üretir.


#### Jump ve lexical scope kaynak ömrü

`jump(label)` bir basic block'tan diğerine kontrol aktarır; basic block değişimi tek başına kaynak ömrünü bitirmez. Kaynak ömrü lexical scope'a bağlıdır.

- Hedef label, kaynak noktanın içinde bulunduğu lexical scope ile aynı scope'ta veya onun bir üst scope'unda olmalıdır. Daha derin bir scope'a ya da kaynak scope ile kardeş olan başka bir scope'a atlayarak o scope'un giriş/başlatma adımlarını atlamak geçersizdir.
- `jump` aynı lexical scope içinde başka bir basic block'a gidiyorsa scope hâlâ aktiftir. Daha sonra aynı scope içindeki önceki bir etikete geri dönmek de mümkündür; bu durumda scope'a ait masalar ve kaynaklar erken temizlenmez.
- `jump` bir veya daha fazla lexical scope'tan çıkıyorsa, terk edilen scope'ların yaşam süresi sona erer. Bu scope'lara ait otomatik kaynak temizliği, normal scope çıkışıyla aynı şekilde çalıştırılmalıdır.
- Daha sonra terk edilmiş bir scope'un içindeki etikete yeniden girilerek eski kaynak aktivasyonunun sürdürülmesine izin verilmez. Yeni bir scope aktivasyonu ancak normal scope giriş yoluyla oluşturulabilir.
- `}?;` blok kapsamındaki masaların/kaynakların toplu kapatılması için kullanılır. Yalın `?;` Halt/sonlandırma davranışıdır; bir kaynak/masa/DLL hedefine uygulanmış biçimi (ör. `(Optimizer)?;`) o hedefi serbest bırakır. Bu hedefli kullanım, `jump` için genel bir cleanup komutu yerine geçmez.
- Kaynağın `_>` ile başka bir scope'a taşınması halinde sahiplik transferi kuralları geçerlidir; eski scope aynı kaynağı ikinci kez serbest bırakamaz.

Bu kuralların OIR karşılığı ve negatif/pozitif testleri, Multi-Rule OIR test **MR-OIR-016** içinde tanımlanır.

### 5.3.1 NEXUSFLOW: SELECT ve SCAN

NexusFlow'da `select` ve `scan` benzer şekilde dallanma oluşturur; ancak aynı işi yapmazlar:

- `select(expr)`: Bir ifadenin değerini seçeneklerle karşılaştırır. İlk eşleşen branch çalışır.
- `scan expr`: Bir enum, sealed type veya struct değerinin yapısını/pattern'ini inceler. Eşleşen varyantın taşıdığı alanları değişkenlere bağlayabilir.
- İkisi de **statement** yapısıdır; kendi başlarına değer üretmezler.
- Her iki yapıda da branch gövdeleri normal NexusFlow ifadeleri, bloklar veya başka kontrol yapıları içerebilir.

#### A) SELECT — Değere göre seçim

`select`, subject ifadesini değerlendirir ve sonucu branch pattern'leriyle karşılaştırır. Eşleşen ilk branch çalıştırılır; sonraki branch'lere devam edilmez.

Desteklenen branch biçimleri:

| Pattern | Anlamı |
| --- | --- |
| `value -> { ... }` | Tek bir değerle eşleşir. |
| `value1 | value2 -> { ... }` | Verilen değerlerden herhangi biriyle eşleşir. Tek branch'tir. |
| `start..end -> { ... }` | Belirtilen aralıkla eşleşir. |
| `default -> { ... }` | Hiçbir önceki branch eşleşmediyse çalışır. |

Örnek:

```nexus
select(choice) {
    1 | 2 -> {
        echo("Bir veya iki");
    }

    3..5 -> {
        echo("Üç ile beş arası");
    }

    default -> {
        echo("Diğer değer");
    }
}
```

Bu örnekte:

1. `choice` değerlendirilir.
2. Değer 1 veya 2 ise ilk branch çalışır.
3. İlk branch eşleşmez ve değer 3 ile 5 arasındaysa ikinci branch çalışır.
4. Hiçbiri eşleşmezse `default` çalışır.

`1 | 2` iki ayrı branch anlamına gelmez; aynı gövdeye yönlenen çoklu değer pattern'idir. `default` isteğe bağlıdır. Yazılmamışsa eşleşme bulunmayan durumda bu `select` için ek bir branch çalışmaz.

#### B) SCAN — Tür ve yapı pattern eşleştirmesi

`scan`, subject değerinin türünü/varyantını inceler. Enum ve sealed type varyantları eşleştirilebilir; pattern içindeki parametreler yerel değişkenlere bağlanabilir. Struct pattern'leri de aynı pattern mekanizmasının kapsamındadır.

Temel pattern biçimleri:

| Pattern | Anlamı |
| --- | --- |
| `TypeName -> { ... }` | Parametre taşımayan varyant/tür eşleştirmesi. |
| `TypeName(a,b) -> { ... }` | Varyantın alanlarını `a`, `b` değişkenlerine bağlar. |
| `TypeName(a,b) when cond -> { ... }` | Tür/pattern eşleşmesine ek koşul uygular. |
| `else -> { ... }` | Hiçbir pattern eşleşmediğinde varsayılan kol. |

Örnek enum ve kullanım:

```nexus
nt:enum: Task {
    IO(file!str)
    Compute(a!i32, b!i32)
    Network(url!str)
    Cleanup
}

v:current_task!Task = Compute(10, 20)

scan current_task {
    IO(f) -> {
        echo("Dosya: {f}");
    }

    Compute(a,b) when a>0 && b>0 -> {
        v:result = a + b
        echo("Compute sonucu: {result}");
    }

    Network(url) -> {
        echo("Adres: {url}");
    }

    Cleanup -> {
        echo("Temizlik işlemi");
    }

    else -> {
        echo("Bilinmeyen görev");
    }
}
```

**Pattern binding kapsamı:** `IO(f)` eşleştiğinde `f`, yalnızca ilgili branch gövdesinde kullanılabilir. `Compute(a,b)` için de `a` ve `b) aynı şekilde yalnızca o branch'e aittir. Bu isimler dış kapsama taşınmaz ve başka branch'lerin binding'leriyle çakışmaz.

**Guard (`when`):** Önce varyant/pattern eşleşmesi yapılır; ardından `when` koşulu değerlendirilir. Guard doğruysa gövde çalışır. Guard yanlışsa o pattern kolu seçilmez ve tarama sonraki uygun pattern'lerle devam eder. Guard ifadesinin türü `bool` olmalıdır.

#### C) SELECT içinde SCAN kullanımı

İki yapı iç içe kullanılabilir. Dıştaki `select` genel işlem yolunu, içteki `scan` ise seçilen yolun içindeki tür/varyantı belirler.

```nexus
select(choice) {
    1 | 2 -> scan current_task {
        IO(f) -> {
            echo("Dosya işlemi: {f}");
        }

        Compute(a,b) when a>0 && b>0 -> {
            v:result = a + b
            echo("Compute sonucu: {result}");
        }

        Network(url) -> {
            echo("Ağ işlemi: {url}");
        }

        Cleanup -> {
            echo("Temizlik işlemi");
        }

        else -> echo("Bilinmeyen görev")
    }

    3..5 -> {
        echo("Fast path branch");
    }

    default -> {
        echo("Geçersiz seçim");
    }
}
```

Burada dıştaki `select` yalnızca `choice` değerine bakar. Değer 1 veya 2 olduğunda ilgili branch içindeki `scan` çalışır. `scan` ise `current_task` değerinin hangi varyant olduğunu bulur.

#### D) SCAN ve diziler

`scan` subject'i bir dizi olduğunda dizi elemanları üzerinde pattern taraması yapılabilir. Her eleman, ilgili pattern'lerle eşleştirilir. Bu kullanım, tek bir enum değerini taramaktan farklı olarak sequence/iterator akışı gerektirir.

```nexus
v:loop_tasks!Task[] = [Compute(1,2), IO("a.txt"), Cleanup]

scan loop_tasks {
    Compute(a,b) -> loop(i, 0..2) {
        echo("Compute loop: {i}, değerler: {a}+{b}");
    }

    IO(f) -> {
        echo("Dosya okundu: {f}");
    }?;

    Cleanup -> echo("Temizlik yapılıyor")

    else -> echo("Bilinmeyen görev")
}
```

Dizi kullanımında pattern binding'leri ilgili elemanın eşleşmesi sırasında oluşturulur. Derleyici bu modu OIR'de iterator ve loop kontrol akışıyla temsil eder.

**Açık semantik notu:** Dizi taramasında `else` kolunun her eşleşmeyen eleman için mi çalışacağı, yoksa tüm dizi taramasında hiçbir eşleşme bulunmadığında bir kez mi çalışacağı ayrıca kesinleştirilecektir. Bu davranış netleşmeden derleyiciye varsayılan bir yorum yüklenmemelidir.

#### E) Derleyici ve OIR davranışı

Parser, `select` ve `scan` için ayrı AST düğümleri üretir. Analyzer pattern türlerini, binding sayısını/türlerini, guard koşullarını, tekrar eden pattern'leri ve enum/sealed type kapsamını doğrular.

OIR aşamasında:

- `select` sabit ve uygun tamsayı pattern'lerinde jump table'a dönüştürülebilir.
- Aralıklar veya jump table için uygun olmayan pattern'ler karşılaştırma/dallanma zinciriyle işlenebilir.
- `scan` önce tür/varyant etiketini kontrol eder; yalnızca eşleşme doğrulandıktan sonra payload alanlarını çıkarır.
- `when` guard'ı pattern eşleşmesinden sonra ayrı bir koşullu dallanma olarak uygulanır.
- Her branch normal tamamlandığında ortak çıkış bloğuna bağlanır. `ret`, `break` veya `continue` ile sonlanan branch'ler kendi kontrol aktarımını korur.
- Hedef işlemci jump table desteklemiyorsa Codegen aynı davranışı karşılaştırma ve dallanma komutlarıyla üretir.

Özetle: `select` **değer seçer**, `scan` ise **yapı/varyant eşleştirir**. İkisi statement tabanlıdır ve birbirlerinin yerine kullanılmak zorunda değildir.

Ayrıntılı tasarım: [NexusFlow Scan Select Syntax Spec.md](Scan%20Select%20Syntax%20Spec.md)

### 5.3.2 NEXUSFLOW: ÇOKLU KURAL İŞLEYİCİ (MULTI-RULE ENGINE)

Multi-Rule Engine, fonksiyonun ön koşullarını topluca doğrular ve koşullara bağlı trigger'ları/aksiyonları yönetir. Kural kümeleri `rules` ile tanımlanır ve `apply rules(...)` ile fonksiyon akışına uygulanır.

#### 1. Kural tanımı

```nexus
rules CNC_Safety(w, h, tool) {
    # Bariyerler: tümü test edilir, hatalar birlikte raporlanır.
    w > 2800 :=> "Genişlik sınırı aşıldı";
    h > 2100 :=> "Yükseklik sınırı aşıldı";

    # Normal trigger kuralları: eşleşenlerin tamamı kaydedilir.
    (w == h) -> SquareMode;
    tool == "Diamond" -> HighSpeed;

    # İsteğe bağlı özel kurallar:
    always -> LogMetrics;
    default -> StandardSettings;
}
```

#### 2. Bariyerlerin toplu değerlendirilmesi

Bir RuleSet içindeki bütün `:=>` bariyerleri değerlendirilir. Koşulu doğru olan her bariyerin mesajı hata listesine eklenir. İlk hatada durulmaz.

- Hiç hata yoksa trigger değerlendirmesine geçilir.
- Bir veya daha fazla hata varsa fonksiyon durur ve tüm hata mesajları birlikte raporlanır.
- Bariyer hatası oluştuğunda o RuleSet'in trigger'ları, `on` handler'ları ve normal fonksiyon akışı çalıştırılmaz.

Örneğin hem genişlik hem yükseklik sınırı aşılmışsa iki mesaj da raporlanır; fonksiyon tek bir başarısızlık sonucu ile durdurulur.

#### 3. Trigger ve on handler'ları

Bariyerlerin tamamı başarılıysa bütün normal trigger koşulları değerlendirilir. Birden fazla koşul doğruysa tüm trigger adları kaydedilir; yalnızca ilk eşleşme seçilmez.

```nexus
f:buildPart(w, h, tool) {
    apply rules(CNC_Safety(w, h, tool));

    on CNC_Safety.SquareMode {
        v:angle = setAngle(90);
    }

    on CNC_Safety.HighSpeed {
        v:rpm = 18000;
    }

    cut(rpm, mode, angle);
}
```

`on` blokları derleme sırasında ilgili RuleSet/trigger çiftiyle eşleştirilir. İlgili handler'lar, başarılı `apply rules` değerlendirmesinin parçası olarak çalışır. Birden fazla trigger eşleşirse handler'lar kaynak sırasına göre yürütülür. Handler tanımlanmamış trigger hata değildir; yakalanmamış olay olarak kalır.

#### 4. Birden fazla apply rules

`apply rules` çağrıları kaynak kod sırasıyla yürütülür. Önceki RuleSet başarılıysa sıradaki değerlendirilir. Herhangi bir RuleSet bariyer hatası üretirse o RuleSet'in bütün bariyer mesajları raporlanır; sonraki `apply rules` çağrıları ve fonksiyonun normal akışı çalıştırılmaz.

#### 5. always ve default (isteğe bağlı)

İkisi de zorunlu değildir. Ok operatörlü biçimde sağ taraf bir **trigger adıdır**:

```nexus
always -> LogMetrics;
default -> Success;
```

- `always -> LogMetrics;`: Bariyer aşaması başarılı olan her uygulamada `LogMetrics` trigger'ını üretir.
- `default -> Success;`: Hiçbir normal trigger eşleşmediğinde `Success` trigger'ını üretir. Bu trigger, `on RuleName.Success { ... }` ile yakalanabilir.
- Doğrudan aksiyon çalıştırmak için blok biçimi kullanılır:

```nexus
always {
    writeMetrics();
}

default {
    applyStandardSettings();
}
```

Blok biçiminde aksiyon doğrudan yürütülür. `always` ve `default` tanımları yoksa otomatik bir işlem oluşmaz.

#### 6. Derleyici temsili

Parser; `AST_RULESET`, `AST_BARRIER`, `AST_TRIGGER`, `AST_APPLY_RULES` ve `AST_ON` düğümlerini oluşturur. Analyzer isim çözümlemesi, argüman/tip uyumu, yinelenen trigger adları ve handler eşleşmelerini doğrular.

OIR Builder, her bariyer için koşul dallanması ve hata biriktirme adımı üretir. Tüm bariyerler test edildikten sonra hata listesi boş değilse bütün kayıtlar raporlanır ve ayrı bir `RuleSetFailureExit` yoluna gidilir; bu yol normal `RETURN` ile aynı değildir. Hata yoksa bütün normal trigger koşulları birer kez değerlendirilir ve sonuçları SSA boolean değerleri olarak saklanır. Ardından eşleşen `on` handler'ları kaynak sırasıyla koşullu bloklar halinde yürütülür. Sonrasında varsa `always`, normal trigger eşleşmemişse varsa `default` aşaması çalışır. Canlı başarılı yollar `ApplyContinue` bloğunda birleştirilir.

Trigger adları Analyzer aşamasında sabit kimliklere çözülür; OIR'ın çalışma zamanında metin karşılaştırması yapması gerekmez. Handler gövdesi `ret` veya başka bir sonlandırıcı kontrol akışı içerirse sonraki handler'a yapay bir CFG bağlantısı kurulmaz. Hata biriktirme işlemlerinin nihai opcode/runtime ABI karşılığı ayrıca belirlenecektir.

> Not: `group` içindeki `rules ... => group { ... }` biçimi temel `rules Name(params) { ... }` sözdiziminden ayrı bir organizasyon katmanıdır; ayrı lowering/çözümleme adımı olarak ele alınacaktır.

Ayrıntılı AST, Analyzer ve OIR semantiği: [Multi-Rule Engine Formal Specification](Multi-Rule%20Engine%20Formal%20Spec.md).

---

### 5.4 Group Metodu

*Group Fonksiyonu (Metodu) Birbirlerine benzeyen veya belirli bir grup içerisinde, benzer bir amaca hizmet eden fonksiyonları bir araya getirmektir.*
*Özellikle Kütüphane yazımını ve kullanımını kolaylaştırmak için oluşturulmuştur.*
```
# db Grubu.
group db{
    # nosql Alt grup oluşturma.
    nosql => group{
        open => f:(...){....}

        close => f:(...) {....}

        new => f:(...){....}
        
        qurey => f:(...){....}
        
        help =>f:(){.....}
        # default opsiyonel
        default { echo("Yardım için "db.nosql.help" kullanın."); }
    }
    # sqlite Alt grup oluşturma.
    sqlite => group{
        # Alt group fonksiyonları.
        open => f:(...){....}

        close => f:(...) {....}

        new => f:(...){....}
        
        qurey => f:(...){....}
        
        help =>f:(){.....}
        # default opsiyonel
        default { echo("Yardım için "db.sqlite.help" kullanın."); }
    }
    # default opsiyonel
    default { echo("Yardım için "db.sqlite.help yada db.nosql.help" kullanın."); }
}
# Kullanım:
db.nosql.open("category.db");
db.sqlite.open("product.db");
db.sqlite # default çalıştır.
db  # default çalıştır.

# Olanaklar çoktur. Örnek: file grubu, file ile ilgili tüm fonksiyonları aynı yerde toplar.
# (f) <- file.open("test.txt",rw)?->(e){echo("Dosya Açılamadı.");};
#  .....
# file.close(f);

```

### 5.5 - 5: `_>` (Relocate)

### 5.6 Named Thread Block

Nexus Flow'da isimlendirilmiş thread çalışma gövdesi şu biçimdedir:

```nexus
(worker)>{ 
    v:value = 7;
}
```

- `worker` blok kimliğidir; `>{` özel tokenı thread bloğunu açar.
- Parser bunu `AST_THREAD_BLOCK` düğümüne dönüştürür; isim ve gövde AST'de korunur.
- Bu yazım tek başına thread başlatma/spawn işlemi değildir; isimli çalışma gövdesini tanımlar.
- `mode` ve `timeout` AST alanları şimdilik ayrılmıştır ve Parser bunları `NULL` bırakır. Bu alanlara ait syntax henüz tanımlanmamıştır.
- `spawn(worker)` core Nexus Flow Parser syntax'ı değildir. Spawn/start/join ve scheduling davranışı ayrı Thread modülünün sorumluluğundadır.



* **Stratejik Konum:** Bir veri masaya yakalandıktan (`<-`) ve paket açıldıktan (`expand`) hemen sonra, 
* eğer o verinin başka bir bellek bölgesinde işlenmesi gerekiyorsa (örneğin GPU belleğine taşıma veya başka bir thread handler'ına devretme), 
* bu işlem tüm mantıksal kararlardan (`?`, `->`, `>>`) önce yapılmalıdır.

```
(h1) <- ...........;
(h1) -> expand(data) _>(h2);
....
....
(h2) -> process >> final_output;
```
*(H1 masasına veriyi yakala, paketi aç, komple H2 masasına transfer et, sonra işleyip nihai hedefe akıt.)*

---

## Bölüm 6. FONKSİYONLAR (FUNCTIONS)
| Açıklama | Tanımlama | Syntax |
| -------- | --------- | ------
* **Main** | `f:main(argc, argv)!i32 { ... }` |
  
**Process CLI argument contract (normative):** The entry function's `argc` is the number of command-line arguments excluding the executable path; `argv` is a dynamic one-dimensional `str[]` in the same order, also excluding executable path. `argc == len(argv)`; empty invocation gives 0 and an empty array; no NULL sentinel. Runtime creates both before calling `main`; AST treats them as ordinary parameters. Runtime boundary strings are UTF-8. Windows/POSIX adapters normalize platform command-line behavior; bare-metal may supply an injected vector or empty values. Semantic validation and runtime adapters are not yet implemented. 
* **Tanımlama:** | `f:fonksiyon_adi(params)!i32 { ... }` | `fonksiyon_adi(params) { ... }`
* **Tanımlama:** | `f:fonksiyon_adi(a, ...)!i32[] { ... }` | `fonksiyon_adi(a, ...) { ... }` 
* **Zorlama:** | `f:topla(a!i32, b!i32)!i32 { ... }` |  `topla(a, b) { ... }`
* **Varsayılan Parametreler:** | `f:topla(a:10, b:20)!i32 { ... }` | `topla(a:10, b:20) { ... }` 
* **Lambda:** | `f:(a, b) > a*b;` | `v:kare = f:(a, b) > a*b;`
* **Anonim:** | `f:(a, b){...}` | `open => f:(a, b){...}` Group içinde
* **Extern Tanımlama:** | `exf:fonksiyon_adi(a!i32, ...)!i64 { ... }` | `fonksiyon_adi(a, ...) { ... }`
* **Generic:** | Generic type-parameter grameri henüz normatif değil | Mevcut Parser generic parameter listesi desteklemez 
---

**Parametre ayrımı:** Fonksiyon bildirimindeki `a:10` bir varsayılan parametre değeridir. Çağrıdaki `topla(a:10, b:20)` ise isimli argümanlardır. Bunlar aynı token biçimini kullansa da farklı Parser bağlamlarında farklı AST düğümleri üretir. Fonksiyonlar şu an tek bir dönüş tipi taşır; çoklu dönüş tipi grameri tanımlı değildir.

**Pointer/reference örnekleri:**

```nexus
nt:BytePtr = *u8;
v:buffer!*u8;
f:read(buffer:*u8, size!u32)!*u8 {
    ret buffer;
}
f:inspect(value:&Point)!bool {
    ret true;
}
```

Parser pointer/reference yazımlarını type-name string alanlarında korur. Bu, syntax/AST taşımasını sağlar; güvenli dereference, mutable borrow, lifetime ve ownership doğrulaması anlamına gelmez.

**Adres tabanlı Struct instance kuralı:** Struct instance bir aggregate kopyası değil, adresle tanımlanan depolama nesnesidir. Struct ataması, parametre/return aktarımı ve instance method receiver bağlaması aynı instance adresini korur; alanların otomatik kopyalanması yapılmaz. Object Group metodundaki `self`, çağıran instance'ın adres tabanlı receiver alias'ıdır. Bu kural örtük heap allocation anlamına gelmez; literal hedef storage'a initialize edilir ve storage kaynağı açık Semantic/OIR sözleşmesiyle belirlenir. `*T`, `**T` ve `&T` pointer/reference yazımlarının nullability, mutability, aliasing, dangling ve borrow/lifetime kontrolleri ayrı Semantic işi olarak kalır.

### İfade gövdeli anonim fonksiyonlar

`f:(parametreler) > ifade` biçiminde `>` sonrasındaki ifade fonksiyon gövdesidir ve fonksiyonun dönüş değerini oluşturur. Bu kısa biçimde ayrıca `ret` yazılmaz. Örneğin:

```nexus
v:carp = f:(a, b) > a * b;
v:kare = f:(a) > a * a;
```

Bu biçim AST içinde `AST_LAMBDA` olarak tutulur; `body_is_expression` işaretlenir ve `body` doğrudan ifade düğümüdür. Parser bu ifade için sentetik `AST_RETURN` üretmez. Blok gövdeli anonim fonksiyon (`f:(a,b) { ... }`) ise `AST_LAMBDA` olarak kalır ve dönüş gerektiğinde blok içinde açık `ret` kullanılır. İfade gövdesinin dönüş tipi Semantic aşamasında çıkarılır/doğrulanır; Parser veya AST varlığı tek başına bu tip kontrolünün tamamlandığı anlamına gelmez.

İfade gövdeli isimli fonksiyon (`f:name(...) > ifade`) bu belgede normatif bir tanımlama biçimi değildir. Mevcut Parser uyumluluk incelemesi tamamlanana kadar bu yazım yeni dil özelliği olarak kullanılmamalıdır.

---

### Generic Fonksiyonlar — Tasarım Açığı

`f:sum(a!t, b!t)!t` biçiminde `t` type-name yazımı AST type-name string alanlarında korunur; ancak Semantic henüz `t` için generic binding, scope, unification ve inference yapmaz. Bu nedenle generic fonksiyonlar uçtan uca desteklenmiş sayılmaz. Açık `<T>` parameter-list syntax'ı normatif değildir; örtük type-variable modeli netleştirilmeden eklenmemelidir.

---


## Bölüm 7. VERİ YAPILARI (DATA STRUCTURES)**

###  VERİ YAPILARI (STRUCTURES)
- struct: Veri gruplama (Varsayılan hizalı)
- !struct: Bellek paketleme (Packed/No-padding)
- enum: Durum ve veri taşıyan seçenekler
- union: Paylaşımlı bellek alanları
- nt:newtype Tip takma adları (Typedef)

####  7.1 `struct` (Veri Paketi)**

NexusFlow'da `struct` varsayılan olarak en verimli hizada (aligned) saklanır. 
Ancak donanım sürücüsü yazarken `!struct` (Hard-Pack) kullanarak hizalamayı (padding) kapatabilirsin.

```
# Standart Struct
struct:Point {
    v:x!i32,
    v:y!i32
}

# Hard-Packed Struct (Donanım/Kernel için)
# Bellekte hiç boşluk bırakmaz, tam 5 byte yer kaplar.
!struct:NetworkPacket {
    v:id!u8,
    v:data!u32
}

```

####  7.2 `enum` (Seçenekler)**

Enum'lar nxf'de sadece sayı dizisi değildir; her bir seçenek kendi verisini de taşıyabilir (Rust tarzı Algebraic Data Types).

```
enum:State {
    Idle,
    Running(pid!u32),
    Error(code!u16)
}

# Kullanım:
v:current = State.Running(1024);

```

####  7.3 `union` (Ortak Bellek Alanı)**

Kernel ve Driver yazarken aynı bellek alanına farklı tiplerle bakmak gerekir.

```
union:Register32 {
    v:full!u32,
    v:parts!struct {
        v:low!u16,
        v:high!u16
    }
}

```

####  7.4 `nt:` (NewType)(Typedef)**

Karmaşık tipleri kısaltmak veya semantik isimler vermek için kullanılır.

```
nt:Addr = u64;

nt:struct: point {
    v:x!i32,
    v:y!i32
}

v:val!point = {25, 23};

```

**Callback / function type (fonksiyon türü):**

Fonksiyon türü, bir fonksiyonun parametre ve dönüş imzasını ifade eder. İmza biçimi `f:(parametreler)!DönüşTipi` şeklindedir. Bu ifade tek başına fonksiyon gövdesi oluşturmaz; `>` işareti yalnızca lambda/anonim fonksiyon gövdesini başlatır.

**1. Fonksiyon türü alias'ı**

```nexus
nt:Callback = f:(id!u32)!bool;
```

**2. Fonksiyon türünü doğrudan parametre olarak kullanma**

```nexus
f:apply(callback!f:(id!u32)!bool, id!u32)!bool {
    ret callback(id);
}
```

`callback`, belirtilen imzaya sahip fonksiyon adresini taşır. Çağrı sırasında fonksiyon gövdesi değil, bu adres üzerinden çağrılabilir hedef kullanılır. Fonksiyon adresinin ABI/calling-convention temsili ve imza uyumluluğu Semantic/ABI aşamalarında doğrulanacaktır.

**3. İç içe callback türü**

Bir callback'in parametresi de fonksiyon türü olabilir:

```nexus
nt:Predicate = f:(value!i32)!bool;
nt:PredicateHandler = f:(test!f:(value!i32)!bool)!bool;

f:run_test(test!Predicate, handler!PredicateHandler)!bool {
    ret handler(test);
}
```

İç içe imzalar aynı fonksiyon türü gramerini tekrar kullanır. Parametre listesi içindeki virgüller, iç içe parantez derinliği dikkate alınarak ayrıştırılır.

**4. Fonksiyon türü ile lambda arasındaki fark**

```nexus
# Yalnızca tür/imza:
nt:Check = f:(value!u32)!bool;

# Lambda/callable gövdesi:
v:check = f:(value!u32)!bool > (value > 0);
```

İlk örnekte yalnızca fonksiyon türü tanımlanır. İkinci örnekte `>` sonrasındaki ifade lambda gövdesidir. Blok gövdeli anonim fonksiyonlarda `f:(value!u32)!bool { ... }` biçimi kullanılır.

**Uyumluluk:** Kanonik dönüş tipi ayıracı `)!ReturnType` biçimidir. Eski `)>!ReturnType` biçimi geriye dönük uyumluluk için kabul edilir ve AST'nin legacy metin alanında kanonik biçime normalize edilir. Fonksiyon türü AST'de `AST_TYPE_FUNCTION` düğümü ve parametre/dönüş alt ağaçlarıyla temsil edilir. Parser/AST temsili, callback'in ABI uyumluluğu, adres geçerliliği ve yaşam süresi denetimlerinin tamamlandığı anlamına gelmez.
---

## Bölüm 8. Nesne Yönelimli Programlama (Struct & Group)

### Group Metodu (Veri Yapıları ilişkisi)
>**KURAL:** 
> Bir `group`, bir `struct`, `enum`, `union` 'a metot ekleyecekse, isimleri **BİREBİR AYNI** olmalıdır.
> (Struct) Yapı Group içerisinde tanımlanır ise dışarıdan erişilemez. Sadece group metotları kullanılarak ulaşılabilir.
> Nesne Metodu İlk parametre 'self' olmak zorundadır. 


```
group Oyuncu {
    # struct group içerisinde tanımlanır ise dışarıdan direkt olarak erişilemez,
    # sadece group metotları kullanılarak ulaşılabilir 
    nt:struct:Oyuncu {  
        v:ad!str,
        v:puan!i32,
        v:aktif!bool
    };

    # Yapıcı (Constructor) - Statik Fonksiyon
    yeni => f:(isim)!Oyuncu {
                ret Oyuncu { 
                    ad = isim,
                    puan = 0,
                    aktif = true 
                };
            }

    # Tipi (Oyuncu) belirtilmez (infer edilir).
    puan_arttir => f:(self, miktar) { self.puan = self.puan + miktar;}
    
    # Tek satır metot
    isim_getir => f:(self)> self.ad;
    
    # Alt grouplar isimsiz oluşturulur, üst gruba (dal/üye) ismi ile bağlıdır.
    canta => group{
        envanter => { .... }
        silah => { .... }
    }
    # Oyuncu.canta.envanter()  gibi kullanılır.
    
    default { fonksiyon yada blok }
    
}

// Kullanım:
o1 = Oyuncu.yeni("Ahmet");
o1.puan_arttir(10);
echo(o1.isim_getir()); // Ahmet

```
---

## Bölüm 9. Gelişmiş Inline Assembly (`fastexec{asm{}}`)

### Inline Assembly kodları için:
*fastexec, asm kodları için kapsayıcı, değişken geçişlerinin otomatik olarak yapıldığı ve optimize edici bir bloktur.*
> ASM bildirimi, `asmcall`, `asmjmp` ve düşük seviyeli CPU/GPU/NPU ilkel işlemleri yalnızca `fastexec { ... }` kapsamı içinde kullanılabilir; kapsam dışı kullanım derleme hatasıdır.
> `fastexec` normal Nexus Flow değişken ve ifadelerini de içeren ayrı bir AST yürütme kapsamıdır.
> ASM etiketleri aynı FASTEXEC kapsamı içinde `asmcall`/`asmjmp` ile çağrılabilir.
> etiket isimleri ( _, -, $ ) vs.. gibi işaretler ile başlayamaz

-* Derleyici, aşağıdaki fonksiyonları standart kütüphane çağrısı yerine doğrudan assembly veya özel işlem kodlarına dönüştürür:
    -*   `asmcall("ETIKET")`: Tanımlanmış bir `asm` etiketine (`asm: ETIKET { ... }`) fonksiyon çağrısı (call) yapar.
    -*   `asmjmp("ETIKET")`: Tanımlanmış bir `asm` etiketine koşulsuz sıçrama (jmp) yapar.

-* Bu iki fonksiyon ana asm kodunun içerisine yazılan asm kodlarını çağırmak için call yada jmp ekler.
-* `fastexec` blokları içinde kullanılan `asm` yapısı, artık değişkenleri ve yazmaçları (register) doğrudan yönetebilir.
-* Bu geliştirmelerle artık asm bloklarında şunları yapabilirsiniz:
-*   **Değişken Değiştirme (Variable Substitution):** `%degisken` sözdizimi ile NexusFlow değişkenlerinin bellek adreslerine (stack offset) doğrudan erişim.
-*   **Register Eşleme (Register Mapping):** `%degisken:reg` (örn: `%sayac:rcx`) ile değişken değerlerinin otomatik olarak belirtilen register'a yüklenmesi ve işlem sonunda geri yazılması.
-* codegen..  **Clobber Listesi:** `# clobber: rax, rbx` direktifi ile `asm` bloğu tarafından kirletilecek register'ların otomatik korunması (push/pop).
-* Ham ASM gövdesi Lexer tarafından Nexus Flow token'larına ayrıştırılmaz; byte aralığı korunarak tek bir opaque body token olarak taşınır. Dış ASM `{}` çifti gövde sınırıdır; iç içe `{}` dengelenir, tek/çift tırnak içindeki brace karakterleri ve backslash escape'leri delimiter sayılmaz. Target comment biçimleri derleyici tarafından yorumlanmaz; yorumlarda bulunan brace karakterleri de ham delimiter sayımına dahildir.
-* FASTEXEC doğrulaması değişken bağlama, başlatılma/yazılabilirlik, operand genişliği, register ve bellek çakışması, clobber/live-value, label/call/jump, hedef yeteneği ve ABI/hizalama kontrollerini kapsar. Derleyici ham ASM'nin kanıtlayamadığı davranışları güvenli varsaymaz.


```
# agressif optimizasyon uygulanır.
f:main()!i32 {
    fastexec {
        // Standart değişken tanımlama
        v:a!i32 = 10;
        v:b!i32 = 20;
        v:total!i32 = 0;
        
        asm: CRITICAL_ADD { 
            mov rax, %a      # %a -> [rbp - 8] gibi bir adrese dönüşür
            add rax, %b      # %b -> [rbp - 16]
            mov %total, rax  # %total -> [rbp - 24]
        }
        
        asmcall(CRITICAL_ADD); # Aynı FASTEXEC kapsamındaki etiketi çağırır.
        echo(total); # Çıktı: 30
        # asmjmp("MY_ASM_BLOCK"); # FASTEXEC içinde koşulsuz sıçrama; geri dönüş olmayabilir.
    }
    
    ret 0;
}

```

```
# Bir fonksiyon içerisinde tanımlıysa. 
# Fonksiyon içerisinde tanımlarsanız asmcall ve asmjump yapmanıza gerek kalmaz. Normal bir fonksiyon gibi kullanabilirsiniz.
        
f:asm_func(a, b)!i32{
	fastexec {
        v:tot = 0;

        asm: CRITICAL_ADD {
            mov rax, %a
            add rax, %b
            mov %tot, rax
        }
        
		ret tot;
    }
}

f:main()!i32 {
    
	v:a!i32 = 10;
	v:b!i32 = 20;
	v:total!i32 = 0;

	total = asm_func(a, b);
	echo(total);
	
    ret 0;
}

```

#### Inline Assembly (`fastexec{asm{}}`)

* **Optimizer Kararı:**

  * Kullanıcı asm bloğu yazsa bile, optimizer son kararı verir → değişken register mapping, spill, reorder gibi optimizasyonlar uygulanabilir.
  * `%degisken` veya `%degisken:reg` gösterimleri kullanıcıya öneri sağlar, ama gerçek register allocation optimizer tarafından belirlenir.

* **Clobber Listesi:**

  * Kullanıcının kodu sadece öneri verir (`# clobber: rax, rbx`)
  * Gerçek codegen davranışı compiler/optimizer tarafından otomatik korunur → push/pop eklenir veya register yeniden tahsis edilir.

* **Güvenlik Etkisi:**

  * Doğrudan assembly yazmak hâlâ UB riski taşır (yanlış label, jump, veya memory access)
  * Ama codegen optimizer sayesinde register clobber ve spill hataları minimize edilir.

---
---


## Bölüm 10. Yerel Modül Yükleme

- ** Kullanım:
```
use <module>;
use <module> as <yeni_isim>;

Noktalı modül yolları da kabul edilir: `use msvcrt.exf as crt;`. Parser bunu tek bir `AST_USE.module_name` metni olarak saklar; dosya çözümlemesi Module/Resolver aşamasına aittir.

#örnek:
use io, sys, cpu;
use math as m;

# Namespace seçimi ve prefix davranışı:
use db.mysql.sql as connector ns sql; # connector alias + seçilen namespace kökü
use db.mysql ns mysql;               # mysql namespace kökünü seç
use db.mysql.sql ns _;                # otomatik namespace prefix'ini bastırma
use legacy as _;                      # alias "_" (namespace seçimi değildir)

`as _` modül alias'ını "_" yapar; `ns _` ise namespace-prefix seçimini "_" ile bastırır. Bunlar farklı AST alanlarına kaydedilir. Parser/AST codec bunları korur; modülün gerçekten yüklenmesi, alias/namespace sembollerinin bağlanması ve import görünürlüğü Module Resolver + Semantic entegrasyonuna aittir (henüz tamamlanmış değildir).

# Modül dosyalarında dışarı aktarılacak fonksiyon ve grouplar için.
# exp(export) pup(puplic)

exp group file{...}
pup f:myfunction(){...}

```
---
### 11. DLL export ve yükleme — API / target sözleşmesi
** Bu fonksiyon dışarıdan alınan dll ler için geçerli değildir. Kendi yazdığınız dll ler için geçerlidir.

`dllexp:` ve prefix DLL export bildirimi mevcut C Parser gramerinde tanımlı değildir; DLL artifact/export metadata'sı target/build katmanında ayrıca tanımlanmalıdır. 

`import(path)` örneği özel bir dil anahtar kelimesi veya AST intrinsic'i değildir; sıradan fonksiyon çağrısı biçimindedir. DLL yükleme API/runtime sözleşmesi henüz tamamlanmamıştır.

```
#DLL olarak derliyor iseniz f: main(){} yerine aşağıdaki şekilde kullanılır.

dllexp: run_optimizer(*oir_path, *nxir_path, *target) {....}
```

- ** Kullanım:
```
# Dll yi yükle.

(Optimizer) <- import("optimizer.dll") ?-> !!("dll Bulunamadı!");

# 'Optimizer' masası artık içindeki fonksiyonları bir 'group' gibi sunar.
v:sonuc = Optimizer.run_optimizer(oir_path, nxir_path, target_info);

(Optimizer)?; # Serbest bırak. Yüklenen dll yi kaldır.
```

---

### 12. Extern Modül !!

> Extern fonksiyon bildirimi `exf:` ve program kapsamındaki `!!=linkname = "...";` bildirimi Core Parser tarafından desteklenir. `!!=[target ...]` ve genel `!!=link ...` örnekleri Target/Directive katmanı tasarım örnekleridir; bunların prefix grameri tamamlanmış değildir. `buf!*char` pointer parametre yazımı Parser tarafından kabul edilir; pointer güvenliği ve ABI kontrolleri Semantic/Extern aşamasında tamamlanacaktır.

** Extern dll, obj, lib.a dosyaları için..
** Modül Hazırlama
```
# msvcrt.exf  modül olarak hazırlanır. use ile kullanılır.
# modül başında tanımlanır. 
# Dış Kütüphaneleri kullanmanızı sağlar.
# OS Spesifik Extern Tanımlamaları ve Linkleme
!!=[target = "windows"] {
    !!=link name = "msvcrt";
    exf:_write(fd!i32, buf!*char, size!u32)!i32;
}

!!=[target = "linux"] {
    !!=link name = "libc";
    exf:write(fd!i32, buf!*char, size!u32)!i32;
}

# Group Yapısı: Uygulama Arayüzü
group SysIO {
    
    # Her OS için aynı isimli iç metod (Static-like)
    output => f:(msg!str) {
        v:len = len(msg);
        v:ptr = addr(msg);

        !!=[target = "windows"] {
            # Windows stdout: 1
            _write(1, ptr, len);
        }

        !!=[target = "linux"] {
            # Linux stdout: 1
            write(1, ptr, len);
        }

        !!=[target = "bare-metal"] {
            # Hiçbir kütüphane yok, doğrudan VGA belleğine yaz veya UART kullan
            # 0xB8000: Klasik x86 VGA metin tamponu adresi
            loop(i, i < len, i++) {
                poke(0xB8000 + (i * 2), msg[i]);
                poke(0xB8000 + (i * 2) + 1, 7); # Gri renk
            }
        }
    }
}


```

- **Kullanımı:
```
#Projene ekle ve kullan..
use msvcrt.exf; # normal olarak stdlib de bulamaz ise, projedeki lib/ de arar..
# --- KULLANIM (Main Flow) ---
f:main()!i32 {
    # Uygulama geliştiricisi OS detayını görmez.
    SysIO.output("NexusFlow OS Bridge Active.");
    
    ret 0;
}

```
---

## 13. Macro Sistemi — Tasarım / Parser dışı katman

> Aşağıdaki örnekler mevcut C Parser tarafından kabul edilen syntax değildir. Macro expansion/preprocessor grameri, AST metadata'sı ve diagnostic/source-map sözleşmesi henüz tanımlanmamıştır. Bu örnekler normatif dil kodu değil, ileride ayrı Preprocessor katmanında değerlendirilecek tasarım notlarıdır.

```
# Makro programlama
# 1. Makro Tanımı: OS Köprüsü Oluşturucu
:macro! OS_Bridge($name, $win_fn, $lin_fn) {
    group $name {
        run => f:(args) {
            !!=[target = "windows"] {
                $win_fn(args);
            }
            !!=[target = "linux"] {
                $lin_fn(args);
            }
            !!=[target = "bare-metal"] {
                # Bare-metal için varsayılan güvenli döngü
                !!("Unsupported Platform");
            }
        }
    }
}

# 2. Makroyu Tetikleme (Usage)
# Derleyici bu satırı gördüğü an yukarıdaki group yapısını otomatik üretir.
!!=OS_Bridge!(IO, _write, write);
!!=OS_Bridge!(Net, win_send, lin_send);

# 3. Kullanım
f:main() {
    IO.run("Nexus Makro Aktif!");
}
```

```
# Kod bloklarını NexusFlow mantığıyla saran makro
:macro! SafeFlow($target_code!block) {
    # Bloğu çalıştır, eğer içinde bir hata sapması (?->) oluşursa yakala
    $target_code ?-> {
        !!("Kritik Hata: Akış hatası, işlem iptal.");
    }
}

# Kullanım
f:main() {
    !!=SafeFlow! {
        (h) <- load_resource("data.bin"); # Eğer yüklenemezse otomatik ?-> tetiklenir
        process(h);
    }
}
```
```
# Farklı tipler için grup üreten makro
:macro! CreateMath($name!id, $type!type) {
    group $name {
        # Tip makro parametresinden geliyor
        add => f:(a!$type, b!$type)!$type {
            ret (a + b);
        }
    }
}

# 1. Tam sayılar için matematik makinesi üret
!!=CreateMath!(IntMath, i32);

# 2. Ondalıklı sayılar için matematik makinesi üret
!!=CreateMath!(FloatMath, f32);

# --- KULLANIM ---
f:main() {
    v:sonuc1 = IntMath.add(10, 20);      # i32 seviyesinde çalışır
    v:sonuc2 = FloatMath.add(10.5, 5.2); # f32 seviyesinde çalışır
}

```


## Bölüm 14. STANDART MODÜLLER: COREMODULE / OSMODULE

### 14.0.1 Modül rolleri ve varsayılan yükleme

- `coremodule`, işletim sistemi bağımlılığı taşımayan genel fonksiyon kütüphanesidir.
- `osmodule`, dosya işlemleri, standart çıktı/girdi ve benzeri işletim sistemi servislerine bağlı fonksiyonları içerir.
- `osmodule` program için varsayılan olarak otomatik yüklenir.
- `no: osmodule;` yalnızca bu otomatik yüklemeyi kapatır. Genel bir modül devre dışı bırakma sözdizimi değildir; `no: coremodule;` geçersizdir.
- `coremodule` için otomatik yükleme veya açık `use` gereksinimi kararı bu sözleşmede henüz sabitlenmemiştir.

```nxf
f:main() {
    print("Hello World");
    echo("Nexus Flow");
    input();
}
```

Bare-metal / OS'siz hedefte OS modülünün varsayılan yüklemesi kapatılabilir:

```nxf
no: osmodule;

f:main() {
    # Kernel veya bare-metal işlemleri
}
```

Bu bildirim `use` ile yapılan açık import sözdiziminin yerine geçmez. Açık `use osmodule` ile birlikte kullanımın politika sonucu, Module Resolver/host entegrasyonunda ayrıca ele alınmalıdır.

###  CORE ZERO FUNCTIONS (Os Bağımsız)

| Kategori | Fonksiyon | İşlev |
| --- | --- | --- |
| **Donanım** | `write()` `fwrite()` | Donanım bazında girdi/çıktı. |
| **Donanım** | `inb(port)` `outb(port, val)` | Donanım portlarına doğrudan erişim. |
| **Donanım** | `peek(addr)` `poke(addr, val)` | Fiziksel bellek adresini okuma/yazma. |
| **Donanım** | `irq(n, f)` `intr(n)` | Kesme (Interrupt) tanımlama ve çağırma. |
| **İşlemci** | `reg.r(name)` `reg.w(name, v)` | CPU Register (rax, rsp vb.) kontrolü. |
| **Bellek** | `area()` `zone()` `free()` `addr()` | Ham bellek yönetimi. |
| **Sistem** | `panic()` `exit(code)` `defer {}` `swap(a,b)`| Süreç ve güvenlik yönetimi. |
| **Dönüşüm** | `cast()` `typeof()` `sizeof()` | Tip ve yansıma (reflection) işlemleri. |
---

### 14.1 Giriş/Çıkış (I/O)

- `write()`: Bir içeriği alıp bellekte buffer'a  yazar.
- `fwrite()`: Buffer'dan alıp bir dosyaya yazar.

### 14.2 Meta ve Yansıma (Reflection)

- `typeof(v)`: Verinin aktif tipini döndürür.
- `sizeof(v)`: Verinin bellekte kapladığı byte miktarını döndürür.
- `len(v)`: Koleksiyon, string veya buffer uzunluğunu verir.
- `val.is_ok()` / `val.is_err()` : İşlemin başarılı mı başarısız mı olduğunu `bool` olarak söyler.
- `val.is_some()` / `val.is_none()` / `val.is_null()` : Değerin varlığını/null durumunu sorgular.

> Bu noktalı üye çağrıları Parser'da genel member-call AST biçiminde temsil edilir. Global `is_ok(...)` / `is_some(...)` intrinsic tokenları ayrı AST intrinsic düğümlerine gider. Standart `Option` / `Result` türlerinin tanımı ve üreticileri henüz normatif değildir.

### 14.3 Sistem ve Yönetim

- `panic(msg)`: Kritik hata mesajı basar ve programı durdurur.
- `exit(code)`: Belirli bir çıkış koduyla programı sonlandırır.
- `cast(v, type)`: Güvenli/Zorunlu tip dönüşümü yapar.
- `wait(time-ms)`: Belirli bir süre Bekleyemeye alma
- `swap(a,b)`: İki değişkeni yerdeğiştir. xor ile değiştir.
- `defer { block }`: İçinde bulunduğu kapsam kapanırken çalışacak temizlik kodunu mühürler.



#### defer — lexical scope kapanış eylemi

`defer { block }`, bulunduğu lexical scope için bir kapanış eylemi kaydeder. Sözdizimi zaten mevcuttur; bu bölüm davranışını netleştirir.

1. Defer kaydı, yürütme sırasında defer ifadesine ulaşıldığında etkinleşir.
2. Kayıt, bulunduğu lexical scope aktivasyonuna aittir. Aynı scope içindeki basic block geçişleri ve geri jump kayıtları silmez.
3. Scope normal akışla veya `break`, `continue`, `ret` ya da scope dışına çıkan `jump(label)` nedeniyle terk edildiğinde kayıtlı defer blokları çalıştırılır.
4. Aynı scope aktivasyonunda birden fazla defer kaydedilmişse son kaydedilen ilk çalışır (LIFO).
5. Defer blokları, aynı scope'a ait otomatik `}?;` kaynak/masa temizliğinden önce çalışır. Böylece defer gövdesi, otomatik serbest bırakma başlamadan önce hâlâ sahip olunan kaynakları kullanabilir.
6. Scope aktivasyonu sona erdiğinde o aktivasyona ait defer kayıtları tüketilir. Yeni ve geçerli bir scope aktivasyonu yeni kayıt listesi oluşturur.
7. Defer gövdesi, kendisini kaydeden scope'un dışına `jump`, `ret`, `break` veya `continue` ile kontrol aktaramaz.
8. Bu sürümde defer gövdesi içinde yeni defer kaydı oluşturulması desteklenmez; Analyzer bunu reddeder.
9. Defer gövdesindeki hata/panic ve otomatik cleanup hatalarının tamamlanma, biriktirme ve unwind politikası MR-OIR-020'de tanımlanır; cleanup hatası normal başarı gibi kabul edilemez.

Bu kuralların OIR karşılığı ve testleri MR-OIR-017 içinde tanımlanır.

Defer değişkenleri kayıt anında kopyalamaz. Defer gövdesi çalıştırıldığında, kendisini kaydeden scope activation'ındaki değişkenlerin güncel değerlerini görür; gövde içindeki yazmalar aynı değişkenleri günceller. Defer, kaynaklar için ayrı sahiplik veya handle kopyası oluşturmaz. `->_` ile devredilmiş kaynak eski değişken üzerinden kullanılamaz; sahiplik ve otomatik cleanup tek bir ownership kaydı üzerinden izlenir. Defer tarafından kullanılan değişkenler, defer tamamlanana kadar geçerli tutulur. Ayrıntılı OIR ve test sözleşmesi MR-OIR-019'dadır.
Cleanup hata politikası da sabittir: bir defer veya otomatik kaynak temizliği hata verse bile aynı scope'taki kalan defer'ler ve kaynak cleanup işlemleri tamamlanır. Defer'ler LIFO, otomatik kaynaklar ters edinim sırasıyla temizlenir. Normal çıkışta cleanup hatası normal dönüşü engeller ve hata akışına aktarılır; önceden oluşmuş hata veya panic birincil kalır, cleanup hataları ikincil tanı olarak eklenir. !! ve ?; aktif scope'ları cleanup ederek sonlanır. Defer gövdesinde non-local kontrol aktarımı, ?;, }?;, nested defer ve yeni sahip olunan kaynak edinimi yasaktır. Ayrıntılı kurallar MR-OIR-020'dedir.


### 14.4 Donanım ve Düşük Seviye (Kernel Ready)
- `peek(addr)` / `poke(addr, val)`: Doğrudan bellek manipülasyonu.
- `inb(port)` / `outb(port, val)`: I/O Port iletişimi.
- `irq(n, f)` / `intr(n)`: Donanım ve Yazılım kesme yönetimi.
- `asm:Label { ... }`: Satır içi (Inline) assembly blokları.
- `reg(name)`: İşlemci kayıtçılarına (registers) erişim.

### 14.5 Bellek Tahsis (Primitive Memory)
- `area(size)`: Ham bellek bloğu ayırır.
- `zone(size)`: Paylaşımlı bellek bloğu ayırır.
- `free(ptr)`: Ham bellek bloğunu serbest bırakır.
- `addr(v)`: Bir değişkenin fiziksel adresini döndürür.
- `zone.sync()`: Tüm çekirdeklerdeki cache'leri temizler ve verinin güncel olduğundan emin olur.
- `zone.view()`: Zone içeriğini salt-okunur (read-only) bir `area` olarak kopyalamadan map eder.
- `zone.lock()`: Manuel kritik bölge oluşturur (Sadece çok karmaşık işlemlerde).

> Bu üç biçim mevcut Parser'da genel member-call olarak ayrıştırılır; `AST_ZONE_SYNC`, `AST_ZONE_VIEW` ve `AST_ZONE_LOCK` enum etiketleri henüz Parser tarafından üretilmez. Runtime/target etkileri doğrulanana kadar bu etiketler ayrılmış AST kapasitesi sayılır.

### 14.6 Time
- `time()`:
- `clock()`:

----


## 15. EXF — External Function Declarations ve Linkage

### 15.1 Dış fonksiyon bildirimi

Nexus Flow, object/library kaynaklarındaki dış fonksiyonları açık bildirimle kullanır. Örnek:

```nxf
exf:add(a!i32, b!i32)!i32;
```

- `exf:` yalnızca belirtilen dış sembolü Nexus Flow programına fonksiyon arayüzü olarak tanıtır.
- Parametreler normal fonksiyon parametre tipleriyle aynı type-expression altyapısını kullanır.
- `!i32` dönüş tipini belirtir; dönüş tipi olmayan bildirimler ayrıca dil kuralına göre ele alınır.
- EXF ile bildirilen fonksiyon, kaynak kodunda yerel fonksiyon çağrısı gibi kullanılabilir; dış sembol çözümlemesi ayrı linkage aşamasına aittir.

### 15.2 Linkname bildirimi ve EXF kapsamı

```nxf
!!=linkname = "math.obj";

exf:add(a!i32, b!i32)!i32;
exf:sub(a!i32, b!i32)!i32;
````

- `linkname`, dış bağlantı kaynağını bildirir ve OCL linker için linkage metadata sağlar.
- `exf:` bildirimleri, bu kaynaktan Nexus Flow'a açılan sembolleri belirler.
- Linkname verilmesi kütüphanedeki tüm sembolleri otomatik olarak kullanılabilir yapmaz.
- Yalnızca EXF ile açıkça bildirilen fonksiyonlar Nexus Flow dış-fonksiyon arayüzüne dahil edilir.
- EXF sembol adı ile gerçek object/library sembol adı gerekirse ayrı tutulabilir; bu eşleştirme linker metadata'sıdır.
- Bir linkname kaynağı birden fazla EXF bildirimiyle ilişkilendirilebilir.

### 15.3 Aşama sınırı

Bu bölüm dil bildiriminin ve AST'de korunması gereken ilişkinin normatif tanımıdır. Object format çözümleme, ABI/calling-convention doğrulaması, sembol seçimi ve OCL linker uygulaması bu AST işi kapsamında tamamlanmış sayılmaz; ileriki linker aşamasında ele alınacaktır.

**AST gereksinimi:** EXF parametre/dönüş tipleri, sembol adı ve linkname ilişkisi kayıpsız taşınmalıdır. Linkname ile EXF arasındaki ilişkiyi temsil etmek için mevcut AST node/metadata modeli incelenir; gereksiz yeni node oluşturulmaz.

## 16. Function Types and Callbacks

### 16.1 Function type syntax

Nexus Flow function types describe a callable signature as a type. They do not define a function body or a lambda expression.

```nexus
f:(parameter!Type, other:Type)!ReturnType
```

- `f:(...)` declares the parameter list of the function type.
- `name!Type` marks a hard-typed parameter; `name:Type` uses the regular typed form.
- `!ReturnType` specifies the return type. The legacy `>ReturnType` spelling is normalized to the canonical `!ReturnType` form by the parser.
- An empty parameter list is written `f:()!ReturnType`.
- `...` may be used as the final parameter marker for a variadic signature.

### 16.2 Named callback type

Use `nt:` to give a function type a reusable name:

```nexus
nt:Callback = f:(id!u32)!bool;
nt:CompareCallback = f:(left!i32,right!i32)!i32;
nt:NoArgCallback = f:()!bool;
```

The `nt:` declaration is a type alias (new type declaration in the language syntax); it does not create a runtime callback object.

### 16.3 Callback as a function parameter

A function type can be used directly in a parameter declaration. The callback is invoked with ordinary call syntax:

```nexus
f:apply(callback!f:(id!u32)!bool, id!u32)!bool {
    ret callback(id);
}
```

Or use a named alias to make the declaration shorter:

```nexus
nt:Predicate = f:(item!u32)!bool;
f:filter(test!Predicate, value!u32)!bool {
    ret test(value);
}
```

Callback arguments are passed according to the language's address/reference-oriented parameter model; this syntax alone does not imply copying the callback value.

### 16.4 Nested callback signatures

Function types may themselves contain callback parameters:

```nexus
nt:NestedCallback = f:(test!f:(value!i32)!bool)!bool;
```

Read this from the inside out: `test` accepts a callback taking `value!i32` and returning `bool`; the outer callback returns `bool`.

### 16.5 Arrays in callback signatures (syntax target)

Array dimensions may be attached to callback parameter or return types. The recursive parser type builder currently preserves numeric fixed dimensions and empty dynamic dimensions (`[]`) in callback parameter/return ASTs. These examples are parser-level forms; arbitrary expression dimensions and Semantic callback compatibility remain separate work:

```nexus
nt:BufferCallback = f:(buffer!u8[16])!bool;
nt:MatrixCallback = f:(matrix!f32[4][4])!bool;
nt:DynamicArrayCallback = f:(items!u32[])!u32;
nt:ReturnArrayCallback = f:(value!u32)!u8[8];
nt:CallbackTable = f:(handlers!f:(code!u32)!bool[8])!bool;
nt:CallbackMatrix = f:(callbacks!f:(row!u32)!bool[4][4])!bool;
```

Dimension order follows the normal array declarator rules. An empty `[]` represents an unspecified/dynamic dimension; a numeric dimension is fixed-size syntax. Array shape and callback signature compatibility belong to AST/Semantic validation, not Codegen. Parser tests cover parameter arrays, return arrays, malformed dimension spellings, and AST codec round-trip.

### 16.6 AST and semantic boundary

- AST retains the function type, its ordered parameter list, return type, qualifiers, and any array dimensions without reducing it to a plain named type.
- Semantic analysis is responsible for checking callback argument/return compatibility and applicable type qualifiers.
- Calling convention, ABI lowering, and target-specific function-pointer representation are later backend/linker concerns.
- A function type is not an `AST_LAMBDA` expression and does not carry an executable body.

