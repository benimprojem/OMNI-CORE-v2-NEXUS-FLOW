# 🔬 NexusFlowSyntax.md vs Kaynak Kod: Kapsamlı Uyumluluk Analizi

**Tarih:** 2026-02-24 | **Mod:** ULTRA — Derinlemesine Analiz

---

## Genel Değerlendirme (Executive Summary)

| Katman | Dosya | Satır | Tamamlanma | Kritik Eksik |
|--------|-------|-------|------------|--------------|
| **Lexer** | `lexer.c/h` | 637 | **%92** | `map<>`, `Vec2/3/4`, `d32/d64` token yok |
| **Parser** | `parser.c/h` | 2529 | **%78** | `rules`, `macro`, `style`, `unroll`, lambda, `import` |
| **AST** | `ast.c/h` | 337+43K | **%80** | Array/Map literal, rules/macro düğümleri yok |
| **Semantic** | `semantic.c/h` | 937 | **%65** | Auto-scaling, ownership, handler semantiği yok |
| **IR Gen** | (`parser.c` içinde) | ~300 | **%45** | Nexus Flow, struct, enum, group IR dönüşümü yok |
| **Optimizer** | `optimizer.c` | 1329 | **%90** | OIR formatı bağımsız, spesifikasyona bağlı değil |
| **Codegen** | `codegen.c/h` | 933 | **%55** | AST-based codegen yok, sadece IR tabanlı |
| **Stdlib** | `stdlib/` | ~6 dosya | **%10** | CoreModule, flow, ipc modülleri yok |

> **Genel Oran: ~%60** — Parser ve lexer büyük ölçüde çalışıyor, ancak spesifikasyonun tam kapsamından uzak.

---

## Bölüm 1: Tanımlayıcılar ve Tipleme (Spec §2)

### ✅ Kodlanmış (Lexer + Parser)

| Özellik | Spec Ref | Lexer Token | Parser Fonksiyon | Durum |
|---------|----------|-------------|------------------|-------|
| `v:` (VAR) | §2.1 | `TOKEN_PRE_VAR` | `parse_var_decl` | ✅ Tam |
| `c:` (CONST) | §2.1 | `TOKEN_PRE_CONST` | `parse_var_decl` | ✅ Tam |
| `m:` (MUST) | §2.1 | `TOKEN_PRE_MUST` | — | ⚠️ Token var, parser yok |
| `nt:` (NewType) | §2.1 | `TOKEN_PRE_NEWTYPE` | `parse_newtype_decl` | ✅ Tam |
| `r:`, `t:`, `o:` (Reserve) | §2.1 | `TOKEN_PRE_RES/TYPE/OBJ` | — | ⚠️ Token var, parser yok |
| `*` (Pointer) | §2.1 | `TOKEN_STAR` | — | ⚠️ Pointer deref yazılmamış |
| `&` (Ref) | §2.1 | `TOKEN_BIT_AND` | — | ⚠️ Referans semantiği yok |
| `!` (Hard-Lock) | §2.2 | `TOKEN_NOT` | `parse_var_decl` | ✅ Kısmi |
| Temel tipler (u8..u64, f32, f64, str, char, bool) | §2.2 | `TOKEN_TYPE_*` | — | ✅ Token var |

### ❌ Eksik

