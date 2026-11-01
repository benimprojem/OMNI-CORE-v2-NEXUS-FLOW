
# **1. Dosya Sistemi ve Pipeline Planı**

### **1.1 Bin Dizini (`/bin`)**

* Kullanıcı tarafından çalıştırılabilir araçlar:

  * **occ.exe** → Derleyici orchestrator (frontend)
  * **ocm.exe** → Paket & bağımlılık yöneticisi
  * **ocl.exe** → Linker (backend)
  * **ocvm.exe** → JIT / interpreter
  * **oce.exe** → OmniCore IDE / Editör

### **1.2 Lib Dizini (`/lib`)**

* Modüler derleyici yapısı, her bileşen ayrı DLL/so dosyası:

  * **occ_parser.dll** → Kaynak kodu → AST
  * **occ_analyzer.dll** → Tip, değişken, fonksiyon kontrolü
  * **occ_opt.dll** → Optimize edici motor
  * **occ_codegen.dll** → Hedef mimariye makine kodu üretimi
* Aynı yapı diğer araçlar (`ocl`, `ocm`, `oce`, `ocvm`) için de geçerli.

### **1.3 Stdlib (`/stdlib`)**

* **core, io, net, sys** modülleri
* OS bağımsız API’ler için standart kütüphane
* Macro’lar ve OS Bridge burada referans alınır

### **1.4 Targets (`/targets`)**

* Her platform için `.target` dosyaları
* IDE veya derleyici seçimleri için kullanılır
* Geliştirici `.target` oluşturabilir veya var olanı kopyalayıp modifiye edebilir

---

# **2. Dosya Tipleri ve Görevleri**

| Uzantı   | Amaç                           |
| -------- | ------------------------------ |
| `.nx`    | NexusFlow kaynak kodu          |
| `.ocd`   | Arayüz / Document              |
| `.ocs`   | Stil / Görsellik               |
| `.ocb`   | Derlenmiş ara veya nihai kod   |
| `.ocmap` | Debug için kaynak haritası     |
| `.oci`   | Interface dosyası              |
| `.ocf`   | Proje metadata / bağımlılıklar |
| `.nxr`   | Build / derleme direktifleri   |

---

# **3. Target Dosyası Yönetimi**

**3.1 Örnek `.target` yapısı**

```nexus
name        = "x86_64-win64"
arch        = "x86_64"
abi         = "win64"
stack_alignment = 16
scalar_regs = 16
vector_regs = 16
ptr_size    = 8
os          = "windows"
endian      = "little"
default_macros = ["SafeFlow","OS_Bridge"]
default_libs   = ["msvcrt.exf"]
```

**3.2 Kullanım**

* Macro veya derleme direktifleri (`!!=[target="x86_64-win64"]`) bu dosyadan bilgi alır
* IDE otomatik listeler ve kullanıcı seçimi yapabilir
* Kullanıcı kendi `.target` dosyasını yaratabilir (`ocm.exe create-target` komutu gibi)

---

# **4. Build Dosyası `.occ.nxr`**

* Tüm derleme adımlarını zincirleme yönetir
* Örnek:

```nxr
!!=[occ-nexus]
occ source_file.nx -olib -dll
&& source_file2.nx -olib -dll
&& main.nx -lsource_file -lsource_file2 -o file.exe -t x86_64-win64 --release
```

* `&&` → Sırayla derleme
* `-t` → Target seçimi
* `--release` → Release modu
* `-o` → Çıktı dosyası
* IDE veya CLI `occ -nexus` ile tüm zincir çalıştırılır

---

# **5. Önerilen Kullanıcı Akışı**

1. **Proje Oluşturma**

   * `ocm.exe create-project MyApp`
   * `.ocf` dosyası oluşturulur
2. **Target Seçimi**

   * IDE veya CLI ile mevcut target listesi gösterilir
   * Kullanıcı birini seçer veya yeni target oluşturur
3. **Kodlama**

   * `.nx`, `.ocd`, `.ocs` dosyaları hazırlanır
   * Macro ve OS Bridge kullanımı ile platform bağımsız kod yazılır
4. **Derleme**

   * `.nxr` dosyası hazırlanır
   * `occ -nexus` çalıştırılır
5. **Çıktı ve Test**

   * `.ocb` dosyaları, `.ocmap` ile debug edilir
   * IDE veya `ocvm.exe` ile hızlı test yapılır

---

💡 **Tavsiyeler:**

* IDE editöründe `.target` yönetimi ve macro seçimi GUI’den yapılmalı
* `ocm.exe` ile proje ve bağımlılık yönetimi otomatik olmalı
* `.nxr` dosyası IDE tarafından otomatik üretilebilir, kullanıcı sadece ekleme/çıkarma yapar

