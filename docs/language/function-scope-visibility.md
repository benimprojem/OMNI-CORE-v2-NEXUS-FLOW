
# 6.4 Fonksiyon Scope ve Visibility (Geliştirilmiş)

### 1. Genel Kurallar

* Fonksiyonlar ve group’lar **default olarak private** sayılır; sadece tanımlandıkları scope içinde görünür.
* **Public / dışa açmak** için `pup` (public function) veya `exp` (export group) kullanılır.
* `group` veya `struct` içindeki fonksiyonlar **sadece o kapsama görünür**, dışarıdan erişim ancak `pup` veya `exp` ile mümkündür.

### 2. Kullanım Örnekleri

```nexus
# Modül içi tanım
group file {
    pup f:open(path!str) {
        # public fonksiyon, dışarıdan erişilebilir
        ...
    }
    
    f:helper(path!str) {
        # private fonksiyon, sadece file group içinde kullanılabilir
        ...
    }
}

# Modül yükleme
use file;

# Public fonksiyon çağrısı
file.open("test.txt");

# Private fonksiyona erişim -> HATA
file.helper("test.txt"); # ❌ erişilemez
```

---

### 3. Modül İçe Aktarma ve Alias

```nexus
use io, sys, cpu;      # Normal import
use math as m;         # Alias ile import
```

* Alias sayesinde `m.sqrt(9)` gibi çağrılar yapılabilir.

---

### 4. Export / Public Tanımı

```nexus
# Group dışa açma
exp group network {
    pup f:connect(addr!str) { ... }  # Public fonksiyon
    f:internal_ping() { ... }        # Private fonksiyon
}
```

* `exp group` → grubu dışarı aktarır
* `pup f:fonksiyon` → fonksiyonu public yapar
* Tanımlanmayanlar **default private**

---