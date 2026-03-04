not: editör için Target Wizard + otomatik doğrulama sistemi

> NexusFlow akış sistemi **sadece sugar katmanı** olacak.
> Temel dil (core) **endüstriyel, minimal ve SSA-friendly** olacak.

Bu yaklaşımın faydaları:

1. **Compiler kolaylığı:** Flow operatörleri desugar edilerek core dil AST’ye çevrilecek. IR çok karmaşık olmayacak.
2. **Target bağımsızlığı:** Target ne olursa olsun IR tek formatta olacak, backend sadece optimize edip kod üretecek.
3. **Endüstriyel uyumluluk:** Kernel, driver, embedded, GPU, ARM, x86… hepsi tek core üzerine yazılabilir.
4. **Sade syntax:** `if`, `else`, `loop`, `struct`, `enum`, `union`, pointer, inline asm gibi temel yapılar net.
5. **Flow sugar:** `<-`, `->`, `?->` vb. operatörler frontend aşamasında core’a dönüştürülecek, üretkenliği yüksek ama compiler dostu.

---

## Önerilen Minimal Operatör / Keyword Listesi

### Core Language

```text
if / else
loop / break / continue
match / case / default (opsiyonel)
return / panic
struct / !struct
enum
union
pointer / reference / move / unsafe
extern fn / calling convention
@target(...) / @directive
```

### Flow Sugar

```text
h <- fn()        # move/capture
h -> fn()        # pass flow
h ?-> error_fn() # error redirect
h ?=> alt_h      # fallback
_h _> target_h   # relocate
<< / >>          # atomic write / feed (optional)
retry(n, ms)     # retry sugar
```

* Karmaşık operatörler `...>` gibi expand artık keyword veya inline function ile yapılır.
* Etiket tabanlı jump artık sadece `goto label` ve `label:` olarak core’da yer alır.

---

## Next Step: Derleyici Planı

1. **Lexer & Parser:**

   * Core syntax için BNF
   * Flow operatörleri lex ve desugar aşamasında dönüştürülür

2. **Desugar Katmanı:**

   * Flow → Core AST
   * Retry, Move, Pass, Error redirect gibi sugarlar normal if/else ve function çağrılarına dönüştürülür

3. **IR Generation:**

   * SSA-style intermediate representation
   * Target-agnostic (generic registers, memory ops)

4. **Optimizer:**

   * Constant folding, loop unrolling, inline, zero-cost move
   * TargetInfo struct okunur, spesifik optimizasyon uygulanır

5. **Backend:**

   * IR → target machine code
   * Assembly, ARM, x86, GPU vb.
   * `@target` direktifiyle branch

6. **Standard Library:**

   * Handlers, async sugar, rules system, file/db/network abstractions

---

NexusFlow’un **core dili** ve **flow sugar desugar planı** için referans. 
Bu, hem bir **resmi BNF** hem de **derleyici aşamalarıyla nasıl desugar olacağı** planını içeriyor.

---

# 1. NexusFlow Core Language – BNF

