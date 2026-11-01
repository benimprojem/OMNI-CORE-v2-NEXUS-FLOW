
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
    [koşul] :=> "Hata Mesajı";
    [koşul] -> TriggerName;

    # İsteğe bağlı:
    always -> LogMetrics;
    default -> StandardMode;

    # Doğrudan aksiyon için blok biçimi:
    always { writeMetrics(); }
    default { applyStandardMode(); }
}
```

- **`:=>`**: Bariyer. Doğru olan tüm bariyerlerin mesajları toplanır; aşama sonunda hata varsa fonksiyon durdurulur.
- **`->`**: Trigger üretir. Eşleşen normal trigger'ların tamamı kaydedilir.
- **`always -> Name;`**: Her başarılı uygulamada isimli trigger üretir.
- **`default -> Name;`**: Hiçbir normal trigger eşleşmediğinde isimli trigger üretir.
- **`always { ... }` / `default { ... }`**: İlgili durumda doğrudan aksiyon bloğu çalıştırır.
- `always` ve `default` isteğe bağlıdır.

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

always {
    writeMetrics();
}

default {
    applyStandardSettings();
}
```

Ok operatörlü biçimde sağ taraf bir trigger adıdır. `always -> LogMetrics;` her başarılı RuleSet uygulamasında; `default -> StandardSettings;` hiçbir normal trigger eşleşmediğinde ilgili trigger'ı üretir. Bu trigger'lar `on RuleName.TriggerName { ... }` ile yakalanabilir.

Blok biçimleri ise trigger üretmez; içlerindeki aksiyonları doğrudan yürütür. Her iki anahtar da opsiyoneldir.

## 5. Multi-Rule Execution Flow

1. Fonksiyon başlar ve `apply rules(<RuleSet>)` çağrılır.
2. RuleSet içindeki bütün bariyer koşulları değerlendirilir; doğru olanların hata mesajları toplanır.
3. Hata listesi boş değilse tüm mesajlar raporlanır ve fonksiyon yolu sonlandırılır.
4. Hata yoksa bütün normal trigger koşulları değerlendirilir; eşleşenlerin tamamı kaydedilir.
5. Eşleşen normal trigger'ların `on` handler'ları kaynak sırasına göre çalıştırılır.
6. Varsa `always` trigger/handler'ı veya doğrudan aksiyon bloğu çalıştırılır.
7. Hiçbir normal trigger eşleşmediyse varsa `default` trigger/handler'ı veya doğrudan aksiyon bloğu çalıştırılır.
8. `apply rules` tamamlanır ve fonksiyonun normal akışı sürer.

## 6. Örnek: Asal Sayı Kontrolü (PrimeBorders)

```nexus
rules PrimeBorders(n) {
    n < 2 :=> "Sınır İhlali: 2'den küçük asallık olmaz";
    n == 2 -> Success;
    n == 3 -> Success;

    # 2 ve 3 dışındaki sayılar 6k-1 veya 6k+1 biçiminde olmalıdır.
    (n > 3 && n % 6 != 1 && n % 6 != 5) :=> "Asal Değil: 6k ± 1 koşulu sağlanmıyor";
}

f:isPrime(n) {
    apply rules(PrimeBorders(n));

    on PrimeBorders.Success { return true; }

    v:i = 5;
    loop (i * i <= n) {
        (n % i == 0 || n % (i + 2) == 0) ? return false;
        i = i + 6;
    }

    return true;
}

isPrime(17); # Çalıştırma örneği
```

- Bariyerler, 2'den küçük değerleri ve 6k ± 1 koşulunu sağlamayan adayları engeller.
- 2 ve 3 için `Success` trigger'ı üretilir; handler doğrudan `true` döndürür.
- Diğer adaylar normal asal kontrolü döngüsüne devam eder.
- Bu örnekte `always` ve `default` kullanılmaz; ikisi de zorunlu değildir.

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
- `always` ve `default` en fazla birer kez bulunmalı; ok biçimi ve blok biçimi aynı anahtar altında birlikte kullanılamaz.

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
        if always is trigger form:
            emit always.trigger_name
            execute its matched handler if present
        else:
            execute always.body

    if normal_triggers is empty and default exists:
        if default is trigger form:
            emit default.trigger_name
            execute its matched handler if present
        else:
            execute default.body

    continue
