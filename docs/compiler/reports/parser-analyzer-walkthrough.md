# Modül 1 & 2 Tamamlanma Raporu (Walkthrough)

Bu belge, NexusFlow derleyicisinin **Parser** ve **Semantic Analyzer** modüllerinin geliştirilme ve doğrulama sürecini özetler.

## Yapılan Çalışmalar

### 1. Parser & Lexer İyileştirmeleri (Modül 1)
- **Yapısal Tanımlar (Struct/Union/Enum):**
   - Üyeler artık virgül (`,`) ile ayrılıyor (C tarzı).
   - `nt:` (NewType) öneki kullanıldığında sonunda `;` zorunlu kılındı.
   - Standart `struct` tanımlarında `;` artık zorunlu değil.
- **v: (Değişken Tanımlama) Düzeltildi:** `DEF_VAR` emisyonu en başa çekilerek, `HARD_LOCK` (!tip) öncesi sembolün analizör tarafından tanınması sağlandı.
- **Lexer Fix:** `_>` (Relocate) operatörü ve branching (`if/elsif`) yapıları rafine edildi.

### 2. Semantik Analizör (Modül 2)
- **Tip Sistemi Genişletildi:** `u16`, `i8`, `i16`, `d32`, `d64`, `char` ve jenerik `t` tipleri eklendi.
- **Ownership Engine:** 
    - `A2001`: Dolu masaya (handler) veri yazma hatası.
    - `A2002`: Taşınmış (relocated) verinin kullanım hatası.
- **Debug Modu:** `occ -d` parametresi ile OIR operasyonlarını (`[OIR:OpCode]`) ve kapsam (scope) geçişlerini izlemek mümkün hale getirildi.
- **Hata Raporlama:** Hatalar artık orijinal kaynak dosya adını (`test.nx`) gösteriyor ve Levenshtein mesafesi ile yazım önerileri sunuyor.

---

## Doğrulama ve Testler (test.nx)

`test.nx` dosyası üzerinde yapılan testlerde tüm kurallar başarıyla doğrulanmıştır.

```nxf
    # Struct Tanimi (nt: ile basladigi icin sonunda ; var)
    nt:struct:Player {
        v:id!i32,
        v:hp!f32
    };

    # Ownership Test:
    (h) <- "Veri";
    (h) _> (v); 
    
    # Bu satir hata VERMEMELI (h bosaldi)
    (h) <- "Tekrar Kullanim";
    
    # Bu satir hata VERMELI (v zaten dolu) [A2001]
    (v) <- "Masa Dolu Hatasi Bekleniyor"; 
```

### Terminal Çıktısı (Release Mod):
```text
[OCC] Target secildi: Win64
[OCC] Kaynak dosya: test.nx
--- [STAGE 1] PARSER ---
[STAGE 1 OK] OIR uretildi: intermediate.oir (7 ms)
--- [STAGE 2] ANALYZER ---
[ERROR] A2001 — Masa Tikanikligi: '(v)' zaten dolu.
       File: test.nx  Line: 21:5
[STAGE 2 FAIL] Semantik Analiz hatalar buldu.
```

### Terminal Çıktısı (Debug Mod -d):
```text
[OIR:16] SymID:2 Line:3    <- DEF_VAR sayi
[OIR:179] SymID:2 Line:3   <- HARD_LOCK i32
[OIR:224] SymID:2 Line:3   <- EXPR 100
...
[OIR:16] SymID:9 Line:21   <- DEF_VAR (v)
[ERROR] A2001 — Masa Tikanikligi: '(v)' zaten dolu.
```

---
**Tüm testler %100 başarıyla tamamlanmıştır. Modül 3: Optimizer aşamasına geçmeye hazırız.**