---


---

# **OmniCore IDE & Build Workflow**

**1️⃣ Proje Başlatma**

```
ocm.exe create-project MyApp
├─> proje_root/
│    ├─ source/       # .nx, .ocd, .ocs dosyaları
│    ├─ build/        # Derleme çıktıları
│    ├─ .ocf          # Proje bağımlılık ve metadata
│    └─ .nxr          # Derleme direktifleri
```

**2️⃣ Target Seçimi**

```
IDE / CLI
├─> targets/          # x86_64-win64, x86_64-lin, arm64-mac...
└─> user seçer / create-target
```

**3️⃣ Bağımlılık Yönetimi**

```
Proje bağımlılıkları (.ocf) -> ocm.exe
└─> dpslib/            # Tek seferlik yükleme, tekrar tekrar kullanılabilir
```

**4️⃣ Kodlama ve Makrolar**

```
source/
├─ main.nx
├─ io.nx
├─ math.nx
└─ macros.nx
```

* Macro sistemi ile OS bağımsız kod yazılır
* !!=[target="..."] ile platforma özel kod eklenir

**5️⃣ Derleme Akışı (.nxr)**

```
occ -nexus
├─> occ source_file.nx -olib -dll
├─> occ source_file2.nx -olib -dll
├─> occ main.nx -lsource_file -o app.exe -t x86_64-win64 --release
```

**6️⃣ Çıktı ve Test**

```
build/
├─ app.exe / .ocb
├─ .ocmap (Debug için)
└─ IDE veya ocvm.exe ile hızlı test
```

**7️⃣ Bağımlılık Paylaşımı**

```
dpslib/
├─ dep1/
├─ dep2/
└─ dep3/
```

* Diğer projelerde tekrar yüklemeye gerek yok
* Proje .ocf dosyası sadece referans tutar

---

💡 **Özelleştirilmiş Notlar:**

* Target sistemi `.target` dosyaları ile tam entegre
* IDE, target seçiminde macro ve OS Bridge’leri otomatik kullanır
* Build sırasında `.nxr` zincirini otomatik oluşturabilir
* Hedef OS fark etmeksizin aynı kod çalışabilir (Linux, Windows, macOS, ARM)
* Bağımlılık yönetimi merkezi (`dpslib`) ile tekrarlayan yüklemeler engellenir

---


---

# OmniCore / NexusFlow Uygulama ve Derleme Planı

## 1. Proje Başlatma ve Yapılandırma

* Proje klasörü oluştur:

  ```
  myproject/
      src/       # .nx kaynak kodları
      assets/    # Görseller, stil, dokümanlar
      build/     # Derleme çıktı dosyaları
      include/   # Extern başlık dosyaları
      targets/   # Mimariye özel .target dosyaları
  ```
* `.ocf` dosyası oluştur:

  * Proje adı, versiyon, yazar, dil, lisans bilgisi.
  * Include yolları (`lib/` vs.)
  * Bağımlılıklar dpslib klasöründen listelenir.

---

## 2. Target Sistemi

* `targets/` altında her platform ve mimari için `.target` dosyası:

  ```
  arch="x86_64"
  abi="win64"
  stack_alignment=16
  scalar_regs=16
  vector_regs=16
  endianness="little"
  max_threads=8
  simd_width=512
  ```
  
  
* Kullanıcı tanımlı hedefler oluşturabilir (`.target`).
* Editör ve makro sistemi hedef bazlı kodu otomatik seçer:

  ```
  !!=[target="linux"] { ... }
  !!=[target="win64"] { ... }
  !!=[target="bare-metal"] { ... }
  
  ```

---

## 3. Kodlama ve NexusFlow Özellikleri

* `.nx` dosyaları:

  * Handler tabanlı veri akışı `(h) <- ... ?-> ...`
  * Deterministik bellek `area` ve `zone`
  * Fonksiyonlar `f:main()!i32{ ... }`
  * Makrolar: `:macro! OS_Bridge($name, $win_fn, $lin_fn) { ... }`
* Grup ve nesne metodları:

  * `group Oyuncu { ... }`
  * `nt:struct` ve fonksiyonlar `f:(params)!Type{ ... }`

---

## 4. Bağımlılık Yönetimi

* Tüm bağımlılıklar `dpslib/` içerisine yüklenir.
* Ocm.exe veya `occ.nxr` ile proje derlemesi sırasında bağımlılıklar otomatik olarak yüklenir.
* Tekrar kullanım için her proje bağımlılığı tekrar yüklemez.

