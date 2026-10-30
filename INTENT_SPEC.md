# Intent System Specification v0.1

**Durum:** Tasarım temeli / sözdizimi ve uygulama ayrıntıları açık  
**Dil:** Nexus Flow  
**İlgili işaret:** `@`  
**İlgili belge:** `omnicore-sdk/docs/syntax.md`  
**Son güncelleme:** 2026-10-30

## 1. Amaç

Nexus Flow içindeki `@` işareti, programcının bir kod parçasına ilişkin **amacını veya ulaşmak istediği sonucu** bildirmek için ayrılmıştır. Bu bildirimler, gelecekte derleyiciye entegre edilebilecek isteğe bağlı yapay zekâ (AI) yardımcısına bağlam sağlayacaktır.

Intent, derleyiciye doğrudan makine kodu üretme emri vermez. Bir hedefi, beklentiyi veya tercihi ifade eder; bu hedefe nasıl ulaşılacağını değerlendirmek derleyicinin ve gelecekteki AI yardımcısının görevidir.

Bu belge yalnızca karara bağlanmış tasarım temelini tanımlar. Nihai yazım biçimini, kapsam kurallarını veya intent kategorilerinin tamamını henüz belirlemez.

## 2. `!!=` ile görev ayrımı

Nexus Flow'da `!!=` zaten daha katı sözdizimine sahip derleyici talimatları için kullanılmaktadır. `@` bu alanı tekrar etmemelidir.

| Özellik | `!!=` Directive | `@` Intent |
| --- | --- | --- |
| Temel rol | Derleyiciye açık talimat vermek | Amaç/hedef bildirmek |
| İfade biçimi | Kuralı veya uygulanacak talimatı belirtir | İstenen sonucu veya programcı amacını açıklar |
| Bağlayıcılık | Tanımlı directive semantiğine göre bağlayıcıdır | Tek başına bağlayıcı bir yürütme talimatı değildir |
| AI bağımlılığı | AI olmadan da uygulanabilir | AI olmasa da kaynakta ve ara temsilde korunabilir |
| Uygulama | Derleyicinin ilgili directive mekanizması | Gelecekte AI/analiz bileşeni tarafından yorumlanabilir |

Bir intent, açık bir `!!=` talimatını geçersiz kılamaz, gevşetemez veya gizlice değiştiremez. Intent ile directive arasında olası bir çelişki tespit edilirse sistem bunu sessizce çözümlemiş gibi davranmamalıdır. Çelişkinin tanısı ve kesin hata/uyarı politikası, directive ve diagnostic sözleşmeleri netleştiğinde tanımlanacaktır.

## 3. Temel ilkeler

1. **Amaç bildirimi:** `@`, programcının neyi amaçladığını veya hangi sonuca öncelik verdiğini ifade eder.
2. **Talimat değildir:** Intent tek başına bir komut, zorunlu optimizasyon, runtime işlemi veya doğruluk garantisi değildir.
3. **Semantik korunumu:** Intent bildirimi, programın Nexus Flow dilinde tanımlanmış gözlemlenebilir davranışını değiştiremez.
4. **AI isteğe bağlıdır:** AI bileşeni bulunmasa, kapalı olsa veya intent'i anlamasa da program normal derleme yoluyla işlenebilmelidir.
5. **Açık uygulama:** Derleyici, yalnızca intent bulunduğu için gizli ağ erişimi, dış servis çağrısı veya otomatik kod değişikliği yapamaz.
6. **Korunan metadata:** Intent bilgisi lexer/parser sonrasında kaybolmamalı; AST ve kanonik OIR tasarımında temsil edilebilecek şekilde korunmalıdır.
7. **Genişletilebilirlik:** Gelecekte yeni intent türleri eklenebilmelidir. v0.1 aşamasında kapalı ve eksiksiz bir kategori listesi tanımlanmaz.
8. **Doğrulama:** AI tarafından önerilen her kod veya optimizasyon değişikliği, normal tip, semantik, target/backend ve güvenlik kontrollerinden geçmeden uygulanamaz.

## 4. Amaç ve strateji ayrımı

Intent öncelikle **ne istendiğini** ifade etmelidir; **nasıl yapılacağını** zorunlu olarak tarif etmemelidir.

Örneğin programcı gelecekte performansın artırılmasını, bellek kullanımının azaltılmasını veya gecikmenin düşürülmesini hedef olarak bildirebilir. Bunlar yalnızca olası hedef örnekleridir; v0.1 için kesinleşmiş anahtar kelimeler veya sözdizimi değildir.

Bu hedeflere ulaşmak için kullanılacak algoritma, bellek yerleşimi, paralellik veya komut seçimi gibi stratejiler ayrı değerlendirme katmanının konusudur. İleride intent bildirimi hedefin yanında kısıt veya tercih de taşıyabilir; bu yapı henüz karara bağlanmamıştır.

