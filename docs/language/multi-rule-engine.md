
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

AST_ALWAYS  { RuleAction, SourceOrder }
AST_DEFAULT { RuleAction, SourceOrder }

AST_RULE_ACTION {
    Kind: TRIGGER | DIRECT_BLOCK
    TriggerName?
    Body?
    SourceOrder
}

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


### 12.5 MR-OIR-002 — Birden fazla trigger eşleşmesi ve handler sırası

**Amaç:** Birden fazla trigger koşulu doğru olduğunda bütün trigger'ların kaydedildiğini; handler'ların ise trigger tanım sırasına göre değil, fonksiyon içindeki `on` bloklarının kaynak sırasına göre çalıştığını doğrulamak.

Kaynak test:

```nexus
rules NumberModes(n) {
    n > 0 -> Positive;
    n % 2 == 0 -> Even;
}

f:checkNumber(n) {
    apply rules(NumberModes(n));

    on NumberModes.Even {
        echo("Even handler");
    }

    on NumberModes.Positive {
        echo("Positive handler");
    }

    echo("Normal flow");
}
```

Test girdisi: `n = 4`.

Beklenen değerlendirme:

| Sıra | Trigger | Koşul | Sonuç |
|---|---|---|---|
| 1 | Positive | `n > 0` | true |
| 2 | Even | `n % 2 == 0` | true |

İki trigger da eşleşme kümesine eklenir. Her koşul yalnızca bir kez değerlendirilir. Bütün trigger koşulları tamamlanmadan handler yürütmesi başlamaz.

Beklenen handler sırası, fonksiyondaki `on` bloklarının kaynak sırasıdır:

1. `Even handler`
2. `Positive handler`
3. `Normal flow`

Bu test özellikle trigger tanım sırası ile handler kaynak sırasının farklı seçilmesiyle hazırlanmıştır. Böylece iki sıranın yanlışlıkla birbirine bağlanması yakalanır.

Beklenen soyut OIR:

```text
TriggerEval_Positive -> matched_positive
TriggerEval_Even     -> matched_even
       |
       v
HandlerDispatchEntry
       |
       v
BRANCH matched_even
  true  -> Handler_Even -> Handler_Positive_Check
  false ----------------> Handler_Positive_Check
                              |
                              v
                       BRANCH matched_positive
                         true  -> Handler_Positive -> NormalFlow
                         false --------------------> NormalFlow
```

### 12.6 MR-OIR-002 Verifier ve yürütme kontrolleri

1. Her trigger koşulu için bir değerlendirme sonucu üretilmeli ve SSA değeri tanımlanmalıdır.
2. İki trigger koşulu da değerlendirilmeden ilk handler bloğuna erişilmemelidir.
3. Handler dispatch, ilgili trigger'ın daha önce hesaplanmış SSA boolean değerini kullanmalıdır.
4. Handler koşulu için trigger ifadesi tekrar hesaplanmamalıdır.
5. Handler bloklarının sırası kaynak koddaki `on` sırasıyla aynı olmalıdır.
6. Handler'ın canlı normal çıkışı sıradaki handler kontrolüne bağlanmalıdır.
7. İki trigger da eşleştiğinde iki handler da tam bir kez çalışmalıdır.
8. Her iki handler tamamlandıktan sonra normal fonksiyon akışı sürmelidir.

Negatif varyantlar:

- **MR-OIR-002-N1:** İlk handler, bütün trigger koşulları değerlendirilmeden çalıştırılıyor.
- **MR-OIR-002-N2:** Handler dispatch, SSA sonucunu kullanmak yerine trigger koşulunu yeniden değerlendiriyor.
- **MR-OIR-002-N3:** Handler'lar kaynak sırasının tersine veya trigger tanım sırasına göre çalıştırılıyor.
- **MR-OIR-002-N4:** Aynı eşleşmiş trigger handler'ı iki kez çalıştırılıyor.
- **MR-OIR-002-N5:** Bir handler normal tamamlandığı halde sonraki handler kontrolüne CFG kenarı bulunmuyor.

Test girdisi `n = 4` için beklenen çıktı sırası kesin olarak `Even handler`, `Positive handler`, `Normal flow` şeklindedir.

### 12.7 MR-OIR-003 — Handler içinden terminator ile çıkış

**Amaç:** Bir \`on\` handler içindeki \`return\` işleminin mevcut fonksiyonun kontrol akışını sonlandırdığını; sonraki handler'lara, \`always\` / \`default\` aşamalarına veya normal devam bloğuna sahte CFG kenarı oluşturulmadığını doğrulamak.

Bu senaryo iki alt durum içerir. İlkinde normal handler dönüşü, ikincisinde \`always\` doğrudan bloğunun dönüşü sınanır.

#### Durum A — İlk normal handler \`return\` çalıştırır

Kaynak test:

\`\`\`nexus
rules NumberModes(n) {
    n > 0 -> Positive;
    n % 2 == 0 -> Even;
    always {
        echo("Always must not run");
    }
}

f:checkNumber(n) {
    apply rules(NumberModes(n));

    on NumberModes.Even {
        return true;
    }

    on NumberModes.Positive {
        echo("Positive handler must not run");
    }

    echo("Normal flow must not run");
    return false;
}
\`\`\`

Test girdisi: \`n = 4\`.

İki normal trigger da eşleşir. Trigger koşullarının ikisi de değerlendirildikten sonra handler kaynak sırasına göre dispatch edilir. İlk handler olan \`Even\`, \`return true;\` çalıştırır.

Beklenen yürütme:
1. \`Positive\` ve \`Even\` koşulları birer kez değerlendirilir.
2. \`Even\` handler'ı çalışır ve fonksiyondan \`true\` döner.
3. \`Positive\` handler'ı çalışmaz.
4. \`always\` bloğu çalışmaz.
5. \`Normal flow must not run\` çıktısı üretilmez.

Beklenen sonuç: Fonksiyonun dönüş değeri \`true\`; gözlemlenebilir tek handler etkisi dönüş işlemidir.

#### Durum B — Always bloğu \`return\` çalıştırır, default atlanır

Kaynak test:

\`\`\`nexus
rules NoNumberModes(n) {
    n > 0 -> Positive;
    n % 2 == 0 -> Even;

    always {
        return true;
    }

    default {
        echo("Default must not run");
    }
}

f:checkNoNumberMode(n) {
    apply rules(NoNumberModes(n));

    echo("Normal flow must not run");
    return false;
}
\`\`\`

Test girdisi: \`n = -1\`.

Hiçbir normal trigger eşleşmez. Bu nedenle normal handler aşamasında çalıştırılacak handler yoktur. \`always\` doğrudan bloğu çalışır ve fonksiyondan döner. \`default\` aşamasına ve fonksiyonun normal devamına ulaşılmaz.

Beklenen sonuç: Fonksiyonun dönüş değeri \`true\`; \`Default must not run\` ve \`Normal flow must not run\` çıktıları oluşmaz.

Bu alt durum, \`default\` aşamasının normal trigger eşleşmesi olmadığı halde \`always\` terminator'ı nedeniyle atlandığını ayrıca doğrular.

### 12.8 MR-OIR-003 CFG ve Verifier kontrolleri

**Durum A için beklenen soyut CFG:**

\`\`\`text
TriggerEval_Positive -> matched_positive
TriggerEval_Even     -> matched_even
       |
       v
HandlerDispatchEntry
       |
       v
BRANCH matched_even
  true  -> Handler_Even -> RETURN true
  false ----------------> Handler_Positive_Check
                              |
                              v
                       BRANCH matched_positive
                         true  -> Handler_Positive
                                  -> AlwaysStage
                         false ----------------> AlwaysStage

RETURN true:
  no successor
\`\`\`

\`Handler_Even\` bloğundaki \`RETURN\` terminator'ı, \`Handler_Positive_Check\`, \`AlwaysStage\` veya \`ApplyContinue\` bloklarına çıkış kenarı taşıyamaz. Diğer handler'a giden false/normal dispatch yolu yalnızca \`matched_even = false\` kolundan devam eder.

**Durum B için beklenen soyut CFG:**

\`\`\`text
TriggerEval_Positive -> matched_positive
TriggerEval_Even     -> matched_even
       |
       v
HandlerDispatchEntry
       |
       v
AlwaysStage
       |
       v
AlwaysDirectBlock
       |
       v
RETURN true

DefaultStage:
  reachable only from a live normal exit of AlwaysStage
  not reachable from RETURN true

ApplyContinue:
  not reachable from RETURN true
\`\`\`

Verifier ve OIR test altyapısı aşağıdakileri doğrulamalıdır:

1. Durum A'da bütün normal trigger koşulları handler dispatch başlamadan önce değerlendirilir.
2. \`Handler_Even\` içindeki \`RETURN\` gerçek fonksiyon dönüş terminator'ıdır; RuleSet'e özel bir devam sinyali değildir.
3. \`RETURN\` bloğunun successor listesi boştur.
4. \`RETURN\` bloğundan sonraki handler kontrolüne, \`AlwaysStage\` veya \`ApplyContinue\` bloğuna kenar bulunmaz.
5. Durum A'da \`matched_even = true\` girdisiyle yalnızca Even handler yolu yürütülür.
6. Durum B'de normal trigger eşleşmesi olmadığı doğrulanır ve \`AlwaysDirectBlock\` çalıştırılır.
7. Durum B'deki \`RETURN\`, \`DefaultStage\` ve \`ApplyContinue\` bloklarına ulaşmayı engeller.
8. Her iki durumda da dönüş tipi ve dönüş değeri fonksiyonun diğer dönüş yollarıyla Analyzer tarafından tutarlı bulunmalıdır.

Negatif OIR varyantları reddedilmelidir:

- **MR-OIR-003-N1:** \`RETURN\` terminator'ından sonraki handler'a kenar çıkması.
- **MR-OIR-003-N2:** Handler dönüşünden \`AlwaysStage\` bloğuna kenar çıkması.
- **MR-OIR-003-N3:** Handler dönüşünden \`ApplyContinue\` bloğuna kenar çıkması.
- **MR-OIR-003-N4:** Durum B'de \`always\` dönüşünden \`DefaultStage\` bloğuna kenar çıkması.
- **MR-OIR-003-N5:** \`RETURN\` terminator'ının successor kabul etmesi veya normal \`JUMP\` gibi ele alınması.
- **MR-OIR-003-N6:** Bir handler dönüşünün yalnızca handler'ı sonlandırıp çevre fonksiyonu sürdürdüğünün varsayılması.

Not: Bu test, handler ve doğrudan always bloğunun fonksiyon gövdesine inline/lower edilmiş kontrol akışı olduğunu esas alır. Handler'lar veya özel bloklar ayrı runtime fonksiyonlarına dönüştürülürse, bu dönüş davranışını koruyan çağrı/çıkış ABI'si ayrıca tanımlanmalıdır; mevcut test böyle bir ABI'yi varsaymaz.

### 12.9 MR-OIR-004 — Always ve default aşama sırası

**Amaç:** Normal handler'ların \`always\` aşamasından önce çalıştığını; \`always\` aşamasının her başarılı RuleSet uygulamasında yürütüldüğünü; \`default\` aşamasının ise yalnızca hiçbir normal trigger eşleşmediğinde ve \`always\` aşaması canlı bir yolla tamamlandığında çalıştığını doğrulamak.

Bu test iki girdiyle aynı RuleSet'i çalıştırır. Kaynak biçiminde doğrudan bloklar kullanılır; isimli \`always -> Name\` / \`default -> Name\` trigger biçimleri ayrı bir testte doğrulanacaktır.

Kaynak test:

\`\`\`nexus
rules ModeSelection(n) {
    n > 0 -> Positive;

    always {
        echo("Always");
    }

    default {
        echo("Default");
    }
}

f:checkMode(n) {
    apply rules(ModeSelection(n));

    on ModeSelection.Positive {
        echo("Positive handler");
    }

    echo("Normal continuation");
}
\`\`\`

#### Durum A — Normal trigger eşleşir

Test girdisi: \`n = 5\`.

Beklenen sıra:

1. \`Positive\` trigger koşulu değerlendirilir ve eşleşir.
2. \`on ModeSelection.Positive\` handler'ı varsa normal handler aşamasında çalışır.
3. \`always\` doğrudan bloğu çalışır ve \`Always\` çıktısını üretir.
4. Normal trigger eşleştiği için \`default\` bloğu çalışmaz.
5. RuleSet tamamlanır ve fonksiyon normal akışına devam eder.

Bu durum için kaynak testte handler bulunmadığından trigger eşleşmesi kaydedilir ancak handler çıktısı oluşmaz. Beklenen çıktı sırası:

\`\`\`text
Always
Normal continuation
\`\`\`

#### Durum B — Hiçbir normal trigger eşleşmez

Test girdisi: \`n = -5\`.

Beklenen sıra:

1. \`Positive\` trigger koşulu değerlendirilir ve eşleşmez.
2. Normal handler aşaması boş geçilir.
3. \`always\` doğrudan bloğu çalışır ve \`Always\` çıktısını üretir.
4. Hiçbir normal trigger eşleşmediği için \`default\` doğrudan bloğu çalışır ve \`Default\` çıktısını üretir.
5. RuleSet tamamlanır ve fonksiyon normal akışına devam eder.

Beklenen çıktı sırası:

\`\`\`text
Always
Default
Normal continuation
\`\`\`

### 12.10 MR-OIR-004 CFG ve Verifier kontrolleri

**Durum A için beklenen soyut CFG:**

\`\`\`text
TriggerEval_Positive -> matched_positive
       |
       v
HandlerDispatchEntry
       |
       v
AlwaysStage
       |
       v
AlwaysDirectBlock
       |
       v
DefaultEligibilityCheck
       |
       +-- normal_trigger_matched == true --> ApplyContinue
       |
       +-- normal_trigger_matched == false -> DefaultDirectBlock
                                                |
                                                v
                                           ApplyContinue
\`\`\`

**Durum B için beklenen soyut CFG:**

\`\`\`text
TriggerEval_Positive -> matched_positive = false
       |
       v
HandlerDispatchEntry
       |
       v
AlwaysStage
       |
       v
AlwaysDirectBlock
       |
       v
DefaultEligibilityCheck
       |
       +-- normal_trigger_matched == true --> ApplyContinue
       |
       +-- normal_trigger_matched == false -> DefaultDirectBlock
                                                |
                                                v
                                           ApplyContinue
\`\`\`

Durum A'da eligibility koşulu true, Durum B'de false olmalıdır. CFG iki yolu da temsil edebilir; yürütme testi verilen girdiye göre doğru yolu izlemelidir.

Verifier ve yürütme altyapısı aşağıdakileri doğrulamalıdır:

1. Normal trigger koşullarının tamamı \`always\` aşamasından önce değerlendirilmiş olmalıdır.
2. Normal handler'lar varsa hepsi \`always\` aşamasından önce, kaynak sırasıyla yürütülmelidir.
3. \`always\` doğrudan bloğu, bariyer aşaması başarılı olan her iki test girdisinde de tam bir kez çalışmalıdır.
4. Default uygunluk kontrolü, normal trigger koşullarının SSA sonuçlarından türetilmelidir; koşullar yeniden değerlendirilmemelidir.
5. Durum A'da en az bir normal trigger eşleştiği için \`DefaultDirectBlock\` yürütülmemelidir.
6. Durum B'de hiçbir normal trigger eşleşmediği için \`DefaultDirectBlock\`, \`always\` bloğundan sonra tam bir kez çalışmalıdır.
7. Her iki durumda da canlı RuleSet yolları \`ApplyContinue\` bloğuna ulaşmalıdır.
8. \`always\` bloğunun normal çıkışı \`DefaultEligibilityCheck\` aşamasına bağlanmalıdır.
9. \`default\` bloğunun normal çıkışı \`ApplyContinue\` bloğuna bağlanmalıdır.
10. \`always\` bloğundan veya \`default\` bloğundan bir terminator ile çıkılmışsa, o yoldan sonraki aşamaya kenar oluşturulmamalıdır.

Negatif OIR varyantları reddedilmelidir:

- **MR-OIR-004-N1:** \`always\` bloğunun normal handler'lardan önce yürütülmesi.
- **MR-OIR-004-N2:** Normal trigger eşleşmesine rağmen \`default\` bloğunun yürütülmesi.
- **MR-OIR-004-N3:** Hiçbir normal trigger eşleşmediği halde \`default\` bloğunun atlanması.
- **MR-OIR-004-N4:** \`default\` bloğunun \`always\` bloğundan önce yürütülmesi.
- **MR-OIR-004-N5:** \`always\` veya \`default\` bloğunun birden fazla kez yürütülmesi.
- **MR-OIR-004-N6:** Default uygunluk kontrolünün trigger koşullarını yeniden değerlendirmesi.
- **MR-OIR-004-N7:** \`always\` bloğundaki terminator sonrasında \`default\` veya \`ApplyContinue\` bloğuna kenar bulunması.
- **MR-OIR-004-N8:** \`default\` bloğundaki terminator sonrasında \`ApplyContinue\` bloğuna kenar bulunması.

Bu test, doğrudan blok biçimlerinin aşama sırasını doğrular. İsimli özel trigger biçimlerinin (\`always -> Name;\`, \`default -> Name;\`) handler eşlemesi ve dispatch ayrıntıları bu testin kapsamı dışındadır.

### 12.11 MR-OIR-005 — İsimli always/default trigger dispatch

**Amaç:** `always -> Name;` ve `default -> Name;` biçimlerinin özel aşamalara ait isimli trigger ürettiğini; bu trigger'ların `on RuleSet.Name` handler'larına doğru aşamada bağlandığını ve normal trigger handler kuyruğuna karıştırılmadığını doğrulamak.

Bu test, §11.5 ve §11.7'de tanımlanan özel dispatch modelini kapsar. Özel trigger isimleri derleme zamanında çözülür. Runtime'da trigger adı metin olarak aranmaz.

Kaynak test:

```nexus
rules ModeSelection(n) {
    n > 0 -> Positive;
    always -> LogMetrics;
    default -> StandardMode;
}

f:checkMode(n) {
    apply rules(ModeSelection(n));

    on ModeSelection.Positive {
        echo("Positive handler");
    }

    on ModeSelection.LogMetrics {
        echo("Always handler");
    }

    on ModeSelection.StandardMode {
        echo("Default handler");
    }

    echo("Normal continuation");
}
```

#### Durum A — Normal trigger eşleşir

Test girdisi: `n = 5`.

Beklenen değerlendirme ve yürütme sırası:

1. Normal `Positive` koşulu bir kez değerlendirilir ve eşleşir.
2. Normal handler aşamasında `Positive handler` çalışır.
3. Normal handler aşaması tamamlandıktan sonra `always` özel aşaması `LogMetrics` trigger'ını üretir ve `Always handler` çalışır.
4. Normal trigger eşleştiği için `default` özel trigger'ı `StandardMode` üretilmez; `Default handler` çalışmaz.
5. RuleSet başarılı biçimde tamamlanır ve normal fonksiyon akışı sürer.

Beklenen çıktı:

```text
Positive handler
Always handler
Normal continuation
```

#### Durum B — Hiçbir normal trigger eşleşmez

Test girdisi: `n = -5`.

Beklenen değerlendirme ve yürütme sırası:

1. Normal `Positive` koşulu bir kez değerlendirilir ve eşleşmez.
2. Normal handler aşaması boş geçilir.
3. `always` özel aşaması `LogMetrics` trigger'ını üretir ve `Always handler` çalışır.
4. Hiçbir normal trigger eşleşmediği için `default` özel aşaması `StandardMode` trigger'ını üretir ve `Default handler` çalışır.
5. RuleSet başarılı biçimde tamamlanır ve normal fonksiyon akışı sürer.

Beklenen çıktı:

```text
Always handler
Default handler
Normal continuation
```

### 12.12 MR-OIR-005 CFG ve Verifier kontrolleri

**Durum A için beklenen soyut CFG:**

```text
TriggerEval_Positive -> matched_positive
       |
       v
NormalHandlerStage
       |
       +-- matched_positive --> PositiveHandler
       |                           |
       +-- not matched ------------+
                                   |
                                   v
                             AlwaysStage
                                   |
                                   v
                         EmitSpecialTrigger(LogMetrics)
                                   |
                                   v
                           LogMetricsHandler
                                   |
                                   v
                         DefaultEligibilityCheck
                                   |
                  normal_match=true
                                   |
                                   v
                             ApplyContinue

StandardModeHandler:
  unreachable for this input
```

**Durum B için beklenen soyut CFG:**

```text
TriggerEval_Positive -> matched_positive = false
       |
       v
NormalHandlerStage (no handler selected)
       |
       v
AlwaysStage
       |
       v
EmitSpecialTrigger(LogMetrics)
       |
       v
LogMetricsHandler
       |
       v
DefaultEligibilityCheck
       |
  normal_match=false
       |
       v
EmitSpecialTrigger(StandardMode)
       |
       v
StandardModeHandler
       |
       v
