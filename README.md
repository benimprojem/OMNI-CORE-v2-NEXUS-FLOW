# OMNI CORE v2 NEXUS FLOW - REFERENCE GUIDE
**Version:** 1.0 (gama)  
**Architecture:** Nexus Flow & Handler-Oriented Memory  
**Compiler:** OCC (Omni Core Compiler)
**Language:** Nexus Flow
**Güvenlik:** Hash doğrulamalı bütünlük kontrolü (EXE/DLL).

> Nexus Flow için, önerilerde bulunabilirsinizz. Neler daha iyi olabilir, neler fazlalık, neler eksik; Tüm yapıcı önerileri kabul ediyorum bilginize.
Şuan ki Durum:
```
████████████████████████████████████████  LEXER (%92)
██████████████████████████████            PARSER (%78)
████████████████████████████████          AST (%80)
██████████████████████                    SEMANTIC (%65)
█████████████                             IR GEN (%45)
█████████████████████████████████████     OPTIMIZER (%90)
████████████████████                      CODEGEN (%55)
████                                      STDLIB (%10)

```
---

## Bölüm 1. TASARIM FELSEFESİ: KASITLI MİNİMALİZM VE AKIŞ

Nexus Flow, "Soyutlama Katmanı" yerine "Kontrol Katmanı"dır. Modern dillerin donanıma olan mesafesini sıfıra indirmeyi hedefler.

1.  **Handler-Oriented (Masa):** Değişkenler sadece veri değil, birer "istasyon" (Handler) olarak görülür. `(h)` boşsa veri akar, doluysa kilitlenir.
2.  **Deterministik Bellek:** `area` ve `zone` ile bellek fiziksel olarak yönetilir. Sahiplik (Ownership) her zaman son bloktadır.
3.  **Hata-Akış Entegrasyonu:** Hata bir "Exception" değil, akışın doğal bir yanal sapmasıdır.

---

## Bölüm 2. TANIMLAYICILAR VE TİPLEME SİSTEMİ**

###  2.1 Identifiers (Niyet Belirleyiciler)**
| Önek | Adı | Açıklama |
| --- | --- | --- |
| `v:` | **VAR** | Değiştirilebilir değişken. Sahiplik son bloktadır. |
| `c:` | **CONST** | Derleme anı sabitleri (Değiştirilemez). |
| `m:` | **MUST** | Zorunlu bağımlılıklar. Yüklenemezse derleme durur. |
| `nt:`| **NewType**| Tip takma adları (Typedef-like). |
| `r:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
| `t:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
| `o:` | **RESERVE**| Gelecekteki kullanımlar için ayrılmış. |
| `*`  | **Pointer**| Ham bellek adresi. |
| `**` | **P2P** | Pointer to Pointer. |
| `&`  | **Ref** | Bellek referansı. |

###  2.2 Akıllı Tipleme (Auto-Scaling)**
Nexus Flow, "En Küçük Alan" ilkesiyle sayıları otomatik mühürler:
- `100` -> `u8`
- `300` -> `u16`
- **Implicit Casting:** `i64 + u8` işleminde derleyici hata vermez, veriyi güvenli olan en geniş tipe yükseltir.
- **Hard-Lock (`!`):** `v:sayi!u64 = 50`. Veri "50" 8 bit olsa bile 64-bit yer ayrılır.

**Tipler:** `bit`, `byte` (u8), `hex`, `char`, `f32`, `f64`, `i8..i64`, `u8..u64`, `d32`, `d64`, `bool`.
**Oyun/Grafik:** `Vec2`, `Vec3`, `Vec4` (SIMD Destekli).

---

###  2.3 Değişkenler**  Veri çeşitleri
* Değişkenler
```
v:sayi = 100;       # Sistem bunu otomatik 'u8' olarak mühürler. 
                    # Tipine göre bellekte yer ayrılır kaydedilir.
                    
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

f:hassas_toplam(a:!f64, b:!f64)!f64 {ret a + b }
# Burada, içeri giren her veriyi f64'e zorlar.

f:topla(a, b) {ret a + b }
# Kullanım: topla(5, 10) veya topla(a:5, b:10)

v:sonuc = topla(a:10, b:20);
```
	 
* Diziler (Arrays)
```
v:sayi[]; # Dinamik array Tanımlama
  sayi = [10, 20, 30, 40, 50]; # Atama. Otomatik u8 olur.
  

v:sayilar[5]!i32 = [1, 2, 3, 4, 5]; #Statik Tanımlama ve atama.

v:matris[3][3]!f32 = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1]
];


# Bir fonksiyona parametre tipi olarak array vermek için `a![]`, ya da tip zorlama bir array `a!i32[]`

# Normal
f:listele(a![]) { ... }

# Tip zorlama
f:liste(a!i32[], ...){ ... }

```

* Haritalar (Maps / Dictionaries) `map<key, val>` Anahtar-Değer (Key-Value) çiftlerini tutar.

```nxf
v:notlar!map<str, i32>;
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
    v:v1 = Vec3[1.0, 2.0, 3.0];
    v:v2 = Vec3[4.0, 5.0, 6.0];
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

```

# Kayan noktalı işlemler için IEEE 754-2019 standardı.
# sıfıra bölme hatası için bölme işlemlerinde kontrol.
 