```bnf
<program>       ::= { <top_level_decl> }

<top_level_decl>::= <function_decl> | <struct_decl> | <enum_decl> | <union_decl> | <nt_decl> | <group_decl> | <directive>

<directive>     ::= "@target" "(" <target_spec> ")" 
                  | "@directive" "(" <directive_name> "," <directive_value> ")"

<function_decl> ::= "f:" <identifier> "(" [<param_list>] ")" ["!" <type>] "{" <stmt_list> "}"
                  | "exf:" <identifier> "(" [<param_list>] ")" ["!" <type>] "{" <stmt_list> "}"

<param_list>    ::= <param> { "," <param> }
<param>         ::= <identifier> [ "!" <type> ] [ "=" <expr> ]

<stmt_list>     ::= { <stmt> }

<stmt>          ::= <var_decl> | <assign_stmt> | <if_stmt> | <loop_stmt>
                  | <return_stmt> | <panic_stmt> | <expr_stmt> | <goto_stmt>
                  | <label_decl> | <flow_stmt> | <block_stmt>

<var_decl>      ::= ("v:" | "c:" | "m:") <identifier> [ "!" <type> ] [ "=" <expr> ]

<assign_stmt>   ::= <identifier> "=" <expr>

<if_stmt>       ::= <expr> "=?>" <block_stmt> [ "?=>" <block_stmt> ]
<loop_stmt>     ::= "loop" "(" [<loop_cond>] ")" <block_stmt>
<loop_cond>     ::= <expr> [ "," <stmt> ]  // optional increment
<return_stmt>   ::= "return" [ <expr> ]
<panic_stmt>    ::= "panic" "(" <expr> ")"
<expr_stmt>     ::= <expr>
<goto_stmt>     ::= <identifier> "?>"
<label_decl>    ::= <identifier> ":>"

<block_stmt>    ::= "{" <stmt_list> "}"

<flow_stmt>     ::= <handler_expr> ("<-" | "->" | "?->" | "?=>" | "_>") <expr>

<handler_expr>  ::= "(" <identifier> ")"

<expr>          ::= <literal> | <identifier> | <call_expr> | <binary_expr> | <unary_expr> | <struct_expr> | <access_expr>
<call_expr>     ::= <identifier> "(" [ <arg_list> ] ")"
<arg_list>      ::= <expr> { "," <expr> }
<binary_expr>   ::= <expr> <bin_op> <expr>
<unary_expr>    ::= <unary_op> <expr>
<struct_expr>   ::= <struct_type> "{" <field_list> "}"
<field_list>    ::= <field_assign> { "," <field_assign> }
<field_assign>  ::= <identifier> "=" <expr>
<access_expr>   ::= <expr> "." <identifier>

<struct_decl>   ::= ["!"] "struct:" <identifier> "{" <var_decl_list> "}"
<var_decl_list> ::= <var_decl> { "," <var_decl> }
<enum_decl>     ::= "enum:" <identifier> "{" <enum_item_list> "}"
<enum_item_list>::= <identifier> [ "(" <field_list> ")" ] { "," <identifier> [ "(" <field_list> ")" ] }
<union_decl>    ::= "union:" <identifier> "{" <var_decl_list> "}"
<nt_decl>       ::= "nt:" <identifier> "=" <type>

<group_decl>    ::= "group" <identifier> "{" { <top_level_decl> | <group_decl> } "}"

<type>          ::= "i32" | "i64" | "u32" | "u64" | "f32" | "f64" | "str" | <identifier> | <type> "[]" | "*" <type> | "&" <type>

<literal>       ::= <integer_literal> | <float_literal> | <string_literal> | "true" | "false"

<bin_op>        ::= "+" | "-" | "*" | "/" | "%" | "==" | "!=" | "<" | "<=" | ">" | ">=" | "&&" | "||" | "<<" | ">>"
<unary_op>      ::= "-" | "!" | "*"
```

---

# 2. Flow Sugar – Desugar Plan

| Sugar                  | Desugar To                                               | Açıklama                                   |
| ---------------------- | -------------------------------------------------------- | ------------------------------------------ |
| `(h) <- fn()`          | `tmp = fn(); h = move tmp;`                              | Capture / move value to handler            |
| `(h) -> fn()`          | `fn(h); h = null;`                                       | Pass flow to next function, clear handler  |
| `(h) ?-> (e){...}`     | `if (!h) { e(); }`                                       | Error redirect                             |
| `(h) ?=> alt_h`        | `if (!h) { h = alt_h; }`                                 | Fallback value                             |
| `(h) ...>`             | `expand(h)`                                              | Expand buffer / list                       |
| `(h) _> target_h`      | `target_h = move(h); h = null;`                          | Relocate / move to another handler/context |
| `retry(n, ms) -> fn()` | `for i in 1..n { if fn() success break else sleep(ms) }` | Retry wrapper                              |

> Böylece frontend operatörleri core AST’ye dönüştürülüp IR generator tarafından SSA tarzına çevrilebilir.

---

# 3. Compiler Pipeline

```
Source Code (NexusFlow) 
    │
    ▼
Lexer → Parser
    │
    ▼
Flow Desugarer (Sugar → Core AST)
    │
    ▼
Type Checker / Ownership Checker
    │
    ▼
IR Generator (SSA-style)
    │
    ▼
Optimizer (TargetInfo based)
    │
    ▼
Backend (Target-specific Codegen: x86, ARM, GPU, etc.)
```

---

# 4. Target-Aware Optimization

* `TargetInfo` struct okunur:

```c
typedef struct {
    char arch[16];
    char abi[16];
    int stack_alignment;
    int scalar_regs;
    int vector_regs;
} TargetInfo;
```

