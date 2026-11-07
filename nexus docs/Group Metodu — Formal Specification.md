
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

* **Grup** içerisinde tanımlanan yapılar ve metodlar **grup dışından erişilemez**. Bu, kapsülleme (encapsulation) ilkesini destekler.
* **Metodlar** sadece **group adı ile çağrılır**. Dışarıdan doğrudan yapılamaz.

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