| Özellik | Spec Ref | Durum |
|---------|----------|-------|
| **Auto-Scaling** (100→u8, 300→u16) | §2.2 | ❌ Semantic'te otomatik tip çıkarımı yok |
| **Implicit Casting** (i64 + u8 → i64) | §2.2 | ❌ `are_types_compatible` çok basit |
| `bit`, `hex`, `d32`, `d64` tipleri | §2.2 | ⚠️ `bit`, `byte`, `hex` token var ama semantic desteği yok |
| `Vec2`, `Vec3`, `Vec4` (SIMD) | §2.2 | ❌ Hiçbir katmanda yok |
| `map<key, val>` generic tipi | §2.3 | ❌ Token, parser, semantic — hiçbiri yok |
| Bilimsel gösterim (`10E3`) | §2.3 | ⚠️ Lexer'da kısmi — `E` ayrıştırması belirsiz |
| Üslü sayılar (`10^^15`) | §2.3 | ⚠️ `TOKEN_POWER` var ama `^^` literal ayrıştırma yok |
| `.=` string birleştirme | §2.7 | ⚠️ `TOKEN_STRING_APPEND` var, parser'da kullanılmıyor |
| `..` range operatörü | §2.7 | ⚠️ `TOKEN_RANGE` var, `parse_range` fonksiyonu var ama kısmi |

---

## Bölüm 2: Nexus Flow Operatörleri — 21 Anahtar (Spec §3)

### ✅ Kodlanmış

| # | Operatör | Token | AST Düğümü | Parser | Semantic | IR/Codegen | Durum |
|---|----------|-------|-----------|--------|----------|------------|-------|
| 1 | `(h)` Handler | — | `ASTNexusCapture` | ✅ | ⚠️ | ❌ | Parser OK, geri kalanı eksik |
| 2 | `<-` Capture | `TOKEN_CAPTURE` | `AST_NEXUS_CAPTURE` | ✅ | ⚠️ | ❌ | Parser OK |
| 5 | `?(n,ms)->` Rolling | `TOKEN_QMARK` | `ASTNexusFlow` (retry_count/delay) | ✅ | ❌ | ❌ | Parser OK |
| 6 | `=?>` If | `TOKEN_IF_FLOW` | `AST_IF` | ✅ | ✅ | ⚠️ | Çalışıyor |
| 7 | `?->` Catch | `TOKEN_ELSE_FLOW` | `AST_NEXUS_FLOW` | ✅ | ❌ | ❌ | Parser OK |
| 8 | `?=>` Fallback | `TOKEN_FALLBACK` | `AST_NEXUS_FLOW` | ✅ | ❌ | ❌ | Parser OK |
| 9 | `->` Success | `TOKEN_PIPE` | `AST_NEXUS_PIPE` | ✅ | ❌ | ❌ | Parser OK |
| 10 | `!->` Ignore | `TOKEN_IGNORE` | `AST_NEXUS_FLOW` | ⚠️ | ❌ | ❌ | Kısmi |
| 15 | `?>(label)` Jump | `TOKEN_JUMP` | `AST_NEXUS_JUMP` | ✅ | ✅ | ⚠️ | İyi |
| 16 | `(label):>` Label | `TOKEN_NEXUS_LABEL` | `AST_NEXUS_LABEL` | ✅ | ✅ | ⚠️ | İyi |
| 17 | `?;` Halt | `TOKEN_HALT` | `AST_NEXUS_HALT` | ✅ | ❌ | ❌ | Parser OK |
| 18 | `!!` Panic | `TOKEN_FORCE` | `AST_PANIC` | ✅ | ❌ | ❌ | Parser OK |
| 19 | `@` Intent | `TOKEN_INTENT` | `AST_NEXUS_INTENT` | ✅ | ❌ | ❌ | Parser OK |
| 20 | `??` Y.Z. | `TOKEN_AI` | `AST_AI_LOGIC` | ⚠️ | ❌ | ❌ | Çok kısmi |
| 21 | `!!=` Directive | `TOKEN_FORCE_ASSIGN` | — | ❌ | ❌ | ❌ | Sadece token |

### ❌ Eksik