* Bu bilgiler optimizer ve backend için:

  * Register allocation
  * Stack alignment
  * Vector instruction selection
  * Inline/fast path generation

---

# 5. Önerilen Minimal Flow / Keywords

* **Flow operators:** `<-`, `->`, `?->`, `?=>`, `_>`
* **Core keywords:** `if`, `else`, `loop`, `return`, `panic`, `struct`, `!struct`, `enum`, `union`, `nt`, `group`, `@target`, `@directive`, `goto`, `label:`
* **Optional:** `retry(n, ms)` sugar, `expand(...)`

---

💡 Bu tasarım ile:

* Endüstriyel kullanım (kernel, driver, embedded) mümkün
* Akış yönetimi (handler ve masalar) net
* Operatörler desugar ile core’a indirgenmiş, karmaşıklık azaltılmış
* Target bağımsız IR ile farklı işlemciler desteklenebilir

---

Bu üç sistemi için önerilerim, kullanım kolaylığı, esneklik ve endüstriyel gereksinimler açısından odaklanacak şekilde tasarlandı:

### 1. **Yerel Modül Yükleme & DLL Yükleme & Extern Modül Yükleme**

Bu bölümde temel amacımız **modül yükleme**, **DLL entegrasyonu** ve **extern modüller** ile ilgili geniş bir destek sağlamaktır. Sistem tasarımını sadeleştirip esnek hale getirmek adına aşağıdaki iyileştirmeleri öneriyorum:

#### **a. Modül Yükleme**

* **Modül İsim Uzayı (Namespace)** ve **Aliasing**: `use` komutlarıyla modüllerin daha esnek ve çakışmasız şekilde yüklenmesini sağlayacak bir yapı oluşturmak önemlidir. Ayrıca, `as` keyword’ü ile modüllere alternatif isimler verilmesi sağlanabilir, böylece her modülün bağımsız bir isim alanı (namespace) olacak ve projede daha kolay yönetilebilirler.

* **Eksport ve Public Tanımlamalar**: `exp` ve `pup` anahtar kelimeleriyle dışarıya açık fonksiyon ve grupların yönetimi daha açık bir şekilde tanımlanabilir. Bu, modüllerin dışarıya fonksiyon veya gruplarını **export** etmesine olanak verirken, **pup** ile daha iyi kontrol edilen erişim sağlar.

**Öneri:**

```nexus
use math as m;   # Aliasing kullanarak dış kütüphaneyi başka isimle çağır
use cpu, io;

exp group file {
    # Export edilen grup
    pup f:my_function();
}

```

#### **b. DLL Yükleme ve Import**

* **DLL ve dış kütüphane yükleme** işlemini basitleştirip, daha taşınabilir ve platform bağımsız bir hale getirebiliriz. Modüller doğrudan platform bazında tanımlanacak ve gerektiğinde ilgili platform için doğru kütüphane import edilecektir.

* **DLL export ve import fonksiyonları**: Buradaki `dllexp` komutu ile dış kütüphane fonksiyonları tanımlanabilir. Ancak **platform bağımlılığı** açısından, `import(path)` komutu sayesinde DLL, `.so` veya `.obj` gibi dosyalar dinamik olarak yüklenebilir. Bu modüller, bir grup gibi işlem yaparak dış fonksiyonları doğrudan kullanılabilir hale getirir.

**Öneri:**

```nexus
dllexp: run_optimizer(*oir_path, *nxir_path, *target) { ... }

# DLL dosyasını yükle ve fonksiyonları çalıştır
(Optimizer) <- import("optimizer.dll") ?-> !!("DLL Bulunamadı!");

# Optimizer grubunun fonksiyonları kullanıma sunulur.
v:sonuc = Optimizer.run_optimizer(oir_path, nxir_path, target_info);
```

#### **c. Extern Modüller ve Platform Tabanlı Modüller**

* **OS spesifik modüllerin desteklenmesi** için platform kontrolü `!!=[target = "os"]` gibi bir yapı ile sağlanabilir. Bu, farklı işletim sistemleri için fonksiyonları içeren modülleri yüklerken **platform bağımsızlığını** güvence altına alır. Modülün **statik olarak linklenmesi** ve uygun fonksiyonların çalıştırılması gerektiğinde `!!=` komutları kullanılabilir.

* **Makro Tabanlı Modül Tanımlamaları**: Bu, modüllerin daha özelleştirilebilir olmasını sağlar. Kullanıcılar, özel platformları hedefleyen gruplar ve fonksiyonlar tanımlarken makro yapısını kullanabilirler.

