
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
