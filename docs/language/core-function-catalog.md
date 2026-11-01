
# **NexusFlow Core – Tam Fonksiyon / Metot Listesi**

## **1. Bellek ve İşaretçi Primitifleri**

| Fonksiyon / Metot                                 | İmza (Nx) | Return                          | Açıklama |
| ------------------------------------------------- | --------- | ------------------------------- | -------- |
| `core.ptr.read_u8(addr!u64)!u8`                   | u8        | Belirtilen adresten 1 byte okur |          |
| `core.ptr.write_u8(addr!u64, val!u8)!void`        | void      | Belirtilen adrese 1 byte yazar  |          |
| `core.ptr.offset(ptr!*u8, n!i64)!*u8`             | pointer   | Pointer aritmetiği              |          |
| `core.mem.size_of([T])!u64`                       | u64       | Tipin bellekteki boyutu         |          |
| `core.mem.align_of([T])!u64`                      | u64       | Tipin hizalama sınırı           |          |
| `core.mem.copy_raw(src!*u8, dst!*u8, n!u64)!void` | void      | Denetimsiz ham bellek kopyalama |          |
| `mem.area(size!u64)!*u8`                          | pointer   | Tekil mülkiyetli alan           |          |
| `mem.zone(size!u64)!*u8`                          | pointer   | Paylaşımlı alan                 |          |
| `mem.resize(h!*u8, new_size!u64)!bool`            | bool      | Alan boyutunu değiştirir        |          |
| `mem.move(dest!*u8, src!*u8, size!u64)!void`      | void      | Ham blok taşıma                 |          |
| `mem.peek(ptr!*u8, type!Type)!Any`                | Any       | Ham adrese okuma                |          |
| `mem.poke(ptr!*u8, val!Any)!void`                 | void      | Ham adrese yazma                |          |
| `mem.zero(h!*u8, size!u64)!void`                  | void      | Alanı sıfırlar                  |          |

---

## **2. Donanım ve CPU Komutları**

| Fonksiyon / Metot                       | İmza (Nx) | Return                      | Açıklama |
| --------------------------------------- | --------- | --------------------------- | -------- |
| `core.cpu.out_b(port!u16, val!u8)!void` | void      | Porta byte gönderir         |          |
| `core.cpu.in_b(port!u16)!u8`            | u8        | Porttan byte okur           |          |
| `core.cpu.halt()!void`                  | void      | İşlemciyi durdurur (HLT)    |          |
| `core.cpu.nop()!void`                   | void      | 1 çevrim boşta bekler       |          |
| `core.cpu.cli()!void` / `sti()!void`    | void      | Interrupt’ları kapat / aç   |          |
| `core.cpu.load_gdt(ptr!*u8)!void`       | void      | GDT yükler                  |          |
| `core.cpu.cpuid(leaf!u32)!u32[]`        | u32[]     | İşlemci özellikleri         |          |
| `sys.io_read(port!u16)!u8`              | u8        | OS bağımlı I/O okuma        |          |
| `sys.io_write(port!u16, val!u8)!void`   | void      | OS bağımlı I/O yazma        |          |
| `sys.reg_r(name!str)!u64`               | u64       | CPU register oku            |          |
| `sys.reg_w(name!str, val!u64)!void`     | void      | CPU register yaz            |          |
| `sys.call(id!i32, args!Any[])!Any`      | Any       | Syscall / Interrupt çağrısı |          |
| `sys.info(query_id!i32)!Any`            | Any       | OS / Donanım bilgisi        |          |
| `sys.yield()!void`                      | void      | İşlemciyi gönüllü bırak     |          |

---

## **3. Akış Kontrolü ve Yığın Yönetimi**