| # | Operatör | Durum |
|---|----------|-------|
| 3 | `...>` Expand/Unpack | ⚠️ Token var (`TOKEN_UNPACK`), AST var, parser'da kullanım yok |
| 4 | `_>` Relocate | ⚠️ Token var (`TOKEN_RELOCATE`), parser'da kullanım yok |
| 11 | `<<` Zone Write | ⚠️ Token `TOKEN_LSHIFT` olarak var, zone semantiği yok |
| 12 | `>>` Flow Feed | ⚠️ Token `TOKEN_RSHIFT` olarak var, akış semantiği yok |
| 13 | `<--` Iterate / Foreach | ⚠️ `TOKEN_ITERATE` var, loop foreach'te kullanılıyor |
| 14 | `(e){}` Error Block | ⚠️ `TOKEN_ERR_HANDLER` var, parser'da eksik |

> **Sonuç:** 21 operatörden **yalnızca ~8 tanesi** parser'dan semantic'e ve IR'a kadar tam akıyor. Geri kalanı sadece token/AST düzeyinde tanımlı.

---

## Bölüm 3: Kontrol Yapıları (Spec §5)

| Yapı | Spec Ref | Parser | Semantic | IR | Durum |
|------|----------|--------|----------|-----|-------|
| `loop{}` (infinity) | §5.1 | ✅ | ✅ | ⚠️ | Çalışıyor |
| `loop(cond){}` (while) | §5.1 | ✅ | ✅ | ⚠️ | Çalışıyor |
| `loop(i, cond, inc){}` (for) | §5.1 | ✅ | ✅ | ⚠️ | Çalışıyor |
| `loop(v:item <-- list){}` (foreach) | §5.1 | ✅ | ⚠️ | ❌ | Parser OK, semantik kısmi |
| `loop(1..100){}` (range) | §5.1 | ⚠️ | ❌ | ❌ | Range parsing eksik |
| `unroll(c) -> loop` | §5.1 | ❌ | ❌ | ❌ | **Tamamen eksik** |
| `=?>` / `?=>` (if/else) | §5.2 | ✅ | ✅ | ⚠️ | Çalışıyor |
| `break` / `continue` | §5.2 | ✅ | ✅ | ❌ | Parser+semantic OK |
| `(label):>` / `(label)?>` goto | §5.3 | ✅ | ✅ | ⚠️ | İyi |
| **`rules` blokları** | §5.3.1 | ❌ | ❌ | ❌ | **Tamamen eksik** |
| `group` metodu | §5.4 | ✅ | ⚠️ | ❌ | Parser iyi, codegen yok |
| `_>` Relocate | §5.5 | ❌ | ❌ | ❌ | Sadece token |

---

## Bölüm 4: Fonksiyonlar (Spec §6)

| Özellik | Parser | Semantic | IR | Durum |
|---------|--------|----------|-----|-------|
| `f:name(params)!type {}` | ✅ | ✅ | ⚠️ | Çalışıyor |
| `f:main(argc, argv)!i32` | ✅ | ✅ | ⚠️ | Çalışıyor |
| İsimli parametreler `topla(a:10, b:20)` | ❌ | ❌ | ❌ | **Eksik** |
| Lambda `f:(a,b)> a*b` | ❌ | ❌ | ❌ | **Eksik** |
| Anonim `f:(a,b){...}` | ❌ | ❌ | ❌ | **Eksik** |
| `exf:` extern fonksiyon | ✅ | ⚠️ | ❌ | Parser OK |
| `...` variadic parametreler | ⚠️ | ❌ | ❌ | Token var, parser eksik |
| Çoklu dönüş tipi `!i32, !str` | ❌ | ❌ | ❌ | **Eksik** |
| Fonksiyon işaretçileri / Callback | ❌ | ❌ | ❌ | **Eksik** |

---

## Bölüm 5: Veri Yapıları (Spec §7)