ApplyContinue
```

Verifier ve yürütme altyapısı aşağıdakileri doğrulamalıdır:

1. Bütün normal trigger koşulları, normal handler dispatch başlamadan önce bir kez değerlendirilir.
2. Normal handler'lar, eşleşme durumuna göre kaynak sırasıyla çalışır.
3. `always` özel trigger'ı her başarılı RuleSet uygulamasında tam bir kez üretilir.
4. `always` handler'ı normal handler aşamasından sonra ve default uygunluk kontrolünden önce çalışır.
5. `default` özel trigger'ı yalnızca hiçbir normal trigger eşleşmediğinde üretilir.
6. `default` handler'ı, varsa `always` handler'ından sonra çalışır.
7. Durum A'da `StandardMode` trigger'ı ve handler'ı yürütülmez.
8. Durum B'de `LogMetrics` ve ardından `StandardMode` handler'ları tam birer kez çalışır.
9. Özel trigger'ların kimlikleri Analyzer tarafından çözülmüş sabit kimliklerdir; OIR'de string karşılaştırması veya runtime isim araması bulunmaz.
10. Özel handler'lar normal handler dispatch listesine eklenmez; ayrı `AlwaysStage` ve `DefaultStage` üzerinden çağrılır.
11. Her handler'ın normal çıkışı bir sonraki doğru özel aşamaya veya `ApplyContinue` bloğuna bağlanır.
12. Bir handler terminator ile çıkarsa sonraki handler/aşama için o yoldan CFG kenarı oluşturulmaz.

Analyzer ve Verifier negatif testleri:

- **MR-OIR-005-N1:** `always` handler'ının normal handler aşamasında çalıştırılması.
- **MR-OIR-005-N2:** Normal trigger eşleştiği halde `default` trigger'ının üretilmesi.
- **MR-OIR-005-N3:** Hiçbir normal trigger eşleşmediği halde `default` trigger'ının üretilmemesi.
- **MR-OIR-005-N4:** `default` handler'ının `always` handler'ından önce çalıştırılması.
- **MR-OIR-005-N5:** `always` özel trigger'ının başarılı uygulamada hiç üretilmemesi veya birden fazla üretilmesi.
- **MR-OIR-005-N6:** `default` özel trigger'ının aynı uygulamada birden fazla üretilmesi.
- **MR-OIR-005-N7:** Özel trigger handler'ının yanlış RuleSet/trigger kimliğine bağlanması.
- **MR-OIR-005-N8:** `always` veya `default` trigger adının normal trigger adıyla çakışmasına Analyzer'ın izin vermesi.
- **MR-OIR-005-N9:** Aynı özel trigger için birden fazla `on` handler tanımının kabul edilmesi.
- **MR-OIR-005-N10:** Özel trigger adının runtime string aramasıyla çözülmesi.
- **MR-OIR-005-N11:** `always` handler'ındaki terminator sonrasında `default` aşamasına veya `ApplyContinue` bloğuna sahte CFG kenarı bulunması.
- **MR-OIR-005-N12:** `default` handler'ındaki terminator sonrasında `ApplyContinue` bloğuna sahte CFG kenarı bulunması.

Bu test, isimli `always` / `default` biçimlerinin handler dispatch ve CFG sırasını kapsar. Doğrudan blok biçimlerinin terminator davranışı MR-OIR-003 ve aşama sırası MR-OIR-004 kapsamındadır.

### 12.13 MR-OIR-006 — Birden fazla apply rules çağrısının sıralı yürütülmesi

**Amaç:** Aynı fonksiyon içindeki birden fazla `apply rules` çağrısının kaynak sırasına göre ve birbirinden bağımsız uygulama kapsamlarında yürütüldüğünü; önceki çağrının bariyer hatasının sonraki RuleSet çağrılarını ve normal fonksiyon akışını engellediğini doğrulamak.

Kaynak test:

```nexus
rules InputCheck(n) {
    n >= 0 :=> "Input must be non-negative";
    n > 0 -> Positive;
}

rules QuotaCheck(q) {
    q <= 100 :=> "Quota limit exceeded";
    q >= 80 -> NearLimit;
}

rules FinalCheck(enabled) {
    enabled == true -> Enabled;
}

f:runChecks(n, q, enabled) {
    apply rules(InputCheck(n));
    apply rules(QuotaCheck(q));
    apply rules(FinalCheck(enabled));

    on InputCheck.Positive {
        echo("Input accepted");
    }

    on QuotaCheck.NearLimit {
        echo("Quota near limit");
    }

    on FinalCheck.Enabled {
        echo("Final feature enabled");
    }

    echo("Normal continuation");
}
```

#### Durum A — İkinci RuleSet bariyer hatası üretir

Test girdisi: `n = 5`, `q = 120`, `enabled = true`.

Beklenen yürütme:

1. İlk `apply rules(InputCheck(n))` kendi hata biriktiricisini oluşturur; bariyer koşulu hatasızdır.
2. `InputCheck.Positive` koşulu değerlendirilir ve eşleşir; ilgili handler çalışarak `Input accepted` çıktısını üretir.
3. İkinci `apply rules(QuotaCheck(q))` yeni ve bağımsız bir hata biriktiricisi oluşturur.
4. `QuotaCheck` bariyeri doğru olur; hata kaydı eklenir. Bariyer aşaması tamamlanınca hata raporlanır ve fonksiyonun normal CFG yolu sonlandırılır.
5. Üçüncü `apply rules(FinalCheck(enabled))` çağrısına ulaşılmaz; `FinalCheck.Enabled` koşulu değerlendirilmez ve handler çalışmaz.
6. `Normal continuation` çıktısı üretilmez.

Beklenen gözlemlenebilir çıktı:

```text
Input accepted
Quota limit exceeded
```

İkinci RuleSet başarısız olduğunda ilk RuleSet'in daha önce tamamlanmış etkileri geri alınmaz. Bu test bir transaction/rollback mekanizması tanımlamaz.

#### Durum B — Bütün RuleSet çağrıları başarılıdır

Test girdisi: `n = 5`, `q = 90`, `enabled = true`.

Beklenen yürütme:

1. `InputCheck` bariyer ve trigger aşamalarını tamamlar; `InputCheck.Positive` handler'ı çalışır.
2. `QuotaCheck` kendi hata biriktiricisiyle değerlendirilir; bariyer hatası yoktur ve `NearLimit` handler'ı çalışır.
3. `FinalCheck` kendi uygulama kapsamıyla değerlendirilir; `Enabled` handler'ı çalışır.
4. Üç çağrı da başarılı tamamlandığı için fonksiyon normal akışına geçer.

Beklenen çıktı:

```text
Input accepted
Quota near limit
Final feature enabled
Normal continuation
```

### 12.14 MR-OIR-006 CFG ve Verifier kontrolleri

**Durum A için beklenen soyut CFG:**

```text
Apply_InputCheck
  -> InputCheck_ErrorListCreate
  -> InputCheck_Barriers
  -> InputCheck_TriggerEval
  -> InputCheck_HandlerStage
  -> Apply_QuotaCheck
       -> QuotaCheck_ErrorListCreate
       -> QuotaCheck_Barriers
       -> QuotaCheck_ErrorListIsEmpty
            true  -> QuotaCheck_TriggerEval
            false -> ReportQuotaErrors
                     -> RuleSetFailureExit

Apply_FinalCheck:
  unreachable after QuotaCheck failure

NormalContinuation:
  unreachable after QuotaCheck failure
```

**Durum B için beklenen soyut CFG:**

```text
Apply_InputCheck
  -> InputCheck_Barriers
  -> InputCheck_TriggerEval
  -> InputCheck_HandlerStage
  -> Apply_QuotaCheck
       -> QuotaCheck_Barriers
       -> QuotaCheck_TriggerEval
       -> QuotaCheck_HandlerStage
       -> Apply_FinalCheck
            -> FinalCheck_Barriers
            -> FinalCheck_TriggerEval
            -> FinalCheck_HandlerStage
            -> ApplyContinue
```

Verifier ve yürütme altyapısı aşağıdakileri doğrulamalıdır:

1. Her `apply rules` çağrısı kendi uygulama kimliğine, parametre bağlamına, hata biriktiricisine ve normal trigger eşleşme sonuçlarına sahiptir.
2. Çağrı argümanları ilgili `apply rules` noktasında değerlendirilir; sonraki çağrıların argümanları önceki çağrıdan bağımsızdır.
3. RuleSet çağrıları fonksiyon kaynak sırasına göre CFG'ye yerleştirilir.
4. Başarılı bir çağrının canlı çıkışı yalnızca bir sonraki `apply rules` çağrısına veya son çağrıysa normal devam bloğuna bağlanır.
5. Bir çağrının bariyer hatası yalnızca o çağrının bariyerlerinin tamamı değerlendirildikten sonra raporlanır.
6. Başarısız çağrının trigger koşulları ve handler'ları çalıştırılmaz.
7. Başarısız çağrıdan sonra gelen `apply rules` çağrıları erişilemez olmalıdır.
8. Önceki başarılı çağrının handler etkileri, sonraki çağrının başarısızlığı nedeniyle OIR kontrol akışında geriye dönük silinmez veya yeniden yürütülmez.
9. Her RuleSet'in hata listesi ve trigger SSA değerleri başka bir RuleSet'in değerleriyle karıştırılmaz.
10. Bütün çağrılar başarılıysa son çağrının canlı çıkışı normal devam bloğuna ulaşır.

Analyzer, OIR Builder ve Verifier negatif testleri:

- **MR-OIR-006-N1:** Kaynakta ikinci sırada olan RuleSet'in ilk sırada yürütülmesi.
- **MR-OIR-006-N2:** Bir RuleSet'in hata biriktiricisinin sonraki RuleSet tarafından yeniden kullanılması.
- **MR-OIR-006-N3:** İlk RuleSet'in trigger eşleşme SSA değerinin sonraki RuleSet'e sızması.
- **MR-OIR-006-N4:** Başarısız RuleSet'in kendi trigger koşullarının değerlendirilmesi.
- **MR-OIR-006-N5:** Başarısız RuleSet'in kendi handler'larından birinin çalıştırılması.
- **MR-OIR-006-N6:** Bariyer hatası sonrasında sonraki `apply rules` çağrısına CFG kenarı bulunması.
- **MR-OIR-006-N7:** Bariyer hatası sonrasında fonksiyon normal devam bloğuna CFG kenarı bulunması.
- **MR-OIR-006-N8:** Bir RuleSet'in bariyer hatasının, önceki başarılı RuleSet'in tamamlanmış handler'ını yeniden yürütmesi veya geriye dönük iptal etmesi.
- **MR-OIR-006-N9:** RuleSet parametrelerinin yanlış çağrı noktasından veya başka bir RuleSet'in parametre bağlamından alınması.
- **MR-OIR-006-N10:** Bütün RuleSet çağrıları başarılı olduğu halde son çağrıdan normal devam bloğuna canlı CFG yolu bulunmaması.

Bu test, çağrıların sıralı kontrol akışını ve uygulama kapsamlarının ayrılığını tanımlar. RuleSet'ler arası transaction, rollback veya hata kurtarma davranışı bu testin kapsamında değildir; ayrıca tasarlanmadıkça böyle bir davranış varsayılmaz.

### 12.15 MR-OIR-007 — Aynı RuleSet'in farklı parametrelerle tekrar uygulanması

**Amaç:** Aynı fonksiyon içinde aynı RuleSet'e yapılan birden fazla `apply rules` çağrısının her birinin kendi argüman değerleri, trigger eşleşme sonuçları ve handler dispatch kapsamıyla çalıştığını doğrulamak. Bir `on RuleSet.Trigger` handler tanımı, aynı fonksiyon kapsamındaki ilgili RuleSet uygulamalarının her biri için kullanılabilir; ancak handler'ın çalışıp çalışmayacağı her uygulamanın kendi trigger sonucuna bağlıdır.

Kaynak test:

```nexus
rules ThresholdMode(value) {
    value >= 10 -> High;
    value < 10 -> Low;
}

f:checkTwoValues(first, second) {
    apply rules(ThresholdMode(first));
    apply rules(ThresholdMode(second));

    on ThresholdMode.Low {
        echo("Low handler");
    }

    on ThresholdMode.High {
        echo("High handler");
    }

    echo("Normal continuation");
}
```

#### Durum A — İki çağrıda farklı trigger eşleşir

Test girdisi: `first = 15`, `second = 4`.

Beklenen yürütme:

1. İlk çağrı `ThresholdMode(first)`, `first` argümanını kendi çağrı noktasında değerlendirir.
2. İlk uygulamanın `High` koşulu doğru, `Low` koşulu yanlış olur.
3. İlk uygulama için yalnızca `High` handler'ı çalışır.
4. İkinci çağrı `ThresholdMode(second)), `second` argümanını kendi çağrı noktasında değerlendirir.
5. İkinci uygulamanın `High` koşulu yanlış, `Low` koşulu doğru olur.
6. İkinci uygulama için yalnızca `Low` handler'ı çalışır.
7. Her iki uygulama başarılı tamamlandığından fonksiyon normal akışına devam eder.

Handler tanımlarının kaynak sırası `Low` sonra `High` olmasına rağmen ilk çağrının `High` handler'ı, ikinci çağrının `Low` handler'ı kendi uygulama noktasında yürütülür. Handler kaynak sırası yalnızca aynı uygulamada birden fazla handler eşleştiğinde aralarındaki sırayı belirler.

Beklenen çıktı:

```text
High handler
Low handler
Normal continuation
```

#### Durum B — Aynı trigger iki ayrı çağrıda da eşleşir

Test girdisi: `first = 12`, `second = 20`.

Beklenen çıktı:

```text
High handler
High handler
Normal continuation
```

Bu durum, aynı trigger kimliğinin iki farklı uygulamada eşleşmesinin tek bir global eşleşmeye indirgenmediğini ve handler'ın her uygulama için ayrı yürütüldüğünü doğrular.

#### Durum C — İlk çağrıda eşleşen trigger, ikinci çağrıda eşleşmez

Test girdisi: `first = 18`, `second = 2`.

Beklenen çıktı:

```text
High handler
Low handler
Normal continuation
```

Bu durum, ilk çağrının `High` eşleşme bilgisinin ikinci çağrıya sızmadığını; ikinci çağrının kendi `Low` sonucunu kullandığını doğrular.

### 12.16 MR-OIR-007 CFG ve Verifier kontrolleri

**Durum A için beklenen soyut CFG:**

```text
Apply_ThresholdMode_0(first)
  -> BindArgs_0
  -> Eval_High_0 -> matched_high_0
  -> Eval_Low_0  -> matched_low_0
  -> HandlerDispatch_0
       -> matched_high_0 ? HighHandler : NextHandler
       -> matched_low_0  ? LowHandler  : Apply_ThresholdMode_1

Apply_ThresholdMode_1(second)
  -> BindArgs_1
  -> Eval_High_1 -> matched_high_1
  -> Eval_Low_1  -> matched_low_1
  -> HandlerDispatch_1
       -> matched_high_1 ? HighHandler : NextHandler
       -> matched_low_1  ? LowHandler  : ApplyContinue

ApplyContinue
```

CFG şeması, handler gövdelerinin ilgili uygulama noktasına inline/lower edildiğini gösterir. Aynı kaynak handler gövdesi iki farklı dispatch noktasında kullanılabilir; bu, iki uygulamanın aynı SSA değerlerini veya aynı runtime eşleşme durumunu paylaştığı anlamına gelmez.

Verifier ve yürütme altyapısı aşağıdakileri doğrulamalıdır:

1. Her `apply rules` çağrısı ayrı bir uygulama kimliği ve argüman bağlama kapsamı alır.
2. İlk çağrının argüman SSA değeri ikinci çağrının parametresi olarak kullanılamaz; her çağrı kendi argümanını bağlar.
3. Her çağrının normal trigger koşulları kendi argüman bağlamıyla ve yalnızca bir kez değerlendirilir.
4. Her çağrının trigger eşleşme SSA değerleri diğer çağrının değerlerinden ayrıdır.
5. Aynı `on RuleSet.Trigger` handler tanımı, ilgili her çağrı noktasında o çağrının eşleşme sonucuna göre dispatch edilir.
6. Bir çağrıda eşleşmeyen trigger, başka bir çağrıda eşleşti diye o çağrının handler'ını çalıştıramaz.
7. Aynı trigger iki uygulamada da eşleşirse handler her iki uygulamada birer kez çalışır; uygulamalar arasında global deduplication yapılmaz.
8. Her uygulamanın handler dispatch'i, sonraki `apply rules` çağrısından önce tamamlanır.
9. Son uygulamanın canlı çıkışı fonksiyonun normal devam bloğuna ulaşır.
10. Handler kaynak sırası, tek bir uygulamadaki eşleşmiş handler'lar için korunur; farklı uygulamaların sırası ise `apply rules` kaynak sırasıdır.
11. Bir uygulama bariyer hatasıyla sonlanırsa o uygulamanın trigger/handler aşamaları ve sonraki uygulamalar erişilemez olur; bu davranış MR-OIR-006 ile tutarlıdır.
12. Handler gövdesi bir terminator ile sonlanırsa o uygulamanın sonraki handler'larına veya sonraki `apply rules` çağrısına sahte CFG kenarı oluşturulmaz.

Analyzer ve Verifier negatif testleri:

- **MR-OIR-007-N1:** İkinci uygulamanın argüman bağlamında ilk uygulamanın parametre SSA değerinin kullanılması.
- **MR-OIR-007-N2:** İki uygulamanın trigger eşleşme SSA değerlerinin aynı kimlik/değer alanını paylaşması.
- **MR-OIR-007-N3:** İlk uygulamada eşleşen trigger'ın ikinci uygulamada da otomatik eşleşmiş sayılması.
- **MR-OIR-007-N4:** İki uygulamada da eşleşen aynı trigger handler'ının yalnızca bir kez çalıştırılması.
- **MR-OIR-007-N5:** İkinci uygulamanın handler dispatch'inin ilk uygulama tamamlanmadan yürütülmesi.
- **MR-OIR-007-N6:** Handler tanımının yalnızca ilk `apply rules` çağrısına bağlanıp sonraki uyumlu çağrılarda göz ardı edilmesi.
- **MR-OIR-007-N7:** Handler tanımının aynı uygulama içinde birden fazla kez dispatch edilmesi.
- **MR-OIR-007-N8:** Farklı uygulamaların sıralı yürütülmesi yerine trigger türüne göre gruplanması.
- **MR-OIR-007-N9:** İlk uygulamadaki handler terminator'ından sonra ikinci `apply rules` çağrısına sahte CFG kenarı oluşturulması.
- **MR-OIR-007-N10:** Her iki uygulama başarılı olduğu halde son uygulamadan normal devam bloğuna canlı CFG yolu bulunmaması.

Bu test, aynı RuleSet'in bir fonksiyon içinde tekrar uygulanmasını kapsar. Farklı fonksiyonlarda ortak RuleSet tanımının paylaşılması ve ayrı fonksiyonlardaki handler kapsamları bu testin konusu değildir.

## 12.17 MR-OIR-008 — Handler içindeki break / continue ve loop kapsamı

**Amaç:** Bir `on` handler içindeki `break` ve `continue` ifadelerinin gerçek NEXUS Flow `loop` kapsamına bağlandığını; handler'ın `apply rules` noktasında çalıştırılmasının yeni veya yapay bir döngü kapsamı oluşturmadığını doğrulamak.

### Kesinleştirilen kapsam kuralı

1. `break` ve `continue`, kaynak kodda bulundukları noktayı sözdizimsel olarak kapsayan en yakın `loop` yapısına bağlanır.
2. `on` handler gövdesi, `apply rules` noktasında CFG'ye yerleştirilse bile lexical loop kapsamını handler'ın kaynak kod konumundan alır.
3. Handler bir `loop` gövdesinin içindeyse `break` / `continue` o döngüye bağlanır.
4. Handler herhangi bir `loop` kapsamının dışındaysa, ilgili `apply rules` çağrısı başka bir yerde döngü içinde olsa bile handler'daki `break` / `continue` geçersizdir.
5. İç içe döngülerde hedef, handler'ın kaynak konumunu kapsayan en içteki `loop` yapısıdır. Dış döngüye doğrudan atlamak için bu iki kontrol ifadesi kullanılmaz.
6. Handler içindeki `break` veya `continue` gerçek bir terminator akışı üretir. Sonlandırılmış handler yolundan sonraki handler'a, `always` / `default` aşamasına, sonraki `apply rules` çağrısına veya normal devam bloğuna sahte kenar eklenemez.

Bu kural, NEXUS Flow Syntax Reference içindeki “`break` ve `continue` yalnızca geçerli bir loop kapsamı içindeyken kullanılabilir” şartıyla aynıdır. Multi-Rule Engine bu kapsam kuralını değiştirmez.

### Kaynak test

```nexus
rules LoopControl(value) {
    value == 1 -> SkipIteration;
    value == 2 -> StopLoop;
}

f:testLoopControl() {
    v:i = 0;

    loop(i, i < 3, i++) {
        apply rules(LoopControl(i));

        on LoopControl.SkipIteration {
            continue;
        }

        on LoopControl.StopLoop {
            break;
        }

        echo("Normal iteration tail");
    }

    echo("After loop");
}
```

### Beklenen yürütme

| Döngü değeri | Eşleşen trigger | Handler davranışı | Sonuç |
|---:|---|---|---|
| 0 | Yok | Handler çalışmaz | Döngü sonu çıktısı çalışır, sonra artış |
| 1 | `SkipIteration` | `continue` | Normal iteration tail atlanır; loop'un sonraki iterasyonuna geçilir |
| 2 | `StopLoop` | `break` | Döngü sonu çıktısı atlanır; döngüden çıkılır |

Beklenen çıktı:

```text
Normal iteration tail
After loop
```

