# NEXUS FLOW — Syntax Reference Guide
**Language:** NEXUS Flow  
**Compiler project:** OMNI-CORE Compiler (OCC)  
**Document type:** Language syntax and semantics reference  
**Version:** 1.1 (gama; consolidated from the former project README)

> Bu dosya NEXUS Flow dilinin sözdizimi, tip sistemi, operatörleri, kontrol yapıları, bellek/handler modeli ve dil özellikleri için başvuru kaynağıdır. Derleyici projesinin amacı, modülleri, geliştirme durumu ve yol haritası ana [OMNI-CORE Compiler README](README.md) dosyasında tutulur.
>
> **Olgunluk notu:** Bu kılavuz mevcut tasarım kayıtlarını bir araya getirir. Açık semantik kararlar ayrıca belirtilir; örneklerde henüz doğrulanmamış veya taslak durumda olan noktalar kesinleşmiş dil özelliği sayılmamalıdır.
---

## Bölüm 1. TASARIM FELSEFESİ: MİNİMALİZM VE AKIŞ

1.  **Handler-Oriented (Masa):** Değişkenler sadece veri değil, birer "istasyon" (Handler) olarak görülür. `(h)` boşsa veri akar, doluysa kilitlenir.
2.  **Deterministik Bellek:** `area` ve `zone` ile bellek fiziksel olarak yönetilir. Sahiplik (Ownership) her zaman son bloktadır.

---

## Bölüm 2. TANIMLAYICILAR VE TİPLEME SİSTEMİ**

###  2.1 Identifiers (Niyet Belirleyiciler)**
---
| Önek | Adı | Açıklama |
| --- | --- | --- |
| `v:` | **VAR** | Değiştirilebilir değişken. Sahiplik son bloktadır. |
| `c:` | **CONST** | Derleme anı sabitleri (Değiştirilemez). |
| `m:` | **MUST** | Zorunlu bağımlılıklar. Yüklenemezse derleme durur. |
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
---

###  2.2 Tipler **

- v:x = 100;        # default i32
- v:x!u8 = 100;     # u8 zorlama


- **Hard-Lock (`!`):** `v:sayi!u64 = 50`. Veri "50" 8 bit olsa bile 64-bit yer ayrılır.

**Tipler:** `bit`, `byte` (u8), `hex`, `char`, `f32`, `f64`, `i8..i64`, `u8..u64`, `d32`, `d64`, `bool`.
**Oyun/Grafik:** `Vec2`, `Vec3`, `Vec4` (SIMD Destekli).

---

###  2.3 Değişkenler**  Veri çeşitleri
* Değişkenler
```
v:sayi = 100;   # i32
                    
v:liste = [1, 2, 3]; # listenin içindeki en büyük elemana göre tipi ayarlar.

# Sabit
c:PI = 3.14159;

# Bilimsel gösterim (10e3)
v:val= 10E3;

# Üslü sayılar.
v:vall= 10^^15;

# Zorunlu: Modül yada bağımlılık yüklenemez ise derleme durur hata verir..
m:CoreModule = "CoreModule.nxf";

## Tip zorlama. '!' Eğer programcı zorlamak isterse:
v:pi!f64 = 3.1415; # '!' operatörüyle tip mühürlenir (Hard-lock).

loop(i, i < 100, i++) -> {
    v:sonuc = i * 2.5; 
    # 'sonuc' burada 'float' olur, bir sonraki adımda gerekirse 'int'e evrilir.
}

f:hassas_toplam(a:!f64, b:!f64)!f64 {return a + b }
# Burada, içeri giren her veriyi f64'e zorlar.

f:topla(a, b) {return a + b }
# Kullanım: topla(5, 10) veya topla(a:5, b:10)

v:sonuc = topla(a:10, b:20);
```
	 
* Diziler (Arrays)
```
v:sayi[]; # Dinamik array Tanımlama
  sayi = [10, 20, 30, 40, 50]; # Atama. Otomatik u8 olur.
  

v:sayilar[5]!i32 = [1, 2, 3, 4, 5]; #Statik Tanımlama ve atama.

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

```

* Haritalar (Maps / Dictionaries) `map<key, val>` Anahtar-Değer (Key-Value) çiftlerini tutar.

```nxf
v:notlar!map str, i32;
  notlar["Ali"] = 90;
  notlar["Veli"] = 85;
  
v:notu = notlar["Ali"]; # 90  
# Yukarıdaki v:notu , notlar["Ali"] içeriğinin kopyasıdır. 

notu = notu +5; # notu değeri 95 oldu.
# notlar["Ali"] değeri yeni bir değer kaydedilene kadar değişmez.

notlar["Ali"]<-notu; # Artık "notlar["Ali"]"nin değeri 90 değil 95 dir. 
# notu içerisi boşaltıldığı için 0 dır, tekrar kullanılmıyorsa blok bittiğinde silinir.

# Normal
f:listele(a!map, ...) { ... }

```

