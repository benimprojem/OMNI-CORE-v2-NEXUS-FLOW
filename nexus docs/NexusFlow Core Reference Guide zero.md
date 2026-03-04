
# NexusFlow Core Reference Guide (Markdown)

## 1. Bellek ve İşaretçi Primitifleri (Memory & Pointers)

> Hiçbir import gerekmez, kernel veya bare-metal seviyede çalışır.

| Fonksiyon / Metot        | İmza (Nx)                     | Return  | Açıklama                        | Örnek                                  |
| ------------------------ | ----------------------------- | ------- | ------------------------------- | -------------------------------------- |
| `core.ptr.read_u8`       | `addr!u64`                    | `u8`    | Belirtilen adresten 1 byte okur | `v:val = core.ptr.read_u8(0xB8000);`   |
| `core.ptr.write_u8`      | `addr!u64, val!u8`            | void    | Belirtilen adrese 1 byte yazar  | `core.ptr.write_u8(0xB8000, 0x41);`    |
| `core.ptr.offset`        | `ptr!*u8, n!i64`              | pointer | Pointer aritmetiği              | `v:new_ptr = core.ptr.offset(ptr, 5);` |
| `core.mem.size_of([T])`  | `[T]`                         | u64     | Tipin bellekteki boyutu         | `v:size = core.mem.size_of([i32]);`    |
| `core.mem.align_of([T])` | `[T]`                         | u64     | Tipin hizalama sınırı           | `v:align = core.mem.align_of([i64]);`  |
| `core.mem.copy_raw`      | `src!*u8, dst!*u8, n!u64`     | void    | Denetimsiz ham bellek kopyalama | `core.mem.copy_raw(src, dst, 64);`     |
| `mem.area`               | `size!u64`                    | pointer | Tekil mülkiyetli alan           | `v:ptr = mem.area(1024);`              |
| `mem.zone`               | `size!u64`                    | pointer | Paylaşımlı alan                 | `v:z = mem.zone(2048);`                |
| `mem.resize`             | `h!*u8, new_size!u64`         | bool    | Alan boyutunu değiştirir        | `mem.resize(ptr, 4096);`               |
| `mem.move`               | `dest!*u8, src!*u8, size!u64` | void    | Ham blok taşıma                 | `mem.move(dest, src, 128);`            |
| `mem.peek`               | `ptr!*u8, type!Type`          | Any     | Ham adrese okuma                | `v:val = mem.peek(addr, i32);`         |
| `mem.poke`               | `ptr!*u8, val!Any`            | void    | Ham adrese yazma                | `mem.poke(addr, 1234);`                |
| `mem.zero`               | `h!*u8, size!u64`             | void    | Alanı sıfırlar                  | `mem.zero(ptr, 256);`                  |

---

## 2. Donanım ve CPU Komutları

| Fonksiyon / Metot               | İmza                 | Return | Açıklama                      | Örnek                                 |
| ------------------------------- | -------------------- | ------ | ----------------------------- | ------------------------------------- |
| `core.cpu.out_b`                | `port!u16, val!u8`   | void   | Donanım portuna byte gönderir | `core.cpu.out_b(0x60, 0x01);`         |
| `core.cpu.in_b`                 | `port!u16`           | u8     | Porttan byte okur             | `v:key = core.cpu.in_b(0x60);`        |
| `core.cpu.halt`                 | -                    | void   | İşlemciyi durdurur            | `core.cpu.halt();`                    |
| `core.cpu.nop`                  | -                    | void   | 1 çevrim boşta bekler         | `core.cpu.nop();`                     |
| `core.cpu.cli` / `core.cpu.sti` | -                    | void   | Interrupt’ları kapat / aç     | `core.cpu.cli(); ... core.cpu.sti();` |
| `core.cpu.load_gdt`             | `ptr!*u8`            | void   | GDT yükler                    | `core.cpu.load_gdt(gdt_ptr);`         |
| `core.cpu.cpuid`                | `leaf!u32`           | u32[]  | İşlemci özellikleri           | `v:info = core.cpu.cpuid(0x1);`       |
| `sys.io_read`                   | `port!u16`           | u8     | OS bağımlı I/O okuma          | `v:val = sys.io_read(0x3F8);`         |
| `sys.io_write`                  | `port!u16, val!u8`   | void   | OS bağımlı I/O yazma          | `sys.io_write(0x3F8, 0x41);`          |
| `sys.reg_r`                     | `name!str`           | u64    | CPU register oku              | `v:rax = sys.reg_r("rax");`           |
| `sys.reg_w`                     | `name!str, val!u64`  | void   | CPU register yaz              | `sys.reg_w("rbx", 1234);`             |
| `sys.call`                      | `id!i32, args!Any[]` | Any    | Syscall / Interrupt           | `sys.call(0x80, [1,2,3]);`            |
| `sys.info`                      | `query_id!i32`       | Any    | OS / donanım bilgisi          | `v:cores = sys.info(1);`              |
| `sys.yield`                     | -                    | void   | İşlemciyi gönüllü bırak       | `sys.yield();`                        |

---

## 3. Akış Kontrolü ve Yığın Yönetimi

