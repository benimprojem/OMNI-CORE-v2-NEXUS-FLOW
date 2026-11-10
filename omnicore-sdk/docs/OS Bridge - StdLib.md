 **OS Bridge / StdLib**

---

### 1. Windows

* **Temel I/O** → Windows API (`WriteFile`, `ReadFile`, `CreateFile`, `GetStdHandle`) veya `msvcrt` (`printf`, `fwrite`)
* **Memory / Heap** → `HeapAlloc`, `VirtualAlloc` veya `malloc`/`free`
* **Thread / Sync** → `CreateThread`, `WaitForSingleObject`, `CRITICAL_SECTION`
* **Time** → `GetSystemTimeAsFileTime`, `QueryPerformanceCounter`
* **Exit / Panic** → `ExitProcess`

> C kütüphanesini (`msvcrt.exf`) köprü olarak kullanmak daha hızlı ve cross-compiler dostu. Windows API’ye direk çağrı kritik low-level durumlar için (driver / bare-metal-like işlemler).

---

### 2. Linux / Unix

* **Temel I/O** → POSIX `read`, `write`, `open`, `close`
* **Memory / Heap** → `malloc`, `free`, `mmap`
* **Thread / Sync** → `pthread_create`, `pthread_mutex_*`, `pthread_cond_*`
* **Time** → `clock_gettime`, `gettimeofday`
* **Exit / Panic** → `_exit`

> POSIX fonksiyon bildirimleri ve OS köprü uygulaması `osmodule.nxf` içinde tanımlanır; harici nesne/kütüphane bağlantı tanımları gerektiğinde ayrı `.exf` dosyalarıyla sağlanır.

---

### 3. MacOS

* Genelde Linux ile benzer POSIX API, ayrıca CoreFoundation veya darwin libc kullanılabilir.
* `malloc`, `free`, `pthread_*`, `write`, `read` yeterli.

---

### 4. Ortak Mantık

* **coremodule** → Yalnızca OS bağımsız CPU / memory / handler / primitive operations.
* **osmodule** → Her OS’ye özgü fonksiyonları kapsar; coremodule'den bağımsızdır.
* **Cross-Platform Abstraction** → Kullanıcı `SysIO.output()`, `mem.area()`, `thread.create()` gibi çağırır; alt katmanda `osmodule.nxf` içindeki OS-specific wrapper'lar, gerektiğinde `.exf` harici bağlantıları kullanır.
* **Dead-Code Optimization** → Kullanılmayan OS-specific fonksiyonlar linker tarafından çıkarılır; EXE minimal kalır.

---

💡 **Özet:**

* **Windows:** `msvcrt` ve gerekli low-level Windows API çağrıları
* **Linux / Mac:** POSIX + libc
* **coremodule:** tamamen bağımsızdır, OS’ye dokunmaz
* **osmodule:** OS-specific modüller, coremodule üstünde çalışır ve kullanıcıya unified API sunar



---

## **osmodule.nxf için eski API taslak notları (normatif değildir)**

| Fonksiyon / Metot                                    | Açıklama                             | OS Wrapper / Notes                                |
| ---------------------------------------------------- | ------------------------------------ | ------------------------------------------------- |
| `echo(msg!str)`                                      | Konsola veya stdout’a yazı basar     | Windows → `_write` / Linux → `write`              |
| `area(size!usize)!*void`                             | Ham bellek bloğu tahsis eder         | Windows/Linux → `malloc`                          |
| `zone(size!usize)!*void`                             | Paylaşımlı bellek bloğu tahsis eder  | OS bağımsız; coremodule sorumluluğundadır                  |
| `free_mem(ptr!*void)`                                | Ham bellek bloğunu serbest bırakır   | Windows/Linux → `free`                            |
| `sleep(ms!u32)`                                      | Belirli süre bekleme                 | Windows → `Sleep(ms)` / Linux → `usleep(ms*1000)` |
| `time_now()!u64`                                     | Sistem zamanı veya epok zamanı döner | OS wrapper, osmodule standardize eder           |
| `thread.spawn(f!callback, arg!*void)!*thread_handle` | Yeni thread başlatır                 | **threading sistemi**, OS bağımsız    |
| `thread.join(handle!*thread_handle)`                 | Thread’in bitmesini bekler           | Hafif threading sistemi                           |
| `exit(code!i32)!void`                                | Programı sonlandırır                 | Windows → `_exit` / Linux → `exit`                |

---

### **Özellikler ve Notlar**

1. **OS bağımlılığı minimumda:**

   * Yalnızca `echo`, `area`, `free_mem`, `sleep`, `exit` gibi fonksiyonlar OS wrapper içeriyor.
   * Diğer tüm işlevler (thread, zone, Core pointer, asm, stack) tamamen OS bağımsız.

2. **Threading Sistemi:**

   * Hafif ve kendi runtime ile çalışıyor.
   * OS’den bağımsız, hem Windows hem Linux hem de bare-metal platformlarda aynı API.
   * Kullanımı kolay: `thread.spawn(f, arg)` → `thread.join(handle)`

3. **Memory Management:**

   * `area` → tekil ownership
   * `zone` → paylaşımlı alan, cache sync ve lock mekanizmaları osmodule tarafında yönetiliyor

4. **Dead Code Optimization:**

   * Kullanılmayan OS wrapper’ları EXE’ye dahil edilmiyor.

---

Kanonik kaynak dosyaları `omnicore-sdk/occ/libs/osmodule.nxf` ve `omnicore-sdk/occ/libs/coremodule.nxf` konumundadır. Her ikisi şimdilik başlangıç dosyasıdır; bu belgedeki eski API tabloları uygulanmış/kararlaştırılmış arayüz sayılmaz.

---



## Yeni modül adları

- Eski `CoreModule.exf` dosyası `osmodule.nxf` olarak yeniden adlandırılmıştır. Dosya/standart I/O ve işletim sistemi servisleri bu modüle aittir.
- Eski `Core` modülünün yeni adı `coremodule` olacaktır; bu modül OS bağımsız fonksiyonlardan oluşmalıdır ve `osmodule` bağımlılığı taşımamalıdır.
- Yalnızca `osmodule` varsayılan otomatik yükleme kapsamındadır; `no: osmodule;` bu yüklemeyi kapatır. `no:` genel modül kapatma bildirimi değildir.
- Mevcut `osmodule.nxf` içindeki grupların bir kısmı (ör. Array/String/Math/Crypto) OS bağımsız adaylar içerir. Bunların `coremodule`'e taşınması, bağımlılık ve Group `default` davranışları incelenerek ayrı bir değişiklik grubunda yapılacaktır; bu belge mevcut karışık kaynak içeriğini tamamlanmış bir ayrıştırma olarak göstermez.