* Matematiksel Vektörler - Oyun ve grafik programlama için SIMD destekli tipler.
*   `Vec2`: `[x, y]` 
*   `Vec3`: `[x, y, z]` 
*   `Vec4`: `[x, y, z, w]` 

``` nxf
    v:v1!Vec3 = [1.0, 2.0, 3.0];
    v:v2!Vec3 = [4.0, 5.0, 6.0];
    v:sonuc = v1 + v2; # [5.0, 7.0, 9.0] - otomatik toplanır.
    
    f:vec_process(a!Vec3, ...) { ... }
```

---

### 2.4 TAM OPARATÖR REFERANSI

####  2.4.1 Aritmetik ve Atama (Bellek Odaklı)**
Bu operatörler, işlem sonucunu her zaman en küçük sığabileceği tipe (u8, i16 vb.) otomatik dökümler.

* `+`, `-`, `*`, `/`, `%` (Modülo)
* `^^` (Üs alma)
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

####  2.6 Bit Düzeyinde İşlemler (Kernel & Driver İçin Şart)**

* `&` (Bitwise AND)
* `|` (Bitwise OR)
* `^` (Bitwise XOR)
* `~` (Bitwise NOT/Tersleme)
* `<<`, `>>` (Bit Kaydırma)
* `|=`, `&=`, `^=` (Bitwise bileşik atama)

####  2.7 Özel Operatörler**

* `.(nokta)` :  Grup/Namespace çözümleyici.Üye erişimi (Metot veya özellik).
* `..` : Aralık (Range) belirtme (`1..100`).
* `...` : Fonksiyonlar için çoklu prametre
* `.=` : String birleştirme operatörü. Mevcut string'e yeni string ekler.
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
| **12** | `?;`  | **Halt (Son)** | Akış biter, yerel masalar imha edilir. |
| **13** | `!!`  | **Panic** | Kritik durdurma. |
| **14** | `@`   | **Intent** | Derleyiciye niyet bildirimi. |
| **15** | `!!=` | **Directive** | .target sistemi ve derleyici talimatları. |

---


## Bölüm 4. MASA (HANDLER) VE SAHİPLİK MEKANİZMASI

### 4.1 Masa Kuralları
- **Table Occupied:** `(h)` doluysa ve `<-` ile yeni veri yazılmaya çalışılırsa derleyici hata verir. Masa boşaltılmalıdır.
- **Move-on-Flow:** `(h) -> write()` işlemi veriyi kopyalamaz, **taşır**. İşlem sonunda `(h)` sıfırlanır (Nulled).
- **Zero-Cost Handover:** Masalar arası transfer sadece adres etiketinin değişimidir.


### 4.1.1 `(h)` Masa (Handler) Yaşam Döngüsü

* **Scope:** `(h)` sadece tanımlandığı blokta geçerlidir. Başka bloklardan doğrudan erişilemez.
* **Ömür (Lifetime):** Blok sonunda `}?;` ile otomatik olarak serbest bırakılır.
* **Transfer:** Başka bloğa taşımak için `_>` kullanılır. Taşındığında orijinal masa **silinir**.
* **Güvenlik Etkisi:**

  * Ömrü garanti altındadır → dangling pointer riskini minimize eder.
  * Başka bloklar `(h)`'yi okuyamaz → izinsiz erişim yok.

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
>* Kural: `(h)<- fn() ?->{}` oparatörü.  (false,null,none,unknown) durumu 
        Ternary kontrole benzer: Başarılı ise sola, başarısız ise Akış sağa kayar.
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
loop(1..100){ echo(it) }

# For Loop
loop(i, a < b, i++){... kodlar...}

# Foreach
loop(v:item in list){ echo(item) }
                         
# While (Conditional Loop)
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



### 5.3  14-15: `label:>` `label?>` `jump(label)` Etiketli atlama.

> `label?>` or `jump(label)` `label:>`
| **Atlama (Jump)** | Akışı tamamen başka bir etiketli `label:>` noktaya transfer eder. |

```
     (a > 4) -> out?>
#or  (a > 4) -> jump(out)
...
...
...
    out:>

```


**Önemli kontrol akışı kuralı:** `jump(label)` aynı basic block içinde bir konuma atlamakla sınırlı değildir. Hedef etiket, fonksiyon içindeki başka bir basic block'un giriş noktası olabilir. Jump, bulunduğu block'u sonlandırır ve kontrolü hedef block'a aktarır; aradaki kaynak satırları yürütülmez. Hedef block kaynak kodda ileri veya geri konumda bulunabilir. Bu nedenle `jump`, fonksiyon içindeki farklı kontrol akışı bölgelerine doğrudan geçiş yapmak için kullanılan genel etiketli aktarım komutudur. Fonksiyon kapsamı ve lexical-scope güvenliği ayrıca denetlenir.

