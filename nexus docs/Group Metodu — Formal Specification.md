
# 5.4 Group Metodu — Formal Specification

## 1. Tanım

**Group Metodu**, bir grup içerisinde benzer işlevlere sahip fonksiyonları organize etmek amacıyla kullanılan bir yapıdır. Bu metot, özellikle **kütüphane yazımı** ve **modülerlik** konusunda kolaylık sağlar. Aynı zamanda, **nesne yönelimli programlama (OOP)** yaklaşımını **group** yapıları üzerinde implement etmek için de uygundur.

**Group** metodu, bir tür **namespace** gibi çalışarak, fonksiyonları **gruplar** altında organize eder. Bu gruplar, hem **alt grup** hem de **default** grup yapıları ile genişletilebilir. Ayrıca **object-like** bir yapı oluşturularak, gruptaki fonksiyonlar nesne metodları gibi kullanılabilir.

---

## 2. Temel Yapı

Bir **group** şu şekilde tanımlanır:

```plaintext
group <group_adı> {
    <metot_adı> => f:(parametreler) { ... }
    ...
    default { ... }
}
```

* **`group_adı`**: Grubun adı (örneğin: `db`, `file`, `user`).
* **`<metot_adı>`**: Grup içerisinde tanımlanan işlevin adı.
* **`f:(parametreler)`**: Fonksiyon tanımı; parametreler ve fonksiyon içeriği belirtilir.
* **`default`**: Gruba ait varsayılan işlem (opsiyonel). Kullanıcı **group adı** belirtmezse varsayılan grup çalıştırılır.

Grup içerisinde **alt grup** (subgroup) tanımlanabilir. Alt grup, **grup adı** ve **subgroup adı** şeklinde organize edilir.

---

## 3. Kullanım

### 3.1 Group Tanımlaması

Bir **Group** fonksiyonu aşağıdaki gibi tanımlanır:

```plaintext
group db {
    # NoSQL alt grubu
    nosql => group {
        open => f:(...) { ... }
        close => f:(...) { ... }
        new => f:(...) { ... }
        query => f:(...) { ... }
        help => f:() { ... }
        default { echo("Yardım için db.nosql.help kullanın."); }
    }

    # SQLite alt grubu
    sqlite => group {
        open => f:(...) { ... }
        close => f:(...) { ... }
        new => f:(...) { ... }
        query => f:(...) { ... }
        help => f:() { ... }
        default { echo("Yardım için db.sqlite.help kullanın."); }
    }

    # Varsayılan (default) işlem
    default { echo("Yardım için db.sqlite.help veya db.nosql.help kullanın."); }
}
```

Bu tanımlamada:

* **`nosql` ve `sqlite` alt grupları** ile farklı veritabanı türlerine yönelik fonksiyonlar tanımlanır.
* **`default`** opsiyonel bir alan olup, grup ismi verilmeden çağrılan fonksiyon için varsayılan davranışı belirler.

### 3.2 Kullanım Örneği

Aşağıda, `db` grubunun nasıl kullanılacağını gösteren örnekler bulunmaktadır:

```plaintext
# NoSQL veritabanını açmak
db.nosql.open("category.db");

# SQLite veritabanını açmak
db.sqlite.open("product.db");

# Varsayılan db işlemi çalıştırmak
db  # Varsayılan grup çalıştırılır.
db.sqlite  # SQLite grubunun varsayılan fonksiyonu çalıştırılır.
```

---

## 4. Nesne Yönelimli Kullanım (Struct & Group)

### 4.1 Group ve Struct İlişkisi

Eğer bir `group`, bir **struct**, **enum**, veya **union**'a metot ekleyecekse, isimlerin **birebir aynı** olması gerektiği kuralı vardır. Bu, **OOP** tarzı bir kullanım sunar ve dışarıdan erişimi kontrol eder.

### 4.2 Nesne Metodu

Bir nesne metodunun ilk parametresi **`self`** olmak zorundadır. Bu sayede **nesneye ait özellikler** fonksiyon içinde manipüle edilebilir.

```plaintext
group Oyuncu {
    nt: struct:Oyuncu {
        v:ad!str,
        v:puan!i32,
        v:aktif!bool
    };

    # Yapıcı Fonksiyon (Constructor) - Statik
    yeni => f:(isim)!Oyuncu {
        return Oyuncu { 
            ad = isim,
            puan = 0,
            aktif = true 
        };
    }

    # Puan artırma fonksiyonu
    puan_arttir => f:(self, miktar) { 
        self.puan = self.puan + miktar;
    }

    # İsim getir fonksiyonu
    isim_getir => f:(self) > self.ad;
    
    # Alt grup tanımlamaları
    canta => group {
        envanter => { ... }
        silah => { ... }
    }
    
    default { echo("Yardım için Oyuncu.help kullanın."); }
}
```

