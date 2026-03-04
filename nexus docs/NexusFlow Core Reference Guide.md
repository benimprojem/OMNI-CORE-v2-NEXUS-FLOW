
* Kategori
* Fonksiyon İmzası (Nx Syntax)
* Return tipi
* Opsiyonel parametreler
* Bare-metal / OS açıklaması


---

# **NexusFlow Core API Spec (Zero Functions / Bare-Metal & OS)**

---

## **A. Bellek ve Bölge Yönetimi (Area & Zone)**

| Fonksiyon | İmza (Nx)                                    | Return  | Opsiyonel | Bare-metal / OS | Açıklama                                |
| --------- | -------------------------------------------- | ------- | --------- | --------------- | --------------------------------------- |
| `area`    | `mem.area(size!u64)!*u8`                     | pointer | -         | ✓               | Tekil mülkiyetli ham alan ayırır        |
| `zone`    | `mem.zone(size!u64)!*u8`                     | pointer | -         | ✓               | Paylaşımlı alan açar, multi-core için   |
| `resize`  | `mem.resize(h!*u8, new_size!u64)!bool`       | bool    | -         | ✓               | Masadaki alanın boyutunu değiştirir     |
| `copy`    | `mem.copy(dest!*u8, src!*u8, size!u64)!void` | void    | -         | ✓               | Zorunlu durumlarda derin kopyalama      |
| `move`    | `mem.move(dest!*u8, src!*u8, size!u64)!void` | void    | -         | ✓               | Ham blok taşır, adres referansını korur |
| `stat`    | `mem.stat(h!*u8)!MemStat`                    | MemStat | -         | ✓               | Alanın doluluk ve bilgi durumunu döner  |
| `peek`    | `mem.peek(ptr!*u8, type!Type)!Any`           | Any     | -         | ✓               | Ham adrese erişim                       |
| `poke`    | `mem.poke(ptr!*u8, val!Any)!void`            | void    | -         | ✓               | Ham adrese yazma                        |
| `offset`  | `mem.offset(ptr!*u8, n!i64)!*u8`             | pointer | -         | ✓               | Pointer aritmetiği                      |
| `zero`    | `mem.zero(h!*u8, size!u64)!void`             | void    | -         | ✓               | Alanı sıfırlar, güvenlik için           |
| `clone`   | `mem.clone(h!*u8, size!u64)!*u8`             | pointer | -         | ✓               | Derin kopya döner, explicit kullanım    |

---

## **B. Veri Akışı ve Senkronizasyon (Flow)**

| Fonksiyon  | İmza                              | Return | Opsiyonel | Bare-metal / OS | Açıklama                                              |
| ---------- | --------------------------------- | ------ | --------- | --------------- | ----------------------------------------------------- |
| `push`     | `flow.push(h!*u8, data!Any)!void` | void   | -         | ✓               | `<<` operatörünün fonksiyonel karşılığı               |
| `pull`     | `flow.pull(h!*u8)!Any`            | Any    | -         | ✓               | `>>` operatör fonksiyonu                              |
| `sync`     | `flow.sync(z!*u8)!void`           | void   | -         | ✓               | Zone üzerindeki bekleyen yazmaları işler              |
| `lock`     | `flow.lock(z!*u8)!bool`           | bool   | -         | ✓               | Kritik bölge kilidi                                   |
| `unlock`   | `flow.unlock(z!*u8)!void`         | void   | -         | ✓               | Kilidi açar                                           |
| `try_lock` | `flow.try_lock(z!*u8)!bool`       | bool   | -         | ✓               | Non-blocking kilit denemesi                           |
| `delay`    | `flow.delay(ms!u32)!void`         | void   | -         | ✓               | Asenkron bekleme (OS: sleep / Bare-metal: timer_wait) |

---

## **C. Donanım ve Sistem Arayüzü (Hardware & OS)**

| Fonksiyon  | İmza                                  | Return | Opsiyonel | Bare-metal / OS | Açıklama                     |
| ---------- | ------------------------------------- | ------ | --------- | --------------- | ---------------------------- |
| `io_read`  | `sys.io_read(port!u16)!u8`            | u8     | -         | ✓               | Donanım port okuma           |
| `io_write` | `sys.io_write(port!u16, val!u8)!void` | void   | -         | ✓               | Donanım port yazma           |
| `reg_r`    | `sys.reg_r(name!str)!u64`             | u64    | -         | ✓               | CPU register oku             |
| `reg_w`    | `sys.reg_w(name!str, val!u64)!void`   | void   | -         | ✓               | CPU register yaz             |
| `env`      | `sys.env(key!str)!str`                | str    | -         | OS              | OS ortam değişkeni           |
| `exit`     | `sys.exit(code!i32)!void`             | void   | -         | ✓               | Programı güvenli kapatır     |
| `call`     | `sys.call(id!i32, args!Any[])!Any`    | Any    | -         | ✓               | Doğrudan syscall / interrupt |
| `info`     | `sys.info(query_id!i32)!Any`          | Any    | -         | ✓               | Donanım / OS hakkında bilgi  |
| `yield`    | `sys.yield()!void`                    | void   | -         | ✓               | İşlemciyi gönüllü bırakır    |
| `sleep`    | `sys.sleep(ms!u32)!void`              | void   | -         | ✓               | OS-dependent sleep           |
| `reboot`   | `sys.reboot()!void`                   | void   | -         | ✓               | Sistemi yeniden başlatır     |