**Öneri:**

```nexus
!!=[target = "windows"] {
    !!=link name = "msvcrt";
    exf:_write(fd!i32, buf!*char, size!u32)!i32;
}

!!=[target = "linux"] {
    !!=link name = "libc";
    exf:write(fd!i32, buf!*char, size!u32)!i32;
}

group SysIO {
    output => f:(msg!str) {
        v:len = len(msg);
        v:ptr = addr(msg);

        !!=[target = "windows"] {
            _write(1, ptr, len);
        }

        !!=[target = "linux"] {
            write(1, ptr, len);
        }

        !!=[target = "bare-metal"] {
            loop(i, i < len, i++) {
                poke(0xB8000 + (i * 2), msg[i]);
                poke(0xB8000 + (i * 2) + 1, 0x07); # Gri renk
            }
        }
    }
}
```

---

### 2. **Macro Sistemi**

Makrolar, daha **yüksek düzeyde soyutlama** sağlayan güçlü bir özellik. Ancak kullanımı karmaşıklaştırmamak ve yanlış kullanımın önüne geçmek için aşağıdaki önerileri dikkate almak iyi olur:

#### **a. Makro Tanımları**

* **Makro Güvenliği**: Makroların çalıştırılacak kodu doğrudan etkileyen bir yapıda olması, **hata ayıklama** ve **kolay izleme** açısından önemli. Makroların açıkça tanımlanması, kodun hem okunabilirliğini artırır hem de hataları en aza indirir.

**Öneri:**

```nexus
:macro! OS_Bridge($name, $win_fn, $lin_fn) {
    group $name {
        run => f:(args) {
            !!=[target = "windows"] {
                $win_fn(args);
            }
            !!=[target = "linux"] {
                $lin_fn(args);
            }
            !!=[target = "bare-metal"] {
                !!("Unsupported Platform");
            }
        }
    }
}

!!=OS_Bridge!(IO, _write, write);
!!=OS_Bridge!(Net, win_send, lin_send);
```

#### **b. Makro Gelişimi ve Kullanımı**

* **Makro Tetikleme**: Kullanıcılar daha dinamik, platform bağımsız ve çeşitli tiplerde fonksiyonlar oluşturabilmeli. Bunun için **makro parametrelerini** daha esnek kullanmak, yani `CreateMath` gibi fonksiyonların parametrelerine yeni türler ekleyebilmek faydalı olacaktır.

* **Grup Bazlı Makrolar**: Makrolar sayesinde kod yeniden kullanılabilirliği artırılabilir. Makroların sadece fonksiyon oluşturmak değil, **komple grup yapıları** yaratması sağlanabilir.

**Öneri:**

```nexus
:macro! CreateMath($name!id, $type!type) {
    group $name {
        add => f:(a!$type, b!$type)!$type {
            ret (a + b);
        }
    }
}

!!=CreateMath!(IntMath, i32);
!!=CreateMath!(FloatMath, f32);

f:main() {
    v:sonuc1 = IntMath.add(10, 20);  # i32 işlemi
    v:sonuc2 = FloatMath.add(10.5, 5.2);  # f32 işlemi
}
```

---

### 3. **Genel Öneriler ve İyileştirmeler**

* **Karmaşıklığı Azaltma**: Dili daha **basitleştirmek** için bazı `flow` operatörlerini (örneğin, `?->`, `?=>`) anahtar kelimelerle değiştirilebilir. Bu, daha anlaşılır ve daha az yazım hatasına yol açan bir dil yapısı sunar.
* **Platform Bağımsızlık**: Makroların ve `extern` modüllerin platform bağımsız çalışabilmesi için, derleyicinin sağladığı **hizmetlerin** ve **os spesifik komutların** doğru şekilde işlenmesi gerekir.

  Bunun dışında, **modüler yapıyı** her platformda aynı şekilde kullanmak için **modül tabanlı yüklemeler** daha esnek ve platform bağımsız olmalıdır.

---

Bu yapılarla NexusFlow, endüstriyel kullanıma uygun hale gelebilir ve gelişmiş özelliklere sahip güçlü bir dil oluşturulabilir. Tasarımı daha da optimize edebiliriz, ancak bu yapının sağlam bir temel oluşturacağını düşünüyorum.