| Yapı | Parser | Semantic | Codegen | Durum |
|------|--------|----------|---------|-------|
| `struct:Name {}` | ✅ | ✅ | ❌ | Parser+semantic iyi |
| `!struct:Name {}` (packed) | ✅ | ⚠️ | ❌ | `is_packed` flag var |
| `enum:Name {}` | ✅ | ✅ | ❌ | İyi |
| Algebraic Enum — `Error(code!u16)` | ✅ | ⚠️ | ❌ | Parser kısmi, veri parse ediliyor |
| `union:Name {}` | ✅ | ✅ | ❌ | İyi |
| `nt:Name = Type` | ✅ | ✅ | ❌ | İyi |
| `nt:struct:Name {}` | ✅ | ⚠️ | ❌ | Parser destekliyor |
| `group Name {}` (OOP) | ✅ | ⚠️ | ❌ | Alt-group, `self`, `?=>` default eksik |

---

## Bölüm 6: Inline Assembly (Spec §9)

| Özellik | Parser | Codegen | Durum |
|---------|--------|---------|-------|
| `fastexec { ... }` blok | ✅ | ❌ | Parser block olarak okuyor, fastexec özel değil |
| `asm: LABEL { ... }` | ✅ | ❌ | Parser label+block oluşturuyor |
| `asmcall("LABEL")` | ⚠️ | ❌ | Token var (`TOKEN_KW_ASMCALL`) |
| `asmjmp("LABEL")` | ⚠️ | ❌ | Token var (`TOKEN_KW_ASMJMP`) |
| `%variable` substitution | ❌ | ❌ | **Tamamen eksik** |
| `%var:reg` register mapping | ❌ | ❌ | **Tamamen eksik** |


---

## Bölüm 7: Core Fonksiyonlar (Spec §10)

| Fonksiyon | Lexer Token | Parser AST | Semantic | IR |
|-----------|-------------|-----------|----------|-----|
| `prnt()` / `prmt()` | ✅ | ✅ | ⚠️ | ❌ |
| `echo()` | ✅ | ✅ | ⚠️ | ⚠️ |
| `peek()` / `poke()` | ✅ | ✅ | ❌ | ❌ |
| `inb()` / `outb()` | ✅ | ✅ | ❌ | ❌ |
| `irq()` / `intr()` | ✅ | ✅ | ❌ | ❌ |
| `reg()` | ✅ | ✅ | ❌ | ❌ |
| `area()` / `zone()` / `free()` | ✅ | ✅ | ❌ | ❌ |
| `typeof()` / `sizeof()` / `len()` | ✅ | ✅ | ⚠️ | ❌ |
| `panic()` / `exit()` | ✅ | ✅ | ❌ | ❌ |
| `cast()` / `swap()` | ✅ | ✅ | ❌ | ❌ |
| `defer {}` | ✅ | ✅ | ❌ | ❌ |

| `date()` / `time()` / `clock()` | ✅ | ✅ | ❌ | ❌ |

> **Sonuç:** Tüm core fonksiyonlar token+AST düzeyinde tanımlı. Ancak **hiçbirinin gerçek semantik kontrolü veya kod üretimi yapılmıyor**.

---

## Bölüm 8: Modül Sistemi (Spec §11)

| Özellik | Durum |
|---------|-------|
| `use <module>` | ✅ Parser (`parse_use`), basit modül yükleme |
| `use <module> as <alias>` | ✅ Parser destekliyor |
| `exp` / `pup` (export/public) | ✅ Flag'ler var |
| `import("path.dll")` | ❌ **Tamamen eksik** |
| `dllexp:` | ❌ **Tamamen eksik** |
| `exf:` extern fonksiyon tanımı | ✅ Parser var |
| `!!=` Directive / `.target` | ❌ **Tamamen eksik** — sadece token |
| Macro sistemi (`:macro!`) | ❌ **Tamamen eksik** |

| `no:CoreModule` | ⚠️ `TOKEN_KW_NO` var, kullanım yok |

---

| `(name)>{...}` Thread block | ✅ | ✅ | ❌ | Parser var (`parse_thread_block`) |

---

## Bölüm 10: Codegen & Optimizer Pipeline

