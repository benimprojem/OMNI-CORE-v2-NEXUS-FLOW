
# 14. ÇEKİRDEK FONKSİYONLAR (CORE — KÜTÜPHANESİZ) — Formal Specification

## 1. Tanım

**Core Fonksiyonları**, OS veya standart kütüphanelere bağımlı olmadan **donanım, bellek, CPU ve sistem yönetimini** sağlayan temel fonksiyonlar kümesidir.
Amaç:

* Bare-metal ve kernel-ready çalışmak,
* Düşük seviye kontrol ve yönetim,
* Hafif, deterministik ve hızlı çalışma ortamı sağlamak.

> Not: Core fonksiyonları, `no:CoreModule;` altında kullanılabilir.

---

## 2. Fonksiyon Kategorileri

| Kategori                 | Fonksiyon                                              | İşlevi / Açıklama                              |
| ------------------------ | ------------------------------------------------------ | ---------------------------------------------- |
| **Donanım**              | `write()` / `fwrite()`                                 | Donanım bazlı I/O, buffer veya fiziksel çıktı. |
| **Donanım**              | `inb(port)` / `outb(port, val)`                        | Donanım portlarına doğrudan erişim.            |
| **Donanım**              | `peek(addr)` / `poke(addr, val)`                       | Fiziksel bellek okuma/yazma.                   |
| **Donanım**              | `irq(n, f)` / `intr(n)`                                | Kesme (Interrupt) tanımlama ve çağırma.        |
| **İşlemci**              | `reg.r(name)` / `reg.w(name, val)`                     | CPU register okuma/yazma (rax, rsp vb.).       |
| **Bellek**               | `area(size)` / `zone(size)` / `free(ptr)` / `addr(v)`  | Ham bellek ve paylaşılmış bellek yönetimi.     |
| **Sistem**               | `panic(msg)` / `exit(code)` / `defer {}` / `swap(a,b)` | Süreç ve güvenlik yönetimi.                    |
| **Dönüşüm & Reflection** | `cast(v, type)` / `typeof(v)` / `sizeof(v)`            | Tip dönüştürme ve yansıma (reflection).        |
| **Zaman**                | `time()` / `clock()`                                   | CPU ve sistem zaman ölçümü.                    |

---

## 3. Giriş / Çıkış (I/O)

### 3.1 Fonksiyonlar

* `write(data)`: Veriyi donanım buffer’ına yazar.
* `fwrite(buffer)`: Buffer’dan hedef donanım veya dosyaya yazar.

> Kullanım: Basit I/O, dosya veya aygıt iletişimi.

---

## 4. Meta ve Yansıma (Reflection)

* `typeof(v)` → Değişkenin aktif tipini döndürür.
* `sizeof(v)` → Değişkenin bellekte kapladığı boyutu (byte) döndürür.
* `len(v)` → Koleksiyon, string veya buffer uzunluğu.
* `val.is_ok()` / `val.is_err()` → İşlemin başarılı mı başarısız mı olduğunu `bool` olarak verir.
* `val.is_some()` / `val.is_none()` / `val.is_null()` → Option tipindeki değerlerin varlık kontrolü.

---

## 5. Sistem ve Yönetim

| Fonksiyon         | Açıklama                                           |
| ----------------- | -------------------------------------------------- |
| `panic(msg)`      | Kritik hata mesajı basar ve uygulamayı durdurur.   |
| `exit(code)`      | Belirtilen çıkış kodu ile programı sonlandırır.    |
| `cast(v, type)`   | Güvenli veya zorunlu tip dönüşümü yapar.           |
| `wait(time-ms)`   | CPU’yu belirtilen süre uyutur / bekletir.          |
| `swap(a, b)`      | İki değişkeni yerinde değiştirir (XOR tabanlı).    |
| `defer { block }` | Kapsam kapanırken çalışacak cleanup/temizlik kodu. |

---

## 6. Donanım ve Düşük Seviye (Kernel Ready)

* `peek(addr)` / `poke(addr, val)` → Fiziksel bellek okuma/yazma.
* `inb(port)` / `outb(port, val)` → I/O port iletişimi.
* `irq(n, f)` → Belirli kesmeyi tanımlar ve fonksiyon ile ilişkilendirir.
* `intr(n)` → Kesmeyi manuel tetikler.
* `asm:Label { ... }` → Inline assembly blokları tanımlar.
* `reg.r(name)` / `reg.w(name, val)` → İşlemci register’larına erişim.

---

## 7. Bellek Yönetimi (Primitive Memory)

* `area(size)` → Ham bellek bloğu ayırır (heap-like).
* `zone(size)` → Paylaşımlı ve cache-friendly bellek bloğu ayırır.
* `free(ptr)` → Daha önce tahsis edilmiş bloğu serbest bırakır.
* `addr(v)` → Değişkenin fiziksel adresini döndürür.
* `zone.sync()` → Cache’leri temizler, tüm çekirdeklerde veri tutarlılığı sağlar.
* `zone.view()` → Zone içeriğini **read-only** olarak map eder.
* `zone.lock()` → Manuel kritik bölge oluşturur, düşük seviye concurrency kontrolü sağlar.

---

## 8. Zaman Fonksiyonları

* `time()` → Sistem / gerçek zaman saatini döndürür.
* `clock()` → CPU saat döngüsü veya uptime ölçümü.

> Bu fonksiyonlar, bare-metal ve OS bağımsız zaman ölçümü için kullanılır.

---

## 9. Örnek Kullanım

```nexus id="core_example"
f:kernel_main()!i32 {
    v:buffer = area(1024);

    write("Başlatılıyor...\n");

    irq(0x20, timer_tick);  # Timer kesmesi tanımlandı

    poke(0xB8000, 0x1F);    # Ekran renk register’ı

    v:a!i32 = 10;
    v:b!i32 = 20;

    swap(a,b);               # Değerleri yer değiştir

    defer { free(buffer); }  # Kapanışta hafızayı serbest bırak

    return 0;
}
```

* Bare-metal ortamda çalışmaya hazır.
* Masraf yok, OS kütüphanesi bağımlılığı yok.
* Kesme, memory ve CPU register kontrolü direkt.

---