Harika, bu noktada hedef sistem (target) tanımı, hem derleyici hem de makro sistemi için **çapraz platform ve donanım bağımsızlığı** açısından kritik. Şu anki basit örnek (`arch="x86_64"`, `abi="win64"`, vb.) iyi bir başlangıç ama endüstriyel kullanım ve gerçek sistem hedefleri için çok daha detaylı hale getirilmesi gerekir. İşte önerim:

---

## 1. Target Sistemi İçin Önerilen Geliştirme Alanları

### 1.1 Temel Donanım Bilgileri

Mevcut `arch`, `abi`, `stack_alignment`, `scalar_regs`, `vector_regs` zaten önemli, ama eklenebilir:

| Özellik            | Açıklama                                                                             |
| ------------------ | ------------------------------------------------------------------------------------ |
| `arch`             | İşlemci mimarisi (x86_64, ARM64, RISC-V, vb.)                                        |
| `abi`              | Hedef işletim sistemi ve calling convention (win64, linux64, bare-metal, sysv, etc.) |
| `endianness`       | Bellek sıralama (little/big)                                                         |
| `pointer_size`     | Pointer boyutu (32/64 bit)                                                           |
| `cache_line_size`  | CPU cache hattı boyutu, optimize edilmiş memory copy için                            |
| `page_size`        | Varsayılan bellek sayfa boyutu                                                       |
| `scalar_regs`      | Genel amaçlı register sayısı                                                         |
| `vector_regs`      | SIMD / vector register sayısı                                                        |
| `fp_regs`          | Floating point register sayısı                                                       |
| `stack_alignment`  | Stack alignment zorunluluğu (byte cinsinden)                                         |
| `max_thread_local` | Maksimum thread-local storage                                                        |
| `features`         | CPU özellikleri (SSE4, AVX2, NEON, FMA, vs.)                                         |

---

### 1.2 OS ve Sistem Özellikleri

Makrolarla ve platform hedeflemeleri için gerekli:

| Özellik             | Açıklama                                       |
| ------------------- | ---------------------------------------------- |
| `os_name`           | Windows, Linux, MacOS, Bare-metal, RTOS        |
| `filesystem_type`   | FAT32, ext4, NTFS gibi                         |
| `std_lib_available` | OS standard library var mı yok mu (True/False) |
| `syscall_support`   | Hangi syscall API’leri mevcut (POSIX, WinAPI)  |
| `interrupt_vector`  | Donanım interrupt tablosu (bare-metal için)    |

---

### 1.3 Derleyici ve Optimizasyon Parametreleri

Target sistemi sadece hardware değil, **derleyici davranışını da** etkiler:

| Özellik             | Açıklama                               |
| ------------------- | -------------------------------------- |
| `default_alignment` | Struct ve veri hizalaması için default |
| `unroll_limit`      | Derleyici unroll/loop limitleri        |
| `inline_threshold`  | Fonksiyon inline eşiği                 |
| `vector_width`      | SIMD vector genişliği                  |
| `atomic_support`    | Atomik işlemler destekleniyor mu       |
| `max_stack_size`    | Stack limitleri (bare-metal/OS)        |

---

### 1.4 Örnek Gelişmiş Target Tanımı

```nexus
target "win64_x86" {
    arch = "x86_64"
    abi = "win64"
    endianness = "little"
    pointer_size = 8
    stack_alignment = 16
    scalar_regs = 16
    vector_regs = 16
    fp_regs = 8
    cache_line_size = 64
    page_size = 4096
    max_thread_local = 64KB
    features = ["SSE4.2","AVX2","FMA"]
    os_name = "Windows"
    filesystem_type = "NTFS"
    std_lib_available = true
    syscall_support = ["WinAPI"]
    default_alignment = 8
    unroll_limit = 32
    inline_threshold = 64
    vector_width = 256
    atomic_support = true
    max_stack_size = 1MB
}
```

Benzer şekilde `linux_x86` ve `bare_metal_arm` gibi diğer platformları da detaylandırabiliriz:

```nexus
target "linux_x86" {
    arch = "x86_64"
    abi = "sysv"
    endianness = "little"
    pointer_size = 8
    stack_alignment = 16
    scalar_regs = 16
    vector_regs = 16
    fp_regs = 8
    cache_line_size = 64
    page_size = 4096
    max_thread_local = 64KB
    features = ["SSE4.2","AVX","FMA"]
    os_name = "Linux"
    filesystem_type = "ext4"
    std_lib_available = true
    syscall_support = ["POSIX"]
    default_alignment = 8
    unroll_limit = 32
    inline_threshold = 64
    vector_width = 256
    atomic_support = true
    max_stack_size = 8MB
}
```

