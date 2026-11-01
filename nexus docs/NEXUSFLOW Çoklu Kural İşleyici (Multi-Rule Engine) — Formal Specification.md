
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

* Koşul doğruysa hata kaydına eklenir. RuleSet içindeki diğer bariyerler de değerlendirilir; aşama sonunda bir veya daha fazla hata varsa fonksiyon durdurulur ve tüm mesajlar birlikte raporlanır.
* Mesaj kullanıcıya veya log sistemine iletilir.
* Barrier kurallar trigger kurallardan önce değerlendirilir. Bariyer hatası varsa trigger aşamasına geçilmez.

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

* **Kuralların sıralaması**: Önce tüm barrier kuralları değerlendirilir ve hatalar topluca raporlanır; hata yoksa tüm trigger kuralları değerlendirilir.
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


---

## 9. Kesinleştirilmiş Çalışma Semantiği

Bu bölüm, Multi-Rule Engine davranışını normatif olarak tanımlar. Örneklerdeki eski "ilk bariyerde dur" ifadesi bu kuralla değiştirilmiştir.

### 9.1 Bariyerlerin toplu değerlendirilmesi

Bir RuleSet içindeki **bütün barrier kuralları** değerlendirilir. Bir bariyerin koşulu doğru olduğunda:
- İlgili hata kaydı (RuleSet adı, kural sırası/kimliği ve mesaj) hata listesine eklenir.
- Diğer bariyerler değerlendirilmeye devam edilir.
- Bariyer aşaması tamamlandığında hata listesi boş değilse RuleSet başarısız olur.
- Başarısız RuleSet için hiçbir trigger değerlendirilmez ve hiçbir `on` handler çalıştırılmaz.
- Fonksiyonun normal akışı durdurulur; toplanan hata kayıtlarının tamamı çağırana/üst hata mekanizmasına sunulur.

Tek bir bariyer hatası da birden fazla bariyer hatası da aynı şekilde akışı durdurur. Fark, raporlanan hata sayısıdır. Bariyer değerlendirmesi sırasında yan etkili ifadeler kullanılmamalıdır; derleyici/analyzer mümkün olduğunda bariyer koşullarının salt-okunur olmasını doğrulamalıdır.

### 9.2 Trigger'ların toplu değerlendirilmesi

Bariyer aşaması hatasız tamamlanırsa RuleSet içindeki **bütün trigger koşulları** değerlendirilir. Her doğru koşulun trigger adı TriggerSet'e eklenir. Birden fazla koşul doğruysa hepsi kaydedilir; ilk eşleşmede durulmaz.

TriggerSet, aynı RuleSet uygulaması için geçerlidir. Aynı isimli trigger birden fazla kez tanımlanırsa derleme/analyzer hatası verilir; böylece handler çözümlemesi belirsiz olmaz.

### 9.3 Birden fazla apply rules

`apply rules(...)` çağrıları kaynak kod sırasına göre yürütülür. Her çağrı kendi RuleSet değerlendirmesini tamamlar. Bir çağrıda bariyer hatası oluşursa:
- O çağrının tüm bariyer mesajları raporlanır.
- Sonraki `apply rules` çağrıları çalıştırılmaz.
- Fonksiyonun normal gövdesine geçilmez.

Önceki RuleSet başarılıysa sonraki RuleSet değerlendirilir. Her RuleSet kendi parametre bağlamına ve TriggerSet'ine sahiptir.

### 9.4 on handler çözümlemesi ve çalışma noktası

`on RuleName.TriggerName { ... }` blokları derleme sırasında ilgili RuleSet/trigger çiftiyle eşleştirilir. Handler'ın kaynak kodda `apply rules` satırından sonra yazılması, handler'ın daha geç çalışacağı anlamına gelmez: eşleşen handler, ilgili `apply rules` değerlendirmesi başarılı olduktan sonra, o çağrının parçası olarak çalıştırılır.

- Yalnızca TriggerSet içinde bulunan trigger'ların handler'ları çalışır.
- Birden fazla trigger eşleşmişse bunların handler'ları tanımlı kaynak sırasına göre çalıştırılır.
- Eşleşen trigger için handler tanımlanmamış olması hata değildir; trigger gözlemlenebilir/işlenebilir bir olay olarak kalır.
- Aynı RuleSet ve trigger çifti için birden fazla `on` handler tanımlanması bu sürümde analyzer hatasıdır.