**Çoklu giriş ve ölçeklenebilirlik:** Aynı hedef label'a fonksiyon içindeki birden fazla farklı block'tan `jump(label)` ile ulaşılabilir. Bir hedefin predecessor sayısı veya fonksiyon içindeki toplam jump sayısı için dil seviyesinde keyfî sabit bir sınır tanımlanmaz. Gerçek sınırlar OIR formatının temsil kapasitesi, kullanılabilir bellek ve uygulama güvenlik limitleridir; bunlara ulaşıldığında derleme açık hata vermeli, atlama/kenarları sessizce kaybetmemelidir. Her basic block tek bir terminator ile sona erer; birden fazla çıkış gerekiyorsa ilgili koşullu veya çok seçenekli kontrol yapısı ayrı CFG kenarları üretir.


#### Jump ve lexical scope kaynak ömrü

`jump(label)` bir basic block'tan diğerine kontrol aktarır; basic block değişimi tek başına kaynak ömrünü bitirmez. Kaynak ömrü lexical scope'a bağlıdır.

- Hedef label, kaynak noktanın içinde bulunduğu lexical scope ile aynı scope'ta veya onun bir üst scope'unda olmalıdır. Daha derin bir scope'a ya da kaynak scope ile kardeş olan başka bir scope'a atlayarak o scope'un giriş/başlatma adımlarını atlamak geçersizdir.
- `jump` aynı lexical scope içinde başka bir basic block'a gidiyorsa scope hâlâ aktiftir. Daha sonra aynı scope içindeki önceki bir etikete geri dönmek de mümkündür; bu durumda scope'a ait masalar ve kaynaklar erken temizlenmez.
- `jump` bir veya daha fazla lexical scope'tan çıkıyorsa, terk edilen scope'ların yaşam süresi sona erer. Bu scope'lara ait otomatik kaynak temizliği, normal scope çıkışıyla aynı şekilde çalıştırılmalıdır.
- Daha sonra terk edilmiş bir scope'un içindeki etikete yeniden girilerek eski kaynak aktivasyonunun sürdürülmesine izin verilmez. Yeni bir scope aktivasyonu ancak normal scope giriş yoluyla oluşturulabilir.
- `}?;` blok kapsamındaki masaların/kaynakların toplu kapatılması için kullanılır. `?;` ise Halt/sonlandırma davranışıdır; `jump` için genel bir cleanup komutu yerine geçmez.
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
- Her branch normal tamamlandığında ortak çıkış bloğuna bağlanır. `return`, `break` veya `continue` ile sonlanan branch'ler kendi kontrol aktarımını korur.
- Hedef işlemci jump table desteklemiyorsa Codegen aynı davranışı karşılaştırma ve dallanma komutlarıyla üretir.

Özetle: `select` **değer seçer**, `scan` ise **yapı/varyant eşleştirir**. İkisi statement tabanlıdır ve birbirlerinin yerine kullanılmak zorunda değildir.

Ayrıntılı tasarım: [NexusFlow Scan Select Syntax Spec.md](nexus%20docs/NexusFlow%20Scan%20Select%20Syntax%20Spec.md)

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

Trigger adları Analyzer aşamasında sabit kimliklere çözülür; OIR'ın çalışma zamanında metin karşılaştırması yapması gerekmez. Handler gövdesi `return` veya başka bir sonlandırıcı kontrol akışı içerirse sonraki handler'a yapay bir CFG bağlantısı kurulmaz. Hata biriktirme işlemlerinin nihai opcode/runtime ABI karşılığı ayrıca belirlenecektir.

> Not: `group` içindeki `rules ... => group { ... }` biçimi temel `rules Name(params) { ... }` sözdiziminden ayrı bir organizasyon katmanıdır; ayrı lowering/çözümleme adımı olarak ele alınacaktır.