### 4.3 Nesne Metodu Kullanımı

```plaintext
# Yeni bir Oyuncu oluştur
o1 = Oyuncu.yeni("Ahmet");

# Puan artır
o1.puan_arttir(10);

# Oyuncunun ismini getir
echo(o1.isim_getir());  # Çıktı: Ahmet
```

Burada:

* **`self`** ilk parametre olarak kullanılarak, **Oyuncu** nesnesine ait özelliklere (`ad`, `puan`, `aktif`) erişilebilir.
* **Alt grup** olan `canta`, **Oyuncu** nesnesiyle ilişkili olarak kullanılır: `Oyuncu.canta.envanter()`.

### 4.4 Yapıcı Fonksiyon (Factory / Constructor API)

Nexus Flow Group modelinde yapıcı işlev, **instance metodu değil statik Group method'udur**. Group ile aynı isimdeki Struct, oluşturulacak instance'ın nominal türünü belirler; yapıcı işlev ise bu türden bir değer üretir.

Normatif kurallar:

1. Constructor/factory, Group'un public method alias'ı olarak tanımlanır.
2. Constructor çağrısı `Type.alias(args)` biçimindedir. Group-qualified çağrı aynı statik callable API'ye ulaşır; Group ve bağlı Type aynı callable yüzeyi kullanır.
3. Constructor ilk parametrede `self` alamaz. `self` alan bir method instance method'dur ve `instance.alias(args)` biçiminde çağrılır.
4. Factory dönüş türü, Group'a bağlanmış Struct türü olmalıdır. Dönüş tipi imzası `!Type` biçiminde açıkça yazılır; Semantic çağrı ifadesinin türünü bu nominal tür olarak belirler.
5. Factory'nin döndürdüğü değer gerçekten oluşturulmuş/başka yerden edinilmiş bir Struct instance olmalıdır. Factory imzası tek başına bellekte instance oluşturmaz.
6. Public alias ile implementation adı ayrıdır. Çağrı public alias üzerinden çözülür; SemanticResult/OIR gerçek implementation symbol ID'sini taşır.
7. Struct literal/field initializer ile doğrudan instance üretimi ayrı bir dil özelliğidir. Parser/AST/Semantic/OIR zincirinde doğrulanmadan bu örnek çalışır kabul edilmez.

#### Kullanım biçimi A — Type üzerinden factory çağrısı

```nexus
group Oyuncu {
    struct:Oyuncu {
        v:ad!str,
        v:puan!i32,
        v:aktif!bool
    }

    yeni => f:(isim!str)!Oyuncu {
        # Instance üretimi/alan initializer desteği ayrıca gerekir.
        ret oyuncu;
    }

    puan_arttir => f:(self,miktar!i32) {
        self.puan = self.puan + miktar;
        ret;
    }
}

v:oyuncu!Oyuncu = Oyuncu.yeni("Ahmet");
oyuncu.puan_arttir(10);
```

`Oyuncu.yeni(...)` statik çağrıdır; implicit `self` aktarılmaz. Çağrı sonucu `Oyuncu` nominal türünde değerlendirilir.

#### Kullanım biçimi B — Group-qualified factory çağrısı

```nexus
v:oyuncu!Oyuncu = Oyuncu.yeni("Ahmet");
# Type ve Group qualifier aynı Group callable yüzeyine erişir.
v:oyuncu2!Oyuncu = Oyuncu.yeni("Mehmet");
```

Nested Group yolu Module Resolver/dotted-path aşamasına bağlıdır; bu örnek nested Group desteğinin tamamlandığı anlamına gelmez.

#### Kullanım biçimi C — Factory sonucunu başka fonksiyona/parametreye aktarma

```nexus
f:kaydet(oyuncu!Oyuncu) {
    oyuncu.puan_arttir(1);
    ret;
}

v:oyuncu!Oyuncu = Oyuncu.yeni("Ahmet");
kaydet(oyuncu);
```

Factory call expression türü atama ve fonksiyon parametre türü denetiminde kullanılabilir. Bu, factory'nin alanları initialize ettiği anlamına gelmez.

#### Instance metotları ile farkı

```nexus
Oyuncu.yeni("Ahmet");       # statik factory
oyuncu.puan_arttir(10);     # instance method; self otomatik bağlanır
Oyuncu.puan_arttir(10);     # HATA: instance method statik çağrılamaz
oyuncu.yeni("Ahmet");        # HATA: factory/static method instance üzerinden çağrılamaz
```

#### Doğrudan Struct literal sınırı

Aşağıdaki biçim **hedeflenen initializer örneğidir; mevcut destek olarak kabul edilmez**:

```nexus
# Henüz Parser + Semantic + OIR tarafından doğrulanmış constructor syntax değildir.
ret Oyuncu {
    ad = isim,
    puan = 0,
    aktif = true
};
```