---

## **D. Kripto ve Güvenlik (Security)**

| Fonksiyon | İmza                                       | Return  | Opsiyonel | Bare-metal / OS | Açıklama                 |
| --------- | ------------------------------------------ | ------- | --------- | --------------- | ------------------------ |
| `hash`    | `crypto.hash(h!*u8, algorithm!str)!u64`    | u64     | -         | ✓               | Alanın hash’ini hesaplar |
| `verify`  | `crypto.verify(h!*u8, signature!*u8)!bool` | bool    | -         | ✓               | Dijital imza doğrulaması |
| `seal`    | `crypto.seal(h!*u8, size!u64)!void`        | void    | -         | ✓               | Alanı read-only mühürler |
| `zero`    | `crypto.zero(h!*u8, size!u64)!void`        | void    | -         | ✓               | Hassas veriyi sıfırlar   |
| `random`  | `crypto.random(bytes!u64)!*u8`             | pointer | -         | ✓               | Güvenli rastgele veri    |

---

## **E. Hata ve Tanı (Diagnostic & Logging)**

| Fonksiyon | İmza                              | Return   | Opsiyonel | Bare-metal / OS | Açıklama                           |
| --------- | --------------------------------- | -------- | --------- | --------------- | ---------------------------------- |
| `last`    | `err.last()!ErrObj`               | ErrObj   | -         | ✓               | Son hatayı döner                   |
| `trace`   | `err.trace()!TraceObj`            | TraceObj | -         | ✓               | Panic durumunda register/stack izi |
| `echo`    | `log.echo(msg!str)!void`          | void     | -         | ✓               | Standart çıktı                     |
| `warn`    | `log.warn(msg!str)!void`          | void     | -         | ✓               | Uyarı mesajı                       |
| `info`    | `log.info(msg!str)!void`          | void     | -         | ✓               | Bilgi mesajı                       |
| `panic`   | `panic(msg!str)!void`             | void     | -         | ✓               | Kritik hata ve durdurma            |
| `assert`  | `assert(cond!bool, msg!str)!void` | void     | -         | ✓               | Koşul doğrulama                    |

---

## **F. Zaman ve Hassas Ölçüm (Timing)**

| Fonksiyon      | İmza                            | Return | Opsiyonel | Bare-metal / OS | Açıklama                     |
| -------------- | ------------------------------- | ------ | --------- | --------------- | ---------------------------- |
| `ticks`        | `time.ticks()!u64`              | u64    | -         | ✓               | İşlemci çevrim sayısı        |
| `now`          | `time.now()!u64`                | u64    | -         | OS              | Epok zamanı veya uptime      |
| `clock`        | `time.clock()!f64`              | f64    | -         | ✓               | High-res CPU / uptime ölçümü |
| `sleep`        | `time.sleep(ms!u32)!void`       | void   | -         | ✓               | OS-dependent bekleme         |
| `delay_cycles` | `time.delay_cycles(n!u64)!void` | void   | -         | ✓               | Döngü tabanlı delay          |

---

## **G. Matematik ve Bit Manipülasyonu (Primitive Ops)**

| Fonksiyon | İmza                            | Return | Opsiyonel | Bare-metal / OS | Açıklama               |
| --------- | ------------------------------- | ------ | --------- | --------------- | ---------------------- |
| `abs`     | `math.abs(x!T)!T`               | T      | -         | ✓               | Mutlak değer           |
| `min`     | `math.min(a!T,b!T)!T`           | T      | -         | ✓               | Min                    |
| `max`     | `math.max(a!T,b!T)!T`           | T      | -         | ✓               | Max                    |
| `clamp`   | `math.clamp(x!T,min!T,max!T)!T` | T      | -         | ✓               | Sınırlandırma          |
| `rol`     | `math.rol(v!T,n!u8)!T`          | T      | -         | ✓               | Bit rotate left        |
| `ror`     | `math.ror(v!T,n!u8)!T`          | T      | -         | ✓               | Bit rotate right       |
| `bittest` | `math.bittest(v!T,pos!u8)!bool` | bool   | -         | ✓               | Belirli bit kontrol    |
| `bswap`   | `math.bswap(v!T)!T`             | T      | -         | ✓               | Byte swap (endianness) |

---

