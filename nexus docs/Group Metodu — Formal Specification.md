
# 5.4 Group Metodu — Formal Specification

## 1. Tanım

**Group Metodu**, bir grup içerisinde benzer işlevlere sahip fonksiyonları organize etmek amacıyla kullanılan bir yapıdır. Bu metot, özellikle **kütüphane yazımı** ve **modülerlik** konusunda kolaylık sağlar. Aynı zamanda, **nesne yönelimli programlama (OOP)** yaklaşımını **group** yapıları üzerinde implement etmek için de uygundur.

**Group** metodu, bir tür **namespace** gibi çalışarak, fonksiyonları **gruplar** altında organize eder. Bu gruplar, hem **alt grup** hem de **default** grup yapıları ile genişletilebilir. Ayrıca **object-like** bir yapı oluşturularak, gruptaki fonksiyonlar nesne metodları gibi kullanılabilir.

---

## 2. Temel Yapı

Bir **group** şu şekilde tanımlanır:

```plaintext
group <group_adı> {
    <metot_adı> => f:(parametreler) { ... }
    ...
    default { ... }
}
```

* **`group_adı`**: Grubun adı (örneğin: `db`, `file`, `user`).
* **`<metot_adı>`**: Grup içerisinde tanımlanan işlevin adı.
* **`f:(parametreler)`**: Fonksiyon tanımı; parametreler ve fonksiyon içeriği belirtilir.
* **`default`**: Gruba ait varsayılan işlem (opsiyonel). Kullanıcı **group adı** belirtmezse varsayılan grup çalıştırılır.

Grup içerisinde **alt grup** (subgroup) tanımlanabilir. Alt grup, **grup adı** ve **subgroup adı** şeklinde organize edilir.

---

## 3. Kullanım

### 3.1 Group Tanımlaması

Bir **Group** fonksiyonu aşağıdaki gibi tanımlanır:

```plaintext
group db {
    # NoSQL alt grubu
    nosql => group {
        open => f:(...) { ... }
        close => f:(...) { ... }
        new => f:(...) { ... }
        query => f:(...) { ... }
        help => f:() { ... }
        default { echo("Yardım için db.nosql.help kullanın."); }
    }

    # SQLite alt grubu
    sqlite => group {
        open => f:(...) { ... }
        close => f:(...) { ... }
        new => f:(...) { ... }
        query => f:(...) { ... }
        help => f:() { ... }
        default { echo("Yardım için db.sqlite.help kullanın."); }
    }

    # Varsayılan (default) işlem
    default { echo("Yardım için db.sqlite.help veya db.nosql.help kullanın."); }
}
```

Bu tanımlamada:

* **`nosql` ve `sqlite` alt grupları** ile farklı veritabanı türlerine yönelik fonksiyonlar tanımlanır.
* **`default`** opsiyonel bir alan olup, grup ismi verilmeden çağrılan fonksiyon için varsayılan davranışı belirler.

### 3.2 Kullanım Örneği

Aşağıda, `db` grubunun nasıl kullanılacağını gösteren örnekler bulunmaktadır:

```plaintext
# NoSQL veritabanını açmak
db.nosql.open("category.db");

# SQLite veritabanını açmak
db.sqlite.open("product.db");

# Varsayılan db işlemi çalıştırmak
db  # Varsayılan grup çalıştırılır.
db.sqlite  # SQLite grubunun varsayılan fonksiyonu çalıştırılır.
```

---

## 4. Nesne Yönelimli Kullanım (Struct & Group)

### 4.1 Group ve Struct İlişkisi

Eğer bir `group`, bir **struct**, **enum**, veya **union**'a metot ekleyecekse, isimlerin **birebir aynı** olması gerektiği kuralı vardır. Bu, **OOP** tarzı bir kullanım sunar ve dışarıdan erişimi kontrol eder.

### 4.2 Nesne Metodu

Bir nesne metodunun ilk parametresi **`self`** olmak zorundadır. Bu sayede **nesneye ait özellikler** fonksiyon içinde manipüle edilebilir.

```plaintext
group Oyuncu {
    nt: struct:Oyuncu {
        v:ad!str,
        v:puan!i32,
        v:aktif!bool
    };

    # Yapıcı Fonksiyon (Constructor) - Statik
    yeni => f:(isim)!Oyuncu {
        return Oyuncu { 
            ad = isim,
            puan = 0,
            aktif = true 
        };
    }

    # Puan artırma fonksiyonu
    puan_arttir => f:(self, miktar) { 
        self.puan = self.puan + miktar;
    }

    # İsim getir fonksiyonu
    isim_getir => f:(self) > self.ad;
    
    # Alt grup tanımlamaları
    canta => group {
        envanter => { ... }
        silah => { ... }
    }
    
    default { echo("Yardım için Oyuncu.help kullanın."); }
}
```