Bu yazımın geçerli hale gelmesi için Struct literal AST node/field mapping, duplicate/unknown/missing field kontrolleri, field tür dönüşümü ve OIR instance oluşturma temsilinin tamamlanması gerekir. Bu aşama tamamlanana kadar factory, var olan bir `Oyuncu` değerini döndüren typed API olarak kullanılabilir; gerçek allocation/initialization garantisi vermez.


## 4.5 Struct Literal ve Alan Başlatma (Normatif Tasarım)

Struct instance oluşturmanın ifade biçimi **`Type { field = expression, ... }`** olarak belirlenir. Bu biçim, Group factory çağrısından bağımsız bir expression'dır; factory gövdesinde `ret Type { ... };` ile kullanılabilir. Lexer'da yeni token gerekmez: Type identifier'ından sonra `{` geldiğinde Parser bunu expression bağlamında Struct literal olarak ele alır. Struct declaration (`struct:Type { ... }`) ise declaration bağlamında kalır.

```nexus
group Oyuncu {
    struct:Oyuncu {
        v:ad!str,
        v:puan!i32,
        v:aktif!bool
    }

    yeni => f:(isim!str)!Oyuncu {
        ret Oyuncu {
            ad = isim,
            puan = 0,
            aktif = true
        };
    }
}

v:oyuncu!Oyuncu = Oyuncu.yeni("Ahmet");
```

### Alan başlatma kuralları

1. Literal'in Type adı, erişilebilir ve Group binding'iyle uyumlu nominal Struct türüne çözülmelidir. Group-owned type erişim izolasyonu aynen uygulanır.
2. Her initializer girdisi `field_name = expression` biçimindedir; alanlar virgülle ayrılır. Alan sırası Struct declaration sırasından bağımsızdır.
3. Her alan adı, Struct'ın bildirilen alanlarından tam birine çözülür. Bilinmeyen alan hata; aynı alanın ikinci kez yazılması duplicate-field hatasıdır.
4. Struct alanlarının tamamı literal içinde tam bir kez başlatılmalıdır. Eksik alan için default değer varsayılmaz; ileride default-field özelliği ayrıca tanımlanabilir.
5. Her expression'ın türü ilgili field türüne Semantic conversion kurallarıyla uyumlu olmalıdır. Hard-lock (`!`) türler implicit dönüşümle gevşetilmez.
6. AST, literal Type yolunu ve ordered field initializer listesini kayıpsız korur. SemanticResult'te `AST_STRUCT_LITERAL` annotation'ı Struct nominal `resolved_symbol_id` ve `resolved_type_id` değerlerini taşır. Her `AST_STRUCT_FIELD_INIT` annotation'ı ilgili field'ın `resolved_symbol_id` değerini ve initializer expression'ın `resolved_type_id` değerini taşır; field initializer'ın `value` child bağlantısı AST üzerinde korunur. Böylece yeni bir SemanticResult tablosu gerekmeden mevcut annotation sözleşmesi yeterlidir. Kaynak isimler diagnostic ve AST fidelity içindir; OIR çözümlemesi isim tahmini yapmaz.
7. OIR, instance değerini tek bir nominal Struct type ID ile temsil eder ve her field initializer'ı resolved field ID üzerinden instance initialization işlemine bağlar. Field evaluation sırası kaynak initializer sırasıdır; depolama/layout sırası Struct tanımına aittir.
8. Literal başarısızsa kısmi/valid instance değeri üretilmez. Diagnostic'ler aynı derleme biriminde toplanır; hatalı SEMANTIC_AST OIR lowering'e verilmez.

### AST ve uygulama kapısı

Bu özellik için append-only AST node ID ile ayrı Struct literal ve Struct field initializer node'ları gereklidir; mevcut `AST_ARRAY_LITERAL` veya `AST_MEMBER_ACCESS` yeniden amaçlandırılmaz. Node payload, ownership, codec field schema, parser grammar, SemanticResult cross-reference ve OIR instance-init opcode/record sözleşmesi birlikte tamamlanmalıdır.

**Uygulama durumu:** Bu bölüm syntax ve semantic/OIR kontratını normatif olarak sabitler; Parser/AST/codec/Semantic/OIR/test implementation henüz tamamlanmış değildir. Bu nedenle Struct literal kullanılabilir bir özellik olarak ilan edilmez.

---

## 5. Kapsam (Scope)

Group üyelerinin erişimi, Group namespace üyeliği ve `pup` / `exp` görünürlük kurallarıyla belirlenir. Group içindeki her üye otomatik olarak dışarıya kapalı değildir; public API üyeleri Group yolu üzerinden çözümlenebilir.

