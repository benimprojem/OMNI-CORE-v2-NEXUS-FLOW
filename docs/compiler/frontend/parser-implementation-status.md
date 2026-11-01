# NexusFlow Syntax & Parser Implementation Status
**Versiyon:** 1.0 (OIR v2 Parser Katmanı)

Bu belge, **Modül 1 (Parser DLL)** çerçevesinde C++ ile başarıyla kodlanmış, tokenizasyonu (lexer) ve AST ayrıştırması (parser) tamamlanıp OIR çıktı formatına dönüştürülebilen tüm syntax (sözdizimi) yeteneklerini özetler.

---

## 1. Temel Tanımlayıcılar (Identifiers & Prefixes)
NexusFlow değişken ve yapı tanımlamalarında ön ekler (prefix) kullanır. Aşağıdaki yapılar tamamen desteklenmektedir:

- `v:` Değişkenler (Mutable)
- `c:` Sabitler (Constant)
- `m:` Must / Bağımlılık (şimdilik parser token olarak tanır)
- `f:` Fonksiyonlar
- `exf:` Extern fonksiyonlar
- `r:` Rules (Kurallar)
- `nt:` NewType / Struct / Enum / Union bağlamaları

**Tip Zorlama (Hard-Lock):** `!type` biçiminde tip dayatması desteklenir. 
Örnek: `v:sayi!i32 = 10;`, `v:data!u8[]`

---

## 2. Tipler (Primitive & Complex Types)
Tüm temel tip anahtar kelimeleri parser tarafından tanınmakta ve analizöre doğru tip OIR kodlarıyla aktarılmaktadır:
- **Sayısal:** `i8..i64`, `u8..u64`, `f32, f64`, `d32, d64`
- **İlkel:** `bit`, `byte`, `hex`, `char`, `str`, `bool`
- **Jenerik/Sanal:** `t` (generic), `fptr`
- **Oyun/Grafik (SIMD):** `Vec2`, `Vec3`, `Vec4`
- **Koleksiyonlar:** Dinamik Array `[]`, Statik Array `[N]`, Harita `map str, i32`

---

## 3. Akış ve Handler Operatörleri ("The 15 Keys")
Aşağıdaki tüm özel NexusFlow 15-Key operatörleri, lexer seviyesinde tokenleştirilir ve parser'da özel işlem görerek (veya `parseHandleExpression` içinde) derlenip OIR'a yansıtılır:

- `(h)`: Masa/Handler `(OP_REG_HANDLE)`
- `<-`: Yakala (Capture / Bind)
- `->`: Başarı/İleri sür (Pipe)
- `_>`: Taşı (Relocate)
- `?(n, ms)`: Rolling Retry (Zaman Ayarlı Tekrar)
- `?->`: Catch (Saptırıcı)
- `?=>`: Fallback (Alternatif)
- `!->`: Ignore (Sessiz devam)
- `<<`, `>>`: Zone Write ve Flow Feed
- `!!`: Panic
- `!!=`: Derleyici Direktifi
- `}?;`: Halt / Temizlik ve Sonlandırma Terminatorü

---

## 4. Değişken Atamaları ve Yapılar
```nxf
# Standart atama
v:x = 100;
v:pi!f64 = 3.14159;

# Pointer ve Ref (Kısmi sözdizimi ayrıştırma)
v:ptr!*i32;

# Dinamik ve Statik Diziler
v:sayi[];
v:sayilar[5]!i32 = [1, 2, 3, 4, 5];

# Çok boyutlu diziler desteklenmektedir
v:matris[3][3]!f32 = [ ... ];

# Maps (Haritalar)
v:notlar!map str, i32;
```

---

## 5. Fonksiyonlar ve Modüller

**Standart Fonksiyon:**
```nxf
f:topla(a!i32, b!i32)!i32 { 
    return a + b; 
}
```

**Lambda / Tek Satır Fonksiyonlar:**
```nxf
v:kare = f:(a)> a * a;
f:isim_getir(self)> self.ad;
```

**Modül İçe Aktarma (Imports):**
```nxf
use "CoreModule.nxf" as core;
import "NetModule";
```

---

## 6. Döngüler (Unified Loop)
Tek bir `loop` anahtar kelimesi altında 5 farklı varyasyon parser tarafından ayrıştırılır:
```nxf
# 1. Range Loop
loop(1..100) { ... }

# 2. For Loop
loop(i, a < b, i++) { ... }

# 3. Foreach Loop
loop(v:item in list) { ... }

# 4. While Loop
loop(a < b) { ... }

# 5. Infinite Loop
loop { ... }
```
Ayrıca `break;` ve `continue;` ifadeleri döngü içinde OIR loop sonlandırma opcode'larına başarıyla çevrilir.

---

## 7. Dallanma (If / Elsif / Els)
Geleneksel kontrol akışı Nexus kurallarıyla %100 entegre edilmiştir. İçiçe veya art arda bloklar parser tarafından tanınır:

```nxf
if (a == b) {
    (h) <- echo("Esit");
} elsif (a > b) {
    (h) <- echo("Buyuk");
} els {
    (h) <- echo("Kucuk");
}
```
*(Parantezsiz veya blok süslü parantez olmadan tek satır ifadelere de destek sunulmuştur)*

---

## 8. Veri Kalıpları (Struct, Enum, Union, Group)

`struct`, `!struct`, `enum`, `union` tiplerinin ve bunlarla ilişkili metotları gruplandıran `group` yapısının ayrıştırması (parseGroup vb. metotlarla) eksiksizdir:

```nxf
group Oyuncu {
    # Tip tanımı
    nt:struct:Karakter {
        v:ad!str,
        v:puan!i32
    };

    # Group İçi Metot (Anonim binding)
    yeni => f:(isim) { ... }

    # Alt group
    canta => group {
        ac => f:() { ... }
    }
    
    # Default handler
    default { echo("Bilinmeyen çağrı"); }
}
```

---

## 9. Kurallar (Rules ve Multi-Rule Engine)
`rules` blokları, içerisindeki özel `:=>` (Bariyer), `->` (Yönlendirme), `always ->` ve `default ->` durumlarıyla hatasız parse edilebilmekte, sembol ve opcode tablolarına aktarabilmektedir. `apply` anahtar kelimesiyle enjeksiyonlar parser'da ele alınır.

```nxf
rules SafeCheck(w, h) {
    w > 2000 :=> "Sinir asildi";
    (w == h) -> SquareMode;
    always -> Log;
    default -> Normal;
}
```

---

## 10. Inline Assembly ve Derleyici Direktifleri
**Derleyici Direktifi:**
```nxf
!!= target = "Linux64"
!!= link "opengl32"
```

**Gelişmiş Assembly Blokları:**
```nxf
fastexec {
    asm: LABEL_1 {
        mov rax, %a
        add rax, %b
    }
}
asmcall(LABEL_1);
```

---

### Modül 1 Çıktısı (OIR v2.0)
Tüm yukarıdaki kodlar hatasız şekilde binary `*.oir` dosyası olarak sentezlenmektedir. Oluşturulan AST (Abstract Syntax Tree) doğrudan lineer bir Instruction dizilimine (OIR) yapılandırılmakta; bilinmeyen tanımlayıcılar veya yanlış blok sınırlandırmalarında `ErrorList` devreye girerek (Panic-mode recovery destekli) süreci raporlamaktadır.
