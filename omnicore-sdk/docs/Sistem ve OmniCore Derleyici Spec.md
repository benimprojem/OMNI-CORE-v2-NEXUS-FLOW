
# **NexusFlow Sistem ve OmniCore Derleyici Spec**

## 1. Proje Yönetimi ve CLI (`ocm`)

### 1.1 Proje Başlatma

```bash
ocm init <proje_adı>
```

Oluşan klasör yapısı:

```
/proje_adı
  ├── ocm.ocf           # Proje metadata ve bağımlılık haritası
  ├── src/              # Kaynak kodlar
  │   └── main.nx      # Ana giriş noktası
  ├── lib/              # Yerel gruplar ve modüller
  ├── build/            # Derleme çıktıları (Git-ignored)
  └── tests/            # Test senaryoları
```

### 1.2 Workspace / Çoklu Proje Yönetimi

* `ocm.ocf` içinde workspace ve bağımlılıkları tanımlanır.
* Örnek:

```toml
[package]
name = "occ"
version = "0.2.0"
edition = "2026"
author = "DissConnecTed"

[package.metadata]
date = "12-01-2026"
info = "Omni Core Nexus Flow"
language = "en"

[include]
./lib

[dependencies]

flags = ["-O2", "-march=native"]
default_macros = ["SafeFlow", "OS_Bridge"]
default_libs = ["msvcrt.exf"]
```

---

## 2. Derleme Aşamaları

| Aşama          | Çıktı         | Açıklama                       |
| -------------- | ------------- | ------------------------------ |
| `-ast`         | `.ast`        | Soyut sözdizimi ağacı          |
| `-ir`          | `.ir`         | Intermediate Representation    |
| `-asm`         | `.asm/.s`     | Mimariye özgü Assembly         |
| `-obj`         | `.o/.obj`     | Bağlanmaya hazır nesne dosyası |
| `--target lib` | `.dll/.so/.a` | Dinamik veya statik kütüphane  |
| `--target bin` | `.exe/.elf`   | Çalıştırılabilir dosya         |

---

## 3. Hata Raporlama

* **Error Aggregation:** Derleyici tüm dosyaları tarar ve tüm hataları listeler.
* Örnek:
```
[E042] Hata: 'User' yapısında 'yaş' alanı bulunamadı.
       Dosya: src/main.nxf Satır: 45:12
       Öneri: Belki 'age' yazmak istediniz? (Benzerlik: %85)

[W012] Uyarı: 'temp' değişkeni tanımlandı ama hiç kullanılmadı.
       Dosya: src/logic.nxf Satır: 12:5
       İpucu: Bellek tasarrufu için bu satırı silebilirsiniz.

```

---

## 4. Derleyici İç İşleyişi

### 4.1 Lexical Analysis (Lexer)

* Multi-threaded, SIMD destekli tokenizasyon.
* Yorumları ayrıştırır ve dokümantasyon belleğine alır.

### 4.2 Parsing / AST

* LALR(1) gramer kullanır.
* Syntax hataları ayrıntılı mesajlarla bildirilir.

### 4.3 Semantik Analiz & Tip Kontrol

* Scope resolution
* Ownership check (masa ve handler ömürleri)
* Tip çıkarımı (`v:x = 5` → i32)

### 4.4 Omni-IR

* SSA (Static Single Assignment) kullanır.
* Optimizasyon için mimari bağımsız ara dil.

### 4.5 Optimizasyon Katmanları

1. **HLO:** Inlining, Constant Folding, Loop Unrolling
2. **Middle-Level:** DCE, Common Subexpression Elimination
3. **LTO:** Cross-module optimizasyon

### 4.6 Register Allocation

* Graph Coloring algoritması
* Spilling minimize ediliyor

### 4.7 Code Generation

* Mimariye özgü komut seçimi
* Peephole optimizasyon

### 4.8 Linker

* Granular / Function-Level Linking
* Dead-Code Elimination
* Section-per-function ve dependency graph
* CoreModule için static inlining + LTO

---

## 5. Cross-Compiling

* `.target` dosyaları okunur ve Talimat Tablosu oluşturulur.
* Kernel yazımında `vector_table` kuralları geçerlidir.
* OS bağımlı ve bare-metal farkları dikkate alınır.

| Özellik       | Bare-Metal              | OS-dependent             |
| ------------- | ----------------------- | ------------------------ |
| Giriş Noktası | `_start`/`reset_vector` | `main`/`WinMain`         |
| Adresleme     | Sabit Fiziksel Adres    | Göreceli Sanal Adres     |
| Kütüphane     | Minimal / sadece IRQ    | OS API sarmalayıcıları   |
| Fazlalık      | %0                      | Minimal OS loader header |

---