```nexus
target "bare_metal_arm" {
    arch = "ARM64"
    abi = "aapcs"
    endianness = "little"
    pointer_size = 8
    stack_alignment = 16
    scalar_regs = 31
    vector_regs = 32
    fp_regs = 32
    cache_line_size = 64
    page_size = 4096
    max_thread_local = 0
    features = ["NEON","FP"]
    os_name = "bare-metal"
    filesystem_type = "none"
    std_lib_available = false
    syscall_support = []
    default_alignment = 8
    unroll_limit = 16
    inline_threshold = 32
    vector_width = 128
    atomic_support = true
    max_stack_size = 256KB
}
```

---

### 2. Neden Bu Detaylar Önemli?

1. **Makro sistemi**: `!!=[target = "linux"]` veya `!!=[target="bare-metal"]` gibi hedefleme çok daha güçlü hale gelir.
2. **Derleyici optimizasyonu**: register sayısı, stack alignment ve SIMD genişliği gibi bilgiler derleyici için kritik.
3. **Cross-OS Kodlama**: Aynı fonksiyon veya grup hem Windows hem Linux hem bare-metal üzerinde çalışacaksa, target sistemi sayesinde otomatik seçim yapılabilir.
4. **Endüstriyel Uygulamalar**: Kernel, driver, embedded ve endüstriyel sistemler için gerekli bütün low-level parametreleri kapsar.

---
Kesinlikle, doğru düşünüyorsun. 🔹 Endüstriyel bir dilde target sistemi önceden tanımlı olabilir ama yeni işlemci, yeni OS veya custom hardware çıktığında geliştiricinin **kendi `.target` dosyasını oluşturabilmesi ve derleyiciye tanıtabilmesi** gerekir.

Bunu iki aşamalı bir yapı ile sunabiliriz:

---

## 1️⃣ Target Dosyası Yapısı (`.target`)

* Dosya tamamen **insan tarafından okunabilir ve düzenlenebilir** olmalı.
* Basit key-value + liste formatı kullanılabilir.
* NexusFlow ile doğal şekilde parse edilebilir.

Örnek `.target` dosyası:

```nexus
# Windows x86_64
target_name = "win64_x86"
arch = "x86_64"
abi = "win64"
endianness = "little"
pointer_size = 8
stack_alignment = 16
scalar_regs = 16
vector_regs = 16
fp_regs = 8
cache_line_size = 64
page_size = 4096
max_thread_local = 64KB
features = ["SSE4.2", "AVX2", "FMA"]
os_name = "Windows"
filesystem_type = "NTFS"
std_lib_available = true
syscall_support = ["WinAPI"]
default_alignment = 8
unroll_limit = 32
inline_threshold = 64
vector_width = 256
atomic_support = true
max_stack_size = 1MB
```

> Not: Liste ve sayısal değerleri `string`, `integer` ve `array` olarak desteklemeliyiz.

---

## 2️⃣ Target Dosyası Oluşturma Kılavuzu

1. **Yeni bir `.target` dosyası oluşturun**
   Örn: `bare_metal_rpi.target`

2. **Minimum gerekli alanları doldurun**:

   * `target_name`
   * `arch`, `abi`
   * `pointer_size`, `stack_alignment`
   * `scalar_regs`, `vector_regs`
   * `os_name`

3. **İsteğe bağlı alanlar**:

   * `fp_regs`, `cache_line_size`, `page_size`
   * `features` (SSE, AVX, NEON vs.)
   * `syscall_support`, `std_lib_available`
   * `atomic_support`, `max_stack_size`

4. **Dosyayı kaydedin ve derleyiciye gösterin**:

```nexus
!!=load_target!("bare_metal_rpi.target");
```

5. **Makrolarda ve extern modüllerde kullanın**:

```nexus
!!=[target="bare_metal_rpi"] {
    SysIO.output("Bare-metal RPi platformu aktif!");
}
```

---

## 3️⃣ İleri Seviye Öneriler

