---

# 🔹 NexusFlow Scan + Select Syntax Spec (Markdown)

## 1. Genel Mantık

* `select(expr)` → ana seçim bloğu. İlk eşleşen branch çalışır.
* `scan expr` → enum / sealed type / struct pattern tarayıcı. Branchler pattern binding ile çalışır.
* `default -> { ... }` → hiçbir branch eşleşmezse çalışır.
* Kod blokları `{ ... }` içindeyken masalar `(h)` veya değişkenler kullanılır, blok sonunda otomatik silinir.
* Expression yok, tamamen statement tabanlı, minimal ve okunabilir.

---

## 2. Keyword & Operator Tablosu

| **Keyword / Operator**                | **Tip / Tür**     | **Kullanım**                                                          | **Açıklama / Örnek**                             |                                     |
| ------------------------------------- | ----------------- | --------------------------------------------------------------------- | ------------------------------------------------ | ----------------------------------- |
| `select(expr)`                        | Statement         | `select(choice) { ... }`                                              | Top-level seçim. Jump table optimize edilebilir. |                                     |
| `val1                                 | val2              | val3 -> { ... }`                                                      | Branch                                           | Birden çok literal değerle eşleşme. |
| `start..end -> { ... }`               | Branch            | Range eşleşmesi.                                                      |                                                  |                                     |
| `default -> { ... }`                  | Branch            | Hiçbir case eşleşmezse çalışır.                                       |                                                  |                                     |
| `scan expr`                           | Statement         | `scan enum_var { ... }`                                               | Enum / sealed type tarayıcı.                     |                                     |
| `TypeName(var...) -> { ... }`         | Pattern           | Enum destructuring veya struct binding.                               |                                                  |                                     |
| `else -> { ... }`                     | Default Pattern   | Taranan enum veya sealed type hiç eşleşmezse çalışır.                 |                                                  |                                     |
| `when cond -> { ... }`                | Guard             | Pattern ile birlikte koşul ekleme.                                    |                                                  |                                     |
| `bind(var)`                           | Pattern binding   | Enum veya struct içindeki değerleri bağlar.                           |                                                  |                                     |

---

## 3. Örnek Kullanım: Scan + Select + Multi Pattern

```nexus
# Sealed type / enum tanımı
nt:enum: Task {
    IO(file!str)
    Compute(a!i32, b!i32)
    Network(url!str)
    Cleanup
}

# Örnek veri
v:current_task!Task = Compute(10, 20)
v:choice!i32 = 1

# Select ile yüksek seviyede seçim
select(choice) {

    # Giriş işlemleri: scan ile enum branch
    1 | 2 -> scan current_task {

        IO(f) -> {
            (h) <- file.open(f, r) ?-> !!("Dosya açılamadı")
            (h) -> file.read >> data
            echo("Dosya okundu: {f}")
        }

        Compute(a,b) when a>0 && b>0 -> {
            v:result = a + b
            echo("Compute sonucu: {result}")
        }

        Network(url) -> {
            (h) <- http.open(url) ?-> !!("Ağa bağlanılamadı")
            (h) -> http.download >> content
            echo("İçerik indirildi: {url}")
        }

        Cleanup -> {
            echo("Temizlik işlemi yapılıyor")
        }

        # Herhangi bir enum eşleşmezse
        else -> echo("Bilinmeyen task tipi")
    }

    # Hızlı path seçimi
    3..5 -> {
        echo("Fast path branch")
    }

    # Tüm diğer durumlar
    default -> {
        echo("Varsayılan branch çalıştı")
    }
}
```

**Özellikler:**

* Enum / sealed type exhaustiveness kontrolü
* Pattern binding: `Compute(a,b)`, `Network(url)`
* Guard koşulu: `when a>0 && b>0`
* Masalar `(h)` kullanımı, blok sonunda temizlenir
* Default branch ile tüm catch-all durumları kapsar

---

## 4. Örnek: Loop + Scan + Pattern + Default

```nexus
v:loop_tasks!Task[] = [Compute(1,2), IO("a.txt"), Cleanup]

scan loop_tasks {

    Compute(a,b) -> loop(i, 0..2) {
        echo("Compute loop: {i}, değerler: {a}+{b}")
    }

    IO(f) -> {
        (h) <- file.open(f,r) ?-> !!("Dosya açılamadı")
        (h) -> file.read >> data
        echo("Dosya okundu: {f}")
    }?;

    Cleanup -> echo("Temizlik yapılıyor")

    else -> echo("Bilinmeyen görev")
}
```

* `scan` ile array üzerinden tarama
* Pattern binding ve loop birleştirilmiş
* `else` ile bilinmeyen tip yakalanmış
* Masalar ve IO operasyonları blok bazlı

---

## 5. Minimal ve Gerçekçi Syntax Özeti

```nexus
select(expr) {
    val1 | val2 -> { ... }
    start..end -> { ... }
    scan enum_var {
        TypeName(var...) -> { ... }
        TypeName2(...) when cond -> { ... }
        else -> { ... }
    }
    default -> { ... }
}
```

* `scan` + `select` kombinasyonu, multi-rule engine gibi çalışabilir
* Kod blokları çalıştırılır, expression-free
* Pattern / destructuring / guard destekli
* Minimal, okunabilir, yüzlerce satır işleyebilir

---

