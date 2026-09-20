
## Nexus Flow Core: Güvenlik / Risk Checklist

| Alan / Operatör                  | Risk / Tehlike                                 | Lifetime / Sahiplik                                                | Not / Güvenlik Önlemi                                                                                                      |
| -------------------------------- | ---------------------------------------------- | ------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------- |
| **(h) Handler (Masa)**           | Başka bloktan izinsiz erişim, dangling pointer | Sadece tanımlandığı blokta geçerli; blok sonunda `}?;` ile silinir | `_>` ile başka bloğa taşınabilir, taşındığında eski blokta artık yok                                                       |
| `<-` Capture                     | Yanlış veri kilitleme veya overwrite           | `(h)` scope'u ile sınırlı                                          | Eğer masa doluysa derleyici hata verir                                                                                     |
| `_>` Relocate                    | Verinin birden fazla blokta mutable erişimi    | Orijinal blokta silinir, yeni blokta yaşam başlar                  | Sahiplik transferi gerçekleşir, eski blokta artık yok                                                                      |
| `?->` Catch                      | Hatalı yönlendirme / kayıp error               | Hata blokları ömürlerine göre yönetilir                            | Hata bloğu sonrasında masa ömrü kontrol edilir                                                                             |
| `?=>` Fallback                   | Yanlış null handling                           | Blok scope                                                         | Alternatif veri ile güvenli akış sağlanır                                                                                  |
| `->` Success                     | Yanlış veri akışı                              | Blok scope                                                         | Veriyi bir sonraki işleme taşır                                                                                            |
| `!->` Ignore                     | Hataların gizlenmesi                           | Blok scope                                                         | Hata olsa bile akış devam eder; dikkat, kritik hata gözden kaçabilir                                                       |
| `<<` Zone Write                  | Paylaşımlı bellek overwrite                    | Zone bazlı                                                         | Atomik yazma, race condition riskini minimize eder                                                                         |
| `>>` Flow Feed                   | Yanlış veri okuma                              | Zone bazlı                                                         | Atomik okuma, senkronizasyon yapılmalı                                                                                     |
| `(e){}` Error Block              | Hata kaybı / yanlış catch                      | Hata bloğu scope'u                                                 | Tüm saptırılan hatalar buraya yönlendirilir                                                                                |
| `?;` Halt                        | Verilerin serbest bırakılmaması                | Blok sonunda masalar imha edilir                                   | Güvenli sonlandırma                                                                                                        |
| `!!` Panic                       | Kritik durdurma                                | N/A                                                                | Programı durdurur, hata raporu verir                                                                                       |
| `@` Intent                       | Yanlış derleyici niyeti                        | N/A                                                                | Derleyiciye optimize edilebilir bilgi verir                                                                                |
| `!!=` Directive                  | Yanlış hedef / talimat                         | N/A                                                                | Derleyici ve target sistemi için talimat verir                                                                             |
| **fastexec / asm**               | Register overwrite, spill, UB, yanlış jump     | Optimizer belirler                                                 | Clobber listesi codegen sorumluluğu, kullanıcı sadece öneri verir; optimizer register mapping ve lifetime kontrolünü yapar |
| `core.ptr.read_u8/write_u8`      | Yanlış adres okuma / yazma                     | Kullanıcı belirler                                                 | Donanım register veya VGA yazma için güvenli offset kullan                                                                 |
| `core.mem.copy_raw`              | Overlap, overflow, unsafe copy                 | Kullanıcı belirler                                                 | Kernel seviyesinde denetimsiz → dikkat                                                                                     |
| `core.stack.set_ptr/get_ptr`     | Yanlış yığın adresi → crash                    | Kernel control                                                     | Bootloader ve kernel geçişlerinde doğru ayar zorunlu                                                                       |
| `core.exec.jump/call_raw`        | Yanlış adres / UB                              | Kullanıcı belirler                                                 | Jump hedefi dominance region içinde olmalı                                                                                 |
| `core.cpu.*`                     | Yanlış interrupt, port, HLT, CLI/ STI          | CPU                                                                | Yarış durumu / güç tasarrufu                                                                                               |
| `mem.area/zone/free/resize`      | Double free, leak                              | Ownership kuralları                                                | Masalar ve zone lar scope bazlı kontrol edilir                                                                             |
| `flow.push/pull/sync/lock/delay` | Race condition, stale data                     | Masalar ve zone                                                    | Senkronizasyon ve memory barrier ile güvenli                                                                               |
| `crypto.hash/verify/seal`        | Yanlış verify, hash collision                  | Masalar / zone                                                     | Güvenli hash algoritmaları kullanılmalı                                                                                    |
| `err.last/trace`                 | Debug info sızması                             | Scope                                                              | Hata takibi için safe, üretimde log filtrelenmeli                                                                          |
| `time.ticks/now`                 | Yanlış zaman ölçümü                            | N/A                                                                | Bare-metal ve OS farklılıklarına dikkat                                                                                    |