### 9.5 always ve default

`always` ve `default` kuralları isteğe bağlıdır; RuleSet bunlardan birini, ikisini veya hiçbirini içerebilir.

- `always -> Action;`: Bariyer aşaması başarılı olan her RuleSet uygulamasında çalışır.
- `default -> Action;`: Hiçbir trigger koşulu eşleşmediğinde çalışır.
- `default`, bir trigger eşleşmişse çalışmaz.
- Bu kuralların bulunmaması varsayılan bir işlem üretmez.
- Bu sürümde her RuleSet için en fazla bir `always` ve bir `default` tanımına izin verilir; tekrarları analyzer hatasıdır.

Çalışma sırası: bariyerlerin tamamı → trigger koşullarının tamamı → eşleşen `on` handler'ları → varsa `always` → koşulu sağlanmışsa varsa `default`. Bariyer hatası varsa bu zincir ilk aşamada kesilir.

### 9.6 AST temsili

Parser, kaynak sözdizimini aşağıdaki düğüm ailelerine dönüştürür:

```text
AST_RULESET {
    Name
    Parameters[]
    Rules[]
}

AST_BARRIER {
    Condition
    ErrorMessage
    SourceOrder
}

AST_TRIGGER {
    Condition
    TriggerName
    SourceOrder
}

AST_ALWAYS  { Action, SourceOrder }
AST_DEFAULT { Action, SourceOrder }

AST_APPLY_RULES {
    RuleSetReference
    Arguments[]
    SourceOrder
}

AST_ON {
    RuleSetReference
    TriggerName
    Body
    SourceOrder
}
```

Parser yalnızca yapıyı kurar. İsim çözümleme, argüman/tip uyumu, yinelenen isimler ve handler eşleşmeleri Analyzer aşamasında doğrulanır.

### 9.7 Analyzer ve OIR lowering

Analyzer en az şu kontrolleri yapar:
- RuleSet adı ve parametreleri çözülmüş olmalı; argüman sayısı ve tipleri eşleşmeli.
- Barrier koşulları ve trigger koşulları boolean olmalı.
- Barrier mesajı geçerli bir mesaj/string ifadesi olmalı.
- Aynı RuleSet içinde trigger adları benzersiz olmalı.
- `on` referansı var olan RuleSet ve trigger adına bağlanmalı.
- Aynı RuleSet/trigger için birden fazla handler olmamalı.
- `always` ve `default` en fazla birer kez bulunmalı.

OIR'de RuleSet uygulaması normal kontrol akışına açık bloklar olarak indirilir. Temel şema:

```text
ApplyRules(RuleSet, Arguments):
    errors = []
    for each barrier in source order:
        if barrier.condition:
            errors.append(barrier.error)

    if errors is not empty:
        ReportAll(errors)
        terminate current function path

    triggers = []
    for each trigger in source order:
        if trigger.condition:
            triggers.append(trigger.name)

    for each matched handler in source order:
        execute handler

    if always exists:
        execute always action

    if triggers is empty and default exists:
        execute default action

    continue
```

Bu şema semantik akışı gösterir; gerçek OIR'de döngüsel pseudo-code değil, koşul başına BRANCH, hata biriktirme işlemleri, handler bloklarına JUMP/BRANCH ve gerekli merge blokları üretilir. Bariyer hataları tek bir erken çıkışta birleştirilir; hata listesi fonksiyonun normal dönüş değerinden ayrı bir kontrol/hata sonucu olarak taşınır.

### 9.8 Group entegrasyonu

Bu dokümandaki `rules Name(params) { ... }` sözdizimi temel RuleSet biçimidir. `group` içindeki `rules Safety => group { ... }` biçimi ayrı bir üst seviye organizasyon sözdizimidir. İkisi aynı AST düğümüymüş gibi ele alınmaz; Group çözümlemesi, temel RuleSet'lere bağlanan ayrı bir frontend/lowering adımı olarak tanımlanmalıdır. Group biçiminin tam grameri ayrıca netleştirilmeden temel RuleSet gramerine karıştırılmamalıdır.