`continue`, `loop(i, condition, increment)` yapısında döngünün sonraki iterasyonuna geçiş semantiğini izler; `break` ise döngü çıkış bloğuna gider. Bu kontrol aktarımı RuleSet uygulamasının normal devamı değildir.

### Beklenen soyut CFG

```text
LoopHeader
  -> LoopBody
       -> Apply_LoopControl
            -> BarrierStage
            -> TriggerEvaluation
            -> HandlerDispatch
                 -> matched_skip -> ContinueTerminator -> LoopLatch
                 -> matched_stop -> BreakTerminator -> LoopExit
                 -> no_match     -> ApplyContinue
       -> IterationTail
       -> LoopLatch
  -> LoopExit
       -> AfterLoop
```

Şema, yalnızca canlı yolları gösterir. `ContinueTerminator` ve `BreakTerminator` yolları `IterationTail` veya `ApplyContinue` üzerinden devam etmez. Gerçek OIR'de terminator isimleri nihai opcode kararı değildir; bunlar CFG hedeflerini açıklayan tasarım etiketleridir.

### Verifier kontrolleri

1. Handler'ın kaynak konumunda geçerli bir lexical loop kapsamı bulunmalıdır.
2. `continue` kenarı aynı kapsamın loop latch / devam kontrol noktasına bağlanmalıdır.
3. `break` kenarı aynı kapsamın loop exit bloğuna bağlanmalıdır.
4. Handler içindeki terminator sonrasında aynı uygulamanın sonraki handler'larına kenar bulunmamalıdır.
5. `continue` yolundan iteration tail veya `ApplyContinue` bloğuna sahte kenar bulunmamalıdır.
6. `break` yolundan iteration tail, loop latch veya `ApplyContinue` bloğuna sahte kenar bulunmamalıdır.
7. Terminator olmayan handler yolları, sonraki handler dispatch aşamasına normal şekilde bağlanabilmelidir.
8. Döngüden çıkış sonrası kod yalnızca `break` çıkışından veya döngünün normal bitiş koşulundan erişilebilir olmalıdır.

### Negatif testler

- **MR-OIR-008-N1:** Loop kapsamı dışındaki bir `on` handler içinde `break` kullanılması reddedilir.
- **MR-OIR-008-N2:** Loop kapsamı dışındaki bir `on` handler içinde `continue` kullanılması reddedilir.
- **MR-OIR-008-N3:** `continue` kenarının loop exit bloğuna bağlanması.
- **MR-OIR-008-N4:** `break` kenarının loop latch bloğuna bağlanması.
- **MR-OIR-008-N5:** `continue` terminator'ından sonra iteration tail'e sahte kenar oluşturulması.
- **MR-OIR-008-N6:** `break` terminator'ından sonra sonraki handler'a sahte kenar oluşturulması.
- **MR-OIR-008-N7:** Handler'daki terminator sonrasında `always` veya `default` aşamasına sahte kenar oluşturulması.
- **MR-OIR-008-N8:** Handler'ın lexical loop kapsamı yerine yalnızca `apply rules` çağrısının bulunduğu başka bir loop'un hedeflenmesi.
- **MR-OIR-008-N9:** İç içe loop içinde handler'ın en yakın lexical loop yerine dış loop'a bağlanması.
- **MR-OIR-008-N10:** `break` / `continue` yolunun normal `RETURN` veya RuleSet bariyer hata çıkışıyla karıştırılması.

### Test sınıflandırması

- **Frontend/Parser:** `on` gövdesindeki kontrol ifadelerinin AST'de korunması.
- **Analyzer:** Handler kaynak konumundaki lexical loop kapsamının çözülmesi; kapsam yoksa hata verilmesi.
- **OIR Builder:** Doğru loop latch / exit hedeflerinin handler terminator'larına bağlanması.
- **OIR Verifier:** Terminator hedeflerinin ve erişilebilirlik kenarlarının doğrulanması.

Bu test, handler'ın lexical loop kapsamını ve temel `break` / `continue` hedeflerini sabitler. Farklı kontrol ifadelerinin (örneğin etiketli `jump(label)`) handler içindeki kapsam davranışı ayrı test konusu olarak kalır.

## 12.18 MR-OIR-009 — Handler içindeki jump(label) ve etiket kapsamı

**Amaç:** Bir `on` handler içindeki etiketli atlamanın, aynı fonksiyonun etiket tablosundaki hedefe bağlandığını; handler'ın `apply rules` noktasına lowering edilmesinin hedef çözümlemesini değiştirmediğini doğrulamak.

### Kesinleştirilen temel kurallar

1. `label:>` ile tanımlanan etiket, onu içeren fonksiyonun etiket kapsamına kaydedilir.
2. `jump(label)` hedefi aynı fonksiyonun etiket tablosunda çözülür. Başka bir fonksiyonun etiketi hedeflenemez.
3. Aynı fonksiyon içinde yinelenen etiket adı Analyzer hatasıdır.
4. Handler içindeki `jump(label)`, handler'ın kaynak kod konumundaki fonksiyon etiket kapsamını kullanır. Handler'ın OIR'de `apply rules` noktasına yerleştirilmesi bu çözümlemeyi değiştirmez.
5. Koşulsuz `jump(label)` kontrol aktarımıdır: hedef bloğa gider ve handler'ın mevcut yolu sonlanır. Bu yoldan sonraki handler, `always`, `default` veya normal devam bloğuna otomatik kenar eklenmez.
6. Bu testte atlama hedefi, kaynak konumdan erişilebilir bir kontrol akışına dışarı doğru veya aynı kapsam seviyesinde geçiş yapar. Daha derin bir lexical scope içine atlayarak o kapsamın giriş/başlatma adımlarını atlamak bu test tarafından desteklenmez; bu tür hedefler Analyzer tarafından reddedilmelidir.
7. Etiketin bulunamaması, fonksiyon dışı etikete başvurulması veya kapsam dışına atlama, Analyzer hatasıdır.

### Kaynak test

```nexus
rules RouteControl(value) {
    value == 1 -> SkipToFinish;
}

f:testLabelJump(value) {
    apply rules(RouteControl(value));

    on RouteControl.SkipToFinish {
        jump(finish);
    }

    echo("Normal continuation");

    finish:>
    echo("Finish label reached");
}
```

### Beklenen yürütme

**Durum A — value = 1**

- `SkipToFinish` eşleşir.
- Handler çalışır ve `jump(finish)` yürütülür.
- `Normal continuation` atlanır.
- Akış `finish:>` etiketinden devam eder.

Beklenen çıktı:

```text
Finish label reached
```

**Durum B — value = 0**

- Normal trigger eşleşmez.
- Handler çalışmaz.
- `apply rules` normal tamamlanır.
- Normal devam satırı çalışır ve ardından etiketli kod doğal akışla yürütülür.

Beklenen çıktı:

```text
Normal continuation
Finish label reached
```

### Beklenen soyut CFG

```text
ApplyEntry
  -> BarrierStage
  -> TriggerEvaluation
  -> HandlerDispatch
       matched_SkipToFinish
         -> SkipToFinish_Handler
         -> JUMP finish
       no_match
         -> ApplyContinue
         -> NormalContinuation
         -> finish

finish:
  -> FinishLabelBody
  -> FunctionContinuation
```

Bu şemada `JUMP finish` terminator'ından `NormalContinuation` bloğuna kenar bulunmaz. `no_match` yolu ise normal fonksiyon akışını korur. Hedef etiket tek bir CFG basic block girişine bağlanmalıdır.

### Analyzer ve Verifier kontrolleri

1. Her `label:>` tanımı fonksiyonun etiket sembol tablosuna eklenmelidir.
2. Aynı fonksiyonda yinelenen etiket adı reddedilmelidir.
3. `jump(label)` hedefi aynı fonksiyonda çözülmelidir.
4. Tanımsız veya başka fonksiyona ait etiket hedefi reddedilmelidir.
5. Handler'ın kaynak kodunda çözülen hedef, lowering sonrasında aynı hedef basic block'a bağlanmalıdır.
6. `JUMP` terminator'ının tek CFG hedefi etiket bloğu olmalıdır.
7. `JUMP` sonrasında handler dispatch kuyruğuna veya `ApplyContinue` bloğuna sahte kenar bulunmamalıdır.
8. Etiket hedef bloğunun predecessor listesi gerçek atlama ve doğal fallthrough kenarlarıyla tutarlı olmalıdır.
9. Atlamanın hedefi daha derin bir lexical scope içine giriyorsa ve scope giriş adımlarını atlıyorsa Analyzer bunu reddetmelidir.
10. Etiketli atlama, `RETURN`, `break`, `continue` ve RuleSet hata çıkışıyla aynı terminator türü olarak modellenmemelidir.

### Negatif testler

- **MR-OIR-009-N1:** Tanımlanmamış etikete `jump(label)` yapılması reddedilir.
- **MR-OIR-009-N2:** Başka fonksiyonda tanımlı etikete atlama reddedilir.
- **MR-OIR-009-N3:** Aynı fonksiyon içinde aynı etiket adının iki kez tanımlanması reddedilir.
- **MR-OIR-009-N4:** Handler'daki `jump(label)` hedefinin, lowering sırasında yanlışlıkla handler'ın hemen sonrasındaki bloğa çevrilmesi.
- **MR-OIR-009-N5:** `JUMP` terminator'ından sonraki handler'a sahte CFG kenarı oluşturulması.
- **MR-OIR-009-N6:** `JUMP` terminator'ından `ApplyContinue` bloğuna sahte CFG kenarı oluşturulması.
- **MR-OIR-009-N7:** `JUMP` hedefinin aynı fonksiyon içindeki etiket yerine başka fonksiyonun basic block'una bağlanması.
- **MR-OIR-009-N8:** Etiketli atlamanın daha derin bir lexical scope'a girerek scope giriş/başlatma adımlarını atlaması.
- **MR-OIR-009-N9:** Etiket bloğunun predecessor listesi ile gerçek CFG kenarlarının uyuşmaması.
- **MR-OIR-009-N10:** `JUMP` işleminin normal fallthrough üreten bir ifade gibi modellenmesi.

### Açık bırakılan sözdizimi: label?>

Syntax Reference içinde `label?>` biçimi de belgelenmiştir; ancak mevcut kısa açıklama, koşulun nasıl yazıldığını ve koşul sağlanmadığında akışın tam olarak nereye gittiğini tek başına kesinleştirmeye yetmemektedir. Bu nedenle MR-OIR-009 yalnızca açıkça örneklenmiş `jump(label)` biçiminin kontrol akışını doğrular. `label?>` için ayrı bir parser/semantik kararı verilmeden OIR lowering kuralı varsayılmayacaktır.

### Test sınıflandırması

- **Frontend/Parser:** `label:>` ve `jump(label)` AST düğümlerinin oluşturulması.
- **Analyzer:** Fonksiyon etiket tablosu, benzersizlik, hedef çözümleme ve scope geçişi doğrulaması.
- **OIR Builder:** Etiket hedefinin basic block'a bağlanması ve handler jump'ının ilgili apply noktasına lowering edilmesi.
- **OIR Verifier:** JUMP terminator hedefi, predecessor/successor tutarlılığı ve terminator sonrası sahte kenarların reddedilmesi.



## 12.19 MR-OIR-010 — İleri / geri yönlü jump ve döngüsel CFG

**Amaç:** `jump(label)` komutunun kaynak kod sırasına bağlı olmayan, fonksiyon içindeki etiketli basic block'lar arasında koşulsuz kontrol aktarımı olduğunu doğrulamak. Test; ileri atlama, geri atlama, birden fazla etiket arasında yön değiştirme, koşullu geri dönüş ve oluşan CFG döngüsünü kapsar.

### Kesinleştirilen kurallar

1. `jump(label)`, hedef etiket aynı fonksiyonun etiket tablosunda çözüldüğü sürece, kaynak kodda daha ileride veya daha önce tanımlanmış bir etikete gidebilir.
2. İleri ve geri yön, kaynak dosyadaki metinsel sıraya göre belirlenir; bu ayrım komutun semantiğini değiştirmez.
3. Her `jump(label)` OIR'de hedef basic block'a giden tek hedefli bir `JUMP` terminator'ıdır. Terminator sonrasında aynı kaynak yolunda doğal fallthrough kenarı bulunmaz.
4. Geri atlama CFG'de çevrim oluşturabilir. Bu çevrim bir `loop` sözdizimi olmak zorunda değildir; Analyzer ve Verifier bunu geçerli bir kontrol akışı çevrimi olarak ele almalıdır.
5. Bir çevrimin sonlanması gerekiyorsa koşullu kontrol veya başka bir kontrol akışı yolu çevrimden çıkışı sağlamalıdır. Koşulsuz geri atlama kendi başına sonlanma garantisi vermez.
6. Etiket hedefleri kaynak kod sırasına göre değil, fonksiyon kapsamındaki etiket sembol tablosuna göre çözülür. Aynı fonksiyon içindeki benzersizlik, fonksiyon dışına çıkmama ve lexical scope güvenliği MR-OIR-009 kuralları olarak geçerliliğini korur.
7. Handler içindeki `jump(label)` de aynı fonksiyon etiket tablosunu kullanır. Handler'ın `apply rules` noktasına lowering edilmesi, ileri/geri hedef çözümlemesini değiştirmez.
8. `jump`, `break`, `continue` veya `return` ile eş anlamlı değildir. Hedef etikete transfer eder; hedefin bir döngü çıkışı veya döngü başı olması komutun türünü değiştirmez.

### Kaynak test

Etiketlerin kaynak sırası özellikle atlama yönlerinin ikisini de oluşturacak biçimde düzenlenmiştir:

```nexus
rules RouteBack(value) {
    value == 1 -> StartRoute;
}

f:testBidirectionalJump(value) {
    v:i = 0;

    apply rules(RouteBack(value));

    on RouteBack.StartRoute {
        jump(label1);
    }

    jump(label3);

    label2:>
    i = i + 1;
    (i < 3) ? jump(label1);
    jump(label3);

    label1:>
    i = i + 1;
    jump(label2);

    label3:>
    echo(i);
}
```

Kaynak sırası `label2` → `label1` → `label3` şeklindedir. Buna rağmen akışta hem ileri hem geri yönlü atlamalar bulunur.

### Beklenen yürütme

**Durum A — value = 1**

1. `StartRoute` trigger'ı eşleşir.
2. Handler, `label1` hedefine atlar.
3. `label1` içindeki `jump(label2)`, kaynak kodda daha önce bulunan etikete geri döner.
4. `label2` içindeki koşul doğru olduğu sürece `jump(label1)` ile ileri yönde tekrar geçiş yapılır.
5. Koşul yanlış olduğunda `jump(label3)` ile çevrimden çıkılır.

Beklenen çıktı:

```text
4
```

**Durum B — value = 0**

1. Trigger eşleşmez; handler çalışmaz.
2. Normal akıştaki `jump(label3)` yürütülür.
3. Ara etiket blokları atlanır.

Beklenen çıktı:

```text
0
```

### Beklenen soyut CFG

```text
ApplyEntry
  -> BarrierStage
  -> TriggerEvaluation
  -> HandlerDispatch
       matched_StartRoute
         -> StartRoute_Handler
         -> JUMP label1
       no_match
         -> ApplyContinue
         -> JUMP label3

label2:
  -> Increment_i
  -> BRANCH (i < 3)
       true  -> JUMP label1
       false -> JUMP label3

label1:
  -> Increment_i
  -> JUMP label2

label3:
  -> Echo_i
  -> FunctionContinuation
```

CFG'de `label1 -> label2 -> label1` çevrimi vardır. `label2` koşulunun false kolu `label3` çıkışına ulaşır. `no_match` yolu da doğrudan `label3` hedefine gider. Kaynak kodda etiketlerin sıralanışı bu CFG bağlantılarını sınırlamaz.

### Analyzer ve Verifier kontrolleri

1. İleri ve geri hedefler aynı fonksiyonun etiket tablosunda çözülmelidir.
2. Etiket çözümlemesi kaynak sırasına göre yalnızca ileri arama yapmamalıdır; fonksiyon etiket tablosu tüm etiketleri kapsamalıdır.
3. Her koşulsuz `jump(label)` tek hedefli `JUMP` terminator'ına dönüşmelidir.
4. `JUMP` terminator'ından kaynak kodun bir sonraki satırına sahte fallthrough kenarı oluşturulmamalıdır.
5. Geri atlama nedeniyle oluşan CFG çevrimi geçerli predecessor/successor ilişkileriyle temsil edilmelidir.
6. Döngüsel CFG üzerinde SSA değerleri, ilgili block parameter / merge değerleriyle doğru taşınmalıdır; eski iterasyona ait SSA değeri yanlışlıkla yeniden kullanılmamalıdır.
7. Koşullu kontrolün true ve false yolları ayrı CFG kenarlarına bağlanmalıdır. False kolu sonraki kaynak statement'ından devam eder.
8. Handler'dan yapılan atlama da aynı hedef basic block'a bağlanmalı; handler dispatch kuyruğuna veya `ApplyContinue` bloğuna sahte kenar oluşturmamalıdır.
9. Etiketin bulunamaması, yinelenmesi, başka fonksiyona ait olması veya yasaklı daha derin lexical scope'a girmesi MR-OIR-009 kapsamında reddedilmelidir.
10. Verifier, CFG'nin kaynak satır sırasına göre monoton ilerlemesini şart koşmamalıdır. İleri ve geri kenarlar geçerlidir; geçerlilik terminator, hedef, kapsam ve SSA kurallarıyla belirlenir.

### Negatif testler

- **MR-OIR-010-N1:** Analyzer'ın yalnızca ileri yöndeki etiketleri çözümleyip daha önce tanımlanmış hedefi reddetmesi.
- **MR-OIR-010-N2:** Geri yönlü `JUMP` kenarının yanlış basic block'a bağlanması.
- **MR-OIR-010-N3:** `JUMP` terminator'ından kaynak kodun sonraki satırına fallthrough kenarı eklenmesi.
- **MR-OIR-010-N4:** Geri atlamayla oluşan CFG çevriminin Verifier tarafından yalnızca çevrim içerdiği için reddedilmesi.
- **MR-OIR-010-N5:** Döngüsel CFG'de block parameter / SSA merge değerinin eksik veya yanlış bağlanması.
- **MR-OIR-010-N6:** Koşullu kontrolün false kolunun kaybolması veya yanlışlıkla hedef etikete bağlanması.
- **MR-OIR-010-N7:** Handler içindeki ileri/geri `jump` hedefinin handler'ın kaynak fonksiyon kapsamı yerine yapay handler kapsamından çözülmesi.
- **MR-OIR-010-N8:** Handler'daki `JUMP` sonrasında sonraki handler'a veya `ApplyContinue` bloğuna sahte kenar oluşturulması.
- **MR-OIR-010-N9:** `jump` komutunun geri hedefe gittiği için otomatik olarak `continue` veya `break` sayılması.
- **MR-OIR-010-N10:** Koşulsuz geri atlamanın kendiliğinden sonlanacağı varsayılması veya Verifier'ın sonlanma garantisi üretmesi.

### Test sınıflandırması

- **Frontend/Parser:** Etiket bildirimleri, `jump(label)` ve koşullu kontrol içindeki jump ifadesinin AST'de korunması.
- **Analyzer:** Fonksiyon çapında etiket tablosu, ileri/geri hedef çözümlemesi ve lexical scope denetimi.
- **OIR Builder:** Kaynak sırasından bağımsız basic block bağlantıları, koşullu kenarlar ve geri atlamalar.
- **OIR Verifier:** CFG çevrimleri, predecessor/successor tutarlılığı, terminator kuralları ve SSA/block parameter doğrulaması.

Bu test, etiketli atlamanın ileri/geri yönlü olabileceğini ve CFG'nin çevrim içerebileceğini sabitler. Çevrim analizi, dominator hesaplaması, loop recognition ve optimizasyonların ayrıntıları ilgili OIR/Optimizer spesifikasyonlarında ayrıca ele alınacaktır.


## 12.20 MR-OIR-011 — Dominator analizi, doğal döngü ve güvenli CFG optimizasyonu

**Amaç:** MR-OIR-010 ile kabul edilen ileri/geri yönlü etiketli atlamaların oluşturduğu CFG üzerinde dominator ilişkilerinin doğru hesaplanmasını; doğal döngülerin yalnızca gerçek dominance koşuluyla tanınmasını ve optimizer'ın döngüleri kanıt olmadan yeniden yazmamasını doğrulamak.

Bu test, optimizer'ın gerçek bir uygulamasının yazıldığı anlamına gelmez. OIR Builder, Verifier ve ilerideki Optimization DLL için normatif analiz sözleşmesini tanımlar.

### 12.20.1 CFG ve dominator kuralları

1. Dominator analizi her fonksiyonun giriş basic block'undan başlar. Bir block, girişten o block'a ulaşan **bütün** yollar üzerinden geçmek zorundaysa giriş ve ilgili ara block'lar tarafından dominate edilir.
2. Giriş block'u yalnızca kendisini dominate eder; diğer erişilebilir block'ların dominator kümeleri sabit noktaya ulaşılana kadar iteratif hesaplanır.
3. Erişilebilir giriş dışı block için temel denklem:
   `Dom(B) = {B} ∪ intersection(Dom(P))`
   Burada `P`, B'nin erişilebilir predecessor block'larıdır.