```


#### 2.5 Karşılaştırma ve Mantık**

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

> **`.=` : String birleştirme operatörü** 

```
v:a= "";
a .= "Bugün hava";
a .= " bulutlu";
a .= " sıcaklık 16 derece.";
# a = "Bugün hava bulutlu sıcaklık 16 derece."; 
```

---

## Bölüm 3. NEXUS AKIŞ OPERATÖRLERİ (THE 21 KEYS)**

| Öncelik | Operatör | Adı | İşlevi / Açıklaması |
| --- | --- | --- | --- |
| **1** | `(h)` | **Handler (Masa)** | Bellek adresini rezerve eder. İşlem öncesi masa hazır olmalıdır. |
| **2** | `<-` | **Capture (Yakala)**| Değeri veya sonucu masaya kilitler. |
| **3** | `...>`| **Expand (Genişlet)**| Buffer/List verisini açar. |
| **4** | `_>` | **Relocate** | Masayı/Context'i başka bir bölgeye taşır (Örn: GPU'ya). |
| **5** | `?(n,ms)->`| **Rolling** | Hata durumunda n kez ms milisaniye bekleyerek tekrar dener. |
| **6** | `=?>` | **If (Koşul)** | Akışın yönü için ilk mantıksal karar noktası. |
| **7** | `?->` | **Catch (Saptırıcı)**| Rolling başarısızsa akışı hata bloğuna sapıtır. |
| **8** | `?=>` | **Fallback** | Hata saptırıcı yoksa veya veri `null` ise alternatif veri enjekte eder. |
| **9** | `->`  | **Success (Başarı)**| Veriyi bir sonraki işleme/fonksiyona taşır. |
| **10** | `!->` | **Ignore (Sessiz)** | Hata olsa bile akışı zorla devam ettirir. |
| **11** | `<<`  | **Zone Write** | Veriyi atomik olarak paylaşımlı belleğe yazar. |
| **12** | `>>`  | **Flow Feed** | Veriyi atomik olarak akışa besler. |
| **13** | `<--` | **Iterate** | Veri kümesini tek tek akışa besler (Foreach). |
| **14** | `(e){}`| **Error Block** | Saptırılan hatanın işlendiği son durak. |
| **15** | `?>(label)`| **Jump** | Akışı etiketli noktaya transfer eder (Goto). |
| **16** | `(label):>`| **Label** | Atlama noktası. |
| **17** | `?;`  | **Halt (Son)** | Akış biter, yerel masalar imha edilir. |
| **18** | `!!`  | **Panic** | Kritik durdurma. |
| **19** | `@`   | **Intent** | Derleyiciye niyet bildirimi. |
| **20** | `??`  | **Y.Z.** | Y.Z. Entegrasyon/Niyet çözümü. |
| **21** | `!!=` | **Directive** | .target sistemi ve derleyici talimatları. |

---

## Bölüm 4. MASA (HANDLER) VE SAHİPLİK MEKANİZMASI

### 4.1 Masa Kuralları
- **Table Occupied:** `(h)` doluysa ve `<-` ile yeni veri yazılmaya çalışılırsa derleyici hata verir. Masa boşaltılmalıdır.
- **Move-on-Flow:** `(h) -> write()` işlemi veriyi kopyalamaz, **taşır**. İşlem sonunda `(h)` sıfırlanır (Nulled).
- **Zero-Cost Handover:** Masalar arası transfer sadece adres etiketinin değişimidir.

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
(h) <- file.open("a.txt") ?-> (e){ echo(e); };
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
loop(v:item <-- list){ echo(item) }
                         
# While (Conditional Loop)
loop(a < b){ ....  a++;}
loop(a < b, a++){ .... }

# Loop (infinity)
loop{....  (a == EOF)=?> break;}

# Rolling Loop (Zaman Ayarlı) Hata saptırma.. Hata saptırmak yerine bir etiketede atlayabilir. ?(5, 500ms)->(newhttp);
# Aşağıdaki kod http.open() başarılı olursa kontrol sola akar. 
# Başarısız olursa toplamda 5 kez tekrar dener her denemede 500ms bekler. Hala hata alırsa kontrol sağa kayar.
(h) <- http.open("www.indir.net?545.zip")?(5, 500ms)->(e){echo("hata: {e} /n")};
(h) -> ......



# unroll(c) ->loop... Döngüyü derleme anında düzler (Zero-jump).
f:fast_process() {
    v:data = [10, 20, 30]

    # Normalde (loop) ile dönerdik ama sayı sabitse:
    unroll(3) -> loop(v:item <-- data) {
        # Derleyici burayı 3 kez kopyalar
        core.io.print(data[item]) 
    }
}

f:send_packets() {
    v:ports = [80, 443, 8080]

    # Derleyici burayı 3 bağımsız fonksiyon çağrısı gibi aşağı doğru açar.
    unroll(3) -> loop(v:p <-- ports) {
        # Artık 'p' bir değişken değil, o adımın sabit değeridir.
        core.net.open_port(p)
    }
}

```

### 5.2 - 6: Dallanma (If/Else Alternatives)

```
# Standart Karşılaştırma
A:  (a == b) =?> { ... }   # if
             ?=> { ... };  # else

B:  (a == b) =?> { ... }   # if
    (a >= 1) =?> { ... }   # if
             ?=> { ... };  # else

C:  (a == b) =?> break;
    (a == b) =?> (jump_end)?>
    
D:  (a == b) =?> status = 1; ?=> status = -1;


# Döngü kaçış yapıları
break
continue
```



### 5.3  14-15: `(label):>` `(label)?>` Etiketli atlama. (goto/jump)