```

Bu şema semantik akışı gösterir; gerçek OIR'de döngüsel pseudo-code değil, koşul başına BRANCH, hata biriktirme işlemleri, handler bloklarına JUMP/BRANCH ve gerekli merge blokları üretilir. Bariyer hataları tek bir erken çıkışta birleştirilir; hata listesi fonksiyonun normal dönüş değerinden ayrı bir kontrol/hata sonucu olarak taşınır.

### 9.8 Group entegrasyonu

Bu dokümandaki `rules Name(params) { ... }` sözdizimi temel RuleSet biçimidir. `group` içindeki `rules Safety => group { ... }` biçimi ayrı bir üst seviye organizasyon sözdizimidir. İkisi aynı AST düğümüymüş gibi ele alınmaz; Group çözümlemesi, temel RuleSet'lere bağlanan ayrı bir frontend/lowering adımı olarak tanımlanmalıdır. Group biçiminin tam grameri ayrıca netleştirilmeden temel RuleSet gramerine karıştırılmamalıdır.


## 10. Default Trigger Kararı

`default -> TriggerName;` bir doğrudan fonksiyon çağrısı veya serbest metin aksiyonu değildir; RuleSet'e ait bir trigger üretir. Bu trigger, `on RuleSet.TriggerName { ... }` ile yakalanabilir. Örneğin:

```nexus
rules PrimeBorders(n) {
    n < 2 :=> "2'den küçük sayı asal olamaz";
    n == 2 -> Success;
    n == 3 -> Success;
    default -> ContinueCheck;
}

f:isPrime(n) {
    apply rules(PrimeBorders(n));

    on PrimeBorders.Success {
        return true;
    }

    on PrimeBorders.ContinueCheck {
        # Normal asal kontrolüne devam edilir.
    }

    # Devam eden normal fonksiyon akışı...
}
```

Doğrudan bir fallback işlemi isteniyorsa trigger adı yerine blok biçimi kullanılır:

```nexus
default {
    applyStandardSettings();
}
```

Bu ayrım, trigger bildirimi ile doğrudan aksiyon yürütülmesini birbirinden ayırır ve `on` handler modelini tutarlı kılar.


---

## 11. Multi-Rule OIR Control-Flow Tasarımı

Bu bölüm, §9.7'deki kavramsal lowering şemasını gerçek CFG yapısına yaklaştırır. Buradaki işlem adları OIR tasarım seviyesindedir; nihai opcode adları ve runtime ABI henüz sabitlenmemiştir.

### 11.1 ApplyRules kapsamı

Her `apply rules(RuleSet(args))` çağrısı ayrı bir RuleSet uygulama kapsamı oluşturur:

- Parametreler çağrı noktasında değerlendirilir ve RuleSet parametrelerine bağlanır.
- Hata biriktiricisi ve normal trigger eşleşme durumu bu uygulamaya özeldir.
- RuleSet'in bariyerleri ve trigger koşulları tanımlı kaynak sırasıyla değerlendirilir.
- Başarısız uygulama, sonraki `apply rules` çağrılarına ve normal fonksiyon akışına geçemez.
- Başarılı uygulama, handler ve isteğe bağlı always/default aşamalarını tamamladıktan sonra çağrı noktasındaki devam bloğuna bağlanır.

RuleSet gövdesi bilinen bir derleme zamanı tanımı olduğundan, temel biçimde `apply rules` dinamik bir isim araması değildir. OIR Builder, RuleSet yapısını çağrı noktasında CFG bloklarına açar. Ortak kod üretimi daha sonra ayrı fonksiyonlara çıkarma optimizasyonu olarak değerlendirilebilir.

### 11.2 Barrier CFG ve hata biriktirme

Her uygulama için bir hata biriktiricisi başlatılır. Her bariyer kendi koşulunu değerlendirir:

```text
ApplyEntry
  -> ErrorListCreate
  -> BarrierCheck_0
       true  -> ErrorAppend_0 -> BarrierCheck_1
       false ----------------> BarrierCheck_1
  -> ...
  -> BarrierCheck_N
       true  -> ErrorAppend_N -> BarrierComplete
       false ----------------> BarrierComplete
  -> ErrorListIsEmpty
       true  -> TriggerEvaluationEntry
       false -> ReportAllErrors -> RuleSetFailureExit
