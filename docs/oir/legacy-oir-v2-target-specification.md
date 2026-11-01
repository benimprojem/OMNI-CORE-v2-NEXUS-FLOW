

# 📑 NexusFlow v2: OIR & Target Formal Specification

## 1. OIR v2 (Omni Intermediate Representation) Spesifikasyonu

OIR, derleyicinin iç organları arasındaki tek iletişim dilidir. **Binary** formatta, **Streaming** yapısında ve **Ayrıntılı (Verbose)** bir içeriğe sahiptir.

### 1.1 Dosya Yapısı (Binary Layout)

| Offset | Boyut | İsim | Açıklama |
| --- | --- | --- | --- |
| 0x00 | 4B | `Magic` | `0x4F495232` ("OIR2") |
| 0x04 | 2B | `Version` | `0x0002` (v2.0) |
| 0x06 | 2B | `Mode` | `0x00` Release, `0x01` Debug |
| 0x08 | 32B | `Target` | Hedef Target ID (örn: `x86_64-win`) |
| 0x28 | 4B | `SymTbl_Off` | Sembol Tablosu başlangıç adresi |
| 0x2C | 4B | `Code_Off` | Opcode Stream başlangıç adresi |

### 1.2 Sembol Tablosu (Symbol Table)

Her sembol (değişken, fonksiyon, group) burada tanımlanır. `Analyzer` burayı doldurmakla yükümlüdür.

* **Entry:** `[ID:4B] [Prefix:1B] [TypeID:4B] [Flags:2B] [Name_Len:2B] [Name:v]`
* **Prefixes:** `0x01: v:`, `0x02: c:`, `0x03: m:`, `0x04: f:`, `0x05: r:`, `0x06: o:`.
* **Flags:** `0x01: Mutable`, `0x02: Public`, `0x04: Extern`, `0x08: Atomic`.

### 1.3 Opcode Stream (Fat Opcodes)

Sadece işlem değil, bağlam (context) taşır. Her komut 16 byte sabit uzunluktadır (Hızlı seek için).

* **Format:** `[Op:1B] [Type:1B] [SymbolID:4B] [OperandID:4B] [Line:4B] [Col:2B]`
* **Kritik Nexus Opcodes:**
* `0x10 (OP_PIPE)` -> `->` Akış işlemi.
* `0x11 (OP_CAPTURE)` -> `<-` Veri yakalama.
* `0x12 (OP_RELOCATE)` -> `_>` Sahiplik transferi.
* `0x20 (OP_RULES_ON)` -> Kural kontrolü başlat.
* `0x30 (OP_FAST_ASM)` -> `fastexec` bloğu girişi.



---

## 2. Target Spec v1.0 (Machine Specification)

Codegen'in register ataması ve komut üretimi için baktığı "İşlemci Sözlüğü"dür.
```
[target_info]
spec_version = 1.0
identifier = "x86_64-windows"
author = "NexusFlow Team"

[cpu_arch]
family = "x86"
bits = 64
endian = "little"
# Codegen için register havuzu
registers = ["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"]
scratch_regs = ["r10", "r11"] # Geçici işlemler için
reserved_regs = ["rsp", "rbp"] # Dokunulmazlar

[instruction_set]
simd = "avx512"
atomic = true
fastexec_support = true

[memory_model]
page_size = 4096
stack_alignment = 16
default_area_size = 1M  # area() default
zone_sync_mode = "atomic"

[abi_convention]
name = "ms_x64"
shadow_space = 32 # Windows için gerekli 32-byte alan
return_reg = "rax"
param_regs = ["rcx", "rdx", "r8", "r9"]

[output_format]
binary_type = "pe" # Portable Executable
object_ext = ".obj"
shared_lib_ext = ".dll"
entry_point = "WinMain"

[nexus_primitives]
# NexusFlow'a özel düşük seviye eşlemeler
handler_init = "xor rax, rax"
relocate_op = "mov rdi, rsi; xor rsi, rsi" # Sahiplik transferi mantığı
```
### 2.1 [abi] Katmanı

* **`stack_alignment`**: Stack'in kaç byte'a hizalanacağı (örn: 16).
* **`calling_convention`**: Fonksiyon çağrı standardı (Microsoft x64, System V, AAPCS).
* **`shadow_space`**: Fonksiyon çağrısı öncesi stack'te ayrılacak boş alan.

### 2.2 [registers] Katmanı

* **`volatile_regs`**: Fonksiyon çağrısında korunan registerlar.
* **`scratch_regs`**: Optimizer'ın ara işlemler için kullanabileceği serbest registerlar.
* **`arg_regs`**: Parametrelerin taşındığı register sırası (örn: `rcx, rdx, r8, r9`).

### 2.3 [nexus_logic] Katmanı (Donanım Soyutlaması)

Donanım bazlı Nexus operatörlerinin nasıl çözüleceği:

* **`atomic_inc`**: Donanımın atomik artırma komutu (`lock inc` veya `ldrex/strex`).
* **`handler_cleanup`**: `}?;` anında yapılacak donanım bazlı temizlik komutu.

---

## 3. Modül Kontratları (The Rules of Conduct)

1. **Parser.dll:** Sadece `parser.h`'ı bilir. Çıktısı olan OIR dosyasında `TypeID` kısımları boştur, sadece `Symbol` isimlerini ve `AST` yapısını yazar.
2. **Analyzer.dll:** OIR'i okur. Sembol tablosundaki `TypeID`'leri doldurur. Geçersiz `Prefix` kullanımında (`c:` olanın değiştirilmesi gibi) hata verip süreci durdurur.
3. **Optimizer.dll:** OIR'i okur. `rules` segmentine bakar. Eğer bir kural (`always x < 100`) çiğnenme riski taşıyorsa OIR'e `WARN` flag'i ekler veya kodu budar.
4. **Codegen.dll:** OIR + Target'ı okur. En son `.ocb` (Omni-Binary) dosyasını üretir. Register mapping hatalarını burada yakalar.

---

## 4. Debug vs. Release Stratejisi

* **Debug Modu:** OIR dosyası içinde her komutun yanında orijinal `.nx` kodunun bir kopyası (`SourceMap`) bulunur. Hata anında `occ.exe` hangi DLL'in hangi satırda patladığını raporlar.
* **Release Modu:** OIR içinde isimler silinir, sadece ID'ler ve Adres Offsetleri kalır. Dosya boyutu %80 küçültülür.

### Sistem Mimarı Notu

Bu spec, bizim **"Single Source of Truth"** (Tek Gerçeklik Kaynağı) dosyamızdır. İleride C++ ile `class OIR_Manager` yazdığımızda, bu tablodaki offsetlere göre `fread` ve `fwrite` yapacağız.