4. Dominator kümeleri CFG'deki gerçek predecessor kenarlarından hesaplanır. Kaynak kod satır sırası, etiketlerin metinsel konumu veya block ID sırası dominance ölçütü değildir.
5. Geri atlama, hedef block kaynak block'u dominate ediyorsa **natural-loop backedge** olarak sınıflandırılabilir. Yalnızca kaynak dosyada daha önceki bir etikete gitmesi, kenarı tek başına natural-loop backedge yapmaz.
6. Natural loop'un header'ı backedge hedefidir. Loop body, header ve backedge kaynağından predecessor yönünde header'a kadar geri yürünerek belirlenir.
7. Bir strongly connected component (SCC) çevrim içerdiği halde tek bir block diğer çevrim block'larını dominate etmiyorsa, bu SCC otomatik olarak tek header'lı natural loop sayılmaz. Böyle bir yapı irreducible CFG olabilir.
8. Girişten erişilemeyen block'lar dominator ağacına dahil edilmez. Bunlar açıkça unreachable olarak sınıflandırılır; erişilemeyen block için sahte dominator ilişkisi üretilmez. Unreachable block'ların kaldırılıp kaldırılmayacağı ayrı dead-code/side-effect politikasıdır.
9. Dominator ve SCC analizleri farklı soruları cevaplar: dominance tüm giriş yollarındaki zorunlu geçişi; SCC ise karşılıklı erişilebilir çevrimli block kümelerini gösterir. Birinin sonucu diğerinin yerine kullanılamaz.
10. CFG'de çevrim bulunması tek başına hata değildir ve optimizer'a çevrimi kaldırma, sonsuz döngüyü sonlandırma veya atlamaları yeniden sıralama yetkisi vermez.

### 12.20.2 Kaynak test — Natural loop

```nexus
rules RouteLoop(value) {
    value == 1 -> StartLoop;
}

f:testNaturalLoop(value) {
    v:i = 0;

    apply rules(RouteLoop(value));

    on RouteLoop.StartLoop {
        jump(header);
    }

    jump(exit);

    header:>
    i = i + 1;
    (i < 3) ? jump(body);
    jump(exit);

    body:>
    jump(header);

    exit:>
    echo(i);
}
```

Bu örnekte:
- Trigger eşleşirse handler `header` block'una gider.
- `header`, koşul doğru oldukça `body` block'una geçer.
- `body`, `jump(header)` ile geri döner.
- `header` koşulu false olduğunda `exit` block'una gidilir.
- Trigger eşleşmezse ilk normal akıştaki `jump(exit)` çalışır.

**Durum A — value = 1**

Beklenen çıktı:

```text
3
```

**Durum B — value = 0**

Beklenen çıktı:

```text
0
```

### 12.20.3 Beklenen CFG ve dominance sonucu

```text
FunctionEntry
  -> ApplyEntry
  -> TriggerDispatch
       matched -> StartLoop_Handler -> JUMP header
       no_match -> JUMP exit

header:
  -> Increment_i
  -> BRANCH (i < 3)
       true  -> JUMP body
       false -> JUMP exit

body:
  -> JUMP header

exit:
  -> Echo_i
  -> FunctionReturn
```

Erişilebilir block'lar için beklenen temel ilişkiler:

| Block | Dominator ilişkisi |
|---|---|
| `FunctionEntry` | Yalnızca kendisi |
| `ApplyEntry` | `FunctionEntry` ve kendisi |
| `TriggerDispatch` | `FunctionEntry`, `ApplyEntry`, kendisi |
| `StartLoop_Handler` | Önceki zorunlu giriş/dispatch block'ları ve kendisi |
| `header` | Handler yolundan geldiği için `StartLoop_Handler` ve önceki zorunlu block'lar |
| `body` | `header` tarafından dominate edilir |
| `exit` | `TriggerDispatch` tarafından dominate edilir; `header` tarafından dominate edilmez |

**Natural loop sonucu:**
- Backedge: `body -> header`
- Header: `header`
- Loop body: `{header, body}`
- `header`, `body` block'unu dominate eder.
- `header`, `exit` block'unu dominate etmez; çünkü no-match yolu doğrudan `exit` hedefine gider.

### 12.20.4 Irreducible CFG varyantı

Verifier/Optimizer test altyapısı, yukarıdaki CFG'nin ayrı bir yapısal varyantını da üretmelidir: `TriggerDispatch` block'undan `body` block'una ek bir erişilebilir giriş kenarı eklenir.

Bu varyantta:
- `header` block'una handler ve `body` üzerinden ulaşılır.
- `body` block'una hem `header` hem de `TriggerDispatch` üzerinden ulaşılır.
- `header`, `body` block'unu dominate etmez.
- `body`, `header` block'unu dominate etmez.
- İki block arasında çevrim/SCC bulunur; ancak `body -> header` kenarı dominance koşulunu sağlamadığı için natural-loop backedge olarak sınıflandırılamaz.
- Optimizer bu SCC'yi tek header'lı natural loop varsayarak loop-invariant code motion veya loop rotation uygulayamaz.

Bu varyant, kaynak diline yeni bir kontrol sözdizimi eklemez; CFG analizinin yapısal test girdisidir.

### 12.20.5 Optimizer güvenlik sözleşmesi

1. Dominator ağacı, yalnızca Verifier tarafından doğrulanmış CFG'den üretilmelidir.
2. Bir block'ta tanımlanan SSA değeri, yalnızca tanım block'u kullanıldığı block'u dominate ediyorsa doğrudan kullanılabilir. Kenarlar arası değer aktarımı block parameter / merge kurallarına uymalıdır.
3. Loop-invariant code motion yalnızca natural loop ve bağımlılık kanıtı varsa uygulanabilir.
4. Bellek yazımı, I/O, atomik işlem, hata üretme veya başka gözlemlenebilir yan etkiler içeren işlemler, etkilerinin güvenli taşınabildiği kanıtlanmadan döngü dışına çıkarılamaz.
5. Trap/hata oluşturabilecek işlemler, yürütülme sıklığı veya erişilebilirliği değişecek biçimde kanıt olmadan taşınamaz.
6. Bir `JUMP` terminator'ı, hedefi ve kenar argümanları korunmadan kaldırılıp doğal fallthrough'a dönüştürülemez.
7. Bir SCC'nin varlığı, döngünün sonlanacağına dair kanıt değildir. Optimizer sonlanma garantisi varsayamaz.
8. Constant branch sadeleştirmesi yalnızca koşulun derleme zamanı sabitliği kanıtlandığında yapılabilir; canlı ve ölü kenarlar buna göre güncellenmeli, predecessor listeleri ve block parameter argümanları yeniden doğrulanmalıdır.
9. Unreachable block temizliği, erişilebilirlik hesabı ve yan etki/diagnostic/debug metadata politikası birlikte değerlendirilerek ayrı bir dönüşüm olarak yapılmalıdır.
10. Her CFG dönüşümünden sonra predecessor/successor listeleri, terminator hedefleri, dominance ve SSA/block parameter doğrulaması yeniden çalıştırılmalıdır.

### 12.20.6 Verifier ve Optimizer negatif testleri

- **MR-OIR-011-N1:** Dominator hesabının yalnızca kaynak kod sırasına göre yapılması.
- **MR-OIR-011-N2:** `header` block'unun `exit` block'unu dominate ediyor sayılması; no-match giriş yolunun göz ardı edilmesi.
- **MR-OIR-011-N3:** `header` block'unun `body` block'unu dominate etmediği irreducible varyantta `body -> header` kenarının natural-loop backedge sayılması.
- **MR-OIR-011-N4:** Her SCC'nin otomatik olarak tek header'lı natural loop kabul edilmesi.
- **MR-OIR-011-N5:** Girişten erişilemeyen block'ların dominator ağacına rastgele/varsayılan ilişkilerle eklenmesi.
- **MR-OIR-011-N6:** SSA tanımının kullanıldığı block'u dominate etmediği halde değerin doğrudan kullanılması.
- **MR-OIR-011-N7:** Loop-invariant code motion sırasında bellek yazımı/I/O/atomik işlem gibi yan etkilerin güvenli kanıt olmadan taşınması.
- **MR-OIR-011-N8:** Potansiyel trap/hata üreten işlemin yürütülme davranışının kanıtsız değiştirilmesi.
- **MR-OIR-011-N9:** Geri atlamanın yalnızca geriye yönlü olduğu için kaldırılması veya fallthrough'a çevrilmesi.
- **MR-OIR-011-N10:** CFG dönüşümünden sonra predecessor/successor veya block parameter argümanlarının güncellenmemesi.
- **MR-OIR-011-N11:** Koşulsuz çevrimin sonlanacağı varsayılarak sonsuz döngünün kaldırılması.
- **MR-OIR-011-N12:** Constant branch dönüşümünden sonra artık erişilemeyen kenarın dominance/SSA analizinde canlı bırakılması.

### 12.20.7 Test sınıflandırması

- **Analyzer:** Kaynak etiketleri ve fonksiyon kapsamı; MR-OIR-009/010 kuralları.
- **OIR Builder:** Basic block ve gerçek kontrol kenarlarının üretilmesi.
- **OIR Verifier:** Girişten erişilebilirlik, predecessor/successor, block parameter ve SSA doğrulaması.
- **CFG Analysis:** Dominator kümeleri, immediate dominator, SCC ve natural-loop backedge sınıflandırması.
- **Optimizer:** Yalnızca kanıtlanmış dönüşümlerin uygulanması; dönüşüm sonrası CFG/SSA doğrulaması.

Bu test, dominance ve natural-loop tanımını sabitler. Dominator algoritmasının performans seçimi (iteratif çözüm, Lengauer–Tarjan veya başka yöntem), optimizer pass sırası ve ayrıntılı alias/memory-effect modeli ileride ayrı tasarım kararları olarak ele alınacaktır.


## 12.21 MR-OIR-012 — Cross-block jump ve güvenli CFG sadeleştirmeleri

**Amaç:** `jump(label)` komutunun temel kullanımının bir basic block'tan çıkıp aynı fonksiyon içindeki başka bir basic block'a kontrol aktarmak olduğunu doğrulamak. Ardından optimizer'ın jump chaining/threading, sabit koşullu branch sadeleştirmesi, erişilemeyen block temizliği ve dead instruction elimination dönüşümlerini CFG semantiğini bozmadan uygulamasını test etmek.

Bu testte `jump` aynı basic block içinde kalmaya zorlanmaz. Hedef etiket farklı bir basic block'un girişidir. Kaynak ve hedef block'lar fonksiyon içinde farklı noktalarda olabilir; ileri/geri yön ve aradaki block sayısı semantiği değiştirmez.

### 12.21.1 Normatif cross-block jump kuralları

1. `jump(label)`, bulunduğu basic block'u sonlandıran koşulsuz kontrol aktarımıdır.
2. Hedef `label:>`, aynı fonksiyonun etiket tablosunda çözülen bir basic block girişini belirtir. Hedefin ayrı bir basic block olması normal ve temel kullanım biçimidir.
3. OIR Builder, `jump(label)` için kaynak block'tan hedef block'a açık bir CFG kenarı üretir. Kaynak block'un terminator'ı `JUMP target` olur.
4. `JUMP` terminator'ından kaynak block'un sonraki kaynak satırına veya metinsel olarak sıradaki block'a fallthrough kenarı üretilmez.
5. Etiketler, fonksiyon içindeki kontrol akışının giriş noktalarıdır. Analyzer etiket hedeflerini fonksiyon kapsamındaki sembol tablosundan çözer.
6. Bir block'un tek predecessor'ı olması veya başka block'larla birleştirilebilir görünmesi, o block'a atlayan etiketlerin çözümlemesini ortadan kaldırmaz. Optimizasyon, tüm girişleri ve etiket referanslarını güncellemelidir.
7. Jump threading/chaining, yalnızca aradaki block'un kontrol aktarımını güvenle temsil ettiği ve gerekli block argümanları/etiket eşlemeleri korunabildiği durumda yapılabilir.
8. Bir block kaldırılır veya birleştirilirse ona yönelen bütün CFG kenarları, jump hedefleri, block parameter argümanları, debug/source metadata ve sembolik label eşlemeleri tutarlı biçimde güncellenmelidir.
9. Dönüşüm sonrası CFG'de her block'un tek terminator'ı bulunmalı; terminator sonrası sahte instruction veya fallthrough kalmamalıdır.
10. Cross-block transfer, `break`, `continue` veya `return` olarak yeniden yorumlanamaz.

### 12.21.2 Kaynak test A — Çok bloklu jump zinciri

```nexus
f:testJumpChain() {
    jump(route1);

    route1:>
    jump(route2);

    route2:>
    jump(destination);

    destination:>
    echo("Reached destination");
}
```

Beklenen çıktı:

```text
Reached destination
```

Optimizer öncesi CFG:

```text
Entry -> route1 -> route2 -> destination -> Return
```

Jump threading sonrası izin verilen CFG:

```text
Entry ----------------------> destination -> Return
```

`route1` ve `route2` yalnızca koşulsuz jump zincirinden ibaretse ve tüm hedef/metadata eşlemeleri güvenle güncellenebiliyorsa zincir kısaltılabilir. Debug bilgisi veya başka label referansı korunması gerekiyorsa block fiziksel olarak silinmeyebilir; alias/redirect eşlemesi tutulabilir.

### 12.21.3 Kaynak test B — Ayrı block'lara koşullu transfer

```nexus
f:testCrossBlockBranch(flag) {
    v:i = 0;

    (flag == 1) ? jump(selected);
    jump(alternative);

    selected:>
    v = 10;
    jump(join);

    alternative:>
    v = 20;
    jump(join);

    join:>
    echo(v);
}
```

Beklenen davranış:
- `flag == 1` ise `selected` block'una transfer edilir.
- Koşul false ise `jump(alternative)` çalışır.
- İki ayrı block da `join` block'una cross-block jump yapar.
- `join` içindeki `v`, OIR block parameter/SSA merge mekanizmasıyla taşınır.

Beklenen çıktı:
- `flag = 1` → `10`
- `flag = 0` → `20`

Optimizer iki ayrı yolu tek yolmuş gibi birleştiremez; iki farklı koşullu yol ve iki farklı değer korunmalıdır.

### 12.21.4 Kaynak test C — Sabit koşul ve erişilemeyen block

```nexus
f:testConstantJump() {
    v:i = 0;

    (1 == 1) ? jump(chosen);
    jump(notChosen);

    notChosen:>
    v = 99;
    jump(exit);

    chosen:>
    v = 7;
    jump(exit);

    exit:>
    echo(v);
}
```

Beklenen çıktı:

```text
7
```

Derleme zamanı sabit katlama `1 == 1` ifadesinin true olduğunu kanıtlarsa:
- Koşullu branch yalnızca `chosen` hedefine indirgenebilir.
- `notChosen` block'u girişten erişilemez hale gelir.
- Erişilemeyen block ve artık kullanılmayan kenarlar kaldırılabilir.
- `exit` block'unun SSA/block parameter girişleri yeniden doğrulanır.

Bu dönüşüm yalnızca koşulun compile-time sabit olduğu kanıtlandığında yapılabilir. Genel bir runtime koşulu sabitmiş gibi varsayılamaz.

### 12.21.5 Kaynak test D — Jump sonrasındaki kodun erişilemezliği

```nexus
f:testDeadAfterJump() {
    jump(destination);

    echo("Must not execute");

    destination:>
    echo("Destination reached");
}
```

Beklenen çıktı:

```text
Destination reached
```

`jump(destination)` terminator'ından sonra aynı block içinde normal yürütme devam etmez. `echo("Must not execute")` yürütülebilir CFG yolunda bulunamaz. Frontend/Analyzer'ın bu kaynak bölümünü sözdizimi ve tip kuralları bakımından denetlemesi, optimizer'ın ise erişilemeyen instruction'ı yürütülebilir koddan çıkarması ayrı sorumluluklardır.

### 12.21.6 CFG ve dönüşüm sonrası doğrulama

1. Her `jump(label)` kaynak block'un terminator'ı olmalıdır.
2. Her jump kenarının hedefi tam olarak bir geçerli basic block olmalıdır.
3. Kaynak block successor listesinde hedef block; hedef block predecessor listesinde kaynak block bulunmalıdır.
4. Jump terminator'ı sonrası source-order fallthrough bulunmamalıdır.
5. Jump threading sırasında hedefe giden bütün predecessor'lar güncellenmelidir.
6. Hedef block'un block parameter'ları varsa yeni kenarın argümanları hedef imzasıyla eşleşmelidir.
7. Block birleştirme sırasında SSA tanımları, kullanım noktaları ve dominance yeniden doğrulanmalıdır.
8. Sabit branch sadeleştirmesi sonrası ölü kenar kaldırılmalı; predecessor listeleri ve block parameter girişleri güncellenmelidir.
9. Erişilemeyen block temizliği fonksiyon girişinden CFG erişilebilirliği hesaplandıktan sonra yapılmalıdır.
10. Bir block'un kaldırılması, başka bir jump hedefinin çözümsüz kalmasına yol açmamalıdır. Gerekirse label alias/redirect tablosu güncellenmelidir.
11. Side effect, trap, volatile veya atomik semantiğe sahip instruction'lar yalnızca CFG erişilemezliği veya ilgili etki kuralları kanıtlandığında kaldırılabilir.
12. Her dönüşüm sonrasında CFG Verifier; terminator, kenar, block parameter, SSA ve dominance kontrollerini yeniden çalıştırmalıdır.

### 12.21.7 Negatif testler

- **MR-OIR-012-N1:** `jump(label)` hedefinin aynı basic block içinde olması zorunlu tutulması.
- **MR-OIR-012-N2:** Farklı basic block'a giden jump'ın yalnızca metinsel sıradaki block'a bağlanması.
- **MR-OIR-012-N3:** `JUMP` terminator'ından sonra kaynak satırına fallthrough kenarı bırakılması.
- **MR-OIR-012-N4:** Jump chain threading sırasında hedef block'un block parameter argümanlarının kaybolması.
- **MR-OIR-012-N5:** Block kaldırıldığı halde o block'a yönelen label referanslarının güncellenmemesi.
- **MR-OIR-012-N6:** Sabit olmadığı halde runtime branch koşulunun sabit kabul edilmesi.
- **MR-OIR-012-N7:** Sabit branch sadeleştirmesinden sonra predecessor/successor ve SSA merge girişlerinin güncellenmemesi.
- **MR-OIR-012-N8:** Erişilemeyen block temizliğinin yan etkili veya trap üreten erişilebilir instruction'ları yanlışlıkla kaldırması.
- **MR-OIR-012-N9:** Jump sonrasındaki erişilemez instruction'ların yürütme yoluna yeniden eklenmesi.
- **MR-OIR-012-N10:** Jump threading'in ileri/geri yönlü hedefleri farklı kurallarla ele alması.
- **MR-OIR-012-N11:** Birleştirilen block'ların debug/source konumlarının ve label alias eşlemelerinin kaybolması.
- **MR-OIR-012-N12:** CFG dönüşümü sonrası Verifier'ın yeniden çalıştırılmaması veya eski CFG analiz sonuçlarının geçerli kabul edilmesi.

### 12.21.8 Test sınıflandırması

- **Frontend/Parser:** `jump(label)` ve koşullu jump sözdiziminin AST'ye aktarılması.
- **Analyzer:** Fonksiyon kapsamındaki label çözümlemesi ve hedef block kimliği.
- **OIR Builder:** Kaynak block'tan hedef basic block'a explicit CFG kenarı ve JUMP terminator üretimi.
- **CFG Analysis:** Erişilebilirlik, predecessor/successor ve block bağlantıları.
- **Optimizer:** Jump threading, sabit branch sadeleştirmesi, unreachable block ve dead instruction temizliği.
- **OIR Verifier:** Her dönüşümden sonra CFG, terminator, label, block parameter, SSA ve dominance doğrulaması.

Bu test, cross-block jump'ı temel dil/IR davranışı olarak sabitler. Jump threading ve block merging'in pass sırası, label alias metadata biçimi ve debug bilgi formatı ayrı uygulama sözleşmelerinde kesinleştirilecektir.



## 12.22 MR-OIR-013 — Çoklu kaynaktan ortak hedefe atlama ve merge doğrulaması

### 12.22.1 Amaç

Birden fazla ayrı basic block'un aynı hedef basic block'a `jump(label)` ile aktarım yapmasını ve hedef block'un farklı predecessor'lardan gelen değerleri doğru birleştirmesini doğrulamak.

Bu senaryoda ortak hedefe birden fazla giriş bulunur. Her giriş kendi kontrol akışı kenarıdır; hedefe ulaşan değerler de hangi predecessor kenarından gelindiğine göre korunmalıdır. Bu davranış yalnızca optimizer kolaylığı değildir; kaynak programın kontrol akışı ve veri semantiğinin parçasıdır.

### 12.22.2 Kaynak dil testi

Aşağıdaki örnek NEXUS Flow kaynak sözdizimini kullanır. Koşullu jump gerçekleşmezse akış bir sonraki koşula, sonrasında da varsayılan yola ilerler:

```nexus
f:testMultiEntry(flag:i32)!i32 {
    v:i = 0;

    (flag == 1) ? jump(pathA);
    (flag == 2) ? jump(pathB);
    jump(pathC);

    pathA:>
    v = 10;
    jump(join);

    pathB:>
    v = 20;
    jump(join);

    pathC:>
    v = 30;
    jump(join);

    join:>
    echo(v);
    return 0;
}
```