```

- `ErrorAppend`, bariyer kimliği, RuleSet kimliği, kaynak konumu ve hata mesajını içeren bir tanı kaydı ekler.
- Hata mesajı ifadesi yalnızca ilgili bariyer koşulu doğruysa değerlendirilir.
- Hiçbir bariyer, diğer bariyerlerin değerlendirilmesini kısa devre yapamaz.
- `RuleSetFailureExit`, normal fonksiyon dönüşü değildir; fonksiyonun normal CFG yolunu sonlandıran hata çıkışıdır.

`ErrorListCreate`, `ErrorAppend`, `ErrorListIsEmpty` ve `ReportAllErrors` şimdilik soyut OIR işlemleridir. Bunların runtime çağrısı, sabit boyutlu kayıt alanı veya başka bir temsil kullanacağı henüz belirlenmemiştir.

### 11.3 Normal trigger değerlendirme ve kayıt

Bariyerler hatasız tamamlandıktan sonra normal trigger koşulları değerlendirilir. Her trigger için koşul sonucu ayrı bir SSA boolean değeri olarak tutulabilir:

```text
TriggerEval_0 -> matched_0
TriggerEval_1 -> matched_1
...
TriggerEval_N -> matched_N
```

- Bütün koşullar değerlendirilmeden hiçbir normal `on` handler çalıştırılmaz.
- Her koşul yalnızca bir kez değerlendirilir; handler aşamasında koşul yeniden hesaplanmaz.
- Trigger kimlikleri derleme zamanında çözülür. OIR içinde trigger adlarını metin olarak karşılaştırmak gerekmez.
- Trigger eşleşme kümesi, statik olarak bilinen trigger kimlikleri ve SSA boolean değerleriyle temsil edilebilir. Dinamik liste/bitset ancak uygulama gerektirirse seçilir.
- Handler tanımlanmamış bir trigger da eşleşmiş olarak kaydedilir; dışarıya nasıl sunulacağına ilişkin runtime arayüzü ayrıca tanımlanacaktır.

### 11.4 Handler CFG

Normal trigger koşullarının tamamı değerlendirildikten sonra, ilgili RuleSet'e bağlı `on` handler'ları kaynak kod sırasına göre işlenir:

```text
HandlerDispatchEntry
  -> if matched_A then Handler_A else NextHandler
  -> if matched_B then Handler_B else NextHandler
  -> ...
  -> AlwaysStage
```

Her handler için:

- Koşul, ilgili trigger'ın daha önce hesaplanmış SSA boolean değeridir.
- Eşleşme yoksa handler atlanır.
- Eşleşme varsa handler gövdesi çalıştırılır.
- Handler normal şekilde biterse sonraki handler'a geçilir.
- Handler `return`, `break`, `continue` veya başka bir sonlandırıcı kontrol akışı üretirse CFG kuralları uygulanır; sonlandırılmış yoldan sonraki handler'a sahte bir kenar oluşturulmaz.
- Handler'lar arasında canlı yollar gerekiyorsa merge/continuation blokları kullanılır.

Parser, `on` bloklarını `apply rules` satırından sonra görebilir. Bu nedenle Analyzer, fonksiyon kapsamındaki handler bildirimlerini önce RuleSet/trigger çiftlerine bağlamalı; OIR Builder ise eşleşen gövdeleri ilgili `apply rules` noktasına yerleştirmelidir.

### 11.5 Always ve Default aşamaları

Normal handler aşaması tamamlandıktan sonra `always` aşaması gelir. Ardından, **hiçbir normal trigger eşleşmemişse**, `default` aşaması değerlendirilir.

| Kaynak biçimi | OIR davranışı |
|---|---|
| `always -> Name;` | Always aşamasında Name trigger'ını eşleşmiş olarak kaydet; varsa `on RuleSet.Name` handler'ını çalıştır. |
| `always { ... }` | Always aşamasında doğrudan blok gövdesini çalıştır. |
| `default -> Name;` | Normal trigger eşleşmesi yoksa Default aşamasında Name trigger'ını kaydet; varsa handler'ını çalıştır. |
| `default { ... }` | Normal trigger eşleşmesi yoksa Default aşamasında doğrudan blok gövdesini çalıştır. |

`always` ve `default` ok biçimleri de normal trigger'lar gibi metin adıyla runtime araması yapmaz; Analyzer tarafından sabit trigger kimliğine bağlanır. Bu özel aşamaların handler'ları normal handler kuyruğuna karıştırılmaz: `always` normal handler'lardan sonra, `default` ise `always` aşamasından sonra çalışır.

### 11.6 Devam ve hata çıkışları

Başarılı bir RuleSet uygulamasının bütün canlı yolları `ApplyContinue` bloğuna bağlanır. Bu blok, kaynak fonksiyondaki `apply rules` satırından sonraki normal CFG bloğudur.

Bariyer hatası yolu ise `ApplyContinue` bloğuna bağlanmaz. Hatalar raporlandıktan sonra normal fonksiyon akışı sonlandırılır. Hata çıkışının kesin ABI karşılığı (exception, error result, runtime trap veya platforma özgü hata aktarımı) bu spesifikasyonda henüz seçilmemiştir. Bu karar verilene kadar OIR'de ayrı bir `RuleSetFailureExit` terminator/çıkış sınıfı olarak modellenir; normal `RETURN` ile karıştırılmaz.

### 11.7 Analyzer ve AST ayrıntısı

`always` ve `default` düğümleri yalnızca genel bir `Action` alanına indirgenmemelidir. AST, ok biçimi ile doğrudan blok biçimini ayırt etmelidir:

```text
AST_RULE_ACTION {
    Kind: TRIGGER | DIRECT_BLOCK
    TriggerName? 
    Body?
    SourceOrder
}