* **Target template’leri**: Windows, Linux, Bare-metal, ARM Cortex, RPi gibi örnekler hazır bulunsun.
* **Derleyici doğrulaması**: `.target` yüklenirken, eksik zorunlu alanları raporlasın.
* **Target override**: Makro veya extern modüller, runtime’da target üzerinde küçük değişiklik yapabilmeli (`stack_alignment` override vs.).

---

**“Target Oluşturma Rehberi”** 

---

# 📄 NexusFlow Target Dosyası Örneği

**Dosya adı:** `win64_x86_64.target`

```nexus id="target-example-001"
# ======================================================
# NexusFlow Target Configuration File
# ======================================================
# Target Name
target_name = "win64_x86_64"

# Architecture & ABI
arch = "x86_64"
abi = "win64"
endianness = "little"

# Pointer & Stack
pointer_size = 8
stack_alignment = 16
max_stack_size = 1MB

# Registers
scalar_regs = 16
vector_regs = 16
fp_regs = 8
vector_width = 256

# Memory & Cache
cache_line_size = 64
page_size = 4096
max_thread_local = 64KB
default_alignment = 8
atomic_support = true

# CPU Features
features = ["SSE4.2", "AVX2", "FMA"]

# OS & Filesystem
os_name = "Windows"
filesystem_type = "NTFS"
std_lib_available = true
syscall_support = ["WinAPI"]

# Compiler/Optimizer Settings
unroll_limit = 32
inline_threshold = 64
```

---

# 🛠️ NexusFlow Target Oluşturma Rehberi

## 1️⃣ Yeni Target Dosyası Oluşturma

1. `.target` uzantılı bir dosya açın.

2. `target_name` ile benzersiz bir isim verin.

3. Minimum zorunlu alanları doldurun:

   * `arch` → işlemci mimarisi (x86_64, armv7, riscv64 …)
   * `abi` → işletim sistemi çağrı standardı
   * `pointer_size`, `stack_alignment` → bayt cinsinden
   * `scalar_regs`, `vector_regs` → toplam register sayısı
   * `os_name` → Windows, Linux, Bare-metal vb.

4. Opsiyonel alanları doldurun:

   * `fp_regs`, `vector_width`, `features`
   * `cache_line_size`, `page_size`, `max_stack_size`
   * `std_lib_available`, `syscall_support`
   * `atomic_support`, `default_alignment`

---

## 2️⃣ Target Dosyasını Yükleme

```nexus id="target-load-001"
!!=load_target!("win64_x86_64.target");
```

> Bu komut derleyiciye hedef platformu tanıtır. Tüm makrolar ve extern modüller bundan sonra çalışır.

---

## 3️⃣ OS Spesifik Kod Kullanımı

```nexus id="target-macro-001"
!!=[target="win64_x86_64"] {
    SysIO.output("Windows platformu aktif!");
}

!!=[target="linux_x86_64"] {
    SysIO.output("Linux platformu aktif!");
}

!!=[target="bare_metal_rpi"] {
    SysIO.output("Bare-metal RPi platformu aktif!");
}
```

* Makrolar ve `!!=[target=...]` ile **aynı kodu farklı platformlarda kullanabilirsiniz**.
* Kodunuzu tekrar yazmaya gerek yok.

---

## 4️⃣ Yeni Platform Eklerken Dikkat Edilecekler

* Eksik zorunlu alanları doldurun.
* CPU özelliklerini doğru girin (`features` listesi).
* Stack alignment ve pointer size, derleyici ve inline assembly uyumunu sağlar.
* Bare-metal hedeflerde `std_lib_available = false` ve uygun IO yazılmalı (`VGA`, `UART` vb.).
* `.target` dosyaları proje içinde version control ile yönetilebilir.

---

## 5️⃣ Örnek Bare-Metal ARM Target

```nexus id="target-bare-metal-001"
target_name = "bare_metal_rpi"
arch = "armv7"
abi = "bare-metal"
endianness = "little"
pointer_size = 4
stack_alignment = 8
scalar_regs = 16
vector_regs = 0
fp_regs = 0
max_stack_size = 128KB
features = ["NEON"]
os_name = "Bare-metal"
std_lib_available = false
filesystem_type = "none"
syscall_support = []
```

---

✅ Bu yapıyla:

* Geliştirici kendi target’ını tanımlar.
* Makrolar ve extern modüller platformdan bağımsız çalışır.
* Aynı kod Windows, Linux ve bare-metal RPi gibi farklı sistemlerde sorunsuz çalışır.

---