### Codegen Mimarisi
- ✅ **V-Table** pattern ile `ArchOps` soyutlaması — çok iyi tasarım
- ✅ **x86-64** (Win64 + SysV ABI) tam implementasyon
- ✅ **ARM64** taslak implementasyon
- ✅ Codegen `.nxir` dosyasından IR okuyarak çalışıyor 
- ❌ Nexus Flow operatörlerinin assembly karşılıkları yok
- ❌ `struct`, `enum`, `union` bellekte düzenleme yok
- ❌ `handler` / `zone` / `area` bellek yönetimi yok

### Optimizer
- ✅ 14 optimizasyon pass'i — kapsamlı ve iyi yapılandırılmış
- ✅ Graph Coloring register allocation
- ✅ Constant Folding, Dead Code Elim., CSE, GVN, Inline Expansion

---


---

## 📊 Spesifikasyon Kapsam Matrisi

```
████████████████████████████████████████  LEXER (%92)
██████████████████████████████            PARSER (%78)
████████████████████████████████          AST (%80)
██████████████████████                    SEMANTIC (%65)
█████████████                             IR GEN (%45)
█████████████████████████████████████     OPTIMIZER (%90)
████████████████████                      CODEGEN (%55)
████                                      STDLIB (%10)
```

---

## 🎯 Öncelikli Aksiyon Planı

### P0 — Kritik (Spesifikasyonun çekirdeği)

1. **Auto-Scaling Tipleme** — Semantic'te sayısal literallerin otomatik tip çıkarımı
2. **`rules` blokları** — switch/match alternatifi, spesifikasyonun ayırt edici özelliği
3. **`map<K,V>` generic desteği** — Lexer'dan başlayarak tam pipeline
4. **Lambda `f:(a,b)> expr`** — Functional programming temeli
5. **Handler/Masa semantiği** — Ownership ve move semantiği

### P1 — Yüksek Öncelik

6. **`!!=` Directive sistemi** — `.target`, `:macro!`, koşullu derleme
7. **DLL import/export** — `import()`, `dllexp:`
8. **Inline ASM değişken ikamesi** — `%var`, `%var:reg`, `# clobber:`
9. **`unroll(c)` compile-time loop açma**
10. **`Vec2`/`Vec3`/`Vec4` SIMD. avx tipleri**

### P2 — Orta Öncelik

11. Asenkron runtime (`spawn`, `listen`, `done` gerçek implementasyonu)
12. `zone` / `area` bellek yönetimi
13. Multi-return type desteği

---




> **Sonuç:** Derleyici iskelet yapısı sağlam. Lexer ve parser spesifikasyonun büyük bölümünü tanıyor. 
Ancak **semantic analiz**, **IR üretimi** ve **codegen** katmanlarında 
spesifikasyondaki özelliklerin çoğu henüz "kabuk" düzeyinde — token tanınıyor ama **anlam üretilmiyor**. 

En kritik eksikler `rules`, `macro`, auto-scaling tipleme ve handler/ownership semantiğidir.


15. CoreModule standart kütüphanesi
14. `style` sistemi (ANSI renkli çıktı)



## Bölüm 9: Asenkron / Threading (Spec §Bölüm 2-3 STDLIB)

| Özellik | Token | AST | Semantik / Runtime | Durum |
|---------|-------|-----|--------------------|-------|
| `spawn()` | ✅ | ✅ | ❌ | Sadece tanım |
| `listen()` / `!listen()` | ✅ | ✅ | ❌ | Parser `!(listen)` ayrıştırıyor |
| `done()` | ✅ | ✅ | ❌ | Sadece tanım |
| `send()` / `receive()` | ✅ | ✅ | ❌ | Sadece tanım |
| `flow.push/pull/sync/lock` | ❌ | ❌ | ❌ | **Tamamen eksik** |



| `ipc` modülü / `pipe()` / `fork()` | ❌ | ❌ | ❌ | **Tamamen eksik** |