---

## 5. Derleme ve Linkleme

* `.nxr` dosyası örneği:

  ```
  !!=[occ-nexus]
  occ src/file1.nx -olib -dll
  && src/file2.nx -olib -dll
  && main.nx -lsrc/file1 -lsrc/file2 -o app.exe -t win64 --release
  ```
* Derleyici:

  * `occ.exe`: frontend, parser, analyzer, optimizer, codegen
  * `ocl.exe`: linker
  * `ocvm.exe`: JIT/deneme derleyici
* Output:

  * `.ocb` / `.exe` / `.dll` gibi platforma uygun çıktı
  * `.ocmap` debug haritaları

---

## 6. Makro Sistemi

* OS ve platform bağımsız kod yazımı

  ```
  !!=OS_Bridge!(IO, _write, write);
  IO.run("Hello World!");
  ```
* Hata ve akış yönetimi:

  ```
  !!=SafeFlow! {
      (h) <- load_resource("data.bin");
      process(h);
  }
  ```
* Tip bazlı makrolar:

  ```
  !!=CreateMath!(IntMath, i32);
  !!=CreateMath!(FloatMath, f32);
  v:sonuc1 = IntMath.add(10, 20);
  v:sonuc2 = FloatMath.add(10.5, 5.2);
  ```

---

## 7. Editör ve Kullanıcı Arayüzü

* OmniCoreEdit (oce.exe):

  * Syntax highlighting
  * Auto-completion
  * Target seçimi ve macro entegrasyonu
  * Target dosyalarını otomatik oluşturma
  * dpslib bağımlılık yöneticisi entegrasyonu

---

## 8. Hedefler ve Gelecek

* Windows, Linux, macOS, ARM, RISC-V, bare-metal
* Target dosyaları kullanıcı dostu, otomatik oluşturulabilir
* Aynı kod tabanı ile farklı platformlarda derleme
* Makro ve flow mantığı ile platform bağımsız iş akışı

---

---
1. Omni-IR resmi grammar yaz.
2. IR validator pass ekle.
3. Target DSL grammar yaz.
4. Target loader modülü yaz.
5. IR → Target compatibility matrix oluştur.


---

### Açıklama:

1. **Proje Klasörleri**

   * `src/`: NexusFlow kaynak kodları (.nx)
   * `assets/`: Görseller, stiller
   * `build/`: Derleme çıktıları (.ocb, .exe, .dll)
   * `include/`: Extern başlık dosyaları (.h/.oci)
   * `dpslib/`: Paylaşımlı bağımlılıklar (tek yükleme, tekrar kullanılabilir)
   * `targets/`: Mimari ve OS spesifik derleme hedefleri

2. **Derleme Akışı**

   * `occ.exe` → Parser → Analyzer → Optimizer → Codegen → `.ocb`
   * `ocl.exe` → Linker → Platforma özel executable
   * `ocvm.exe` → JIT / hızlı test
   * Makrolar ve target seçimi ile platform bağımsız akış (`!!=[target="win64"] { ... }`)

3. **Target Dosyaları**

   * Mimari ve ABI bilgilerini tutar: `arch`, `abi`, `stack_alignment`, `scalar_regs`, `vector_regs`, `endianness`, `simd_width`, `max_threads`
   * Editör ve makro sistemi bu dosyayı okuyarak uygun platform kodunu seçer.

4. **Bağımlılık Yönetimi**

   * `dpslib/` tek yükleme merkezi
   * `package/` üçüncü parti paketler
   * Her proje kendi `ocf` dosyası ile bağımlılıkları tanımlar, tekrar yüklemeye gerek yok

5. **Makro Sistemi**

   * OS ve tip bazlı otomatik grup oluşturma
   * SafeFlow, OS_Bridge gibi yapılar ile platform bağımsız kod akışı sağlanır

---


---

## 🛠️ Kullanım Örneği (Diyagram İçeriği)

```text
[ Project Root ]
├─ src/   — (NexusFlow kodları)
├─ lib/ — Proje kütüphaneleri
├─ include/ — (extern headers)
├─ build/ — (compiler outputs)
├─ tmp/ — Geçici dosyalar 
├─ occ.ocf — (project config)
└─ occ.nxr — (build script)

         ↓ Load Target
  [ Target Files ] <─── Load ─── IDE / occ

         ↓ Dependencies
  [ dpslib Modules ] <─── ocm / occ

         ↓ Build Pipeline
  occ.exe → Parser → Analyzer → Optimizer → CodeGen
         ↓
      ocl.exe (Link)
         ↓
   Outputs (.ocb / .exe / .dll) → ocvm.exe test
   
```