- Group içinde tanımlanan `struct`, `enum` ve `union` türleri özel Group türleridir. Yalnızca tanımlandıkları Group'un kendi fonksiyon/metotları tarafından kullanılabilir; dış fonksiyonlar ve diğer Group'lar bu türleri doğrudan kullanamaz.
- Group dışında tanımlanan türler, normal lexical scope ve modül görünürlüğü kuralları çerçevesinde Group içinde kullanılabilir.
- Group-owned türlerin izolasyonu `pup` / `exp` ile kaldırılamaz. Public/export görünürlüğü, özel türün sahibini değiştirmez ve türü Group dışına açmaz.
- Nested Group bağımsız bir Group scope'tur; üst Group'un özel türlerini devralmaz. Her alt Group kendi Struct/Enum/Union türlerini tanımlayabilir ve bunlar yalnızca kendi Group'unda görünür.
- Namespace fonksiyonları `Group.member` / `Group.SubGroup.member` yoluyla; Struct ile eşleştirilmiş instance metotları ise receiver kuralına göre `instance.method(args)` biçiminde çözümlenir.


---

## 6. Kullanıcı Tanımlı ve Modüler Yapı

Group metodunun en büyük avantajı, **modüler ve bağımsız** işlevsellik sunmasıdır. Bu yapı sayesinde **kütüphaneler**, **modüller** ve **servisler** birbirinden izole şekilde organize edilebilir.

Örneğin, dosya işlemleri, veritabanı bağlantıları veya ağ servisleri gibi gruplar tek bir çatı altında toplanabilir ve kendi içlerinde alt gruplar kullanılarak işlevler düzenlenebilir.

---

## 7. Özet

* **Group Metodu**, fonksiyonları gruplar halinde organize etmek ve işlevselliği modüler bir şekilde sunmak amacıyla geliştirilmiştir.
* **Alt gruplar**, daha detaylı ve hiyerarşik yapıların oluşturulmasına olanak tanır.
* **Nesne Yönelimli Programlama (OOP)** felsefesiyle, **struct** yapılarıyla ilişkilendirilip metodlar tanımlanabilir.
* Kullanıcılar, **grup içindeki metodları** belirli bir grup adı ile çağırarak kolayca erişebilir.

---


## 8. Kütüphane ve Modül Modeli (Normatif Tasarım)

Group, yalnızca instance metotlarını taşıyan bir nesne kabı değildir. Nexus Flow kütüphane API'sinin isim alanı ve üyelik organizasyonunun da temel yapısıdır. İki kullanım biçimi ayrılır:

- **Namespace Group:** Kütüphanenin fonksiyonlarını, sabitlerini, türlerini ve alt gruplarını isim alanında toplar. Instance oluşturulması gerekmez.
- **Object Group:** Group bildirimi bir Struct ile eşleştirilerek instance alanları üzerinde çalışan metotlar sağlar. Namespace ve instance davranışları birlikte bulunabilir.

### 8.1 Kütüphane yüzeyi ve üyeler

| Üye | Kütüphane API rolü |
|---|---|
| Function / Method | İşlev ve metot dışa aktarımı |
| Nested Group | Alt namespace / alt API |
| Struct / Enum / Union | Kullanıcıya sunulan veri türleri |
| Variable / Const | Kütüphane durumu veya sabitleri |
| Default | Group çağrısı/namespace seçimi için varsayılan davranış |

Aynı isimli API üyelerinin aynı Group içinde çakışması Semantic aşamasında hata olmalıdır; Parser yalnızca yapıyı üretir.

### 8.2 Public / Export görünürlüğü

- **Private:** Group içinde görünür; dış tüketici API'sine dahil değildir.
- **Public (pup):** Kaynak/modül sınırının dışından erişilebilir API üyesi.
- **Export (exp):** Derleme/kütüphane çıktısında dışa sunulacak sembol; export, public erişimden ayrı bir çıktı/linker kararıdır.

Public olmak otomatik olarak export edilmek anlamına gelmez. Export edilen sembolün API görünürlüğü ve ABI kararı Semantic/Module çözümleme aşamasında doğrulanmalıdır. Mevcut pup/exp anlamları genişletilmeden, Group üyelerinin görünürlüğü AST ve Semantic katmanlarında temsil edilmelidir.

### 8.3 m: bağımlılık bildirimi ile use erişimi

İki mekanizma farklı sorumluluk taşır:

- **m:Name = "source";** zorunlu bağımlılığı bildirir. Kaynak çözümlenemez/yüklenemezse derleme başarısız olur.
- **use Name; / use path.segment;** modül adını/üyelerini mevcut kaynak kapsamına açma talebidir. Parser dotted path'i AST_USE içinde tek modül yolu olarak korur; gerçek çözümleme ve yükleme Semantic/Module Resolver sorumluluğudur.