| Fonksiyon / Metot    | İmza              | Açıklama                 | Örnek                           |
| -------------------- | ----------------- | ------------------------ | ------------------------------- |
| `core.stack.set_ptr` | `addr!u64`        | Yığın pointer ayarlama   | `core.stack.set_ptr(0x8000);`   |
| `core.stack.get_ptr` | -                 | Mevcut yığın adresi      | `v:sp = core.stack.get_ptr();`  |
| `core.exec.jump`     | `addr!u64`        | Mutlak adres atlama      | `core.exec.jump(0x1000);`       |
| `core.exec.call_raw` | `addr!u64`        | Adres fonksiyon çağrısı  | `core.exec.call_raw(func_ptr);` |
| `flow.push`          | `h!*u8, data!Any` | Masaya veri iter         | `flow.push(zone, 42);`          |
| `flow.pull`          | `h!*u8`           | Masadan veri alır        | `v:data = flow.pull(zone);`     |
| `flow.sync`          | `z!*u8`           | Bekleyen yazmaları işler | `flow.sync(zone);`              |
| `flow.lock`          | `z!*u8`           | Kritik bölge kilidi      | `if(flow.lock(zone)){...}`      |
| `flow.unlock`        | `z!*u8`           | Kilidi açar              | `flow.unlock(zone);`            |
| `flow.delay`         | `ms!u32`          | Asenkron bekleme         | `flow.delay(100);`              |

---

## 4. Tip ve Meta

| Fonksiyon                | İmza           | Açıklama                   | Örnek                               |
| ------------------------ | -------------- | -------------------------- | ----------------------------------- |
| `core.type.cast`         | `(T, val!Any)` | Veriyi T tipine dönüştürür | `v:x = core.type.cast(i32, 42.7);`  |
| `core.type.is_primitive` | `val!Any`      | Temel tip kontrolü         | `v:ok = core.type.is_primitive(5);` |
| `typeof`                 | `val!Any`      | Çalışma zamanı tip         | `v:t = typeof(3.14);`               |
| `sizeof`                 | `val!Any`      | Bellek boyutu              | `v:size = sizeof(msg);`             |
| `len`                    | `val!Any`      | Koleksiyon uzunluğu        | `v:l = len(arr);`                   |

---

## 5. Kripto ve Güvenlik

| Fonksiyon       | İmza                   | Açıklama | Örnek                                  |                              |
| --------------- | ---------------------- | -------- | -------------------------------------- | ---------------------------- |
| `crypto.hash`   | `h!*u8, algorithm!str` | Hash     | `v:hash = crypto.hash(ptr, "sha256");` |                              |
| `crypto.verify` | `h!*u8, signature!*u8` | bool     | Dijital imza doğrulama                 | `crypto.verify(ptr, sig);`   |
| `crypto.seal`   | `h!*u8, size!u64`      | void     | Read-only mühür                        | `crypto.seal(ptr, 128);`     |
| `crypto.zero`   | `h!*u8, size!u64`      | void     | Hassas veriyi sıfırlar                 | `crypto.zero(ptr, 64);`      |
| `crypto.random` | `bytes!u64`            | *u8      | Rastgele veri üretir                   | `v:buf = crypto.random(16);` |

---

## 6. Hata ve Tanı

| Fonksiyon   | İmza                 | Açıklama                   | Örnek                           |
| ----------- | -------------------- | -------------------------- | ------------------------------- |
| `err.last`  | -                    | Son hatayı döner           | `v:e = err.last();`             |
| `err.trace` | -                    | Panic stack / register izi | `err.trace();`                  |
| `log.echo`  | `msg!str`            | Standart çıktı             | `log.echo("Hello");`            |
| `log.warn`  | `msg!str`            | Uyarı                      | `log.warn("Memory low");`       |
| `log.info`  | `msg!str`            | Bilgi mesajı               | `log.info("Init complete");`    |
| `panic`     | `msg!str`            | Kritik hata ve durdurma    | `panic("Fatal error!");`        |
| `assert`    | `cond!bool, msg!str` | Koşul doğrulama            | `assert(x>0, "x must be > 0");` |

---

## 7. Zaman ve Hassas Ölçüm

| Fonksiyon           | İmza     | Açıklama                     | Örnek                      |
| ------------------- | -------- | ---------------------------- | -------------------------- |
| `time.ticks`        | -        | İşlemci çevrim sayısı        | `v:c = time.ticks();`      |
| `time.now`          | -        | Epok / uptime                | `v:t = time.now();`        |
| `time.clock`        | -        | High-res CPU / uptime ölçümü | `v:f = time.clock();`      |
| `time.sleep`        | `ms!u32` | OS / timer bekleme           | `time.sleep(500);`         |
| `time.delay_cycles` | `n!u64`  | Döngü tabanlı delay          | `time.delay_cycles(1000);` |

---

## 8. Örnek Kullanım: Bare-Metal Hello World (VGA)

```nx
f:_start() {
    v:vga!u64 = 0xB8000;
    v:msg = "Hello NexusFlow!";
    v:i!u64 = 0;

    print_loop:> {
        (i < msg.len()) =?> {
            core.ptr.write_u8(vga + i*2, msg.at(i));
            core.ptr.write_u8(vga + i*2 + 1, 0x0F); # Beyaz
            i = i + 1
            (print_loop)?>
        }
    }

    (loop) {
        core.cpu.halt()
    }
}
```

---
