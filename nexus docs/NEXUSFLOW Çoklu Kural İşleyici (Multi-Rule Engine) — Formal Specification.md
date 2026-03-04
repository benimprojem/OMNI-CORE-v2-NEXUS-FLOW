
# 5.3.2 NEXUSFLOW: Çoklu Kural İşleyici (Multi-Rule Engine) — Formal Specification

## 1. Tanım

**NexusFlow**, çoklu kuralların (**multi-rule**) tek bir fonksiyon üzerinde uygulanmasını sağlayan bir kural motorudur. Bu yapı,:

* Fonksiyonların ön koşullarını kontrol eder (**barrier rules**)
* Koşullara göre aksiyonları tetikler (**trigger rules**)
* Varsayılan veya her zaman çalışacak kurallar tanımlar (**default / always**)

Kural motoru, hem **sıralı hem de bağımsız kurallar** ile çalışabilir. Fonksiyon içinde `apply rules(<RuleSet>)` ile kural seti inject edilir ve tetikleyici bloklar `on <RuleName>.<Trigger>` ile yakalanır.

---

## 2. Sözdizimi (Syntax)

```nexus
rules <RuleName>(param1, param2, ...) {
    # 1. BARRIER RULES: Koşul sağlanırsa fonksiyon durur ve hata mesajı verir
    [koşul] :=> "Hata Mesajı";

    # 2. TRIGGER RULES: Koşul sağlanırsa aksiyon tetiklenir
    [koşul] -> [TriggerName];

    # 3. ÖZEL DURUMLAR / DEFAULT:
    always -> [Aksiyon];   # Her durumda çalışır
    default -> [Aksiyon];  # Hiçbir koşula girmezse çalışır
}
```

* **`:=>`** → Barrier (durdurucu) kural, koşul sağlanırsa **fonksiyonun çalışması durur** ve mesaj döner.
* **`->`** → Tetikleyici kural, koşul sağlanırsa ilgili aksiyon/trigger devreye girer.
* **`always`** → Her durumda çalışır, opsiyonel.
* **`default`** → Hiçbir koşul eşleşmezse devreye girer, opsiyonel.

---

## 3. Kural Setleri ve Fonksiyon Enjeksiyonu

Kurallar, bir fonksiyona `apply rules(<RuleSet>)` ile inject edilir:

```nexus
f:executeTask(w, h, t) {
    apply rules (GeometryCheck(w, h));
    apply rules (SafetyCheck(t));

    # Tetikleyici bloklar (on-blocks)
    on GeometryCheck.SquareMode { ... aksiyon ... }
    on SafetyCheck.Overload { ... aksiyon ... }
}
```

* `apply rules()` → Kurallar sırayla uygulanır.
* `on <RuleName>.<Trigger>` → Kural tetiklendiğinde çalışacak aksiyon.

---

## 4. Kural İşleyici Mantığı

### 4.1 Barrier Rule (Durdurucu)

```nexus
[w > 2800] :=> "Genişlik sınırı aşıldı";
[h > 2100] :=> "Yükseklik sınırı aşıldı";
```

* Koşul doğru ise fonksiyon **anında durur**.
* Mesaj kullanıcıya veya log sistemine iletilir.
* Barrier kurallar, trigger kurallardan **önceliklidir**.

### 4.2 Trigger Rule (Yönlendirici)

```nexus
(w == h) -> SquareMode;
tool == "Diamond" -> HighSpeed;
```

* Koşul doğru ise **tetikleyici ismi** kaydedilir.
* Fonksiyon normal akışına devam eder.
* Tetikleyici aksiyonu `on <RuleName>.<Trigger>` ile çalıştırılır.

### 4.3 Varsayılan / Always

```nexus
always -> LogMetrics;
default -> StandardSettings;
```

* `always` → her durumda çalışır.
* `default` → hiçbir trigger koşulu çalışmazsa devreye girer.

---

## 5. Multi-Rule Execution Flow

1. Fonksiyon başlar.
2. `apply rules(<RuleSet>)` çağrılır.
3. Barrier kurallar (:=>) önce değerlendirilir.

   * Eğer bir barrier koşulu sağlanırsa, fonksiyon durur ve mesaj döner.
4. Trigger kurallar (->) değerlendirilir.

   * Koşul sağlanırsa trigger kaydedilir.
5. Fonksiyon normal akışa devam eder.
6. `on <RuleName>.<Trigger>` blokları tetiklenir.
7. `always` ve `default` opsiyonları çalıştırılır.

---

## 6. Örnek: Asal Sayı Kontrolü (PrimeBorders)

```nexus
rules PrimeBorders(n) {
    n < 2  :=> "Sınır İhlali: 2'den küçük asallık olmaz"; 
    n == 2 -> Success;
    n == 3 -> Success;
    
    ( (n + 1) % 6 == 0 || (n - 1) % 6 == 0 ) :=> "Asal Değil: Tünel Dışı";

    default -> Success;
}

f:isPrime(n) {
    apply rules(PrimeBorders(n));

    on PrimeBorders.Success { return true; }

    i = 5;
    loop (i * i <= n) {
        (n % i == 0 || n % (i + 2) == 0) -> { return false; }
        i = i + 6;
    }

    always -> return true;
}

isPrime(17); # Çalıştırma örneği
```

* Barrier kurallar ile 2’den küçük sayılar engellenir.
* 6n ± 1 kuralı ile tünel dışı sayılar önceden filtrelenir.
* Trigger → Success → true döner.
* Döngü ile kalan sayılar taranır ve sonuç döndürülür.

---

## 7. Optimizasyon Önerileri

* **Kuralların sıralaması**: Önce barrier kurallar, sonra trigger kurallar → erken çıkış.
* **Trigger caching**: Aynı kural birden fazla fonksiyonda kullanılacaksa, tetikleyici önceden cachelenebilir.
* **Loop unification**: Asal sayı örneğinde olduğu gibi **tek bir döngü ile çift koşul kontrolü** → performans artışı.
* **Multi-Rule Parallelism**: Kurallar birbirinden bağımsızsa, paralel evaluate edilebilir → CPU / Thread optimizasyonu.
* **Default ve Always blokları** → ayrı bir pipeline ile asenkron olarak çalıştırılabilir.

---

## 8. Multi-Rule ve Group Entegrasyonu

NexusFlow kural setleri **Group Metodu** ile entegre edilebilir:

```nexus
group CNC {
    rules Safety => group {
        widthCheck => f:(w) { w>2800 :=> "Genişlik aşıldı"; }
        heightCheck => f:(h) { h>2100 :=> "Yükseklik aşıldı"; }
        default { echo("Güvenlik kontrolleri tamam"); }
    }

    rules Performance => group {
        toolCheck => f:(tool) { tool=="Diamond" -> HighSpeed; }
        default { StandardSettings; }
    }
}
```

* `apply rules(CNC.Safety)` → tüm safety kontrollerini çalıştırır.
* `apply rules(CNC.Performance)` → performans tetikleyicilerini çalıştırır.
* Group sayesinde **kural setleri organize edilir ve okunabilirlik artar**.

---