Bunlar birbirinin yerine kullanılmaz. use ifadesi kendi başına dosya yolu veya paket indirme garantisi değildir; dependency resolver'ın kaynak politikası uygulanır.

### 8.4 Namespace çözümleme ve erişim

- **Library.member:** kütüphane/Group üyesi çözümlemesi.
- **Library.SubGroup.member:** iç içe namespace çözümlemesi.
- **Group.member(args):** Group namespace üzerinden statik üye çağrısı; doğrudan Group function veya statik method olabilir.
- **Type.method(args):** Group'a bağlanmış nominal tür üzerinden statik/constructor çağrısı; implicit receiver taşımaz.
- **instance.method(args):** yalnızca Group'a bağlanmış Struct instance'ında geçerli instance çağrısı; ilk self parametresi otomatik receiver alır.
- **Group / Group.SubGroup:** ilgili scope'ta default üye tanımlıysa default davranış çağrısı/çözümlemesi.

Member access AST'de nokta zincirinin parse edilmiş olması yeterli değildir. Semantic; sembolün Group/Struct/Module türünü, üyelik ilişkisini, visibility ve çağrı biçimini doğrulamalıdır. Çözümleme başarılı olduğunda Semantic AST annotation'ı resolved symbol/member/receiver ilişkisini taşımalıdır; OIR ve Codegen isim metninden tahmin yürütmemelidir.

### 8.6 Group–Nominal Type Binding ve Method Dispatch (Normative)

Group adı ile aynı ada sahip, Group'un doğrudan üyesi olan nominal tür varsa bu tür Group'un ana nominal binding adayıdır. Eşleşme yalnızca tam ve case-sensitive isim eşitliğiyle yapılır; benzer isim, nested Group üyesi veya dış kapsam türü binding oluşturmaz.

- Aynı ada sahip doğrudan `struct`, `enum` veya `union` bildirimi Group ile ilişkilendirilebilir. Bir Group için en fazla bir ana nominal binding bulunabilir; aynı isimli birden fazla nominal bildirim zaten tür adı çakışmasıdır ve Semantic hatasıdır.
- Eşleşen tür yoksa Group Namespace Group olarak kalır; bu geçerli bir durumdur ve otomatik Struct üretimi yapılmaz.
- Eşleşen `struct`, Object Group davranışını sağlayabilir. Bu Group'un instance metotları `self` receiver sözleşmesine tabidir.
- Eşleşen `enum` veya `union`, Group ile nominal/statik API ilişkisi kurar; tek başına instance receiver veya `self` semantiği oluşturmaz. Enum/Union instance davranışı ayrıca tanımlanmadan varsayılmaz.
- Group içindeki başka isimli Struct/Enum/Union türleri Group-owned özel türlerdir; ana binding olmasalar da §9 kapsam izolasyonuna tabidir.
- Nested Group kendi adına göre bağımsız binding arar. Üst Group'un ana nominal binding'ini devralmaz.

#### Method isimleri ve receiver

- `public_name => f:implementation_name(...) { ... }` biçiminde `public_name`, Group API yüzeyindeki çağrı adıdır; implementation function adı iç sembol/uygulama kimliğidir.
- Group içinde doğrudan tanımlanan `f:name(...)` / extern function, kendi adıyla erişilen statik Group callable üyesidir; implicit receiver almaz. Buradaki `self` parametresi varsa normal açık parametredir.
- Bir `AST_METHOD` alias implementation'ı `f:implementation_name(...)` biçiminde isimli olabilir. İsim atlanarak `f:(...)` yazılırsa Semantic, implementation symbol kimliği için public alias adını kullanır; iki biçim de aynı Group method sözleşmesine tabidir.
- Semantic, Group içindeki public method/function API adlarının çakışmasını kontrol etmeli ve çağrıları implementation adından değil çözümlenmiş Group üye sembolünden bağlamalıdır.
- Object Group instance metotlarında ilk parametre `self` olmalı ve türü Group'a bağlanmış Struct ile uyumlu olmalıdır. `instance.method(args)` çağrısında receiver bu parametreye otomatik bağlanır; kullanıcı ayrıca `self` argümanı geçmez.
- `Group.method(args)` ve `Type.method(args)` statik çağrıdır; seçilen method ilk parametre `self` bekliyorsa bu çağrı biçimi reddedilir. Benzer şekilde instance üzerinden statik method veya doğrudan Group function çağrısı reddedilir.
- Instance dispatch yalnızca Group'un ana binding'i olan Struct türünde geçerlidir. Aynı Group'a ait başka bir Struct, Group method receiver'ı olarak kullanılamaz.
- Method ve doğrudan Group function imzaları Group body denetiminden önce predeclare edilir; böylece bir method daha sonra tanımlanmış başka bir Group callable'ı çağırabilir.
- `default` normal method implementation'ı değildir; Group'a ait ayrı default handler olarak temsil edilir ve aynı Group içinde en fazla bir kez tanımlanabilir.

