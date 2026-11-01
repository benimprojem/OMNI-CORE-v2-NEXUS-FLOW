
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