### 4.3 Nesne Metodu Kullanımı

```plaintext
# Yeni bir Oyuncu oluştur
o1 = Oyuncu.yeni("Ahmet");

# Puan artır
o1.puan_arttir(10);

# Oyuncunun ismini getir
echo(o1.isim_getir());  # Çıktı: Ahmet
```

Burada:

* **`self`** ilk parametre olarak kullanılarak, **Oyuncu** nesnesine ait özelliklere (`ad`, `puan`, `aktif`) erişilebilir.
* **Alt grup** olan `canta`, **Oyuncu** nesnesiyle ilişkili olarak kullanılır: `Oyuncu.canta.envanter()`.

### 4.4 Yapıcı Fonksiyon

Yapıcı fonksiyonlar, nesne oluştururken gerekli başlangıç verilerini belirler:

```plaintext
# Yeni bir Oyuncu nesnesi oluşturulurken isim, puan ve aktif durumu başlangıçta atanır.
o1 = Oyuncu.yeni("Ahmet");
```

---

## 5. Kapsam (Scope)

Group üyelerinin erişimi, Group namespace üyeliği ve `pup` / `exp` görünürlük kurallarıyla belirlenir. Group içindeki her üye otomatik olarak dışarıya kapalı değildir; public API üyeleri Group yolu üzerinden çözümlenebilir.

- Group içinde tanımlanan `struct`, `enum` ve `union` türleri özel Group türleridir. Yalnızca tanımlandıkları Group'un kendi fonksiyon/metotları tarafından kullanılabilir; dış fonksiyonlar ve diğer Group'lar bu türleri doğrudan kullanamaz.
- Group dışında tanımlanan türler, normal lexical scope ve modül görünürlüğü kuralları çerçevesinde Group içinde kullanılabilir.
- Group-owned türlerin izolasyonu `pup` / `exp` ile kaldırılamaz. Public/export görünürlüğü, özel türün sahibini değiştirmez ve türü Group dışına açmaz.
- Nested Group bağımsız bir Group scope'tur; üst Group'un özel türlerini devralmaz. Her alt Group kendi Struct/Enum/Union türlerini tanımlayabilir ve bunlar yalnızca kendi Group'unda görünür.
- Namespace fonksiyonları `Group.member` / `Group.SubGroup.member` yoluyla; Struct ile eşleştirilmiş instance metotları ise receiver kuralına göre `instance.method(args)` biçiminde çözümlenir.


---

## 6. Kullanıcı Tanımlı ve Modüler Yapı

Group metodunun en büyük avantajı, **modüler ve bağımsız** işlevsellik sunmasıdır. Bu yapı sayesinde **kütüphaneler**, **modüller** ve **servisler** birbirinden izole şekilde organize edilebilir.

Örneğin, dosya işlemleri, veritabanı bağlantıları veya ağ servisleri gibi gruplar tek bir çatı altında toplanabilir ve kendi içlerinde alt gruplar kullanılarak işlevler düzenlenebilir.

---

## 7. Özet

* **Group Metodu**, fonksiyonları gruplar halinde organize etmek ve işlevselliği modüler bir şekilde sunmak amacıyla geliştirilmiştir.
* **Alt gruplar**, daha detaylı ve hiyerarşik yapıların oluşturulmasına olanak tanır.
* **Nesne Yönelimli Programlama (OOP)** felsefesiyle, **struct** yapılarıyla ilişkilendirilip metodlar tanımlanabilir.
* Kullanıcılar, **grup içindeki metodları** belirli bir grup adı ile çağırarak kolayca erişebilir.

---


## 8. Kütüphane ve Modül Modeli (Normatif Tasarım)

Group, yalnızca instance metotlarını taşıyan bir nesne kabı değildir. Nexus Flow kütüphane API'sinin isim alanı ve üyelik organizasyonunun da temel yapısıdır. İki kullanım biçimi ayrılır:

- **Namespace Group:** Kütüphanenin fonksiyonlarını, sabitlerini, türlerini ve alt gruplarını isim alanında toplar. Instance oluşturulması gerekmez.
- **Object Group:** Group bildirimi bir Struct ile eşleştirilerek instance alanları üzerinde çalışan metotlar sağlar. Namespace ve instance davranışları birlikte bulunabilir.