> match ve swich case olmayacak, bunun yerine: Etiketli atlama sistemini kullanabilirsiniz. Yada daha gelişmiş `rules` metodunu kullanabilirsiniz.
`(label)?>` | **Atlama (Jump/Goto)** | Akışı tamamen başka bir etiketli `(label):>` noktaya transfer eder. |

```
f:weather_in_molyvood(season) {
    # Tüm dallar tanımlanır.
    c:table[4] = [winter, spring, summer, fall];
    
    # Rakamlar ile çalışıyorsan hem default hemde kolaylıkla hata çıktısı ayarlayabilirsin.
    (season > 4) =?> (out)?>; #if kontrolü doğru ise echo çalışır sonra fonksiyon sonlanır.
    
    # hangisi seçim yapılır.
    (table[season])?>
    
    # o dala atlanır ve bloktaki kodlar çalıştırılır.
    (winter):> {print("Freezing\n");}
    (spring):>  print("Dirty\n");
    (summer):> {print("Dry\n"); }
    (fall):>    print("Windy\n");
    (out):>     echo("Hatalı veri!!");
}
```
### 5.3.1 NEXUSFLOW: ÇOKLU KURAL İŞLEYİCİ (MULTI-RULE ENGINE)

*** NexusFlow'da `switch` veya `match` yapıları yoktur. Bunun yerine, statik olarak optimize edilebilen ve "Bariyer" mantığıyla çalışan `rules` blokları kullanılır.

```
# Kural İşleyici: Tünel ve Bariyer Kontrolü
rules PrimeBorders(n) {
    n < 2          : "Sınır İhlali: 2'den küçük asallık olmaz"; 
    n == 2         -> Success;
    n == 3         -> Success;
    
    # Tünel Zorlaması: 6n +/- 1 kuralına uymayanlar tünel dışıdır.
    ( (n + 1) % 6 == 0 || (n - 1) % 6 == 0 ) : "Asal Değil: Tünel Dışı";
    
    default       -> StartRadar;
}

isPrime(n) {
    # Kuralları enjekte et. Error olursa fonksiyon burada 'Halt' olur.
    apply rules(PrimeBorders(n));

    # Başarı tetikleyicisi (2 ve 3 için)
    on PrimeBorders.Success { return true; }

    i = 5; 
    # Tekil Döngü (Unified Loop): Koşullu varyasyon.
    loop (i * i <= n) {
        # Zikzak Radarı: Tünelin iki kanadını aynı anda tara.
        (n % i == 0 || n % (i + 2) == 0) =?> {
            return false;
        }; 

        i = i + 6; # Tip taşması olursa sistem otomatik 'i'yi yükseltir. 
    }

    # Zorunlu bitiş: Tüm engelleri geçtiyse asaldır.
    always -> { return true; }
}

isPrime(n);
```

1. SÖZDİZİMİ (SYNTAX)

```nexus
rules [Isim](parametreler) {
    # 1. BARİYERLER (Koşul Sağlanırsa Fonksiyonu Durdurur)
    [koşul] : "Hata Mesajı";

    # 2. YÖNLENDİRMELER (Koşul Sağlanırsa Aksiyonu Tetikler)
    [koşul] -> [Aksiyon/Tetikleyici];

    # 3. ÖZEL DURUMLAR (Koşulsuz veya Varsayılan)
    always -> [Aksiyon];   # Her durumda çalışır
    default -> [Aksiyon];  # Hiçbir kural eşleşmezse çalışır
}
```

2. UYGULAMA VE TETİKLEYİCİLER (ON-BLOCKS)
Kurallar fonksiyona apply ile enjekte edilir. Tetiklenen aksiyonlar on blokları ile yakalanır.

Kod snippet'i
```
void executeTask(w, h, t) {
    # Kurallar sırayla enjekte edilir (Multi-Handler)
    apply rules (GeometryCheck(w, h));
    apply rules (SafetyCheck(t));

    # Tetikleyici Yakalayıcılar
    on GeometryCheck.SquareMode {
        # ... aksiyon kodları ...
    }
}
```
----

3. NexusFlow Kural Mimarisi

```
#Kod snippet'i
# Kural İşleyici Tanımı
rules CNC_Safety(w, h, tool) {
    
    # 1. DURDURUCU KURALLAR (Hata durumunda fonksiyon ölür)
    w > 2800 : "Genişlik sınırı aşıldı"; 
    h > 2100 : "Yükseklik sınırı aşıldı";

    # 2. TETİKLEYİCİLER (Koşul sağlanırsa aksiyona yönlen)
    w == h -> SquareMode;
    tool == "Diamond" -> HighSpeed;

    # 3. VARSAYILAN / HER ZAMAN (Koşulsuz veya Eşleşme Yoksa)
    # 'always' her durumda çalışır.
    # 'default' hiçbir üst kurala girmezse çalışır.
    always -> LogMetrics;
    default -> StandardSettings;
}
```

*** Çoklu Kural Uygulaması (The Execution)

```
#Kod snippet'i
void buildPart(w, h, t) {
    # Kurallar sırayla enjekte edilir
    apply rules(CNC_Safety(w, h, t));
    apply rules(InventoryCheck(t));

    # '->' ile tetiklenen 'on' blokları burada devreye girer
    on CNC_Safety.SquareMode {
        setAngle(90);
    }

    on CNC_Safety.HighSpeed {
        v:rpm = 18000;
    }

    # Gerçek üretim kodu
    cut(rpm,mode,angle);
}
```

 

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
        ?=> { echo("Yardım için "db.nosql.help" kullanın."); }
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
        ?=> { echo("Yardım için "db.sqlite.help" kullanın."); }
    }
    # default opsiyonel
    ?=> { echo("Yardım için "db.sqlite.help yada db.nosql.help" kullanın."); }
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