Beklenen davranış:
- `flag == 1` ise `pathA` üzerinden ortak hedefe gidilir ve 10 değeri gözlemlenir.
- `flag == 2` ise `pathB` üzerinden ortak hedefe gidilir ve 20 değeri gözlemlenir.
- Diğer durumlarda `pathC` üzerinden ortak hedefe gidilir ve 30 değeri gözlemlenir.
- Her çalıştırmada yalnızca seçilen yolun atamaları yürütülür.
- `join` block'u üç ayrı predecessor'dan giriş alır.

Not: Örnek, mevcut kaynak sözdizimindeki koşullu statement biçimini kullanır. OIR'deki block parameter/SSA gösterimi kaynak dilde yeni bir sözdizimi önermez.

### 12.22.3 OIR ve veri birleştirme beklentisi

OIR Builder, üç kaynak yolunu tek bir ortak hedefe bağlayan üç ayrı CFG kenarı üretmelidir. Kavramsal gösterim:

```oir
block pathA:
    v_a = CONST 10
    JUMP join(v_a)

block pathB:
    v_b = CONST 20
    JUMP join(v_b)

block pathC:
    v_c = CONST 30
    JUMP join(v_c)

block join(v_join):
    ECHO v_join
    RETURN 0
```

Bu gösterim normatif opcode veya nihai OIR dosya formatı değildir; edge-specific değer aktarımını anlatan soyut örnektir.

Kurallar:
1. Her predecessor kendi kenar argümanını sağlamalıdır.
2. Hedef block parameter'ı, kontrolün geldiği predecessor'ın karşılık gelen argüman değerini almalıdır.
3. Bir predecessor'ın değeri başka predecessor'ın değeriyle karıştırılamaz veya ortak, koşulsuz bir sabit gibi ele alınamaz.
4. Hedef block'un birden fazla predecessor'ı varsa OIR, her giriş yolunun değer ilişkisini açık ve doğrulanabilir biçimde temsil etmelidir.
5. SSA kullanılıyorsa merge değeri phi/block-parameter benzeri bir mekanizmayla temsil edilir; kesin serileştirme biçimi OIR format sürümünde sabitlenecektir.
6. Kaynak değişkenin birden fazla yolda atanması, OIR Builder'ın kaynak dilin atama ve kapsam semantiğini korumasını gerektirir. Bu test, kaynak dile yeni bir phi sözdizimi eklemez.

### 12.22.4 Çoklu girişli CFG doğrulamaları

Verifier en az aşağıdakileri doğrulamalıdır:
- Her predecessor/successor ilişkisi karşılıklı tutarlıdır.
- Her kenarın hedef block parametre sayısı ve tipleriyle uyumlu argümanları vardır.
- Merge girişleri gerçek predecessor kenarlarıyla bire bir ilişkilidir.
- Her incoming SSA değeri, ilgili kenar üzerinde geçerli ve kullanılabilir olmalıdır.
- Dominance ve backedge kuralları, MR-OIR-010/011'de tanımlanan genel CFG kurallarıyla birlikte uygulanır.
- Aynı hedefe birden fazla jump gelmesi, hedef block'un tek girişli varsayılmasına neden olamaz.
- Aynı source-target çifti için birden fazla mantıksal edge oluşabiliyorsa edge kimliği/argüman eşlemesi kaybolmamalıdır; CFG'nin edge modelinin bu ayrımı destekleyip desteklemediği OIR format tasarımında açıkça belirtilmelidir.

### 12.22.5 Optimizer güvenliği

Optimizer:
- Ortak hedefe gelen farklı girişlerin değerlerini korumalıdır.
- Jump threading veya block merging sırasında her incoming edge'in argümanlarını doğru yeni hedefe taşımalıdır.
- İki predecessor'ı birleştirirken farklı değerleri tek bir değere indirgeyemez; bunu ancak eşdeğerlik kanıtı varsa yapabilir.
- Bir predecessor'ı kaldırırsa hedef block'un predecessor listesi ve merge girişlerini aynı dönüşüm içinde güncellemelidir.
- Bir edge'i yönlendirirse ilgili edge argümanlarını, label eşlemesini, debug/source metadata'yı ve dominance/SSA analizlerini güncellemelidir.
- CFG değiştiren pass sonrasında OIR Verifier ve gerekli CFG/SSA analizleri yeniden çalıştırılmalıdır.

### 12.22.6 Test varyantları

**Test A — Üç farklı predecessor, tek ortak hedef**

Yukarıdaki kaynak örneği kullanılır. Beklenen değerler sırasıyla 10, 20 ve 30'dur.

**Test B — İki seviyeli merge**

İlk iki yol `joinA` hedefinde birleşir; daha sonra bu sonuç üçüncü yolla `joinB` hedefinde birleşir. Her iki merge noktasında da predecessor'a bağlı değer korunmalıdır.

**Test C — Ortak hedefe koşullu ve koşulsuz giriş**

Bir yol koşullu jump, diğer yol koşullu jump'ın false fallthrough devamından gelen koşulsuz jump ile hedefe ulaşır. Builder, örtük fallthrough'u doğru basic block/edge biçimine dönüştürmelidir.

**Test D — Merge sonrası kullanım**

Ortak hedefte birleşen değer, daha sonraki bir hesaplamada kullanılır. OIR, hedefte doğru incoming değerin kullanılmasını ve definition/use zincirinin korunmasını sağlamalıdır.

### 12.22.7 Negatif testler

- **MR-OIR-013-N1:** Bir predecessor'ın gerekli block parameter argümanını üretmemesi.
- **MR-OIR-013-N2:** Edge argüman sayısının hedef block parameter sayısıyla uyuşmaması.
- **MR-OIR-013-N3:** Edge argüman tipinin hedef parameter tipiyle uyuşmaması.
- **MR-OIR-013-N4:** Tüm predecessor'lar için tek bir incoming değerin kopyalanıp farklı yol değerlerinin kaybedilmesi.
- **MR-OIR-013-N5:** Merge girişinin gerçek predecessor edge ile eşleştirilmemesi.
- **MR-OIR-013-N6:** Predecessor listesi güncellenirken successor listesi veya tersi yönde eski ilişkinin bırakılması.
- **MR-OIR-013-N7:** Jump threading sırasında edge argümanlarının yeni hedefe taşınmaması.
- **MR-OIR-013-N8:** Block kaldırma/birleştirme sırasında merge girişlerinden birinin silinmesi veya yanlış predecessor'a bağlanması.
- **MR-OIR-013-N9:** Dominance/SSA doğrulamasının backedge veya çoklu girişli hedefi tek girişli varsayması.
- **MR-OIR-013-N10:** Eşdeğerlik kanıtı olmadan farklı incoming değerlerin tek değere indirgenmesi.
- **MR-OIR-013-N11:** Kaynak dilin lexical-scope initialization güvenliğini ihlal eden bir girişin kabul edilmesi.
- **MR-OIR-013-N12:** CFG dönüşümünden sonra eski predecessor, edge-argument veya SSA analiz sonuçlarının kullanılması.

### 12.22.8 Test sınıflandırması

- **Frontend/Analyzer:** Koşullu jump ve label çözümlemesi.
- **OIR Builder:** Çoklu predecessor kenarları ve edge-specific değer aktarımı.
- **OIR Verifier:** CFG kenar tutarlılığı, block parameter arity/type ve SSA geçerliliği.
- **CFG/SSA Analysis:** Dominance, merge ve backedge ilişkileri.
- **Optimizer:** Jump threading, block merging ve predecessor/merge güncellemeleri.

Bu test, tek bir basic block'un birden fazla farklı kaynaktan giriş alabileceğini ve bu girişlerin değerlerinin birbirinden bağımsız korunması gerektiğini sabitler. Nihai block-parameter serileştirme biçimi ve edge kimliği politikası, OIR format sürümünün ilgili sözleşmesinde kesinleştirilecektir.



## 12.23 MR-OIR-014 — Karmaşık kontrol akışı grafiği ve sınırsız çoklu giriş

### 12.23.1 Amaç

Bu test, temel NEXUS Flow kontrol yapılarının ve doğrudan `jump(label)` aktarımının birlikte kullanılarak karmaşık, çok dallı ve döngülü bir fonksiyon CFG'si oluşturabilmesini doğrular.

Kapsam:
- `if / elsif / els` ile koşullu dallanma,
- `select` ile çok seçenekli dallanma,
- `loop` ile döngü ve geri kenar,
- `jump(label)` ile açık etiketli, ileri/geri ve cross-block aktarım,
- Birden fazla yolun ortak hedefte birleşmesi,
- Birleşen akışın yeniden dallanması,
- Birden fazla merge noktasının ve döngünün aynı fonksiyonda bulunması.

Bu yapılar kaynak dilde ayrı ayrı anlamlarını korur; OIR seviyesinde aynı fonksiyon CFG'sinin block ve edge'leri olarak temsil edilir.

### 12.23.2 Kaynak dil karmaşık akış testi

```nexus
f:complexFlow(mode:i32, choice:i32)!i32 {
    result:i = 0;

    if (mode == 0) {
        result = 10;
    } elsif (mode == 1) {
        result = 20;
    } els {
        result = 30;
    }

    select(choice) {
        1 -> {
            result = result + 1;
            jump(shared);
        }

        2 | 3 -> {
            result = result + 2;
            jump(shared);
        }

        4..8 -> {
            result = result + 3;
            jump(shared);
        }

        default -> {
            result = result + 4;
            jump(shared);
        }
    }

    shared:>
    loop(i, i < 3, i++) -> {
        result = result + i;
    }

    (result > 20) ? jump(highPath);
    jump(lowPath);

    highPath:>
    result = result + 100;
    jump(finalJoin);

    lowPath:>
    result = result - 100;
    jump(finalJoin);

    finalJoin:>
    return result;
}
```

Beklenen CFG davranışı:
1. İlk koşullu yapı üç farklı değer üretim yolunu tek devam akışında birleştirir.
2. `select` dört olası branch üretir ve her branch `shared` hedefine ayrı bir jump ile ulaşır.
3. `shared` noktasında çoklu giriş vardır; branch'e özgü güncel `result` değeri korunur.
4. `loop` kendi koşul, body, latch ve exit akışını CFG içinde oluşturur.
5. Döngüden sonra koşullu jump ve koşulsuz jump, akışı `highPath` veya `lowPath` bloklarına ayırır.
6. Her iki yol `finalJoin` noktasında yeniden birleşir.

Örnek, kontrol yapılarının tek bir fonksiyon içinde bileşim oluşturmasını gösterir. OIR'deki block parameter/SSA mekanizması iç temsildir; kaynak dilde yeni bir merge sözdizimi oluşturmaz.

### 12.23.3 Çoklu jump için limit politikası

**Dil veya CFG semantiği düzeyinde sabit bir jump, predecessor, successor ya da ortak hedef giriş sayısı sınırı tanımlanmayacaktır.**

Normatif kurallar:
1. Bir fonksiyonda ihtiyaç duyulan sayıda `jump(label)` ifadesi bulunabilir; derleyici bunu keyfî bir sabit sayıyla sınırlandıramaz.
2. Bir basic block, farklı kaynak block'lardan keyfî sayıda geçerli predecessor alabilir. MR-OIR-013'teki edge-specific merge kuralları tüm bu girişler için geçerlidir.
3. Bir fonksiyon CFG'sinde toplam block ve edge sayısı için dil seviyesinde küçük/sabit bir üst sınır konulmaz.
4. Bir basic block yine tek bir terminator ile sona erer. Bu kural, aynı block içinde birden fazla terminator bulunabileceği anlamına gelmez. Birden fazla çıkış, conditional/select/switch benzeri terminator yapısının ayrı successor edge'leriyle temsil edilir.
5. Uygulama; dinamik boyutlandırma, güvenli kapasite büyütme ve taşma kontrolleri kullanmalıdır. Sabit boyutlu iç tablo seçilirse kapasite dolduğunda sessiz kesme veya hatalı CFG üretme yerine açık kaynak/derleme-kaynak sınırı hatası verilmelidir.
6. OIR dosya formatındaki sayısal alan genişliği, adreslenebilir bellek, host kaynakları ve güvenlik limitleri pratik/temsilî sınır oluşturabilir. Bunlar dilin keyfî semantik limiti değildir; format ve uygulama sözleşmesinde açıkça belirtilmelidir.
7. Kaynak veya güvenlik limitine ulaşıldığında işlem kontrollü biçimde reddedilir; edge, label, predecessor veya block kaybıyla kısmi/bozuk OIR üretilmez.

### 12.23.4 CFG ve merge doğrulamaları

Verifier:
- `if / elsif / els`, `select`, `loop` ve `jump` kaynaklı tüm block/edge ilişkilerini doğrulamalıdır.
- Her block'un geçerli tek terminator'ı olduğunu ve terminator successor'larının CFG ile eşleştiğini doğrulamalıdır.
- Çoklu predecessor'lı hedeflerde her edge'in kendi block parameter/SSA argümanlarını taşıdığını doğrulamalıdır.
- Merge sonrası yeniden dallanmayı ve sonraki merge noktalarını desteklemelidir.
- Loop backedge'lerini ve loop exit edge'lerini normal CFG doğrulamasına dahil etmelidir.
- Kaynak sıra numarasını dominance veya predecessor sırası olarak varsaymamalıdır.
- CFG'nin reducible veya irreducible olabileceğini kabul etmeli; yalnızca döngü içerdiği için grafiği reddetmemelidir.
- Kaynak dilin fonksiyon kapsamı ve lexical-scope initialization güvenliğini her jump hedefinde uygulamalıdır.

### 12.23.5 Optimizer güvenliği

Optimizer aşağıdaki dönüşümlerde CFG semantiğini korumalıdır:
- Birden fazla merge ve branch noktasını sadeleştirme,
- Güvenli jump threading,
- Sabit koşullu branch ve select kolu sadeleştirmesi,
- Döngü giriş/çıkış kenarlarının korunması,
- Erişilemeyen block ve kullanılmayan değer temizliği,
- Block merging ve predecessor yeniden yönlendirme.

Her dönüşüm; terminator'ları, tüm successor/predecessor edge'lerini, edge-specific block parameter argümanlarını, SSA merge kayıtlarını, label eşlemelerini ve debug/source metadata'yı birlikte güncellemelidir. Gerekli CFG/SSA analizleri yenilenmeli ve OIR Verifier tekrar çalıştırılmalıdır.

### 12.23.6 Negatif testler

- **MR-OIR-014-N1:** `if / elsif / els` dallarından birinin merge noktasına yanlış bağlanması.
- **MR-OIR-014-N2:** `select` içinde ilk eşleşen branch kuralının ihlal edilmesi veya birden fazla branch'in çalıştırılması.
- **MR-OIR-014-N3:** `select` branch'lerinden birinin ortak hedefe edge üretmemesi.
- **MR-OIR-014-N4:** Ortak hedefte predecessor'a özgü `result` değerinin kaybolması.
- **MR-OIR-014-N5:** Loop body, latch, backedge veya exit edge'lerinden birinin eksik/yanlış oluşturulması.
- **MR-OIR-014-N6:** Loop içindeki bir değer tanımının dominance/SSA kurallarına aykırı kullanılması.
- **MR-OIR-014-N7:** Merge sonrasında yeniden dallanmanın predecessor/edge bağlantılarının bozulması.
- **MR-OIR-014-N8:** `finalJoin` noktasındaki iki girişin yanlış SSA değerleriyle eşleştirilmesi.
- **MR-OIR-014-N9:** Optimizer'ın farklı incoming değerleri eşdeğerlik kanıtı olmadan tek değere indirmesi.
- **MR-OIR-014-N10:** Jump veya predecessor sayısına keyfî sabit limit uygulanıp geçerli grafiğin reddedilmesi.
- **MR-OIR-014-N11:** İç kapasite dolduğunda edge/label kaybedilerek bozuk OIR üretilmesi.
- **MR-OIR-014-N12:** CFG değişikliğinden sonra eski dominance, predecessor veya SSA analizinin kullanılması.
- **MR-OIR-014-N13:** Tek basic block'a birden fazla terminator yazılmasına izin verilmesi.
- **MR-OIR-014-N14:** Kaynak sıra düzeninin CFG bağlantısı veya kontrol akışı sırası sanılması.
- **MR-OIR-014-N15:** Lexical-scope initialization güvenliğini ihlal eden jump hedefinin kabul edilmesi.

### 12.23.7 Test sınıflandırması

- **Frontend/Parser:** İç içe ve ardışık kontrol yapılarının AST'ye doğru aktarılması.
- **Analyzer:** Label, fonksiyon kapsamı ve lexical-scope kontrolleri.
- **OIR Builder:** Çok dallı CFG, merge, loop backedge ve cross-block jump üretimi.
- **OIR Verifier:** Terminator, edge, predecessor/successor, block parameter ve SSA doğrulaması.
- **CFG/SSA Analysis:** Dominance, loop, reducibility ve merge analizi.
- **Optimizer:** Karmaşık CFG üzerinde güvenli sadeleştirme ve metadata koruması.

Bu test, NEXUS Flow kontrol yapılarının birbirleriyle bileşiminden karmaşık CFG oluşturulabilmesini ve çoklu jump için keyfî sabit sayı sınırı bulunmamasını normatif tasarım kararı olarak sabitler. Gerçek kaynak/format sınırları uygulama sözleşmesinde açıkça raporlanacaktır.



## 12.24 MR-OIR-015 — İç içe döngüler, SELECT ve etiketli aktarım bileşimi

### 12.24.1 Amaç

Bu test, bir fonksiyon içinde iç içe `loop` yapılarının, döngü içindeki `select` dallarının, koşullu kontrollerin ve `jump(label)` aktarımının birlikte doğru CFG oluşturmasını doğrular.

Test özellikle şu ayrımları korur:
- `break`, en yakın kapsayan `loop` yapısından çıkar.
- `continue`, en yakın kapsayan `loop` yapısının sonraki iterasyon noktasına geçer.
- `jump(label)`, hedef etikete genel kontrol aktarımıdır; kendi başına `break` veya `continue` değildir.
- Bir `select` kolundan yapılan jump, seçilen kolun devamını sonlandırır ve hedef block'a gider.
- İç ve dış döngülerin giriş, body, latch ve exit block'ları birbirine karıştırılmaz.

### 12.24.2 Kaynak dil testi

Aşağıdaki örnek, Syntax Reference içindeki `loop(variable, condition, increment)`, `select(subject)`, `if / els`, `break`, `continue` ve `jump(label)` biçimlerini kullanır:

```nexus
f:testNestedControl(outerLimit:i32, innerLimit:i32)!i32 {
    total:i = 0;
    outer:i = 0;

    loop(outer, outer < outerLimit, outer++) {
        inner:i = 0;

        loop(inner, inner < innerLimit, inner++) {
            select(inner) {
                0 -> {
                    total = total + 1;
                    jump(innerMerge);
                }

                1 | 2 -> {
                    total = total + 2;
                    jump(innerMerge);
                }

                3..5 -> {
                    total = total + 3;
                    jump(innerMerge);
                }

                default -> {
                    total = total + 4;
                    jump(innerMerge);
                }
            }

            innerMerge:>
            if (total > 100) {
                break;
            }

            (inner == 2) ? continue;
            total = total + inner;
        }

        select(outer) {
            0 -> {
                total = total + 10;
                jump(outerMerge);
            }

            1 -> {
                total = total + 20;
                jump(outerMerge);
            }

            default -> {
                total = total + 30;
                jump(outerMerge);
            }
        }

        outerMerge:>
        if (outer == 1) {
            continue;
        }

        total = total + outer;
    }

    return total;
}
```

### 12.24.3 Beklenen kontrol akışı

1. Dış `loop` her iterasyonda iç döngünün başlangıç durumunu kurar.
2. İç `loop` kendi koşul, body, latch ve exit akışına sahiptir.
3. İç `select` ilk eşleşen branch'i çalıştırır. Her branch kendi değer güncellemesini yapar ve `innerMerge` hedefine ayrı bir jump edge üretir.
4. `innerMerge` noktasında tüm branch değerleri predecessor/edge ilişkisine göre birleştirilir.
5. `break`, `innerMerge` sonrasında iç döngüden çıkar; dış döngüyü sonlandırmaz.
6. `continue`, `inner == 2` olduğunda iç döngünün sonraki iterasyonuna geçer; loop increment semantiği korunur.
7. İç döngü tamamlandığında dış döngü içindeki ikinci `select` çalışır. Üç branch `outerMerge` hedefine ayrı jump edge'leri üretir.
8. `outerMerge` noktasındaki `continue`, en yakın kapsayan dış döngüye aittir; dış döngünün sonraki iterasyonuna geçer.
9. `total` değişkeninin bütün merge noktalarındaki değer akışı korunur.

### 12.24.4 Loop kimliği ve terminator bağlamı

OIR Builder ve Verifier, her loop için benzersiz bir iç loop kimliği veya eşdeğer açık loop-context ilişkisi tutmalıdır. Bu kimlik, nihai OIR alan adı olarak şimdiden sabitlenmiş değildir; gereksinim semantiktir.