### 8.1 Kütüphane yüzeyi ve üyeler

| Üye | Kütüphane API rolü |
|---|---|
| Function / Method | İşlev ve metot dışa aktarımı |
| Nested Group | Alt namespace / alt API |
| Struct / Enum / Union | Kullanıcıya sunulan veri türleri |
| Variable / Const | Kütüphane durumu veya sabitleri |
| Default | Group çağrısı/namespace seçimi için varsayılan davranış |

Aynı isimli API üyelerinin aynı Group içinde çakışması Semantic aşamasında hata olmalıdır; Parser yalnızca yapıyı üretir.

### 8.2 Public / Export görünürlüğü

- **Private:** Group içinde görünür; dış tüketici API'sine dahil değildir.
- **Public (pup):** Kaynak/modül sınırının dışından erişilebilir API üyesi.
- **Export (exp):** Derleme/kütüphane çıktısında dışa sunulacak sembol; export, public erişimden ayrı bir çıktı/linker kararıdır.

Public olmak otomatik olarak export edilmek anlamına gelmez. Export edilen sembolün API görünürlüğü ve ABI kararı Semantic/Module çözümleme aşamasında doğrulanmalıdır. Mevcut pup/exp anlamları genişletilmeden, Group üyelerinin görünürlüğü AST ve Semantic katmanlarında temsil edilmelidir.

### 8.3 m: bağımlılık bildirimi ile use erişimi

İki mekanizma farklı sorumluluk taşır:

- **m:Name = "source";** zorunlu bağımlılığı bildirir. Kaynak çözümlenemez/yüklenemezse derleme başarısız olur.
- **use Name; / use path.segment;** modül adını/üyelerini mevcut kaynak kapsamına açma talebidir. Parser dotted path'i AST_USE içinde tek modül yolu olarak korur; gerçek çözümleme ve yükleme Semantic/Module Resolver sorumluluğudur.

Bunlar birbirinin yerine kullanılmaz. use ifadesi kendi başına dosya yolu veya paket indirme garantisi değildir; dependency resolver'ın kaynak politikası uygulanır.

### 8.4 Namespace çözümleme ve erişim

- **Library.member:** kütüphane/Group üyesi çözümlemesi.
- **Library.SubGroup.member:** iç içe namespace çözümlemesi.
- **Type.method:** Struct ile eşleştirilmiş Group'un statik/constructor metodu.
- **instance.method(args):** receiver bağlama kuralıyla instance metodu; ilk self parametresi otomatik receiver alır.
- **Group / Group.SubGroup:** ilgili scope'ta default üye tanımlıysa default davranış çağrısı/çözümlemesi.

Member access AST'de nokta zincirinin parse edilmiş olması yeterli değildir. Semantic; sembolün Group/Struct/Module türünü, üyelik ilişkisini, visibility ve çağrı biçimini doğrulamalıdır. Çözümleme başarılı olduğunda Semantic AST annotation'ı resolved symbol/member/receiver ilişkisini taşımalıdır; OIR ve Codegen isim metninden tahmin yürütmemelidir.

### 8.5 Module Resolver ve Library ABI sınırı

Kütüphane desteği, Group AST'sini doğrudan runtime nesnesine çevirmek değildir. Modüler sorumluluklar:

1. **Parser/AST:** Group, nested Group, member ve use / m: syntax ağacını kayıpsız üretir.
2. **Semantic:** Group scope, üye isim çakışması, tür/Struct eşleşmesi, self, constructor, default ve visibility kontrolü.
3. **Module Resolver:** Dependency kaynağını bulur, modül kimliğini ve import kapsamını oluşturur; dependency graph/cycle ve sürüm/ABI metadata kontrolünü yürütür.
4. **OIR:** Çözülmüş sembol kimliklerini ve çağrı hedeflerini taşır; kaynak modül adını runtime'da tekrar aramaz.
5. **Codegen/Linker:** Export edilen sembolleri hedef ABI'ye göre üretir/bağlar. Linker'ın ayrı modül olması kararı korunur.

**Uygulama durumu:** Bu bölüm tasarım kontratını tanımlar; Semantic/Module Resolver/ABI tamamlandı iddiası değildir. Kütüphane ve Group semantic testleri kapanmadan Group tamamlanmış sayılmaz.
## 9. Group-Owned Type Scope (Normative)

Bu bölüm, Group kapsamındaki nominal tür görünürlüğünü kesinleştirir ve Semantic uygulaması için normatif kuraldır.

### 9.1 Group içinde tanımlanan türler