Group-qualified çağrının implementation sembolü Group child scope içinde bulunabilir. Bu sembole dış kapsamdan erişim yalnızca Group sembolü mevcut kapsamda görünür olduğunda ve GroupMember / GroupMethodImplementation ilişkisi hedef sembolü açıkça sunduğunda geçerlidir. Bu istisna normal lexical lookup veya implementation adının dışarı sızması anlamına gelmez. SemanticResult ve OIR Verifier aynı kuralı doğrulamalıdır.

Bu kuralların Semantic sonucu, Group kimliği, bağlı nominal tür kimliği, Group üye sembolleri, public method adı, resolved implementation symbol ve receiver ilişkisini taşımalıdır. OIR, bu çözülmüş kimlikleri korumalıdır; Codegen isim metninden ilişki çıkarmamalıdır.

**Uygulama notu (2026-11-08):** SemanticResult'a Group symbol ve Group relation türleri eklendi; Semantic checker Group Symbol, doğrudan üyelik, nominal binding, public method implementation ve default handler kayıtlarını üretir. OIR Semantic AST schema 1.1 bu kayıtları taşır; reader 1.0 legacy artifact'lerini korurken 1.0 içinde yeni enum değerlerini reddeder. İlk self receiver katmanı, Group'un eşleşen nominal bildirimini method body'lerinden önce çözerek yalnızca Struct binding için receiver inference/validation yapar. Group callable dispatch katmanı; AST_METHOD public alias → implementation symbol çözümlemesini, doğrudan Group function'larının statik çağrısını, Group/Type statik çağrılarını ve Struct instance çağrılarında implicit self argümanını destekler. İsimli implementation (`f:implementation(...)`) ve alias adını internal symbol olarak kullanan anonim implementation (`f:(...)`) biçimleri desteklenir. Static factory çağrılarında dönüş türü, bağlı Struct türü olarak call expression'a aktarılır. Group callable imzaları predeclare edilir; implementation sembolleri Group scope dışına lexical ad olarak sızdırılmadan Group-qualified çağrılar için yaşatılır. SemanticResult ve OIR Verifier, görünür Group sembolü ile açık GroupMember/GroupMethodImplementation ilişkisi varsa Group child scope içindeki hedef sembole qualified referansı kabul eder. SemanticResult/OIR integration testi hem instance call target'ını hem Group scope dışından statik call target'ını ve alias relation'ını doğrular. Linux, macOS ve Windows CTest 60/60 geçti; ASan/UBSan ve Valgrind de başarılıdır (GitHub Actions run 42426973498). Struct literal construction/field initialization, nested Group dotted-path/Module Resolver, visibility/export politikası ve default handler dispatch henüz tamamlanmadığından §8.6 ve Group modülü tamamlanmış sayılmaz.
### 8.5 Module Resolver ve Library ABI sınırı

Kütüphane desteği, Group AST'sini doğrudan runtime nesnesine çevirmek değildir. Modüler sorumluluklar:

1. **Parser/AST:** Group, nested Group, member ve use / m: syntax ağacını kayıpsız üretir.
2. **Semantic:** Group scope, üye isim çakışması, tür/Struct eşleşmesi, self, constructor, default ve visibility kontrolü.
3. **Module Resolver:** Dependency kaynağını bulur, modül kimliğini ve import kapsamını oluşturur; dependency graph/cycle ve sürüm/ABI metadata kontrolünü yürütür.
4. **OIR:** Çözülmüş sembol kimliklerini ve çağrı hedeflerini taşır; kaynak modül adını runtime'da tekrar aramaz.
5. **Codegen/Linker:** Export edilen sembolleri hedef ABI'ye göre üretir/bağlar. Linker'ın ayrı modül olması kararı korunur.

**Uygulama durumu:** Bu bölüm tasarım kontratını tanımlar; Semantic/Module Resolver/ABI tamamlandı iddiası değildir. Kütüphane ve Group semantic testleri kapanmadan Group tamamlanmış sayılmaz.
## 9. Group-Owned Type Scope (Normative)

Bu bölüm, Group kapsamındaki nominal tür görünürlüğünü kesinleştirir ve Semantic uygulaması için normatif kuraldır.

### 9.1 Group içinde tanımlanan türler

Bir Group'un doğrudan üyeleri arasında bildirilen her `struct`, `enum` veya `union`, bildirildiği Group'a ait özel bir tür sembolüdür.