AST_ALWAYS  { RuleAction }
AST_DEFAULT { RuleAction }
```

Analyzer şu ek doğrulamaları yapar:

- `always -> Name` ve `default -> Name` biçimindeki adlar, ilgili RuleSet kapsamındaki trigger kimliklerine dahil edilir.
- Bu isimler normal trigger adlarıyla çakışamaz.
- `always` veya `default` için ok biçimi ve blok biçimi birlikte kullanılamaz.
- Her özel anahtar en fazla bir kez tanımlanabilir.
- Özel trigger için yazılmış `on RuleSet.Name` handler'ı da aynı tekil handler ve tip/kapsam kontrollerine tabidir.

### 11.8 Lowering özeti

OIR Builder'ın uygulama başına üretmesi gereken CFG aşamaları:

1. Parametre bağlama ve hata biriktiricisi oluşturma.
2. Bütün bariyer koşulları ve koşullu hata kayıtları.
3. Tek bir toplu hata kontrolü ve ayrı başarısızlık çıkışı.
4. Bütün normal trigger koşullarının bir kez değerlendirilmesi.
5. Normal handler'ların kaynak sırasıyla koşullu yürütülmesi.
6. İsteğe bağlı always trigger/handler veya doğrudan always bloğu.
7. Normal trigger eşleşmesi yoksa isteğe bağlı default trigger/handler veya doğrudan default bloğu.
8. Canlı başarılı yolların ApplyContinue bloğunda birleştirilmesi.

Bu lowering, mevcut OIR terminator modelindeki `BRANCH`, `JUMP` ve `RETURN` ile; hata çıkışı için ayrıca tanımlanacak hata terminator sınıfıyla ifade edilir. Her handler ve koşul bloğunun SSA tanımları/kullanımları normal dominance ve block-parameter doğrulamasına tabidir.


---

## 12. Normatif OIR Test Senaryoları

Bu bölümdeki testler, Multi-Rule Analyzer ve OIR Builder/Verifier davranışını doğrulamak için hazırlanmıştır. OIR işlem adları soyut tasarım adlarıdır; nihai opcode adları olarak kabul edilmez.

### 12.1 MR-OIR-001 — Birden fazla bariyer hatasının toplanması

**Amaç:** İlk bariyer hatasının sonraki bariyerlerin değerlendirilmesini durdurmadığını ve hata varken trigger aşamasına geçilmediğini doğrulamak.

Kaynak test:

```nexus
rules ValidateDimensions(w, h, d) {
    w <= 0 :=> "Genişlik sıfır veya negatif olamaz";
    h <= 0 :=> "Yükseklik sıfır veya negatif olamaz";
    d <= 0 :=> "Derinlik sıfır veya negatif olamaz";

    w != h -> DifferentWidth;
    h != d -> DifferentHeight;
}