## 5. Derleyici ve gelecekteki AI bileşeni

### 5.1 AI bulunmadığında

- Derleyici intent metadata'sını ayrıştırıp koruyabilmelidir.
- Intent'in varlığı tek başına derlemeyi başarısız kılmamalıdır.
- AI yardımı olmadan yapılan derleme, aynı kaynak ve target ayarları için normal derleyici kuralları içinde deterministik kalmalıdır.
- Intent'in karşılanacağına dair garanti verilmemelidir.

### 5.2 AI etkin olduğunda

- AI, intent'i bağlam olarak yorumlayıp analiz veya optimizasyon önerileri sunabilir.
- Öneri ile uygulama birbirinden ayrılmalıdır.
- AI'nın önerisi, programcının açık onayı veya ileride tanımlanacak açık bir otomasyon politikası olmadan kaynak kodu sessizce değiştirmemelidir.
- Öneri uygulansa bile ortaya çıkan program tüm dil semantiği ve target/backend doğrulamalarını geçmelidir.
- AI'nın desteklemediği veya güvenilir biçimde yorumlayamadığı intent açıkça raporlanmalıdır; rastgele bir anlama dönüştürülmemelidir.

### 5.3 Yetki sınırı

AI yardımcısı dilin normatif semantiğinin kaynağı değildir. Dil standardı ve derleyici doğrulamaları her zaman AI önerilerinden üstündür. AI, geçersiz programı geçerli ilan edemez ve desteklenmeyen target özelliğini varmış gibi kullanamaz.

## 6. Temsil ve derleyici aşamaları

Gelecekteki uygulama için beklenen kavramsal akış:

1. **Lexing/parsing:** Intent bildiriminin dil yapısı olarak tanınması.
2. **AST:** Intent'in sıradan değişken veya tanımlama öneklerinden ayrı bir düğüm/metadata olarak tutulması.
3. **Semantic analysis:** Intent'in bağlamı ve geçerliliği için gerekli kontrollerin yapılması; programın temel semantiğinin intent'ten türetilmemesi.
4. **OIR:** Intent metadata'sının kayıpsız ve sürümlenebilir biçimde taşınabilmesi.
5. **Optional advisor:** AI/analiz bileşeninin metadata'yı okuyarak öneri üretmesi.
6. **Validation:** Önerilerin mevcut derleyici doğrulama aşamalarından geçirilmesi.

Bu akış mimari hedeftir; mevcut C parser veya OIR'ın bu aşamaları şimdiden desteklediği anlamına gelmez. AST düğüm yapısı ve OIR ikili gösterimi ayrıca tasarlanıp test edilmelidir.

## 7. Bu sürümde açık bırakılan konular

Aşağıdakiler v0.1 kapsamında bilerek kesinleştirilmemiştir:

- `@` için nihai gramer ve örnek kullanım biçimi.
- Intent'in ifade, blok, fonksiyon, modül veya proje düzeyindeki kapsamı.
- Birden fazla intent'in birleşme ve çelişme kuralları.
- Standart intent kategorileri ve bunların parametre şemaları.
- Hedef, kısıt ve tercihlerin tek bildirimde mi yoksa ayrı alanlarda mı ifade edileceği.
- Intent metadata'sının OIR içindeki kesin binary/serialization biçimi.
- AI yardımcısının arayüzü, modeli, çalışma zamanı ve kullanıcı onay akışı.
- Tanınmayan veya desteklenmeyen intent'ler için kesin diagnostic seviyeleri.

Bu kararlar verilmeden parser'a varsayımsal bir `@` grameri eklenmemelidir.

## 8. v0.1 kabul ölçütleri

- [x] `@` işaretinin AI destekli amaç/niyet bildirimi için ayrılması.
- [x] `!!=` directive mekanizmasından açık görev ayrımı.
- [x] Intent'in tek başına yürütme talimatı veya semantik değişiklik olmaması.
- [x] AI olmadan derlemenin mümkün olması.
- [x] Intent metadata'sının AST/OIR boyunca korunmasının mimari gereksinim olarak belirlenmesi.
- [x] AI önerilerinin doğrulama sınırlarının tanımlanması.
- [ ] Nihai gramerin ve kapsamın belirlenmesi.
- [ ] Intent metadata AST/OIR sözleşmesinin tasarlanması.
- [ ] C frontend için parser ve testlerin uygulanması.

## 9. İlgili belgeler

- [Nexus Flow syntax referansı](omnicore-sdk/docs/syntax.md)
- [Mimari kararlar](ARCHITECTURE_DECISIONS.md)
- [Nexus Flow spec inceleme raporu](SPEC_AUDIT.md)
- [Kanonik OIR kararları](ARCHITECTURE_DECISIONS.md#adr-002--tek-ve-açık-ara-temsil-sözleşmesi)