- `break` hedefi, kaynak konumundaki en yakın kapsayan loop'un exit block'udur.
- `continue` hedefi, en yakın kapsayan loop'un devam/latch akışıdır. Üç bileşenli loop biçiminde increment adımı atlanmamalıdır; kontrol, tanımlı iterasyon devam noktasına gitmelidir.
- İç içe loop'taki `break` veya `continue`, dış loop'un hedeflerine bağlanamaz.
- `jump(label)` çözümlemesi loop-context aramasıyla yapılmaz; fonksiyonun label tablosu ve mevcut function/lexical-scope güvenlik kuralları kullanılır.
- `jump` bir block terminator'ıdır. Jump'tan sonra aynı block'taki kaynak komutlar yürütülmez.
- Loop içindeki bir select branch'inden yapılan jump, branch'in kalanını ve select'in normal devam yolunu atlar; hedef block'a doğrudan geçer.

### 12.24.5 Çoklu merge ve SSA beklentisi

- `innerMerge` birden fazla select branch'inden giriş alır.
- `outerMerge` de birden fazla select branch'inden giriş alır.
- `total` için her incoming edge doğru güncel değeri taşımalıdır.
- İç döngüde üretilen değerler, iç loop backedge ve exit yollarında SSA/block parameter kurallarına göre taşınmalıdır.
- İç loop exit'inden sonra dış döngü gövdesindeki kullanım, iç döngünün doğru çıkış değerini almalıdır.
- Dış loop backedge'i, dış loop'un sonraki iterasyonundaki değerleri doğru birleştirmelidir.
- Birden fazla merge noktasının bulunması, predecessor/edge sayısına veya merge derinliğine sabit bir dil sınırı getirmez.

### 12.24.6 Optimizer güvenliği

Optimizer:
- İç ve dış loop'un ayrı loop kimliklerini, header/latch/exit ilişkilerini korumalıdır.
- `break` ve `continue` terminator/edge hedeflerini loop-context bilgisiyle tutarlı biçimde korumalıdır.
- `jump` edge'lerini break/continue edge'leriyle birleştirmemelidir.
- Select branch'lerinden gelen edge-specific değerleri doğru merge noktasında tutmalıdır.
- Loop rotation, unroll, jump threading veya block merging yaparsa nested loop ilişkilerini ve tüm SSA/edge argümanlarını güncellemelidir.
- Dönüşüm sonrası loop, dominance, predecessor/successor ve SSA analizlerini yenilemeli; OIR Verifier'ı tekrar çalıştırmalıdır.
- Sırf iç içe veya çok sayıda edge içerdiği için geçerli CFG'yi reddetmemeli veya sessizce sadeleştirmemelidir.

### 12.24.7 Negatif testler

- **MR-OIR-015-N1:** İç loop `break` hedefinin dış loop exit block'una bağlanması.
- **MR-OIR-015-N2:** İç loop `continue` hedefinin dış loop latch/continue block'una bağlanması.
- **MR-OIR-015-N3:** Üç bileşenli loop'ta continue akışının increment adımını yanlışlıkla atlaması.
- **MR-OIR-015-N4:** Select içinde ilk eşleşen branch yerine birden fazla branch'in yürütülmesi.
- **MR-OIR-015-N5:** Select branch'indeki jump sonrasındaki komutların aynı akışta yürütülmesi.
- **MR-OIR-015-N6:** `innerMerge` predecessor edge'lerinden birinin eksik veya yanlış olması.
- **MR-OIR-015-N7:** `outerMerge` predecessor edge'lerinden birinin eksik veya yanlış olması.
- **MR-OIR-015-N8:** İç döngüden dış döngü gövdesine çıkışta `total` SSA değerinin kaybolması.
- **MR-OIR-015-N9:** İç ve dış loop backedge'lerinin birbirine karıştırılması.
- **MR-OIR-015-N10:** `jump(label)` komutunun yanlışlıkla break/continue olarak sınıflandırılması.
- **MR-OIR-015-N11:** Optimizer'ın nested loop header/latch/exit ilişkisini bozması.
- **MR-OIR-015-N12:** Loop unroll veya block merge sonrasında edge-specific merge argümanlarının kaybolması.
- **MR-OIR-015-N13:** Kaynak label çözümlemesinin loop-context ile karıştırılması.
- **MR-OIR-015-N14:** Geçerli çoklu merge yapısının sabit predecessor/merge derinliği limiti nedeniyle reddedilmesi.
- **MR-OIR-015-N15:** CFG dönüşümünden sonra eski loop, dominance veya SSA analizinin kullanılması.

### 12.24.8 Kapsam dışı / açık tasarım noktası

Bu test, jump hedeflerini aynı etkin lexical loop gövdesi içindeki merge noktalarında tutarak loop'tan çıkış davranışını `break` ile karıştırmaz. Bir `jump(label)` komutunun etkin loop kapsamlarının dışına aktarım yaptığı durumda kaynak kapsamından çıkış, otomatik kaynak temizliği ve açık kaynak/handle kapatma davranışları ayrı bir semantik sözleşme olarak netleştirilecektir. Bu konu, genel jump'ın yapay biçimde sınırlandırıldığı anlamına gelmez; henüz açıkça tanımlanmamış cleanup davranışının varsayılmaması anlamına gelir.

### 12.24.9 Test sınıflandırması

- **Frontend/Parser:** İç içe loop/select/if yapılarının AST'ye doğru aktarılması.
- **Analyzer:** Loop lexical context, label çözümleme ve scope kontrolleri.
- **OIR Builder:** Nested loop CFG, select branch'leri, jump edge'leri ve merge üretimi.
- **OIR Verifier:** Loop hedefleri, terminator, CFG edge ve SSA/block parameter doğrulaması.
- **CFG/Loop Analysis:** İç/dış loop header, latch, exit, backedge ve dominance ilişkileri.
- **Optimizer:** Nested loop ve çoklu merge üzerinde güvenli dönüşüm.

Bu test, `break`, `continue` ve `jump(label)` semantikleri birbirinden ayrılmış haldeyken iç içe kontrol yapılarının birlikte çalışmasını normatif olarak sabitler.


## 12.25 MR-OIR-016 — Jump scope-exit cleanup ve geri dönüşte kaynakların korunması

**Amaç:** `jump(label)` komutunun basic block aktarımı ile lexical scope'tan çıkışı birbirinden ayırdığını; aynı canlı scope içinde geri dönüş mümkün olduğunda kaynakların korunmasını, scope terk edildiğinde ise otomatik cleanup uygulanmasını doğrulamak.

### 12.25.1 Temel semantik

Bu test, NEXUS Flow Syntax Reference'taki `(h)` yaşam döngüsü ve `}?;` blok temizliği ile OIR §12.8 sözleşmesini birlikte sınar.

1. Basic block değişimi lexical scope çıkışı değildir. Aynı scope içindeki jump, o scope'a ait kaynakların yaşamını sona erdirmez.
2. Aynı lexical scope içindeki ileri/geri jump geçerlidir. Kontrol daha sonra aynı scope içindeki önceki bir label'a dönebilir; scope activation ve kaynak durumu korunur.
3. Kaynak scope'un ancestor'ına jump yapılırsa arada kalan scope'lar terk edilir ve bunların otomatik cleanup işlemleri çalıştırılır.
4. Terk edilen scope'un içindeki label'a sonradan doğrudan jump yapılamaz. Böyle bir transfer daha derin scope'a giriş olacağından Analyzer hatasıdır.
5. Kardeş scope'a doğrudan jump da geçersizdir; hedef scope'un normal giriş/başlatma adımları atlanmış olur.
6. Cleanup bir kez yürütülür. Scope terk edildikten sonra eski activation'a ait kaynaklar yeniden kullanılmaz.
7. `_>` ile sahipliği aktarılmış kaynak, eski scope'un cleanup işleminde tekrar serbest bırakılmaz.
8. `break`, `continue`, `return` ve `jump` farklı kontrol terminator'ları olmaya devam eder; ancak bir kontrol aktarımı lexical scope'u terk ediyorsa ilgili scope-exit cleanup gereksinimi ayrıca uygulanır.
9. Yalın `?;` Halt davranışıdır; bir kaynak/masa/DLL hedefine uygulanmış biçimi hedefi serbest bırakır. Hedefli kaynak bırakma işlemi JUMP cleanup terminator'ı değildir. `}?;` blok kaynak temizliğiyle ilişkilidir.
10. Cleanup'ın hata vermesi, cleanup içinden kontrol aktarımı ve kesin runtime ABI davranışları ayrı bir alt sözleşme olarak açık bırakılır; bunlar için sessiz varsayım yapılamaz.

### 12.25.2 Test A — Aynı scope içinde geri dönüş, kaynak korunur

Kaynak test:

```nexus
f:testJumpWithinScope()!i32 {
    v:i = 0;

    retry:>
    v = v + 1;
    (v < 3) ? jump(retry);

    return v;
}
```

Beklenen davranış:
- `retry` ve ona geri dönen jump aynı lexical scope içindedir.
- Scope kapanmamıştır; scope kaynakları için erken cleanup üretilmez.
- CFG çevrimi geçerlidir ve normal `return` ile sonlanır.

### 12.25.3 Test B — İç scope'tan dış scope'a jump, cleanup gerekir

```nexus
f:testJumpLeavesScope()!i32 {
    loop(i, i < 3, i++) {
        (h) <- file.open("session.dat", r);
        jump(afterLoop);
    }

    afterLoop:>
    return 0;
}
```

Beklenen davranış:
- `jump(afterLoop)` loop gövdesinin lexical scope'unu terk eder.
- Loop gövdesine ait otomatik kaynak cleanup, `afterLoop` hedefine ulaşılmadan önce yürütülür.
- `afterLoop` eski loop-body activation'ının dışındadır; eski `(h)` handle'ı burada canlı kabul edilmez.
- Bu örnekte loop'un normal latch/continue yolu JUMP yoluna eklenmez.

### 12.25.4 Test C — Terk edilmiş scope'a geri giriş reddedilir

```nexus
f:testReenterExitedScope()!i32 {
    loop(i, i < 2, i++) {
        inner:>
        jump(outside);
    }

    outside:>
    jump(inner);
}
```

Beklenen davranış:
- İlk jump loop-body scope'undan dışarı çıkar ve scope cleanup gerektirir.
- `outside` noktasından `inner` hedefine jump, terk edilmiş/daha derin lexical scope'a giriş olacağından Analyzer hatasıdır.
- Eski scope activation'ı ve kaynakları yeniden canlandırılmaz.

### 12.25.5 Test D — Kardeş scope'a atlama reddedilir

İki ayrı lexical block'un birbirine doğrudan jump ile bağlanması, hedef block'un normal scope giriş adımlarını atlayabileceğinden Analyzer tarafından reddedilir. Kardeş scope'a geçiş, ortak ancestor üzerinden normal CFG akışıyla yapılmalıdır.

### 12.25.6 Verifier ve Optimizer kontrolleri

1. Her block'un lexical scope ancestry bilgisi tutarlı olmalıdır.
2. JUMP hedefinin scope'u kaynak scope ile aynı veya ancestor olmalıdır.
3. Aynı scope içi jump'ta cleanup listesi yanlışlıkla çalıştırılmamalıdır.
4. Scope dışına jump'ta terk edilen her scope'un cleanup'ı doğru sırada ve tam bir kez çalışmalıdır.
5. Cleanup sonrasında eski scope activation'ına ait handle/masa kullanımına izin verilmemelidir.
6. `_>` ile taşınmış kaynak için çift cleanup üretilmemelidir.
7. CFG dönüşümü, scope ancestry ve cleanup edge bilgilerini korumalıdır.
8. Cleanup gerektiren JUMP yolunda cleanup atlanması veya cleanup'ın iki kez çalıştırılması verifier hatası olmalıdır.
9. Cleanup hata davranışı henüz tanımlı değilse Builder/Verifier bu davranışı sessizce normal akış olarak varsaymamalıdır.
10. OIR'nin kesin cleanup opcode ve metadata encoding'i bu testle sabitlenmiş sayılmaz.

### 12.25.7 Negatif testler

- **MR-OIR-016-N1:** Aynı scope içindeki geri jump nedeniyle gereksiz cleanup yapılması.
- **MR-OIR-016-N2:** Scope dışına jump yapılırken otomatik cleanup'ın atlanması.
- **MR-OIR-016-N3:** Aynı scope cleanup'ının iki kez yürütülmesi.
- **MR-OIR-016-N4:** Terk edilmiş daha derin scope'a doğrudan jump ile yeniden girilmesi.
- **MR-OIR-016-N5:** Kardeş lexical scope'a doğrudan jump ile girilmesi.
- **MR-OIR-016-N6:** `_>` ile taşınmış kaynağın eski scope'ta tekrar serbest bırakılması.
- **MR-OIR-016-N7:** İç içe scope cleanup sırasının dıştan içe uygulanması.
- **MR-OIR-016-N8:** Cleanup sonrasında eski scope activation'ına ait handle'ın kullanılması.
- **MR-OIR-016-N9:** Optimizer'ın JUMP dönüşümünde scope ancestry/cleanup metadata'sını kaybetmesi.
- **MR-OIR-016-N10:** Cleanup gerektiren JUMP edge'inin cleanup aşamasını atlayarak doğrudan hedefe bağlanması.
- **MR-OIR-016-N11:** Aynı scope içi geri dönüşte kaynak activation'ının yanlışlıkla yeniden oluşturulması.
- **MR-OIR-016-N12:** Cleanup hata/terminator davranışının tanımlanmamış olmasına rağmen sahte normal continuation üretilmesi.

### 12.25.8 Açık alt sözleşmeler

- Cleanup işlemi hata verdiğinde diğer cleanup işlemlerinin devam edip etmeyeceği ve hataların nasıl biriktirileceği.
- Cleanup gövdesinin `jump`, `return`, `break`, `continue` veya panic üretmesine izin verilip verilmeyeceği.
- `defer` sözdiziminin bu cleanup mekanizmasıyla ilişkisi ve kesin çalışma sırası.
- Kaynak türüne özel cleanup sırası istisnaları.
- Cleanup opcode/record biçimi ve runtime ABI.

Bu açık noktalar, temel kuralı değiştirmez: canlı scope korunur; terk edilen scope temizlenir; terk edilmiş scope'a doğrudan geri giriş yapılamaz.


## 12.26 MR-OIR-017 — Defer kayıt yaşam döngüsü ve scope-exit sırası

**Amaç:** Mevcut `defer { block }` sözdiziminin kayıt anını, scope aktivasyonuna bağlı ömrünü, LIFO sırasını ve MR-OIR-016 cleanup mekanizmasıyla ilişkisini doğrulamak.

### 12.26.1 Normatif kurallar

1. Defer, yürütme sırasında defer ifadesine ulaşıldığında kaydedilir.
2. Kayıt, bulunduğu lexical scope activation'ına aittir.
3. Aynı scope içindeki basic block geçişi veya backward JUMP defer'i çalıştırmaz.
4. Scope'tan normal çıkış, BREAK, CONTINUE, RETURN veya ancestor-scope JUMP defer kayıtlarını çalıştırır.
5. Aynı scope'taki birden fazla defer LIFO sırasıyla çalışır.
6. Defer gövdeleri otomatik `}?;` kaynak/masa cleanup'ından önce çalışır.
7. İç içe scope'lar terk edilirken her scope kendi defer kayıtlarını ve otomatik cleanup'ını tamamlar; sonra üst scope kapanır.
8. Defer gövdesinden non-local kontrol aktarımı ve nested defer kaydı bu sürümde geçersizdir.
9. Defer gövdesi hata/panic üretirse diğer defer'lerin yürütülme politikası açık hata sözleşmesidir.
10. Defer kayıtları ve scope activation metadata'sı OIR optimizasyonlarında korunmalıdır.

### 12.26.2 Test A — Kayıt noktasına ulaşılmayan yol

Bir kontrol yolu defer ifadesine ulaşmadan scope'u terk ederse o yol için defer kaydı oluşturulmuş gibi cleanup üretilemez. Builder, yalnızca kaynak kodda defer ifadesinin bulunmasına bakarak her çıkışa defer ekleyemez.

### 12.26.3 Test B — Aynı scope içi backward jump

Aynı lexical scope içindeki backward JUMP defer'i erken çalıştırmaz. Defer kaydı scope activation'ında canlı kalır.

Açık nokta: Defer ifadesi aynı scope içindeki loop gövdesinde birden fazla dinamik erişime uğrarsa her erişimde kayıt mı eklenir, yoksa scope activation başına tek kayıt mı tutulur? Bu karar verilene kadar loop içi tekrar kayıt testi taslak olarak kalır; Builder davranış tahmin edemez.

### 12.26.4 Test C — LIFO ve otomatik cleanup sırası

```nexus
f:testDeferLifo()!i32 {
    (h) <- file.open("defer-test.txt", r);

    defer {
        echo("first-registered");
    }

    defer {
        echo("second-registered");
    }

    return 0;
}
```

Beklenen:
- İkinci kayıt birinci çalışır.
- İlk kayıt ikinci çalışır.
- Defer gövdeleri tamamlandıktan sonra scope'a ait otomatik kaynak/masa cleanup uygulanır.
- Gerçek handle tipi ve gözlemlenebilir test çıktısı uygulama testinde doğrulanır.

### 12.26.5 Test D — Scope dışına jump

Loop-body scope'unda kayıt edilen defer, ancestor scope'a JUMP ile çıkılırken çalıştırılır. Önce defer kayıtları LIFO sırayla, ardından otomatik scope cleanup yürütülür; hedef block'a bundan sonra geçilir.

### 12.26.6 Test E — İç içe scope

İç scope'tan dış scope'a çıkışta önce iç scope defer kayıtları, sonra iç scope otomatik cleanup; ardından dış scope defer kayıtları ve dış scope otomatik cleanup yürütülür.

### 12.26.7 Negatif testler

- **MR-OIR-017-N1:** Defer ifadesine yürütme sırasında ulaşılmadan kayıt oluşturulması.
- **MR-OIR-017-N2:** Aynı-scope JUMP'ta defer'in erken çalıştırılması.
- **MR-OIR-017-N3:** LIFO yerine FIFO yürütme.
- **MR-OIR-017-N4:** Otomatik kaynak cleanup'ın defer gövdesinden önce çalışması.
- **MR-OIR-017-N5:** Scope dışına JUMP'ta defer'in atlanması.
- **MR-OIR-017-N6:** İç scope kapanmadan dış scope defer'inin çalıştırılması.
- **MR-OIR-017-N7:** Defer gövdesinden non-local JUMP/RETURN/BREAK/CONTINUE.
- **MR-OIR-017-N8:** Nested defer kaydının kabul edilmesi.
- **MR-OIR-017-N9:** Optimizer'ın defer kayıt sırasını veya activation bağını kaybetmesi.
- **MR-OIR-017-N10:** Bir scope activation'ının defer kayıtlarının başka activation'a taşınması.
- **MR-OIR-017-N11:** Cleanup hata sonucu oluştuğu halde normal continuation kenarının üretilmesi; bu durum MR-OIR-020 ile ayrıntılı test edilir.

### 12.26.8 Sonraki testlere devredilen alt sözleşmeler

- Dinamik kayıt tekilliği ve loop activation sınırları MR-OIR-018'de kesinleştirilir.
- Defer değişkenlerinin canlı bağlanması ve kaynak sahipliği MR-OIR-019'da kesinleştirilir.
- Defer/cleanup hata, panic, ?;, }?; ve kaynak edinim politikası MR-OIR-020'de kesinleştirilir.
- Runtime opcode ve binary record encoding, semantik karar değil OIR format/ABI uygulama sözleşmesidir.


## 12.27 MR-OIR-018 — Dinamik defer kaydı ve loop activation sınırları

**Amaç:** Defer ifadesine her dinamik erişimin ayrı kayıt oluşturmasını, loop body scope'unun iterasyon başına yeni activation olmasını ve aynı activation içindeki tekrar erişimlerin LIFO sırasını doğrulamak.

### 12.27.1 Normatif kurallar

1. Yürütme defer ifadesine her ulaştığında mevcut scope activation'ına yeni bir kayıt eklenir.
2. Aynı defer kaynak satırına bir activation içinde birden fazla kez ulaşılırsa her erişim ayrı kayıttır.
3. Bir activation'daki tüm kayıtlar LIFO sırasıyla çalışır.
4. Loop body bir lexical block scope ise her iterasyon yeni activation oluşturur; o iterasyonun defer kayıtları iterasyon scope'u kapanırken çalışır.
5. Aynı-scope backward JUMP scope activation'ını kapatmadığı için kayıtları çalıştırmaz; defer ifadesine yeniden ulaşılması yeni kayıt ekler.
6. Defer gövdesinin değişkenleri hangi değerlerle gözlemlediği (capture by value/reference) bu testin kapsamı dışındadır ve ayrıca tanımlanacaktır.

### 12.27.2 Test A — Aynı activation içinde tekrar kayıt

```nexus
f:testRepeatedDeferRegistration()!i32 {
    v:i = 0;

    again:>
    defer {
        echo("deferred");
    }

    v = v + 1;
    (v < 3) ? jump(again);

    return v;
}
```

Beklenen:
- `again` aynı lexical scope içindedir; backward jump scope activation'ını kapatmaz.
- Defer ifadesine üç dinamik erişim üç ayrı kayıt oluşturur.
- Fonksiyon scope'u kapanırken üç kayıt ters kayıt sırasıyla yürütülür.
- Jump kaynak satırlarının ardından gelen normal akışı atlar; her erişim için kayıt sayısı doğru tutulur.

### 12.27.3 Test B — Her loop iterasyonunda yeni body activation