* **Stratejik Konum:** Bir veri masaya yakalandıktan (`<-`) ve paket açıldıktan (`...>`) hemen sonra, 
* eğer o verinin başka bir bellek bölgesinde işlenmesi gerekiyorsa (örneğin GPU belleğine taşıma veya başka bir thread handler'ına devretme), 
* bu işlem tüm mantıksal kararlardan (`?`, `->`, `>>`) önce yapılmalıdır.

```
(h1) <- ...........;
(h1) -> data ...> _>(h2);
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
---
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

nt:struct {
    v:x!i32,
    v:y!i32
} point;
# or
nt:struct: point {
    v:x!i32,
    v:y!i32
};

v:val!point = (23, 33);
#or 
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
    /*
    # Alt grouplar isimsiz oluşturulur, üst gruba (dal/üye) ismi ile bağlıdır.
    canta => group{
        envanter => { .... }
        silah => { .... }
    }
    # Oyuncu.canta.envanter()  gibi kullanılır.
    
    ?=> { Default: fonksiyon yada blok }
    */
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

* Derleyici, aşağıdaki fonksiyonları standart kütüphane çağrısı yerine doğrudan assembly veya özel işlem kodlarına dönüştürür:
    *   `asmcall("ETIKET")`: Tanımlanmış bir `asm` etiketine (`asm: ETIKET { ... }`) fonksiyon çağrısı (call) yapar.
    *   `asmjmp("ETIKET")`: Tanımlanmış bir `asm` etiketine koşulsuz sıçrama (jmp) yapar.

* Bu iki fonksiyon ana asm kodunun içerisine yazılan asm kodlarını çağırmak için call yada jmp ekler.
* `fastexec` blokları içinde kullanılan `asm` yapısı, artık değişkenleri ve yazmaçları (register) doğrudan yönetebilir.
* Bu geliştirmelerle artık asm bloklarında şunları yapabilirsiniz:
*   **Değişken Değiştirme (Variable Substitution):** `%degisken` sözdizimi ile NexusFlow değişkenlerinin bellek adreslerine (stack offset) doğrudan erişim.
*   **Register Eşleme (Register Mapping):** `%degisken:reg` (örn: `%sayac:rcx`) ile değişken değerlerinin otomatik olarak belirtilen register'a yüklenmesi ve işlem sonunda geri yazılması.
*   **Clobber Listesi:** `# clobber: rax, rbx` direktifi ile `asm` bloğu tarafından kirletilecek register'ların otomatik korunması (push/pop).


```
# agressif optimizasyon uygulanır.
f:main()!i32 {
    fastexec {
        // Standart değişken tanımlama
        var a!i32 = 10;
        var b!i32 = 20;
        var total!i32 = 0;
        
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
    # Eğer kodunuzda ret veya başka bir jmp yoksa hiç geri dönmeyebilir, ve uygulama çöker.)
    asmjmp("MY_ASM_BLOCK");
    
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
---

## Bölüm 10. ÇEKİRDEK FONKSİYONLAR (CORE - KÜTÜPHANESİZ)

###  CORE ZERO FUNCTIONS (Os Bağımsız)

| Kategori | Fonksiyon | İşlev |
| --- | --- | --- |
| **Donanım** | `prnt()` `prmt()` | Donanım bazında girdi/çıktı. |
| **Donanım** | `inb(port)` `outb(port, val)` | Donanım portlarına doğrudan erişim. |
| **Donanım** | `peek(addr)` `poke(addr, val)` | Fiziksel bellek adresini okuma/yazma. |
| **Donanım** | `irq(n, f)` `intr(n)` | Kesme (Interrupt) tanımlama ve çağırma. |
| **İşlemci** | `reg.r(name)` `reg.w(name, v)` | CPU Register (rax, rsp vb.) kontrolü. |
| **Bellek** | `area()` `zone()` `free()` `addr()` | Ham bellek yönetimi. |
| **Sistem** | `panic()` `exit(code)` `defer {}` `swap(a,b)`| Süreç ve güvenlik yönetimi. |
| **Dönüşüm** | `cast()` `typeof()` `sizeof()` | Tip ve yansıma (reflection) işlemleri. |
---

### 10.1 Giriş/Çıkış (I/O)
#### Bu dört metotda hiç bir işletim sistemi olmadan çalışır.
* `prnt(data...)`: Print Herhangi bir veriyi ekrana basar.
* `prmt()`: Prompt Klavyeden giriş yakalar.
* `write()`: Bir içeriği alıp bellekte buffer'a  yazar.
* `fwrite()`: Buffer'dan alıp bir dosyaya yazar.

### 10.2 Meta ve Yansıma (Reflection)

* `typeof(v)`: Verinin aktif tipini döndürür.
* `sizeof(v)`: Verinin bellekte kapladığı byte miktarını döndürür.
* `len(v)`: Koleksiyon, string veya buffer uzunluğunu verir.
* `val.is_ok()` / `val.is_err()`  : İşlemin başarılı mı başarısız mı olduğunu `bool` olarak söyler.
* `val.is_some()` / `val.is_none()` / `val.is_null()` : Değer  mı yok mu (`Option` için) kontrol eder.

### 10.3 Sistem ve Yönetim

* `panic(msg)`: Kritik hata mesajı basar ve programı durdurur.
* `exit(code)`: Belirli bir çıkış koduyla programı sonlandırır.
* `cast(v, type)`: Güvenli/Zorunlu tip dönüşümü yapar.
* `swap(a,b)`: İki değişkeni yerdeğiştir. xor ile değiştir.
* `defer { block }`: İçinde bulunduğu kapsam kapanırken çalışacak temizlik kodunu mühürler.

### 10.4 Donanım ve Düşük Seviye (Kernel Ready)
- `peek(addr)` / `poke(addr, val)`: Doğrudan bellek manipülasyonu.
- `inb(port)` / `outb(port, val)`: I/O Port iletişimi.
- `irq(n, f)` / `intr(n)`: Donanım ve Yazılım kesme yönetimi.
- `asm:Label { ... }`: Satır içi (Inline) assembly blokları.
- `reg(name)`: İşlemci kayıtçılarına (registers) erişim.

### 10.5 Bellek Tahsis (Primitive Memory)
- `area(size)`: Ham bellek bloğu ayırır.
- `zone(size)`: Paylaşımlı bellek bloğu ayırır.
- `free(ptr)`: Ham bellek bloğunu serbest bırakır.
- `addr(v)`: Bir değişkenin fiziksel adresini döndürür.
- `zone.sync()`: Tüm çekirdeklerdeki cache'leri temizler ve verinin güncel olduğundan emin olur.
- `zone.view()`: Zone içeriğini salt-okunur (read-only) bir `area` olarak kopyalamadan map eder.
- `zone.lock()`: Manuel kritik bölge oluşturur (Sadece çok karmaşık işlemlerde).

### 10.6 Date Tarih
- `date()`:
- `datenow()`:
- `time()`:
- `clock()`:

---


## Bölüm 11. Yerel Modül Yükleme & DLL yükleme & Extern Modül Yükleme

** Kullanım:
```
use <module>;
use <module> as <yeni_isim>;

#örnek:
use io, str, cpu;
use math as m;

# Modül dosyalarında dışarı aktarılacak fonksiyon ve grouplar için.
# exp(export) pup(puplic)

exp group file{...}
pup f:myfunction(){...}

```
---
### 11.1 `import(path/imp.dll)` dll, .o, .obj, .so,vs.. import. `dllexp:` dll export. `impfn()` import fonksiyon.

`dllexp:` Main fonksiyonu yerine dll derlemek için ana fonksiyonu belirler.
`import(path)` dll yükler

```
#DLL olarak derliyor iseniz f: main(){} yerine aşağıdaki şekilde kullanılır.

dllexp: run_optimizer(*oir_path, *nxir_path, *target) {....}
```
* Kullanım:
```
# Dll yi yükle.

(Optimizer) <- import("optimizer.dll") ?-> !!("dll Bulunamadı!");

# 'Optimizer' masası artık içindeki fonksiyonları bir 'group' gibi sunar.
v:sonuc = Optimizer.run_optimizer(oir_path, nxir_path, target_info);

(Optimizer)?; # Serbest bırak. Yüklenen dll yi kaldır.
```

---

### 11.2 Extern Modül !!

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

** Kullanımı:
```
#Projene ekle ve kullan..
use msvcrt; # normal olarak stdlib de bulamaz ise, projedeki lib/ de arar..
# --- KULLANIM (Main Flow) ---
f:main()!i32 {
    # Uygulama geliştiricisi OS detayını görmez.
    SysIO.output("NexusFlow OS Bridge Active.");
    
    ret 0;
}

```
---

## 11.3  Macro Sistemi

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
        !!("Kritik Hata: Akış saptı, işlem iptal.");
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


----

# STDLIBS - Kütüphaler


## Bölüm 1. CoreModule "Çekirdek modül"
** Genişletilmiş Fonksiyon listesini barındırır. Os bağımlıdır ve (CoreModule) otomatik yüklenir. 
** Ana kod sayfası başına ( no:CoreModule; ) kullanılarak iptal edilebilir. Iptal ettiğinizde: Nexus Flow Primordial Katmanı "core" Modülü yüklenir.

### Fonksiyon Listesi
#### I/O - girdi/çıktı.

* `echo(data)`, `echo("Sonuç: {data} /n")`: Herhangi bir veriyi standart çıktıya basar.
* `input("Bir sayı giriniz: ")`: Kullanıcıdan giriş yakalar.

> Styling System (Renkli Çıktı)
* Yerleşik Stiller: error (Kırmızı), warn (Turuncu), info (Mavi), success (Yeşil)
* Stil Tanımlama (Global Kapsamda yapılmalıdır) Opsiyoneldir.
```
style Dikkat = "\033[31;1m";  # Kırmızı ve Kalın
style Bilgi  = "\033[36m";    # Cyan

# Kullanım (İkinci parametre olarak stil adı veya ANSI kodu verilir) opsiyonel

println("Bu kritik bir hatadır!: {e}", Dikkat);
print("İşlem tamamlandı.", Bilgi);
print("Doğrudan ANSI kodu.", "\033[32m"); # Yeşil
print("Normal yazı:{val}");

# Dışarıdan sitil almayanlar

echo("Normal yazı:{val} /n");  # dışarıdan sitil almaz
eprint("[E10010] Hata:.... "); # error stili ile basar 
```

#### File dosya işlemleri.

* `file.open(path,mode)`:  (mode: r, w, rw, a)
* `file.create(path)`: 
* `file.exists(path)`: 
* `file.delete(path)`: 
* `file.copy(src, dest)`: 
* `file.read()`: 
* `file.read_all()`: 
* `file.write()`: 
* `file.write_bytes()`: 
* `file.seek()`: 
* `file.tell()`: 
* `file.size()`: 
* `file.flush()`: 
* `file.close()`: 
* `file.path(path)`:  
---

-# path="C:\mingw64\x86_64-w64-mingw32\include\sec_api\stdio_s.h"  
-/->  0[C:] 1[mingw64] 2[x86_64-w64-mingw32] 3[include] 4[sec_api]  5[stdio_s]  6[.h] 
-` v: fname = file.path(path).last(-1);` # stdio_s  # .start C:   .last .h

#### A. Bellek ve Bölge Yönetimi (Area & Zone)**

* `mem.area(size)`: Tekil mülkiyetli alan açar.
* `mem.zone(size)`: Paylaşımlı bölge açar.
* `mem.resize(h, new_size)`: Masadaki alanın boyutunu (mümkünse yerinde) değiştirir.
* `mem.copy(dest_h, src_h)`: Sadece zorunlu durumlarda (explicit) derin kopyalama yapar.
* `mem.stat(h)`: Alanın doluluk oranını ve bilgisini döner.
* `mem.peek(ptr, type)` / `mem.poke(ptr, val)`: Ham adrese erişim.
* `mem.offset(ptr, n)`: Adres aritmetiği.
* `mem.move(dest, src, size)`: Ham blok taşıma.


#### B. Veri Akışı ve Senkronizasyon (Piping)**

* `flow.push(h, data)`: `<<` operatörünün fonksiyonel karşılığıdır.
* `flow.pull(h)`: `>>` Masadan veri çeker (pop).
* `flow.sync(z)` / `flow.lock(z)`: Bir `zone` üzerindeki tüm bekleyen yazmaları fiziksel belleğe işler (Memory Barrier).
* `flow.delay(ms)`: Akışı asenkron olarak bekletir (OS'te `sleep`, Bare-metal'de `timer_wait`).

#### C. Donanım ve Sistem Arayüzü (Hardware & OS)**

* `sys.io_read(port)` / `sys.io_write(port, val)`: OS'da izinli sürücü erişimi.
* `sys.reg_r(name)` / `sys.reg_w(name, val)`: CPU register manipülasyonu.
* `sys.env(key)`: OS modunda ortam değişkenleri,
* `sys.exit(code)`: Programı güvenli kapatır.
* `sys.call(id, args)`: Doğrudan syscall/interrupt tetikleme.
* `sys.io_r/w(port)`: Donanım port erişimi.
* `sys.info(query_id)`: Donanım/İşletim sistemi hakkında ham bilgi (CPU core sayısı, sayfa boyutu vb.).
* `sys.yield()`: İşlemciyi gönüllü olarak bir sonraki iş parçacığına bırakır.


#### D. Kripto ve Güvenlik (Security Guard)**

* `crypto.hash(h, algorithm)`: Bir `area` veya `zone` içeriğinin anlık hash'ini alır.
* `crypto.verify(h, signature)`: İçeriği dijital imza ile doğrular.
* `crypto.seal(h)`: Bir alanı "read-only" (sadece okunur) hale getirir, değiştirilemezlik mührü basar.

#### E. Hata ve Tanı (Diagnostic)**

* `err.last()`: Akışta oluşan son hatanın detaylı objesini döner.
* `err.trace()`: `panic` durumunda register dump ve stack izini verir.
* `log.echo(msg)`: Standart çıktı (OS: `stdout`.

#### F. Zaman ve Hassas Ölçüm (Timing)**

* `time.ticks()`: İşlemcinin çevrim (cycle) sayısını döner (RDTSC benzeri).
* `time.now()`: Epok zamanı veya sistem çalışma süresi.


----

## Bölüm 2.  Senkron, Asenkron  flow Modülü :  `listen()` `spawn()` `done()`

### Nexus Flow ile Asenkron Programlama: Kullanım Rehberi ve Örnekler

`use flow;` 

#### 1. Temel Operatörler

* `<- spawn(block, mode, Timeout)`: Yeni bir hafif iş parçacığı (green thread) başlatır. 
* (- spawn(th1, async, 5000) - mode: async, sync - Fonksiyon veya Blok: (th1)>{...} )
* `(blok_name)>{...}`: thread blokları..

* `!listen(th, intr)` : Thread işini bitirene kadar bekle/dinle, sonucu al ve akışı başlat. ( baremetal.) 
* `listen(th, w_u_time)`: Thread işini bitirene kadar bekle/dinle, sonucu al ve akışı başlat. ( os bağımlı.)

* `>>` : Veriyi bir sonraki işleme atomik aktar.
* `<<` : Veriyi bir paylaşımlı belleğe (zone) atomik aktar.
* `?->` : Hata durumunda yapılacakları tanımla.
* `done(type, val)` : Thread işlemlerinin bittiğini bildir.
* `?;` : Masayı temizle ve kapat.

`(done())` 3 şey yapar: 
- 1: datayı 1024 byte lık alana yazar. 
- 2: datanın ne olduğunu rzv tablosuna yazar. 1:data 2: bellek adresi + boyut 3: dosya yolu. 
- 3: işlemlerin bittiğini listen e dinleme bayrağını 1 yaparak bildirir. 

Thread'ler arası güvenli veri transferi:
- **`send(target_h, data)`**: Veriyi kopyalamadan sahipliğini hedefe taşır.
- **`receive(source_h)`**: Hedef masadan veri gelene kadar akışı bekleme moduna alır.
- **`done(type, val, addr)`**: İşlem sonucunu Nexus Slot Matrix'e yazar ve `listen`'ı tetikler.


#### 2. Kullanım Örnekleri


**Örnek 1: İşçi Tarafında Veri Üretimi (Done Kullanımı)**

```
use flow;

v:data = area(1024);
f:sensor_reader(port, *d) {
    v:raw = sys.io_read(port);
    
    # İş bitti: Veri tipi 1: (Raw) Max: 1024byte. 2: Verinin bulunduğu adres ve boyutu 8+8byte. 3: dosya yolu.
    done(1, raw, *d); 
}

f:main()!i32 {
    (h) <- spawn(sensor_reader(0xAF, *data), async, 500);
    
    # listen() sadece trigger_flag'e bakar, 
    # flag 1 olunca tabloyu tarayıp (h) slotunu bulur.
    listen(h, 10) >> (val) {
        echo("Sensör: {val.payload}");
    }?;
    
    ret 0;
}

```

**Örnek 2: Bare-Metal Olay Döngüsü (Power Saving)**
Bare-metal modunda `!listen()`, `active_tasks` varsa ve `trigger_flag` sıfırsa işlemciyi uyutur.

```
no:CoreModule;
use flow;

f:kernel_main() {
    (t1) <- spawn(check_disk(), async, 0);
    
    # Burada CPU 'HLT' moduna geçer. 
    # Donanım kesmesi (IRQ) gelip 'done' tetikleyince uyanır.
    !listen(t1, 0) >> (data) {
        process(data);
    }?;
}

```


#### 3. Programcı İçin İpuçları

* **Bellek:** `free()` veya `delete` kullanmanıza gerek yoktur; `?;` operatörü sizin için masayı siler.
* **Güvenlik:** Bir masayı `_>` ile taşıdıktan sonra eski masaya erişmeye çalışırsanız, derleyici (OCC) size derleme anında hata verir.
* **Timeout:** Her zaman mantıklı bir `timeout` değeri verin. Monitor(listen), kilitlenen thread'lerin sistemi çökertmesini engeller.

----

## Bölüm 3. Multi Procesing  ipc modülü

Yapı,Mantık,İşleyiş
fork(),Süreç Klonlama,Mevcut masayı (context) kopyalayarak yeni bir PID oluşturur.
process(path),Bağımsız Süreç,Belirtilen binary'yi tamamen izole bir bellekte başlatır.
core.cpu.affinity,Çekirdek Kilidi,Süreci fiziksel bir CPU çekirdeğine mühürler.
IPC (Inter-Process),Masa Paylaşımı,zone üzerinden süreçler arası ışık hızında veri takası.

```
# Standart uygulamalarda bunlar yüklenmez
use flow; # Threading ve Asenkron yetenekleri ekle
use ipc;  # Pipe ve Process yetenekleri ekle

f:main() {
    # Flow modülünden spawn kullanımı
    (h) <- flow.spawn(worker_task(), async, 100);
    
    # IPC modülünden pipe kullanımı
    (p1) <- ipc.process("./other_app");
    pipe(current, p1);
}

```


```
use ipc;
use flow;

f:main() {
    # 1. Yeni bir bağımsız süreç başlat (Multi-Processing)
    # Bu, spawn'dan farklı olarak kendi bellek alanına (Address Space) sahiptir.
    (p1) <- process.start("./optimizer.nxe");

    # 2. İşlemci çekirdeğine mühürle (Core Affinity)
    # CPU 0 ve 1'i bu sürece ayır.
    core.cpu.affinity(p1, [0, 1]);

    # 3. IPC (Inter-Process Communication)
    # 'zone' bellek alanı, süreçler arası ortak "Masa" görevi görür.
    (shared) <- mem.zone(4096);
    
    # Veriyi sürece gönder (Sahiplik devri değil, paylaşım)
    p1.attach(shared);

    # 4. Sürecin bitmesini dinle
    listen(p1, 5000) >> (exit_code) {
        echo("Süreç tamamlandı. Kod: {exit_code}");
    }?;
}
```

* pipe kullanımı:
```
f:main() {
    # 1. Süreçleri başlat
    (p1) <- process.start("./producer.nxe");
    (p2) <- process.start("./consumer.nxe");

    # 2. Arada bir PIPE (Boru Hattı) kur
    # Bu direktif, işletim sistemi seviyesinde bir I/O stream oluşturur.
    pipe(p1, p2);

    # 3. p1'den gelen veriyi p2'ye AKIT (Pipe üzerinden)
    # '>>' operatörü burada atomik bir 'bridge' görevi görür.
    (p1.output) >> (p2.input);

    # 4. Alternatif: Paylaşımlı Bellek (Zone) Kullanımı
    (shared) <- mem.zone(1024); # 1KB paylaşımlı alan
    
    # p1 veriyi yazar (Atomic Write)
    v:sensor_data = 0xFF;
    (shared) << sensor_data; # Paylaşımlı belleğe atomik yazma

    # p2 veriyi çeker (Flow Feed)
    v:target_data <- flow.pull(shared); # Zone'dan veri çekme
}

```

----


# Bölm 4 Nexus Flow Primordial Katmanı "core"  Modülü

Bu katman `core` takısı ile ifade edilir ve hiçbir `import` gerektirmez. Bu modül "no:CoreModule"  ile otomatik import edilir. Os bağımlılığı kaldırıdığında çalışır.

## 1. Bellek ve İşaretçi Primitifleri (Memory & Pointers)

Bir kernel yazmak için belleğin her hücresine erişim şarttır.

| Fonksiyon/Metot | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.ptr.read_u8(addr: u64)` | Belirtilen adresten 1 byte okur. | Donanım register'larını okumak. |
| `core.ptr.write_u8(addr, val)` | Belirtilen adrese 1 byte yazar. | Video belleğine (VGA) veri göndermek. |
| `core.ptr.offset(ptr, n)` | İşaretçiyi `n` kadar kaydırır. | Dizi ve veri yapısı navigasyonu. |
| `core.mem.size_of([T])` | Bir tipin bellekteki boyutunu döner. | Manuel bellek tahsisi (malloc benzeri). |
| `core.mem.align_of([T])` | Tipin hizalama (alignment) sınırını döner. | Veri yapısı optimizasyonu. |
| `core.mem.copy_raw(src, dst, n)` | Denetimsiz bellek kopyalama. | Kernel seviyesi veri transferi. |

---

## 2. Donanım ve CPU Komutları (CPU Intrinsics)

Bu fonksiyonlar doğrudan CPU talimatlarına (Instructions) derlenir.

| Fonksiyon/Metot | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.cpu.out_b(port, val)` | Donanım portuna byte gönderir. | Donanım cihazlarını (Klavye, Disk) kontrol etmek. |
| `core.cpu.in_b(port)` | Donanım portundan byte okur. | Donanımdan veri almak. |
| `core.cpu.halt()` | İşlemciyi durdurur (HLT). | Güç tasarrufu ve kernel boşta döngüsü. |
| `core.cpu.nop()` | İşlemciyi 1 çevrim boşta bırakır. | Hassas zamanlama/gecikme. |
| `core.cpu.cli()` / `sti()` | Interrupt'ları kapatır/açar. | Yarış durumlarını (Race conditions) engellemek. |
| `core.cpu.load_gdt(ptr)` | Global Descriptor Table yükler. | Bellek segmentasyonu ve korumalı mod geçişi. |
| `core.cpu.cpuid(leaf)` | İşlemci özelliklerini sorgular. | Donanım tanıma. |

---

## 3. Akış Kontrolü ve Yığın Yönetimi (Stack & Control)

| Fonksiyon | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.stack.set_ptr(addr)` | Yığın işaretçisini (ESP/RSP) ayarlar. | Bootloader'dan Kernel'a geçerken yığın oluşturma. |
| `core.stack.get_ptr()` | Mevcut yığın adresini döner. | Hata ayıklama (Stack trace). |
| `core.exec.jump(addr)` | Belirtilen adrese mutlak atlama yapar. | Kernel giriş noktasına (Entry point) gitmek. |
| `core.exec.call_raw(addr)` | Bir adresi fonksiyon gibi çağırır. | Dinamik yüklenen modülleri çalıştırmak. |

---

## 4. Tip ve Veri Tanımlama Metotları (Meta-Core)

Sıfırdan bir dil inşa ederken tiplerin nasıl davranacağını bu metotlar belirler.

* `core.type.cast(T, val)`: Veriyi güvenli/güvensiz olarak T tipine dönüştürür (Bit bazlı).
* `core.type.is_primitive(val)`: Verinin temel tip (int, bool vs.) olup olmadığını döner.

---

## 5. Uygulamalı Örnek: Sıfırdan "Hello World" Kernel (x86)

Hiçbir kütüphane (`io`, `os` vs.) kullanmadan, sadece çekirdek fonksiyonlarla ekrana (VGA Text Buffer) yazı yazan boot aşaması:

```
// OmniCore Kernel Entry
f:_start() {
    # VGA metin belleği adresi 0xB8000'dir.
    v:vga_buffer!u64 = 0xB8000
    v:message = "OmniCore Booted!"
    
    v:i!u64 = 0
    print_loop:> {
        (i < message.len()) =?> {
            v:char = message.at(i)
            
            # Karakteri yaz (Hizalama: Char, Color)
            core.ptr.write_u8(vga_buffer + (i * 2), char)
            core.ptr.write_u8(vga_buffer + (i * 2) + 1, 0x0F) // Beyaz renk
            
            i = i + 1
            (print_loop)?>
        }
    }

    # Kernel'ı sonsuz döngüde tut
    (loop) {
        core.cpu.halt()
    }
}

```

---

## 6. OmniCore Primordial Listesi (Tam Özet)

Aşağıdaki fonksiyonlar `fastexec` bloğu içerisinde **sıfır maliyetle** çalışır:

1. **Giriş/Çıkış:** `core.cpu.in_b`, `core.cpu.out_b`, `core.cpu.in_w`, `core.cpu.out_w`
2. **Bellek:** `core.ptr.add`, `core.ptr.sub`, `core.ptr.to_addr`, `core.addr.to_ptr`
3. **Kesme Yönetimi:** `core.cpu.idt_set`, `core.cpu.irq_mask`, `core.cpu.panic`
4. **Register Erişimi:** `core.cpu.set_reg(name, val)`, `core.cpu.get_reg(name)`
5. **Atomik İşlemler:** `core.atomic.cas` (Compare and Swap), `core.atomic.add` (Lock-free yapılar için).


----