- Türün sahibi, bildirimin bulunduğu en yakın Group kapsamıdır.
- Aynı Group'a ait fonksiyonlar ve metotlar bu türü kendi imzasında, parametrelerinde, dönüş tipinde, yerel değişkenlerinde ve tür ifadelerinde kullanabilir.
- Group dışındaki fonksiyonlar/metotlar bu türü kullanamaz.
- Başka bir Group, kardeş Group veya iç içe Group bu türe doğrudan erişemez.
- Türün adının başka bir kapsamda aynı olması, sembolleri aynı tür yapmaz; tür kimliği ve sahibi ayrı tutulur.
- Group dışı görünürlük `pup`, `exp` veya başka bir görünürlük bayrağıyla açılamaz.

Bu kural Struct, Enum ve Union için aynıdır; yalnızca Struct'a özgü değildir.

### 9.2 Group dışında tanımlanan türler

Bir `struct`, `enum` veya `union` herhangi bir Group'un sahibi değilse, normal lexical scope ve modül görünürlüğü kurallarına tabidir. Bu türler, kapsamları izin verdiği sürece Group içindeki fonksiyon/metotlar tarafından kullanılabilir.

### 9.3 Nested Group izolasyonu

Her nested Group kendi Group scope kimliğine sahiptir.

- Alt Group kendi özel türlerini tanımlayabilir.
- Alt Group, üst Group'un özel türlerini otomatik olarak devralmaz.
- Üst Group, alt Group'un özel türlerini kullanamaz.
- Kardeş Group'lar birbirlerinin özel türlerini kullanamaz.
- Alt Group'un namespace üyelerine erişim, Group türlerinin özel kapsamını genişletmez.

### 9.4 Semantic uygulama yükümlülükleri

Semantic katmanı:

1. Group-owned tür sembolünün sahibi olan Group scope kimliğini saklamalıdır.
2. Tür adı çözümlemesinde yalnızca ad eşleşmesini yeterli saymamalı; mevcut aktif Group kimliği ile türün sahibi olan Group kimliğini doğrulamalıdır.
3. Fonksiyon parametre/dönüş tipleri, değişken bildirimleri, alan tipleri, enum payload tipleri, tür dönüşümleri ve diğer tüm tür referanslarında aynı erişim denetimini uygulamalıdır.
4. Group scope kapandığında o Group'a ait sembolleri aktif sembol çözümlemesinden çıkarmalıdır.
5. Aynı isimli türlerin farklı Group'larda çakışmadan ayrı sembol kimlikleriyle temsil edilmesini sağlamalıdır.
6. Semantic sonucu/OIR için türün gerçek sembol kimliğini korumalı; isim metninden yeniden çözümleme yapılmasına güvenmemelidir.

Parser bu erişim kararını vermez; AST'de bildirimin ve referansın yapısını kayıpsız tutar. Erişim doğrulaması Semantic sorumluluğudur.

### 9.5 Zorunlu test matrisi

| Senaryo | Beklenen sonuç |
|---|---|
| Group içindeki fonksiyon kendi Struct türünü kullanır | Geçer |
| Group içindeki metot kendi Enum türünü kullanır | Geçer |
| Group içindeki fonksiyon kendi Union türünü kullanır | Geçer |
| Dış fonksiyon Group Struct türünü kullanır | Hata |
| Dış fonksiyon Group Enum türünü kullanır | Hata |
| Dış fonksiyon Group Union türünü kullanır | Hata |
| Başka bir Group ilk Group'un özel türünü kullanır | Hata |
| Nested Group üst Group'un özel türünü kullanır | Hata |
| Üst Group nested Group'un özel türünü kullanır | Hata |
| Kardeş Group diğerinin özel türünü kullanır | Hata |
| Nested Group kendi Struct/Enum/Union türünü kullanır | Geçer |
| Group dışında tanımlı tür, görünür olduğu Group içinde kullanılır | Geçer |
| Aynı isimli tür iki farklı Group'ta tanımlanır | Ayrı semboller olarak geçer |
| Group kapandıktan sonra özel tür sembolü çözümlemede bulunur | Hata |

### 9.6 Tamamlanma koşulu

Bu kural ancak Struct, Enum ve Union için yukarıdaki erişim testleri Semantic testlerinde doğrulandığında uygulanmış sayılır. Belgenin güncellenmesi tek başına uygulama veya test tamamlanması anlamına gelmez.

## 10. SemanticResult ve OIR kayıt sözleşmesi (Normative)

Bu bölüm Group semantiğinin kalıcı SemanticResult ve OIR aktarımında nasıl temsil edileceğini tanımlar. Kayıtlar mevcut Symbol ve Relation tablolarıyla taşınır; Group için paralel ve birbirinden kopuk bir üye tablosu oluşturulmaz.

### 10.1 Sembol kimlikleri