```nexus
f:testDeferPerIteration()!i32 {
    loop(i, i < 3, i++) {
        defer {
            echo("iteration-exit");
        }

        echo("iteration-work");
    }

    return 0;
}
```

Beklenen:
- Her iterasyon yeni loop-body scope activation'ı oluşturur.
- Her iterasyonda defer bir kez kaydedilir.
- Her iterasyon sonunda o iterasyonun defer'i çalışır; kayıtlar sonraki iterasyona taşınmaz.
- Loop tamamlandığında önceki iterasyonlara ait bekleyen defer kaydı kalmaz.

### 12.27.4 Negatif testler

- **MR-OIR-018-N1:** Aynı activation içindeki ikinci dinamik defer erişiminin önceki kaydı ezmesi.
- **MR-OIR-018-N2:** Aynı-scope backward JUMP'ın scope activation'ını yanlışlıkla kapatması.
- **MR-OIR-018-N3:** Loop iterasyonlarının defer kayıtlarının tek activation'da birleştirilmesi.
- **MR-OIR-018-N4:** Loop-body scope kapanışında o iterasyonun defer kaydının atlanması.
- **MR-OIR-018-N5:** LIFO sırasının dinamik kayıt sırasından farklı yürütülmesi.
- **MR-OIR-018-N6:** Optimizer'ın tekrar erişilen defer kaydını tek kayda indirgemesi.
- **MR-OIR-018-N7:** Bir iterasyonun defer kaydının sonraki iterasyon activation'ına taşınması.

### 12.27.5 Sonraki sözleşmeye bağlantı

Defer değişkenlerinin live-binding modeli MR-OIR-019'da kesinleştirilmiştir. Bu bölüm yalnızca dinamik kayıt ve loop activation sınırlarını test eder.

## 12.28 MR-OIR-019 — Defer değişken yakalama ve kaynak sahipliği

**Amaç:** Defer gövdesinin değişkenleri hangi anda gözlemlediğini tek ve deterministik bir kurala bağlamak; kaynak sahipliği aktarımıyla çakışma, çift serbest bırakma ve geçersiz kaynak erişimini önlemek.

### 12.28.1 Kesin karar: canlı bağlama (live binding)

Bu sürümde `defer { ... }` **value capture yapmaz**. Defer gövdesi, kaydedildiği lexical scope activation'ındaki değişkenlerin aynı bağlarına erişir ve çalıştırıldığı anda bu değişkenlerin güncel değerlerini okur.

1. Defer kaydı oluşturulurken gövde içindeki değişkenlerin değerleri kopyalanmaz.
2. Defer gövdesindeki değişken okumaları, defer çalıştırıldığı andaki güncel değişken değerini görür.
3. Defer gövdesindeki yazmalar aynı lexical değişken bağını günceller; ayrı bir kopyayı değiştirmez.
4. Defer gövdesi yalnızca kaydedildiği noktada görünür olan ve ilgili scope activation'ı boyunca yaşayan lexical değişkenlere erişebilir.
5. Defer çalıştırılana kadar bu değişken bağları geçerli kalmalıdır. OIR Builder ve Optimizer, defer tarafından gözlemlenebilecek son değerleri korumalıdır.
6. Bu dil sürümünde defer için açık capture listesi veya value-capture sözdizimi yoktur; yeni bir capture sözdizimi eklenmez.

Dolayısıyla kayıt zamanı ile yürütme zamanı arasında değişken güncellenmişse defer gövdesi eski snapshot'ı değil, güncel değeri görür.

### 12.28.2 OIR ve çalışma zamanı temsili

Defer kaydı şu bilgileri taşır:

- Defer gövdesinin statik kimliği/entry noktası.
- Kaydın ait olduğu scope activation kimliği.
- Activation içindeki dinamik kayıt sırası.

Kayıt, dış değişkenlerin değer kopyalarını veya her değişken için ayrı capture buffer'ını taşımaz. Defer gövdesi, kendisini kaydeden activation'ın değişken ortamına (lexical frame/environment) bağlanır.

- Builder, defer tarafından sonradan okunabilecek veya yazılabilecek yerel değişkenleri defer çalışması tamamlanana kadar canlı tutmalıdır.
- SSA tabanlı OIR'de güncel değerler, defer çıkış bloğuna doğru SSA use/merge/block-parameter bağlantılarıyla taşınmalıdır.
- Defer gövdesi için gerekli değişkeni erken stack-slot reuse, register reuse veya dead-store/dead-value optimizasyonuyla yok etmek yasaktır.
- Optimizer, defer'in gözlemlediği son değeri değiştirecek şekilde store/load sırasını yeniden düzenleyemez.
- Defer gövdesi normal tamamlandığında activation içindeki sonraki LIFO kaydına geçilir.

Bu maddeler semantik sözleşmedir; nihai opcode isimleri, frame layout ve runtime ABI ayrı OIR format sözleşmesinde tanımlanır.

### 12.28.3 Kaynak sahipliği ve `->_` aktarımı

Defer, kaynak için ikinci bir sahip, kopya handle veya örtük retain oluşturmaz. Kaynak sahipliği değişkenin mevcut ownership durumunda kalır.

1. Defer gövdesi kaynak değişkenini kullandığında, yürütme anındaki ownership durumu esas alınır.
2. Kaynak sahipliği `->_` ile başka bir değişkene aktarılırsa sahiplik kaynak değişkenden çıkar ve hedef değişkene geçer.
3. Defer gövdesi aktarım sonrasında eski kaynak değişken üzerinden taşınmış kaynağı kullanamaz veya serbest bırakamaz.
4. Defer gövdesi hedef değişkeni lexical görünürlük ve scope-lifetime kuralları içinde kullanabilir; hedef değişken o anda kaynağın geçerli sahibi olmalıdır.
5. Sahip olunan bir kaynak, önce serbest bırakılmadan veya sahipliği aktarılmadan başka bir kaynakla üzerine yazılamaz.
6. Defer gövdesinin her olası çıkış yolunda kullandığı kaynak değişkeni başlatılmış ve geçerli sahip olmalıdır. Path-sensitive ownership analizi bu koşulu kanıtlayamıyorsa Analyzer derlemeyi reddeder.
7. Defer gövdesi bir kaynağı açıkça serbest bırakırsa ownership ledger bu durumu kaydeder; scope'un otomatik cleanup aşaması aynı kaynağı ikinci kez serbest bırakamaz.
8. Defer kaydı kendi başına otomatik kaynak cleanup'ının yerine geçmez ve kaynak sahipliğini scope dışına taşımaz.

Bu modelde defer'in hangi handle'ı kullanacağı kayıt anındaki snapshot'a göre değil, yürütme anındaki değişken bağının güncel ownership durumuna göre belirlenir.

### 12.28.4 Kesin örnek davranışları

**Test A — Skaler değişkenin güncel değerini görme**

```nexus
f:testDeferLiveBinding() {
    value:i = 10;

    defer {
        observe(value);
    }

    value = 25;
}
```

Beklenen: `observe` çağrısı scope kapanışında `25` değerini görür; `10` değerinin snapshot'ı alınmaz.

**Test B — Defer gövdesinin değişkeni güncellemesi**

```nexus
f:testDeferWritesBack() {
    state:i = 1;

    defer {
        state = state + 4;
    }

    state = 3;
}
```

Beklenen: defer çalıştığında `state` değeri `3)'tür ve aynı lexical değişken `7)'ye güncellenir. Defer'e özel ikinci bir değişken oluşmaz.

**Test C — Kaynak sahipliği aktarımı**

Bu test, OIR ownership fixture'ı olarak değerlendirilir; aşağıdaki adlar açıklayıcı sembollerdir, yeni kaynak sözdizimi tanımlamaz:

- `resourceA` başlangıçta tek sahibi olan kaynak değişkenidir.
- Defer gövdesi `resourceA)'yı kullanacak şekilde kaydedilir.
- Daha sonra `resourceA ->_ resourceB` sahiplik aktarımı gerçekleşir.
- Scope kapanışında defer, `resourceA)'nın eski handle'ını kullanamaz. Ownership ledger, kaynağın `resourceB)'ye geçtiğini gösterir.
- Defer gövdesi taşınmış kaynağa eski değişken üzerinden erişmeye çalışıyorsa Analyzer hata verir.
- Kaynak yalnızca `resourceB)'nin sahipliğinde cleanup edilir; çift serbest bırakma oluşmaz.

### 12.28.5 Negatif testler

- **MR-OIR-019-N1:** Defer gövdesinin değişkeni kayıt anındaki eski snapshot değeriyle okuması.
- **MR-OIR-019-N2:** Defer gövdesi değişken yazmasının ayrı bir capture kopyasına gitmesi.
- **MR-OIR-019-N3:** Defer tarafından kullanılacak lexical değişkenin scope kapanmadan önce stack/register reuse ile geçersiz kılınması.
- **MR-OIR-019-N4:** `->_` ile aktarılmış kaynağın eski değişken üzerinden defer içinde kullanılması.
- **MR-OIR-019-N5:** Sahip olunan kaynağın release/transfer olmadan üzerine yazılması.
- **MR-OIR-019-N6:** Defer'in açıkça serbest bıraktığı kaynağın otomatik cleanup tarafından ikinci kez serbest bırakılması.
- **MR-OIR-019-N7:** Kontrol akışının bazı çıkışlarında kaynak sahipliği kanıtlanamadığı halde defer erişiminin kabul edilmesi.
- **MR-OIR-019-N8:** Optimizer'ın defer gövdesinin gözlemlediği güncel değeri eski değerle değiştirmesi.
- **MR-OIR-019-N9:** Defer kaydının değişken ortamını yanlış scope activation'ına bağlaması.
- **MR-OIR-019-N10:** Defer için tanımlanmamış value-capture/capture-list davranışı üretilmesi.

### 12.28.6 Doğrulama ve tamamlanma koşulları

MR-OIR-019 başarılı sayılması için:

- Scalar read/write testleri live-binding davranışını göstermeli.
- Defer'in kullandığı değişkenler activation sonuna kadar geçerli kalmalı.
- Ownership transfer sonrası eski sahibin kullanımı reddedilmeli.
- Defer release ile otomatik cleanup aynı ownership kaydını iki kez tüketmemeli.
- CFG merge noktalarında ownership durumu tüm predecessor yollarında doğrulanmalı.
- OIR round-trip ve optimizer testleri defer gövdesinin activation, değişken bağı ve güncel değer ilişkisini korumalıdır.

Bu kararla value capture/reference capture ayrımı açık konu olmaktan çıkarılmıştır: **NEXUS Flow defer modeli live binding'dir.**

## 12.29 MR-OIR-020 — Defer/cleanup hata ve panic yönetimi

**Amaç:** Defer veya otomatik kaynak temizliği sırasında hata oluştuğunda kalan cleanup işlemlerinin atlanmamasını; normal çıkış, hata çıkışı, panic ve açık sonlandırma davranışlarının deterministik olmasını sağlamak.

### 12.29.1 Kesin karar: cleanup zinciri best-effort ve tamamlama garantili

Bir scope kapanırken başlatılan cleanup zinciri, tek bir defer veya kaynak cleanup hatası nedeniyle durdurulmaz.

1. Scope'un aktif defer kayıtları LIFO sırasıyla çalıştırılır.
2. Bir defer normal tamamlanırsa sıradaki kayda geçilir.
3. Bir defer hata üretirse hata CleanupErrorSet'e eklenir ve sıradaki defer yine çalıştırılır.
4. Bütün defer kayıtları tükendikten sonra scope'un otomatik kaynak cleanup aşaması başlar.
5. Otomatik cleanup'ta bir kaynak serbest bırakma hatası oluşursa hata kaydedilir ve sonraki kaynak için cleanup sürdürülür.
6. Scope cleanup aşamalarının tamamı bitmeden scope activation kapatılmış sayılmaz.
7. Cleanup zinciri tamamlandığında hata kümesi boşsa istenen kontrol akışı hedefine devam edilir; hata kümesi doluysa normal hedefe geçilmez ve cleanup failure yolu izlenir.

Bu davranış best-effort'tür: her cleanup eylemi denenir; bir eylemin başarılı olacağı garanti edilmez. Ancak tek bir eylemin başarısızlığı diğer cleanup eylemlerini engelleyemez.

### 12.29.2 Hata önceliği ve biriktirme

Cleanup sonucu, cleanup başlamadan önceki kontrol akışı sonucuna göre belirlenir:

| Cleanup öncesi durum | Cleanup sırasında hata | Nihai davranış |
|---|---|---|
| Normal scope/function çıkışı | Yok | Normal hedefe devam |
| Normal scope/function çıkışı | Bir veya daha fazla | Normal devam iptal edilir; CleanupFailure üretilir |
| Zaten hata nedeniyle çıkış | Bir veya daha fazla | İlk hata birincil kalır; cleanup hataları ikincil olarak eklenir |
| Panic ( !! ) nedeniyle çıkış | Bir veya daha fazla | İlk panic birincil kalır; cleanup hataları panic kaydına eklenir |
| Açık sonlandırma ( ?; ) | Bir veya daha fazla | Sonlandırma kararı korunur; cleanup hataları sonlandırma tanı kaydına eklenir |

Kurallar:

- Cleanup sırasında oluşan ilk hata, normal çıkışta birincil CleanupFailure olur.
- Sonraki cleanup hataları aynı hata kümesine oluşma sırasıyla eklenir.
- Önceden birincil hata veya panic varsa cleanup hataları onu değiştiremez; yalnızca ikincil tanı olarak eklenir.
- Hata kayıtları kaynak konumu, scope activation kimliği, defer/resource kimliği ve hata türünü taşır.
- Cleanup hatası bir return değerini başarılı dönüşe çeviremez. Return değeri cleanup öncesinde güvenli geçici konuma alınır; cleanup başarısızsa normal return tamamlanmaz ve hata sonucu çağırana taşınır.
- İç scope cleanup hatası oluşursa o scope'un cleanup'ı tamamlanır; ardından hata dış scope'lara yayılır ve dış scope'ların defer/cleanup işlemleri de tamamlanır.
- Çağıran taraf, fonksiyonun diğer hata sonuçlarında olduğu gibi mevcut hata akış mekanizmasıyla sonucu ele alabilir; bu özellik için yeni bir kaynak sözdizimi veya yeni bir operatör eklenmez.

### 12.29.3 Kaynak cleanup'ında tek deneme ve ownership ledger

Otomatik kaynak cleanup her sahip olunan kaynak için en fazla bir kez serbest bırakma girişiminde bulunur.

1. Cleanup çağrısından önce kaynak ownership kaydı RELEASE_ATTEMPTED durumuna geçirilir.
2. Serbest bırakma başarılı olursa kayıt RELEASED olur.
3. Serbest bırakma hata verirse kayıt RELEASE_FAILED olur; aynı scope cleanup zincirinde otomatik olarak tekrar denenmez.
4. Defer gövdesi kaynağı açıkça serbest bırakmışsa ownership kaydı önceden tüketilmiş olur ve otomatik cleanup kaynağı atlar.
5. Ownership transfer edilmiş kaynak eski sahibin cleanup listesinde serbest bırakılmaz; yalnızca güncel sahibin cleanup listesi sorumludur.
6. Aynı fiziksel kaynağa birden fazla sahiplik kaydı oluşturulması Analyzer/Verifier tarafından reddedilir.
7. Cleanup hatası kaynak sahipliğini tekrar kullanılabilir hâle getirmez. Başarısız release girişiminden sonra handle geçersiz/consumed kabul edilir; ikinci release veya eski handle üzerinden erişim yasaktır.

Otomatik cleanup sırası, scope içindeki kaynak edinim sırasının tersidir (LIFO). Böylece kaynaklar, en son edinilenden ilk edinilene doğru kapatılır. Defer kayıtları her zaman otomatik kaynak cleanup'ından önce çalışır.

### 12.29.4 Panic, hata ve açık sonlandırma sırasında unwind

**Panic ( !! )**

- Panic, aktif lexical scope'ların unwind edilmesini başlatır.
- Her scope için önce defer kayıtları LIFO, ardından otomatik kaynak cleanup çalışır.
- Cleanup hataları ilk panic'i bastırmaz veya normal hata sonucuna dönüştürmez.
- Tüm aktif scope cleanup işlemleri tamamlandıktan sonra panic sonlandırması sürer.

**Açık sonlandırma ( ?; )**

- ?; fonksiyonun/akışın normal devamını sonlandırır; aktif scope'ların defer ve otomatik cleanup işlemleri yine yürütülür.
- Cleanup hataları sonlandırmayı iptal etmez; sonlandırma tanı kaydına eklenir.
- ?; bir normal return veya çağırana dönen CleanupFailure değildir.

**Normal hata akışı ( ?-> )**

- Normal yürütmede bir hata ?-> ile yakalanmışsa ilgili hata bloğu kendi tanımlı akışında çalışır.
- Defer/cleanup içindeki bir işlem kendi yerel hata yönlendirmesiyle hatayı tamamen ele alırsa cleanup hatası kaydedilmez.
- Hata yönlendirilmeden defer gövdesinden dışarı çıkarsa cleanup hatası olarak biriktirilir.
- Cleanup failure, mevcut fonksiyon hata sonucuyla çağırana aktarılır; çağıran bunu mevcut ?-> akışıyla ele alabilir.

### 12.29.5 Defer gövdesinin izin verilen işlemleri

Cleanup zincirinin sonlu ve analiz edilebilir kalması için defer gövdesi:

- Normal ifadeler, fonksiyon çağrıları, değişken okumaları/yazmaları ve mevcut kaynakları kullanma/serbest bırakma işlemleri içerebilir.
- Hata üretebilen işlemler için mevcut yerel ?-> hata yönlendirmesini kullanabilir.
- return, break, continue, jump(label), ?; veya scope dışına kontrol aktaran başka bir terminator içeremez.
- }?; ile kendi veya dış scope'un cleanup zincirini elle başlatamaz.
- Yeni defer kaydı oluşturamaz.
- Yeni sahip olunan kaynak edinemez. Cleanup sırasında yeni sahiplik oluşturmak yerine mevcut kaynaklar tüketilmeli veya serbest bırakılmalıdır.
- Bu yasakları dolaylı fonksiyon çağrısı üzerinden ihlal edemez. Analyzer çağrılan fonksiyonun etkilerini doğrulayamıyorsa çağrıyı defer gövdesinde reddeder.

Böylece cleanup sırasında cleanup listesine yeni kayıt eklenmesi, sonsuz cleanup döngüsü veya scope'un ikinci kez kapatılması mümkün olmaz.

### 12.29.6 Kesin scope kapanış algoritması

Aşağıdaki şema semantik algoritmadır; opcode isimleri değildir.

~~~text
CloseScope(scope, pendingOutcome):
    while scope.deferStack is not empty:
        record = pop_LIFO(scope.deferStack)
        result = execute(record.body)
        if result is cleanup_error:
            append_cleanup_error(result)

    for resource in scope.ownedResources in reverse_acquisition_order:
        if resource.state == OWNED:
            resource.state = RELEASE_ATTEMPTED
            result = release(resource)
            if result is success:
                resource.state = RELEASED
            else:
                resource.state = RELEASE_FAILED
                append_cleanup_error(result)

    mark scope activation CLOSED

    if pendingOutcome is PANIC or EXPLICIT_HALT:
        attach cleanup errors to pendingOutcome
        continue unwind/termination
    else if pendingOutcome is ERROR:
        attach cleanup errors to pendingOutcome
        continue outer-scope unwind
    else if cleanup errors exist:
        create CleanupFailure
        continue outer-scope unwind
    else:
        continue requested control-flow target
~~~

İç içe scope'larda algoritma en iç scope'tan dış scope'a doğru tekrarlanır. Bir scope'un cleanup'ı tamamlanmadan dış scope cleanup'ı başlatılmaz.

### 12.29.7 Negatif testler

- **MR-OIR-020-N1:** İlk defer hatasında sonraki defer kayıtlarının atlanması.
- **MR-OIR-020-N2:** Defer hatası nedeniyle otomatik kaynak cleanup'ın hiç başlatılmaması.
- **MR-OIR-020-N3:** İlk kaynak release hatasında sonraki kaynakların bırakılmaması.
- **MR-OIR-020-N4:** Başarısız release işleminin aynı unwind sırasında ikinci kez denenmesi.
- **MR-OIR-020-N5:** Defer tarafından release edilmiş kaynağın otomatik cleanup'ta tekrar bırakılması.
- **MR-OIR-020-N6:** Cleanup hatasının daha önce oluşmuş asıl hata veya panic'i ezmesi.
- **MR-OIR-020-N7:** Cleanup hatasına rağmen normal return/branch hedefinin başarılıymış gibi yürütülmesi.
- **MR-OIR-020-N8:** İç scope cleanup hatasında dış scope defer kayıtlarının atlanması.
- **MR-OIR-020-N9:** !! panic sırasında aktif defer/cleanup zincirinin atlanması.
- **MR-OIR-020-N10:** ?; sonlandırmasının aktif scope cleanup'ını atlaması.
- **MR-OIR-020-N11:** Defer gövdesinden return, break, continue, jump veya ?; ile çıkılması.
- **MR-OIR-020-N12:** Defer gövdesinin }?; ile cleanup zincirini yeniden başlatması.
- **MR-OIR-020-N13:** Defer gövdesinin yeni defer kaydı veya sahip olunan kaynak edinmesi.
- **MR-OIR-020-N14:** Cleanup hata kayıtlarının deterministik oluşma sırasının bozulması.
- **MR-OIR-020-N15:** Optimizer'ın cleanup hata yolunu, ownership state geçişini veya unwind sırasını kaldırması/değiştirmesi.