---

### Notlar:

1. **Masa `(h)` lifetimes:** Her zaman blok sonunda otomatik imha. `_>` ile transfer durumunda eski blokta yok.
2. **ASM blokları:** Kullanıcı yazdı diye kesin uygulanmaz; optimizer son kararı verir. Register mapping ve spill optimizer sorumluluğunda.
3. **Clobber listesi:** Codegen kontrolü; kullanıcı sadece öneri verir. Hatalı clobber listesi UB oluşturmaz, optimizer düzeltir.
4. **Paylaşımlı bellek ve zone:** Atomik erişim ve senkronizasyon, race condition riskini minimize eder.
5. **Kernel/CPU fonksiyonları:** Unsafe, dikkatli kullanılmalı. Bare-metal modda OS kütüphane güvenliği yok.

---




**Nexus Flow Core Reference & Security Guide**

* `(h)` / masa lifetimes ve transferi
* `fastexec` / `asm` davranışı
* Core memory, stack, CPU ve flow fonksiyonları
* Güvenlik ve risk notları

*hepsi tek tabloda ve örneklerle gösteriliyor.

---

# Nexus Flow Core Reference & Security Guide

## 1. Masa ve Handler `(h)` Lifetimes

| Özellik              | Tanım                                   | Risk                             | Lifetime / Sahiplik                               | Not                                                                  |
| -------------------- | --------------------------------------- | -------------------------------- | ------------------------------------------------- | -------------------------------------------------------------------- |
| `(h)` Handler / Masa | Blok içinde geçerli, geçici veri deposu | Yanlış erişim / dangling pointer | Tanımlandığı blok sonunda `}?;` ile silinir       | `_>` ile başka bloğa taşınabilir, taşındığında eski blokta artık yok |
| `_>` Relocate        | Veriyi başka bloğa gönderme             | Çoklu mutable erişim, race       | Orijinal blokta silinir, yeni blokta yaşam başlar | Sahiplik transferi gerçekleşir                                       |
| `?->` Catch          | Hatalı yönlendirme                      | Hata kaybı                       | Hata blokları scope içinde                        | Hata sonrası kontrol sağlanır                                        |
| `?=>` Fallback       | Null / eksik veri                       | Yanlış fallback                  | Blok scope                                        | Alternatif veri ile güvenli akış sağlanır                            |
| `->` Success         | Veri akışı                              | Yanlış hedef                     | Blok scope                                        | Veriyi bir sonraki işleme taşır                                      |
| `!->` Ignore         | Hataları yoksay                         | Kritik hata gözden kaçabilir     | Blok scope                                        | Akış devam eder, dikkat                                              |

---

## 2. Fastexec / Inline Assembly

| Özellik                                | Risk / Tehlike                      | Lifetime           | Not                                                               |
| -------------------------------------- | ----------------------------------- | ------------------ | ----------------------------------------------------------------- |
| `fastexec { asm: ... }`                | Register overwrite, yanlış jump, UB | Optimizer belirler | Clobber listesi codegen sorumluluğu, kullanıcı sadece öneri verir |
| `%degisken:reg`                        | Yanlış register mapping             | Optimizer kontrolü | Register doğru eşlenmezse optimizer düzeltir                      |
| `asmcall("LABEL")` / `asmjmp("LABEL")` | Jump target hatası                  | N/A                | Label dominance region kontrolü optimizer sorumluluğu             |
| `# clobber: rax, rbx`                  | Yanlış clobber                      | N/A                | Codegen tarafından güvenli şekilde uygulanır                      |

---

## 3. Core Memory & Pointer Primitives

| Fonksiyon                        | Tanım                       | Risk / Not                                                 |
| -------------------------------- | --------------------------- | ---------------------------------------------------------- |
| `core.ptr.read_u8(addr)`         | Adresten 1 byte okur        | Donanım register veya VGA yazma için güvenli offset kullan |
| `core.ptr.write_u8(addr, val)`   | Adrese 1 byte yazar         | Yanlış adres → crash                                       |
| `core.ptr.offset(ptr, n)`        | Pointer kaydırma            | Array overflow risk                                        |
| `core.mem.size_of([T])`          | Tip boyutu                  | Manuel tahsis için gerekli                                 |
| `core.mem.align_of([T])`         | Tip hizalaması              | Struct padding için kritik                                 |
| `core.mem.copy_raw(src, dst, n)` | Denetimsiz bellek kopyalama | Overlap/overflow risk, kernel seviyesinde unsafe           |