f:checkDimensions(w, h, d) {
    apply rules(ValidateDimensions(w, h, d));

    on ValidateDimensions.DifferentWidth {
        echo("Genişlik farklı");
    }

    on ValidateDimensions.DifferentHeight {
        echo("Yükseklik farklı");
    }

    echo("Normal fonksiyon akışı");
}
```

Test girdisi:

```text
w = -1
h = -2
d = -3
```

Beklenen bariyer sonuçları:

| Bariyer | Koşul | Beklenen |
|---|---|---|
| B0 | `w <= 0` | Hata kaydı eklenir |
| B1 | `h <= 0` | Hata kaydı eklenir |
| B2 | `d <= 0` | Hata kaydı eklenir |

Beklenen hata listesi tam olarak üç kayıttan oluşur. Kayıt sırası RuleSet kaynak sırasıdır: B0, B1, B2. Her kayıtta RuleSet kimliği, bariyer kimliği/sırası, kaynak konumu ve mesaj bulunur.

Trigger koşulları bu test girdisinde doğru olmasına rağmen değerlendirme aşamasına ulaşılmaz. Dolayısıyla:
- `DifferentWidth` ve `DifferentHeight` handler'ları çalışmaz.
- `Normal fonksiyon akışı` çıktısı üretilmez.
- Başarılı `ApplyContinue` bloğuna kenar oluşturulmaz.
- Bütün hata kayıtları tek bir başarısız RuleSet çıkışından raporlanır.

Beklenen soyut CFG:

```text
ApplyEntry
  -> ErrorListCreate
  -> B0_Check
       true  -> B0_Append -> B1_Check
       false -------------> B1_Check
  -> B1_Check
       true  -> B1_Append -> B2_Check
       false -------------> B2_Check
  -> B2_Check
       true  -> B2_Append -> BarrierComplete
       false -------------> BarrierComplete
  -> ErrorListIsEmpty
       false -> ReportAllErrors -> RuleSetFailureExit
       true  -> TriggerEvaluationEntry
```

Bu girdide `ErrorListIsEmpty` sonucu false olmalıdır. Trigger değerlendirme ve handler blokları CFG'de başarılı kola ait olabilir; fakat bu test çalışmasında erişilebilir olmamalıdır.

### 12.2 MR-OIR-001 Verifier kontrolleri

Verifier veya OIR test altyapısı aşağıdakileri doğrulamalıdır:

1. Üç bariyer kontrol bloğunun da CFG'de bulunması.
2. B0'ın true ve false yollarının B1'e ulaşması.
3. B1'in true ve false yollarının B2'ye ulaşması.
4. B2'nin true ve false yollarının toplu hata kontrolüne ulaşması.
5. Her true bariyer kolunda yalnızca kendi hata kaydının eklenmesi.
6. Hata raporlama bloğunun bütün bariyer kontrollerinden sonra gelmesi.
7. Hata çıkışından trigger değerlendirme bloğuna veya normal devam bloğuna kenar bulunmaması.
8. Hata listesinin boş olmadığı test girdisinde tam üç tanı kaydı üretilmesi ve kaynak sırasının korunması.
9. Trigger handler gövdelerinin hata yolundan erişilebilir olmaması.
10. `RuleSetFailureExit` yolunun normal `RETURN` veya `ApplyContinue` ile karıştırılmaması.

### 12.3 Negatif OIR testleri

Aşağıdaki bozuk CFG varyantları reddedilmelidir:

- **MR-OIR-001-N1:** B0 true kolu doğrudan `RuleSetFailureExit` bloğuna gidiyor ve B1/B2 atlanıyor.
- **MR-OIR-001-N2:** B1 true kolu B2'yi atlayarak hata raporlamaya gidiyor.
- **MR-OIR-001-N3:** Hata raporlamadan sonra trigger değerlendirmesine kenar bulunuyor.
- **MR-OIR-001-N4:** Bariyer hatası yolundan `ApplyContinue` bloğuna kenar bulunuyor.
- **MR-OIR-001-N5:** Hata kaydı yanlış bariyer kimliği veya yanlış RuleSet kimliği taşıyor.
- **MR-OIR-001-N6:** Hata kayıtlarının kaynak sırası kararsız veya testte beklenenden farklı.

### 12.4 Test sınıflandırması

- **Frontend/Analyzer testi:** Kaynak örneğinin parse edilmesi, RuleSet/trigger/handler isim çözümlemesi ve koşul tiplerinin doğrulanması.
- **OIR Builder testi:** Bariyer başına koşul dalları, hata kayıtları ve toplu hata çıkışının oluşturulması.
- **OIR Verifier testi:** CFG kenarları, terminator'lar, blok parametreleri ve SSA doğrulaması.
- **OIR yürütme/simülasyon testi:** `(-1, -2, -3)` girdisiyle üç hata kaydının oluşması ve trigger/normal akışın çalışmaması.

Bir testin başarıyla derlenmesi, tek başına runtime davranışının doğrulandığı anlamına gelmez. Her test katmanı kendi sonucunu ayrı raporlamalıdır.