### 12.29.8 Pozitif testler ve tamamlanma koşulları

**Test A — Bir defer hata verir, diğerleri yine çalışır**

Bir scope'a sırasıyla D1, D2, D3 kaydedilir. Kapanışta D3 hata verir; D2 ve D1 yine çalışır. Otomatik kaynak cleanup bundan sonra başlar. Hata listesi D3'ün hatasını içerir ve normal devam engellenir.

**Test B — Bir kaynak release hatası verir**

Üç kaynak R1, R2, R3 edinilmiştir. Cleanup sırası R3, R2, R1'dir. R3 release hatası verse bile R2 ve R1 denenir. R3 ikinci kez denenmez.

**Test C — Panic sırasında cleanup**

Bir defer gövdesi !! ile panic oluşturur. Kalan defer kayıtları ve tüm aktif scope kaynak cleanup işlemleri tamamlanır. İlk panic birincil sonuç olarak korunur; cleanup hataları ikincil tanı olur.

**Test D — İç scope hatası ve dış scope cleanup**

İç scope defer'i hata verir. İç scope'un kalan defer ve kaynak cleanup işlemleri tamamlanır; ardından dış scope'un defer ve kaynak cleanup işlemleri de tamamlanır. Normal hedefe devam edilmez.

**Test E — Yerel hata yönlendirmesi**

Defer içindeki hata üreten çağrı kendi ?-> bloğunda başarıyla ele alınır. Bu hata CleanupErrorSet'e eklenmez ve sonraki defer kayıtları normal sırayla yürür.

MR-OIR-020 başarılı sayılması için tüm cleanup eylemlerinin tek hata yüzünden atlanmadığı, ownership kaydının tek sefer tüketildiği, birincil hata önceliğinin korunduğu, normal continuation'ın yalnızca temizleme başarılıysa sürdüğü ve OIR round-trip/optimizer'ın bu davranışı koruduğu gösterilmelidir.

Bu kararla defer/cleanup hata ve panic politikası normatif olarak sabitlenmiştir. Runtime opcode ve binary encoding seçimi semantik belirsizlik değildir; OIR format/ABI uygulama sözleşmesinde bu davranışı koruyacak biçimde kodlanacaktır.

## 12.30 MR-OIR-021 — Defer/cleanup OIR kayıtları, CFG lowering ve doğrulama

**Amaç:** MR-OIR-017–020'de kesinleştirilen defer davranışını OCC'nin OIR Builder, Writer, Reader, Verifier ve Optimizer modüllerine doğrudan uygulanabilir tek bir veri ve kontrol akışı modeline dönüştürmek.

### 12.30.1 OIR sürüm ve özellik sözleşmesi

Defer/cleanup kayıtlarını içeren OIR artifact'ı **OIR 1.1** sürümünde yazılır.

- OIR 1.0, defer/cleanup kaydı içermeyen eski artifact'lar için okunabilir kalır.
- OIR 1.0 artifact'ında defer/cleanup özelliği veya bu özelliğe ait kayıt bulunursa Verifier artifact'ı reddeder.
- OIR 1.1, defer/cleanup kayıtlarının tamamını ve bu bölümdeki zorunlu metadata'yı taşır.
- Reader desteklemediği zorunlu feature veya record türünü atlayamaz; açık UnsupportedFeature/UnsupportedVersion hatası üretir.
- Writer, bir fonksiyonda defer bulunuyorsa tüm artifact sürümünü 1.1 olarak yazar.
- OIR 1.1 içinde defer bulunmayan fonksiyonlar da bulunabilir.
- Sürüm yükseltme, eski artifact'ı sessizce yeniden yorumlamak anlamına gelmez. Dönüşüm gerekiyorsa ayrı ve doğrulanan bir OIR migration adımı kullanılır.

### 12.30.2 Kanonik OIR kayıtları

OIR 1.1 defer modeli aşağıdaki kayıtları kullanır. İsimler bu sözleşmede kanonik alan/opcode adlarıdır; Writer'ın binary byte encoding'i aynı semantiği korumak zorundadır.

**ScopeActivation**

| Alan | Tür | Anlam |
|---|---|---|
| `activation_id` | u64 | Çalışma anında benzersiz activation kimliği |
| `parent_activation_id` | u64? | İç içe scope için ebeveyn activation |
| `function_id` | u32 | Ait olduğu fonksiyon |
| `lexical_scope_id` | u32 | Kaynak lexical scope kimliği |
| `state` | enum | OPEN, CLEANING_DEFERS, CLEANING_RESOURCES, CLOSED |
| `defer_head` | record_ref? | En son eklenmiş aktif defer kaydı |
| `resource_head` | record_ref? | En son edinilmiş sahip olunan kaynak kaydı |
| `pending_outcome` | OutcomeRef | Normal dönüş, hata, panic veya explicit halt sonucu |
| `cleanup_error_head` | error_ref? | Bu activation için biriktirilen ilk cleanup hatası |

**DeferRecord**

| Alan | Tür | Anlam |
|---|---|---|
| `record_id` | u64 | Dinamik kayıt kimliği |
| `activation_id` | u64 | Kaydın ait olduğu scope activation |
| `sequence_no` | u64 | Activation içindeki monotonik kayıt sırası |
| `body_id` | u32 | Defer gövdesinin statik OIR entry kimliği |
| `environment_ref` | TargetPtr | Live-binding lexical environment/frame referansı |
| `previous_record` | record_ref? | Önceki defer kaydı |
| `state` | enum | REGISTERED, RUNNING, COMPLETED, FAILED |
| `error_slot` | error_ref | Bu kaydın cleanup sonucu/hata yuvası |

**OwnedResourceRecord**

| Alan | Tür | Anlam |
|---|---|---|
| `resource_id` | u64 | Kaynak sahiplik kaydı kimliği |
| `activation_id` | u64 | Kaynağın sahibi olan activation |
| `sequence_no` | u64 | Activation içindeki edinim sırası |
| `resource_handle` | TargetPtr | Kaynak handle'ı |
| `owner_symbol_id` | u32 | Analyzer sembol tablosundaki güncel owner |
| `state` | enum | OWNED, RELEASE_ATTEMPTED, RELEASED, RELEASE_FAILED, TRANSFERRED |
| `previous_record` | record_ref? | Önceki kaynak kaydı |
| `error_slot` | error_ref | Release hata yuvası |

**CleanupErrorRecord**

| Alan | Tür | Anlam |
|---|---|---|
| `error_id` | u64 | Hata kaydı kimliği |
| `activation_id` | u64 | Hatanın oluştuğu activation |
| `sequence_no` | u64 | Cleanup sırasında oluşma sırası |
| `origin_kind` | enum | DEFER, RESOURCE_RELEASE, DEFER_PANIC, CAPACITY |
| `origin_id` | u64 | Defer/resource kayıt kimliği |
| `source_location` | SourceLocation | Kaynak dosya/satır/sütun |
| `error_code` | u32 | Stabil hata kodu |
| `payload_ref` | TargetPtr? | İsteğe bağlı hata ayrıntısı |

**PendingOutcome**

Tek bir tagged record'dur:

- NORMAL: hedef CFG block kimliği ve varsa dönüş değeri referansı.
- ERROR: birincil hata referansı.
- PANIC: birincil panic payload referansı.
- EXPLICIT_HALT: sonlandırma nedeni ve tanı hedefi.

Bir activation aynı anda yalnızca bir PendingOutcome taşır. Cleanup hataları bu sonucu değiştirmez; MR-OIR-020 öncelik kurallarıyla eklenir.

### 12.30.3 Kanonik OIR işlemleri

| İşlem | İşlev |
|---|---|
| `SCOPE_ENTER` | Yeni ScopeActivation oluşturur ve parent bağlantısını kurar |
| `DEFER_PUSH` | DeferRecord'u etkin activation'ın defer_head başına ekler |
| `RESOURCE_OWN` | Yeni OwnedResourceRecord oluşturur |
| `OWNERSHIP_TRANSFER` | Kaydı eski owner'dan çıkarıp yeni activation/owner'a taşır |
| `SCOPE_UNWIND` | PendingOutcome ve devam hedefiyle cleanup başlatır |
| `DEFER_POP` | En son kayıtlı defer kaydını stack'ten alır |
| `DEFER_INVOKE` | Defer body'yi kayıtlı live environment ile yürütür |
| `RESOURCE_RELEASE_NEXT` | En son edinilmiş ve hâlâ OWNED kaynağı seçer |
| `RESOURCE_RELEASE` | Kaynağı tek denemelik release durum makinesiyle bırakır |
| `CLEANUP_ERROR_APPEND` | Cleanup hatasını ilgili error slot/listesine ekler |
| `SCOPE_CLOSE` | Defers ve kaynaklar tamamlandıktan sonra activation'ı CLOSED yapar |
| `OUTCOME_RESUME` | MR-OIR-020 önceliğine göre normal hedefe, dış unwind'a veya sonlandırmaya devam eder |

Bu işlemler semantik OIR işlemleridir. Code Generator bunları hedef ABI/runtime çağrılarına veya inline kontrol akışına indirebilir; semantik işlemi yok sayamaz.

### 12.30.4 CFG lowering algoritması

OIR Builder, lexical scope'tan çıkan her kontrol akışını cleanup girişine yönlendirir. Bu kapsamda RETURN, BREAK, CONTINUE, ancestor-scope JUMP, hata çıkışı, panic ve explicit halt bulunur.

1. Kaynak kontrol akışı işlemi bir PendingOutcome üretir.
2. Çıkılan en iç ScopeActivation için SCOPE_UNWIND başlatılır.
3. Activation CLEANING_DEFERS durumuna geçer.
4. DEFER_POP son kayıtlı defer'i alır. Kayıt RUNNING olur ve DEFER_INVOKE ile çalıştırılır.
5. Defer normal biterse COMPLETED; hata/panic üretirse FAILED olur ve hata kaydedilir. Her iki durumda da sonraki defer kaydına geçilir.
6. Defer stack boşalınca activation CLEANING_RESOURCES durumuna geçer.
7. RESOURCE_RELEASE_NEXT, en son edinilmiş OWNED kaydı seçer. Kayıt önce RELEASE_ATTEMPTED olur; release sonucu RELEASED veya RELEASE_FAILED durumuna geçer.
8. OWNED kayıt kalmayınca SCOPE_CLOSE activation'ı CLOSED yapar.
9. OUTCOME_RESUME pending sonucu değerlendirir. Dış lexical scope da terk edilecekse onun cleanup girişine geçilir; değilse MR-OIR-020'deki hata önceliği uygulanarak hedefe devam edilir.

Aynı lexical scope için aynı dinamik activation üzerinde birden fazla exit edge, aynı cleanup girişini paylaşabilir. Her giriş kendi PendingOutcome ve continuation değerini taşır; farklı çıkışların dönüş değeri veya hedefi birbirine karışamaz.

Defer gövdesi ayrı OIR subgraph olarak saklanır. DEFER_INVOKE, bu subgraph'a kayıtlı environment_ref ile girer ve yalnızca normal tamamlanma veya cleanup hata sonucu ile cleanup dispatcher'a döner. Defer gövdesi dış CFG'ye doğrudan terminator kenarı üretemez.

### 12.30.5 Dinamik kayıt ve bellek yönetimi

OIR 1.1 runtime modeli genel heap allocation kullanmaz.

- Her thread/task için target tarafından önceden ayrılmış bir CleanupArena bulunur.
- ScopeActivation, DeferRecord, OwnedResourceRecord ve cleanup error slotları bu arena içinden alınır.
- Arena kayıtları sabit boyutlu segmentlerden oluşur; segment içindeki kayıtlar indeks/referans ile bağlanır.
- DeferRecord her dinamik DEFER_PUSH'ta yeni bir slot alır. Aynı body_id veya kaynak satırı önceki kaydı değiştiremez.
- Kayıtlar activation kapanınca LIFO cleanup tamamlandıktan sonra arena free-list'ine geri verilir.
- Arena kapasitesi target konfigürasyonunda belirtilir; dilin anlamsal defer sayısı için sabit bir üst sınır tanımlanmaz.
- Arena yetersizse DEFER_PUSH atomik olarak başarısız olur: yarım kayıt veya eksik stack bağlantısı bırakılmaz. DEFER_CAPACITY hata sonucu oluşturulur ve o ana kadar geçerli biçimde kaydedilmiş cleanup işlemleri çalıştırılır.
- Cleanup hata slotları kayıtlarla birlikte önceden ayrıldığı için cleanup sırasında hata listesi büyütmek amacıyla heap allocation gerekmez.
- Arena kapasitesi veya segment sayısı hiçbir zaman sessiz truncation ile uygulanamaz; kapasite hatası açık ve deterministik olmalıdır.

### 12.30.6 OIR Writer / Reader sözleşmesi

**Writer**
- OIR 1.1 header ve zorunlu feature bilgisini yazar.
- ScopeActivation, defer body, DeferRecord, OwnedResourceRecord, source mapping ve hata slotu referanslarını kararlı kimliklerle serileştirir.
- Pointer değerlerini doğrudan süreç adresi olarak diske yazmaz; environment_ref ve resource referansları artifact içi relocation/handle tabloları üzerinden temsil edilir.
- Record listelerini sequence_no ve kayıt kimliğine göre deterministik sırada yazar.
- Her referansın hedef kaydını ve her body_id'nin geçerli subgraph'ını doğrulamadan başarılı artifact üretmez.

**Reader**
- Header sürümünü ve zorunlu feature setini ilk aşamada kontrol eder.
- Tüm record tablolarını referans çözümlemeden önce okur; ileri referanslar desteklenir.
- Referansları ikinci geçişte çözer; bulunmayan veya yanlış türde hedefe giden referansları reddeder.
- Dosyadan okunan pointer benzeri alanları çalışma zamanı adresi olarak kabul etmez; yalnızca doğrulanmış artifact handle/relocation kaydı üretir.
- OIR 1.0 dosyasında defer kayıtları, OIR 1.1 dosyasında ise eksik zorunlu defer alanları varsa dosyayı reddeder.
- Okuma ve yazma round-trip testi aynı mantıksal record graph, sequence ve CFG'yi üretmelidir.

### 12.30.7 Verifier zorunlu kontrolleri

Verifier aşağıdaki kontrollerin tamamını yapar:

1. Her activation_id benzersiz ve geçerli bir function_id/lexical_scope_id ile ilişkilidir.
2. Parent activation aynı fonksiyona aittir ve lexical parent zincirinde geçerlidir.
3. Her DeferRecord tam olarak bir etkin activation'a aittir; sequence_no aynı activation içinde benzersiz ve artandır.
4. Her body_id geçerli defer subgraph entry'sidir; subgraph'ın dönüşü yalnızca cleanup dispatcher'a yapılır.
5. environment_ref, kayıtlı activation'ın yaşayan lexical environment'ına işaret eder.
6. DeferRecord previous_record zinciri döngüsüzdür ve defer_head'den erişilen kayıt sayısı record tablosuyla tutarlıdır.
7. OwnedResourceRecord tek bir güncel owner'a sahiptir; TRANSFERRED kayıt eski owner cleanup zincirinde kalamaz.
8. Resource release state geçişleri OWNED → RELEASE_ATTEMPTED → RELEASED/RELEASE_FAILED dışında olamaz.
9. Her scope exit edge gerekli SCOPE_UNWIND zincirinden geçer; cleanup atlayan doğrudan RETURN/BREAK/CONTINUE/JUMP edge'i reddedilir.
10. Defer temizliği kaynak temizliğinden önce; kaynak temizliği ters edinim sırasında gerçekleşir.
11. Cleanup dispatcher bütün defer ve resource kayıtlarını tüketmeden SCOPE_CLOSE'a ulaşamaz.
12. PendingOutcome ve continuation block referansları geçerlidir; block parameter ve SSA edge argümanları tip/arity bakımından eşleşir.
13. Cleanup error slotları kayıt kimlikleriyle eşleşir; iki farklı kayıt aynı hata slotunu sahiplenemez.
14. Cleanup subgraph'larında yasaklanan non-local terminator, nested defer, yeni owned resource edinimi ve tekrar scope cleanup bulunamaz.
15. OIR sürümü, feature alanı ve kayıt türleri birbiriyle tutarlıdır.

### 12.30.8 Optimizer koruma kuralları

- Optimizer cleanup dispatcher'ı, defer stack sırasını, activation kimliklerini ve pending outcome ilişkisini korur.
- Defer body_id, environment_ref veya source mapping yalnızca bütün referanslar atomik biçimde güncellenebiliyorsa yeniden eşlenebilir.
- Defer gövdesinin gözlemlediği değişken store/load işlemleri MR-OIR-019 live-binding kurallarına tabidir.
- Ownership transfer ve release state geçişleri dead-code veya common-subexpression optimizasyonuyla silinemez.
- Cleanup hata yolu normal başarı yoluna birleştirilemez.
- Cleanup blokları ancak aynı activation, aynı pending outcome türü, aynı cleanup state ve aynı continuation sözleşmesi kanıtlanırsa birleştirilebilir.
- Cleanup zincirindeki kayıt sırası değiştirilemez.
- Defer bulunmayan ve cleanup etkisi taşımayan fonksiyonlarda normal CFG optimizasyonları uygulanabilir.

### 12.30.9 Pozitif ve negatif testler

**Test A — Tek defer kaydı**

Bir scope'a tek defer kaydedilir. OIR'de tek DeferRecord, geçerli body_id ve activation bağlantısı bulunur. Scope exit, defer subgraph'ına girer ve ardından resource cleanup aşamasına geçer.

**Test B — Aynı activation'da dinamik çoklu kayıt**

Aynı defer satırına backward JUMP ile üç kez ulaşılır. Üç ayrı record_id ve artan sequence_no oluşur. Çalışma sırası üçüncü, ikinci, birinci kayıttır.

**Test C — Çoklu exit edge paylaşımı**

Aynı scope RETURN ve BREAK ile terk edilebilir. Cleanup subgraph ortak kullanılabilir; fakat PendingOutcome ve continuation birbirinden ayrı kalır. Her iki çıkış da aynı cleanup sırasını uygular.

**Test D — İç içe activation**

İç scope'tan dış scope'a çıkışta önce iç activation tamamen kapanır, sonra dış activation cleanup edilir. Parent/child kimlikleri ve resource ownership korunur.

**Test E — OIR round-trip**

OIR Writer 1.1 defer/ownership/cleanup kayıtlarını yazar. Reader yeniden okur. Mantıksal record graph, sequence_no, body/environment bağlantıları ve CFG eşit kalır.

**Test F — Arena kapasitesi**

CleanupArena doluyken yeni DEFER_PUSH denenir. Yeni kayıt oluşmaz; DEFER_CAPACITY hatası üretilir. Önceden kaydedilmiş defer ve kaynak cleanup işlemleri eksiksiz yürütülür.

Negatif testler:

- **MR-OIR-021-N1:** OIR 1.0 artifact'ında defer kaydının kabul edilmesi.
- **MR-OIR-021-N2:** OIR 1.1'de zorunlu feature/record alanının eksik olmasına rağmen Reader'ın devam etmesi.
- **MR-OIR-021-N3:** Geçersiz body_id veya environment_ref.
- **MR-OIR-021-N4:** Başka fonksiyonun veya kapanmış activation'ın environment_ref'inin kullanılması.
- **MR-OIR-021-N5:** Aynı activation'da yinelenen record_id/sequence_no.
- **MR-OIR-021-N6:** Defer stack previous_record zincirinde kopukluk veya döngü.
- **MR-OIR-021-N7:** Cleanup öncesi doğrudan RETURN/BREAK/CONTINUE/JUMP kenarı.
- **MR-OIR-021-N8:** Defer cleanup tamamlanmadan resource cleanup veya SCOPE_CLOSE yapılması.
- **MR-OIR-021-N9:** Resource ownership'in iki owner'a birden atanması.
- **MR-OIR-021-N10:** RELEASE_ATTEMPTED sonrasında ikinci release geçişi.
- **MR-OIR-021-N11:** Cleanup error slotlarının farklı kayıtlarca paylaşılması.
- **MR-OIR-021-N12:** Arena kapasite hatasında yarım DeferRecord veya kopuk stack oluşması.
- **MR-OIR-021-N13:** Optimizer'ın defer kayıt sırasını değiştirmesi veya cleanup hata yolunu silmesi.
- **MR-OIR-021-N14:** Writer/Reader round-trip sırasında record referanslarının veya source mapping'in kaybolması.
- **MR-OIR-021-N15:** Farklı PendingOutcome/continuation taşıyan cleanup girişlerinin yanlış birleştirilmesi.

MR-OIR-021 tamamlanma koşulu: Builder'ın her scope exit'i cleanup zincirine yönlendirmesi; Writer/Reader'ın OIR 1.1 kayıtlarını kayıpsız taşıması; Verifier'ın tüm activation, ownership, CFG ve sürüm invariants'larını reddetme testleriyle doğrulaması; Optimizer'ın cleanup semantiğini koruması; arena kapasite hatasının yarım kayıt oluşturmadan yönetilmesidir.

Bu bölümle defer/cleanup'ın veri modeli, CFG lowering'i, kayıt belleği, sürüm uyumluluğu, Reader/Writer ve Verifier sözleşmesi kesinleştirilmiştir.