---

## 4. Stack & Execution Control

| Fonksiyon                  | Risk / Tehlike       | Not                                  |
| -------------------------- | -------------------- | ------------------------------------ |
| `core.stack.set_ptr(addr)` | Yanlış yığın → crash | Bootloader → Kernel geçişi           |
| `core.stack.get_ptr()`     | Stack trace          | Debug amaçlı                         |
| `core.exec.jump(addr)`     | Yanlış jump → UB     | Hedef dominance region kontrolü şart |
| `core.exec.call_raw(addr)` | Yanlış çağrı         | Dinamik modüller                     |

---

## 5. CPU Intrinsics & Low-Level

| Fonksiyon                   | Risk / Tehlike       | Not                               |
| --------------------------- | -------------------- | --------------------------------- |
| `core.cpu.out_b(port, val)` | Donanım yazma hatası | Register ve port güvenliği önemli |
| `core.cpu.in_b(port)`       | Donanım okuma hatası |                                   |
| `core.cpu.halt()`           | CPU durur            | Boot loop veya güç tasarrufu      |
| `core.cpu.nop()`            | Zamanlama hatası     | Hassas gecikme                    |
| `core.cpu.cli()` / `sti()`  | Yarış durumları      | Interrupt yönetimi                |
| `core.cpu.load_gdt(ptr)`    | Segment hatası → UB  | GDT doğru yapılandırılmalı        |
| `core.cpu.cpuid(leaf)`      | Yanlış bilgi         | Hardware tanıma için safe         |

---

## 6. Flow, Zone & Piping

| Fonksiyon            | Risk / Tehlike   | Not                                  |
| -------------------- | ---------------- | ------------------------------------ |
| `flow.push(h, data)` | Masaya overflow  | Atomik yazma, race condition risk    |
| `flow.pull(h)`       | Yanlış okuma     | Atomik okuma, senkronizasyon gerekli |
| `flow.sync(z)`       | Stale data       | Memory barrier kullan                |
| `flow.lock(z)`       | Deadlock         | Kritik bölge manuel yönetimi         |
| `flow.delay(ms)`     | Yanlış zamanlama | OS: sleep / Bare-metal: timer_wait   |

---

## 7. Crypto & Security

| Fonksiyon               | Risk / Tehlike          | Not                            |
| ----------------------- | ----------------------- | ------------------------------ |
| `crypto.hash(h, algo)`  | Collision / yanlış veri | Safe hash algoritmaları kullan |
| `crypto.verify(h, sig)` | Yanlış verify           | Digital signature kontrolü     |
| `crypto.seal(h)`        | Read-only enforcement   | Değiştirilemezlik mührü        |

---

## 8. Error & Logging

| Fonksiyon       | Risk / Tehlike     | Not                         |
| --------------- | ------------------ | --------------------------- |
| `err.last()`    | Hata kaybı         | Son hatayı döner            |
| `err.trace()`   | Debug info sızması | Stack/register trace üretir |
| `log.echo(msg)` | Output güvenliği   | OS: stdout                  |

---

## 9. Time & Measurement

| Fonksiyon      | Risk / Tehlike       | Not                                |
| -------------- | -------------------- | ---------------------------------- |
| `time.ticks()` | Yanlış çevrim sayısı | RDTSC benzeri                      |
| `time.now()`   | Yanlış epok          | Sistem çalışma süresi veya OS time |

---

## 10. Önemli Güvenlik Notları

1. `(h)` masaları **her zaman** blok sonunda otomatik temizlenir. `_>` ile transfer edildiğinde eski blokta yok.
2. `fastexec` ve `asm` davranışı optimizer tarafından kontrol edilir. Kullanıcı sadece register mapping ve clobber önerisi verir.
3. Clobber listesi ve register spill codegen sorumluluğundadır; kullanıcı yanlış önerirse optimizer düzeltir.
4. Paylaşımlı bellek (`zone`) erişimleri atomik olmalı, race condition riski var.
5. Kernel ve CPU fonksiyonları unsafe; bare-metal modda OS kütüphane güvenliği yok.

---