Ayrıntılı AST, Analyzer ve OIR semantiği: [Multi-Rule Engine Formal Specification](nexus%20docs/NEXUSFLOW%20%C3%87oklu%20Kural%20%C4%B0%C5%9Fleyici%20(Multi-Rule%20Engine)%20%E2%80%94%20Formal%20Specification.md).

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
* **Main** | `f:main(argc, argv)!i32{ ... }` | 
* **Tanımlama:** | `f:fonksiyon_adi(params)!i32, !str { ... }` | `fonksiyon_adi(params) { ... }`
* **Tanımlama:** | `f:fonksiyon_adi(a, ...)!i32[] { ... }` | `fonksiyon_adi(a, ...) { ... }` 
* **Zorlama:** | `f:topla(a!i32, b!i32)i32 { ... }` |  `topla(a, b) { ... }`
* **İsimli Parametreler:** | `f:topla(a:10, b:20)i32 { ... }` | `topla(a:10, b:20) { ... }` 
* **Lamda:** | `f:(a, b)> a*b;` | `v:kare = f:(a, b)> a*b;`
* **Anonim:** | `f:(a, b){...}` | `open => f:(a, b){...}` Group içinde
* **Extern Tanımlama:** | `exf:fonksiyon_adi(a!i32, ...)!i64 { ... }` | `fonksiyon_adi(a, ...) { ... }`
* **Generic:** | `f:add(a!t, b!t)!t{...}` | `add(32, 32){...}` `add(22.7, 22.5){...}` 
---

# Fonksiyonlarda Generic 
f:sum(a!t, b!t)!t {
    return a + b;
}

f:main() {
    v:x!i32 = sum(10, 20);      # t → i32
    v:y!f32 = sum(1.5, 2.5);    # t → f32
}

* Fonksiyon işaretçileri
* callback fonksiyon

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
nt:Callback = f:(id!u32)>!bool;

nt:struct: point {
    v:x!i32,
    v:y!i32
}

v:val!point = {25, 23};

```

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
                return Oyuncu { 
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
> asm blockları fastexec kapsamında değil ise hata üretilir.
> asm blokları başka asm bloklara, asm nin normal etiket çağrıları gibi çağrı (call) yapabilir.
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
        
        echo(total); # Çıktı: 30
        
    }

    asmcall(CRITICAL_ADD);
    # Üretilen ana kod bloğu içerisine call eklenir. Normal fonksiyon gibi asm bloklarınızı kullanabilirsiniz.
    # Birden fazla asm blok yazıyorsanız birbirleri ile bağlantılı ise içlerinden birbirlerine call da yapabilirsiniz.
    # call _print de yapabilirsiniz, nasıl yapılacağı ilgili dokümantasyon dosyasında bulunabilir.
    
    # Veya oraya zıpla (dikkat: Ne yaptığınızı bilmiyor iseniz asm bloklarını kullanmayın. 
    # Eğer kodunuzda return veya başka bir jmp yoksa hiç geri dönmeyebilir, ve uygulama çöker.)
    asmjmp("MY_ASM_BLOCK");
    
    return 0;
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
        
		return tot;
    }
}

f:main()!i32 {
    
	v:a!i32 = 10;
	v:b!i32 = 20;
	v:total!i32 = 0;

	total = asm_func(a, b);
	echo(total);
	
    return 0;
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

#örnek:
use io, sys, cpu;
use math as m;

# Modül dosyalarında dışarı aktarılacak fonksiyon ve grouplar için.
# exp(export) pup(puplic)

exp group file{...}
pup f:myfunction(){...}

```
---
### 11. `dllexp:` dll export. & DLL yükleme `import(path/imp.dll)` dll, .o, .obj, .so,vs.. import.
** Bu fonksiyon dışarıdan alınan dll ler için geçerli değildir. Kendi yazdığınız dll ler için geçerlidir.

`dllexp:` Main fonksiyonu yerine dll derlemek için ana fonksiyonu belirler.

`import(path)` dll yükler

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
                poke(0xB8000 + (i * 2) + 1, 0x07); # Gri renk
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
    
    return 0;
}

```
---

## 13.  Macro Sistemi

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
            return (a + b);
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


## Bölüm 14. ÇEKİRDEK FONKSİYONLAR (CORE - KÜTÜPHANESİZ)

** Bu kütüphanenin yüklemesi için no:CoreModule; gerekir. Yani Normal os bağımlı kütüphane iptal edilmelidir.

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
- `val.is_ok()` / `val.is_err()`  : İşlemin başarılı mı başarısız mı olduğunu `bool` olarak söyler.
- `val.is_some()` / `val.is_none()` / `val.is_null()` : Değer  mı yok mu (`Option` için) kontrol eder.

### 14.3 Sistem ve Yönetim

- `panic(msg)`: Kritik hata mesajı basar ve programı durdurur.
- `exit(code)`: Belirli bir çıkış koduyla programı sonlandırır.
- `cast(v, type)`: Güvenli/Zorunlu tip dönüşümü yapar.
- `wait(time-ms)`: Belirli bir süre Bekleyemeye alma
- `swap(a,b)`: İki değişkeni yerdeğiştir. xor ile değiştir.
- `defer { block }`: İçinde bulunduğu kapsam kapanırken çalışacak temizlik kodunu mühürler.

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

### 14.6 Time
- `time()`:
- `clock()`:

----