Bir Group'un doğrudan üyeleri arasında bildirilen her `struct`, `enum` veya `union`, bildirildiği Group'a ait özel bir tür sembolüdür.

- Türün sahibi, bildirimin bulunduğu en yakın Group kapsamıdır.
- Aynı Group'a ait fonksiyonlar ve metotlar bu türü kendi imzasında, parametrelerinde, dönüş tipinde, yerel değişkenlerinde ve tür ifadelerinde kullanabilir.
- Group dışındaki fonksiyonlar/metotlar bu türü kullanamaz.
- Başka bir Group, kardeş Group veya iç içe Group bu türe doğrudan erişemez.
- Türün adının başka bir kapsamda aynı olması, sembolleri aynı tür yapmaz; tür kimliği ve sahibi ayrı tutulur.
- Group dışı görünürlük `pup`, `exp` veya başka bir görünürlük bayrağıyla açılamaz.

Bu kural Struct, Enum ve Union için aynıdır; yalnızca Struct'a özgü değildir.

### 9.2 Group dışında tanımlanan türler

Bir `struct`, `enum` veya `union` herhangi bir Group'un sahibi değilse, normal lexical scope ve modül görünürlüğü kurallarına tabidir. Bu türler, kapsamları izin verdiği sürece Group içindeki fonksiyon/metotlar tarafından kullanılabilir.

### 9.3 Nested Group izolasyonu

Her nested Group kendi Group scope kimliğine sahiptir.

- Alt Group kendi özel türlerini tanımlayabilir.
- Alt Group, üst Group'un özel türlerini otomatik olarak devralmaz.
- Üst Group, alt Group'un özel türlerini kullanamaz.
- Kardeş Group'lar birbirlerinin özel türlerini kullanamaz.
- Alt Group'un namespace üyelerine erişim, Group türlerinin özel kapsamını genişletmez.

### 9.4 Semantic uygulama yükümlülükleri

Semantic katmanı:

1. Group-owned tür sembolünün sahibi olan Group scope kimliğini saklamalıdır.
2. Tür adı çözümlemesinde yalnızca ad eşleşmesini yeterli saymamalı; mevcut aktif Group kimliği ile türün sahibi olan Group kimliğini doğrulamalıdır.
3. Fonksiyon parametre/dönüş tipleri, değişken bildirimleri, alan tipleri, enum payload tipleri, tür dönüşümleri ve diğer tüm tür referanslarında aynı erişim denetimini uygulamalıdır.
4. Group scope kapandığında o Group'a ait sembolleri aktif sembol çözümlemesinden çıkarmalıdır.
5. Aynı isimli türlerin farklı Group'larda çakışmadan ayrı sembol kimlikleriyle temsil edilmesini sağlamalıdır.
6. Semantic sonucu/OIR için türün gerçek sembol kimliğini korumalı; isim metninden yeniden çözümleme yapılmasına güvenmemelidir.

Parser bu erişim kararını vermez; AST'de bildirimin ve referansın yapısını kayıpsız tutar. Erişim doğrulaması Semantic sorumluluğudur.

### 9.5 Zorunlu test matrisi

| Senaryo | Beklenen sonuç |
|---|---|
| Group içindeki fonksiyon kendi Struct türünü kullanır | Geçer |
| Group içindeki metot kendi Enum türünü kullanır | Geçer |
| Group içindeki fonksiyon kendi Union türünü kullanır | Geçer |
| Dış fonksiyon Group Struct türünü kullanır | Hata |
| Dış fonksiyon Group Enum türünü kullanır | Hata |
| Dış fonksiyon Group Union türünü kullanır | Hata |
| Başka bir Group ilk Group'un özel türünü kullanır | Hata |
| Nested Group üst Group'un özel türünü kullanır | Hata |
| Üst Group nested Group'un özel türünü kullanır | Hata |
| Kardeş Group diğerinin özel türünü kullanır | Hata |
| Nested Group kendi Struct/Enum/Union türünü kullanır | Geçer |
| Group dışında tanımlı tür, görünür olduğu Group içinde kullanılır | Geçer |
| Aynı isimli tür iki farklı Group'ta tanımlanır | Ayrı semboller olarak geçer |
| Group kapandıktan sonra özel tür sembolü çözümlemede bulunur | Hata |

### 9.6 Tamamlanma koşulu

Bu kural ancak Struct, Enum ve Union için yukarıdaki erişim testleri Semantic testlerinde doğrulandığında uygulanmış sayılır. Belgenin güncellenmesi tek başına uygulama veya test tamamlanması anlamına gelmez.