- Her `AST_GROUP_DECL`, kendi AST düğüm kimliğine bağlı bir **Group Symbol** üretir.
- Group sembolü, Group'un lexical scope kaydıyla ilişkilidir. Nested Group ayrı sembol ve ayrı scope alır.
- Group içindeki Struct/Enum/Union sembolleri kendi gerçek nominal sembol kimliklerini korur.
- Function implementation sembolü, AST_FUNC_DECL / AST_EXTERN_DECL bildiriminin gerçek AST node kimliğini taşır. Public method adı implementation sembolünün adını değiştirmez.

### 10.2 Relation türleri

SemanticResult ve OIR relation enum'larına append-only yeni türler eklenir:

| Relation | Owner | Related node | Related symbol | Name |
|---|---|---|---|---|
| GROUP_MEMBER | Group Symbol | Doğrudan üye AST düğümü | Varsa aynı bildirimin sembolü; wrapper üyelerde 0 | Üyenin kaynak/API adı |
| GROUP_TYPE_BINDING | Group Symbol | Bağlanan Struct/Enum/Union bildirimi | Nominal tür sembolü | Nominal tür adı |
| GROUP_METHOD_IMPLEMENTATION | Group Symbol | Implementation AST_FUNC_DECL / AST_EXTERN_DECL | Implementation function sembolü | Public method adı |
| GROUP_DEFAULT_HANDLER | Group Symbol | `AST_METHOD("default")` düğümü | 0 | `default` |

- Relation ordinal değerleri her owner + relation türü grubu içinde sıfırdan başlayarak kesintisiz artar.
- GROUP_TYPE_BINDING en fazla bir kayıt olabilir.
- GROUP_DEFAULT_HANDLER en fazla bir kayıt olabilir.
- GROUP_METHOD_IMPLEMENTATION kayıtları public method isimlerini ve gerçek implementation sembollerini birbirine bağlar.
- GROUP_MEMBER, Group'un doğrudan AST üyelik listesini korur. Nested Group'un kendi üyeleri üst Group'un doğrudan üyeleri sayılmaz.
- Relation içindeki `related_symbol_id` sıfır değilse, sembolün `declaration_node_id` değeri relation'ın `related_node_id` değeriyle aynı olmalıdır. Wrapper düğüm ile implementation düğümü bu nedenle ayrı relation kayıtlarıyla temsil edilir.

### 10.3 OIR doğrulama kuralları

OIR Verifier en az şu kontrolleri yapmalıdır:

1. Group Symbol yalnızca `AST_GROUP_DECL` bildirimiyle eşleşir.
2. GROUP_MEMBER relation owner'ı Group Symbol olmalı; related node yalnızca geçerli doğrudan Group üye bildirimlerinden biri olmalıdır.
3. GROUP_TYPE_BINDING relation owner'ı Group Symbol olmalı; related node Struct, Enum veya Union bildirimi olmalı ve related symbol aynı bildirimin nominal sembolü olmalıdır.
4. GROUP_METHOD_IMPLEMENTATION relation owner'ı Group Symbol olmalı; related node function/extern implementation bildirimi olmalı ve related symbol aynı function sembolünü göstermelidir.
5. GROUP_DEFAULT_HANDLER relation owner'ı Group Symbol olmalı; related node `AST_METHOD` olmalı ve name tam olarak `default` olmalıdır.
6. Aynı Group için birden fazla type binding veya default handler kaydı reddedilmelidir.
7. Group ve relation kimlikleri yalnızca OIR'deki sayısal ID'lerle doğrulanır; isim metni üzerinden yeniden çözümleme yapılmaz.

### 10.4 Şema sürümü ve geriye uyumluluk

Semantic AST OIR şu anda schema 1.0 kullanır ve artifact reader schema minor değerini tam eşitlikle denetler. Yeni enum değerleri eski 1.0 tüketicileri tarafından tanınmayacağından yalnızca enum ekleyip 1.0 olarak yayımlamak geçerli değildir.

- Group semantic kayıtlarını içeren ilk şema sürümü **1.1** olacaktır.
- Yeni reader hem 1.0 hem 1.1 artifact'lerini okuyabilmelidir.
- Schema 1.0 artifact içinde yeni Group symbol/relation enum değerleri bulunursa artifact reddedilmelidir.
- Schema 1.1 artifact'lerinde yeni kayıt türleri ve bunların AST referansları tam doğrulanmalıdır.
- Record boyutları ve mevcut alan offset'leri değişmediği sürece yeni enum değerleri aynı section record layout'unda taşınır.
- Eski reader'ların 1.1 artifact'lerini desteklemesi beklenmez; producer/consumer schema capability eşleştirmesi entegrasyon aşamasında yapılmalıdır.

**Uygulama durumu:** Bu bölüm normatif veri aktarım sözleşmesidir. SemanticResult enum/validator, OIR codec, artifact reader, Verifier ve testler uygulanıp CI doğrulanmadan Group SemanticResult/OIR desteği tamamlanmış sayılmaz.