---

## 1️⃣ Context Level – Sistem ve Çevresi

Bu seviyede sistemin kullanıcıları ve dış dünya ile ilişkisini gösteriyoruz.

**Sistem:** OmniCore SDK / NexusFlow
**Kullanıcılar:**

* Uygulama geliştiriciler (app devs)
* Kernel / OS / Driver geliştiriciler
* Endüstriyel otomasyon ve embedded sistem mühendisleri
* IDE kullanıcıları (OCE Editör)

**Dış Sistemler:**

* İşletim sistemleri (Windows, Linux, macOS, BareMetal hedefleri)
* Extern kütüphaneler (.dll, .so, .obj, .a)
* Paket ve bağımlılık yöneticileri (omni-core paket deposu)

**Context Diagram Notları:**

* Kullanıcı → IDE (OCE) → Compiler (occ) → Targets
* Developer → SDK (dpslib + stdlib)
* Developer → Paket Yöneticisi (ocm) → Bağımlılıklar

---

## 2️⃣ Container Level – Ana Bileşenler

Bu seviye sistemin **ana konteynerlerini** ve işlevlerini gösteriyor.

| Container                        | İşlevi                                                   | Teknoloji / Not                                           |
| -------------------------------- | -------------------------------------------------------- | --------------------------------------------------------- |
| **OCE (Editör)**                 | Kaynak yazım, makro ve target-aware kod üretimi          | C++/C veya custom UI                                      |
| **occ.exe (Compiler Frontend)**  | NexusFlow kaynaklarını AST → IR → hedef kod              | Modular DLL yapısı (parser, analyzer, optimizer, codegen) |
| **ocm.exe (Paket Yöneticisi)**   | dpslib ve bağımlılık yönetimi                            | proje paylaşımı kolay                                     |
| **ocl.exe (Linker / Back-End)**  | .ocb dosyalarını executable/target formatına çevirir     | OS ve target-aware                                        |
| **ocvm.exe (JIT / Interpreter)** | Hızlı test ve debug                                      | RAM-temelli hızlı kod çalıştırma                          |
| **stdlib/**                      | Standart kütüphane fonksiyonları                         | core/io/net/sys modülleri                                 |
| **dpslib/**                      | Paylaşımlı bağımlılıklar, tekrar kullanılabilir modüller | Tek sefer yükle, tüm projelerde kullan                    |
| **targets/**                     | Mimariye özgü derleme ayarları                           | x86_64, arm64, riscv, bare-metal …                        |
| **logs/**                        | Performans ve hata kayıtları                             |                                                           |

**Container Diagram Notları:**

* occ.exe ↔ stdlib, dpslib, targets
* IDE ↔ occ.exe / ocvm.exe (code flow ve live preview)
* Paket yöneticisi ↔ dpslib, targets

---

## 3️⃣ Component Level – Modüller ve İşlevler

Bu seviyede **her konteynerin içindeki ana modüller** detaylandırılır.

**occ.exe (Compiler Frontend)**

* Parser DLL (`occ_parser.dll`) → AST üretir
* Analyzer DLL (`occ_analyzer.dll`) → tip, değişken, macro kontrolü
* Optimizer DLL (`occ_optimizer.dll`) → akış ve kod optimizasyonu
* Codegen DLL (`occ_codegen.dll`) → target kod üretir

**ocvm.exe (JIT / Interpreter)**

* Execution Engine → handler/memory flow
* Debug Interface → masalar, handlers, logs

**ocm.exe (Paket Yöneticisi)**

* Dependency Resolver → dpslib yükleme
* Project Metadata Parser → .ocf / .nxr

**Targets/**

* Hedef tanımlar (.target) → arch, abi, stack_alignment, register set…
* Target-aware code generation → `!!=[target = "linux"]` gibi macro destekli


---

## 4️⃣ Code Level – Detay ve Örnek Akış

Bu seviye, sistemin **gerçek akışını ve data flow’u** gösterir:

* `(h) <- file.open("a.txt") ?-> (e){ echo(e); }` → handler flow örneği
* Macro Expansion → target-aware kod üretimi
* Inline Assembly (`fastexec{asm{}}`) → register map ve variable substitution

**Code Diagram Önerisi:**

* Function / Macro → Handler Flow → Target Code
* Apply rules → Error Blocks → Success / Fallback

---





---

# **OmniCore NexusFlow – Target Dosyası Oluşturma ve Yönetimi Rehberi**

## 1. `.target` Dosyasının Amacı

* Her target, derleyicinin **hedef platformu** hakkında bilgilere sahip olduğu bir yapılandırma dosyasıdır.
* Bu bilgiler:

  * İşlemci mimarisi (`arch`)
  * ABI (`abi`)
  * Stack alignment (`stack_alignment`)
  * Register sayıları (`scalar_regs`, `vector_regs`)
  * Calling conventions ve OS spesifik ayarlar
* `.target` dosyaları derleyici tarafından okunur ve **platforma özgü kod üretimi** için referans olarak kullanılır.

---

## 2. Target Dosyası Örneği

**x86_64-win.target**

```ini
[meta]
version = 1.0
safe_alignment_check = true
requires_lto = true

[arch]
name = "x86_64"

[abi]
name = "win64"
stack_alignment = 16
endianness  = "little"
scalar_regs = 16
vector_regs = 16
calling_convention = "Microsoft x64"
stack_size  = 
heap_base   = 
cpu_core    = 4
max_threads = 8
simd_width  = 512
  
  
[os]
name = "windows"
dynamic_lib_ext = ".dll"
executable_ext = ".exe"
linker_flags = "-console"
enter_label = "WinMain"

[notes]
info = "Windows x64 target for OmniCore NexusFlow"
```
 
 
 
**arm64-lin.target**

```ini
[meta]
version = 1.0
safe_alignment_check = true
requires_lto = true

[arch]
name = "arm64"

[abi]
name = "linux64"
stack_alignment = 16
scalar_regs = 32
vector_regs = 32
calling_convention = "AAPCS64"

[os]
name = "linux"
dynamic_lib_ext = ".so"
executable_ext = ""
linker_flags = "-lc"
enter_label = "_start"

[notes]
info = "Linux ARM64 target for OmniCore NexusFlow"
```

---

## 3. Target Dosyası Konumlandırması

* Tüm target dosyaları **SDK dizinindeki `targets/`** klasöründe tutulur:

```
/omnicore-sdk/targets/
    ├── x86_64-win.target
    ├── x86_64-lin.target
    ├── arm64-win.target
    ├── arm64-lin.target
    └── riscv.target
```

* Derleyici (`occ.exe`) bu klasörü otomatik tarar ve hedef platform listesini oluşturur.

---

## 4. Yeni Target Oluşturma Adımları

1. **Hedef mimariyi seçin** (ör. RISC-V, ARM Cortex, özel FPGA).
2. `targets/` dizinine yeni bir `.target` dosyası oluşturun:
   Örnek: `mychip64.target`
3. Temel başlıkları doldurun:

   ```ini
   [meta]
   version = 1.0
   [arch]
   name = "mychip64"
  
   [abi]
   name = "bare-metal"
   stack_alignment = 8
   scalar_regs = 16
   vector_regs = 8
   calling_convention = "custom"
   ```
4. OS ve derleyiciye özgü parametreleri ekleyin:

   ```ini
   [os]
   name = "bare-metal"
   dynamic_lib_ext = ""
   executable_ext = ".ocb"
   linker_flags = ""
   ```
5. `notes` bölümüne açıklama ekleyin.
6. Dosyayı kaydedin ve derleyici ile test edin.

---

## 5. Target Kullanımı Derleme Sırasında

* Komut satırında target belirleme:

```bash
occ main.nx -o main.ocb -t mychip64 --release
```

* Hedef platform parametreleri otomatik olarak okunur ve derleyici:

  * Register kullanımını
  * Stack alignment’ını
  * OS’ye özel linker ve library ayarlarını
    uygular.

---

## 6. Gelişmiş İpuçları

* **Makro ve Extern Kullanımı**: Target’a göre conditional kod yazabilirsiniz:

```nexus
!!=[target = "windows"] { ... }
!!=[target = "linux"] { ... }
!!=[target = "bare-metal"] { ... }
```

* **Paylaşımlı Target**: Bir kez `.target` oluşturun ve tüm projelerinizde kullanın. Tekrar tekrar tanımlamaya gerek yok.
* **Editör desteği**: İleride planlanan OmniCore Editör (`oce.exe`) target dosyalarını form tabanlı otomatik oluşturacak ve doğrulayacak.

---

## 7. Özet

* `.target` dosyaları **platforma özgü yapılandırma** sağlar.
* `targets/` klasörü merkezi konumdur ve derleyici tüm target dosyalarını buradan tarar.
* Kullanıcı yeni mimari eklemek istediğinde **sadece yeni bir .target dosyası oluşturur**, SDK ve derleyici otomatik kullanır.
* Makro ve conditional kullanım ile platform bağımsız kod yazabilirsiniz.

---