| Fonksiyon                           | İmza                     | Açıklama |
| ----------------------------------- | ------------------------ | -------- |
| `core.stack.set_ptr(addr!u64)!void` | Yığın pointer ayarlama   |          |
| `core.stack.get_ptr()!*u8`          | Mevcut yığın adresi      |          |
| `core.exec.jump(addr!u64)!void`     | Mutlak adres atlama      |          |
| `core.exec.call_raw(addr!u64)!void` | Adres fonksiyon çağrısı  |          |
| `flow.push(h!*u8, data!Any)!void`   | Masaya veri iter         |          |
| `flow.pull(h!*u8)!Any`              | Masadan veri alır        |          |
| `flow.sync(z!*u8)!void`             | Bekleyen yazmaları işler |          |
| `flow.lock(z!*u8)!bool`             | Kritik bölge kilidi      |          |
| `flow.unlock(z!*u8)!void`           | Kilidi açar              |          |
| `flow.delay(ms!u32)!void`           | Asenkron bekleme         |          |

---

## **4. Tip ve Meta**

| Fonksiyon                              | İmza                           | Açıklama |
| -------------------------------------- | ------------------------------ | -------- |
| `core.type.cast(T, val!Any)!T`         | Güvenli / zorunlu tip dönüşümü |          |
| `core.type.is_primitive(val!Any)!bool` | Temel tip kontrolü             |          |
| `typeof(val!Any)!Type`                 | Çalışma zamanı tip bilgisi     |          |
| `sizeof(val!Any)!u64`                  | Bellek boyutu                  |          |
| `len(val!Any)!u64`                     | Koleksiyon / string uzunluğu   |          |

---

## **5. Kripto ve Güvenlik**

| Fonksiyon                                  | İmza                   | Açıklama |
| ------------------------------------------ | ---------------------- | -------- |
| `crypto.hash(h!*u8, algorithm!str)!u64`    | Hash alır              |          |
| `crypto.verify(h!*u8, signature!*u8)!bool` | İmza doğrulama         |          |
| `crypto.seal(h!*u8, size!u64)!void`        | Read-only mühür        |          |
| `crypto.zero(h!*u8, size!u64)!void`        | Hassas veriyi sıfırlar |          |
| `crypto.random(bytes!u64)!*u8`             | Güvenli rastgele veri  |          |

---

## **6. Hata ve Tanı**

| Fonksiyon                         | İmza                       | Açıklama |
| --------------------------------- | -------------------------- | -------- |
| `err.last()!ErrObj`               | Son hatayı döner           |          |
| `err.trace()!TraceObj`            | Panic stack / register izi |          |
| `log.echo(msg!str)!void`          | Standart çıktı             |          |
| `log.warn(msg!str)!void`          | Uyarı mesajı               |          |
| `log.info(msg!str)!void`          | Bilgi mesajı               |          |
| `panic(msg!str)!void`             | Kritik hata ve durdurma    |          |
| `assert(cond!bool, msg!str)!void` | Koşul doğrulama            |          |

---

## **7. Zaman ve Hassas Ölçüm**

| Fonksiyon                       | İmza                         | Açıklama |
| ------------------------------- | ---------------------------- | -------- |
| `time.ticks()!u64`              | İşlemci çevrim sayısı        |          |
| `time.now()!u64`                | Epok / uptime                |          |
| `time.clock()!f64`              | High-res CPU / uptime ölçümü |          |
| `time.sleep(ms!u32)!void`       | OS veya timer bekleme        |          |
| `time.delay_cycles(n!u64)!void` | Döngü tabanlı delay          |          |

---

✅ **Özet:**

* Bu tablo, **Primordial Core + High-Level CORE** API’lerini kapsıyor.
* OS bağımlı fonksiyonlar (`sys.env`, `sys.sleep`, `flow.delay`) ile **bare-metal** fonksiyonları (`core.cpu`, `core.ptr`) bir arada görülebiliyor.
* Artık çekirdek düzeyinde eksik fonksiyon yok; bu liste **tam bir çekirdek referans** olarak kullanılabilir.

