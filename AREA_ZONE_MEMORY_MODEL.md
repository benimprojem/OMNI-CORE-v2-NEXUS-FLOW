# AREA / ZONE — Mantıksal Bellek Alanları Tasarım Notu

**Statü:** Tasarım kararı adayı; uygulama sözleşmesi ve sözdizimi henüz tamamlanmadı  
**Kapsam:** NEXSUS Flow, OCC semantic modeli, hedef/OS bellek eşleme katmanı

## 1. Amaç

`area` ve `zone`, NEXSUS Flow'un işletim sisteminden bağımsız bellek alanı soyutlamalarıdır. Bunlar yalnızca bir OS API çağrısının takma adı değildir. Bare-metal, RTOS ve genel amaçlı OS hedeflerinde aynı dil düzeyindeki anlamı korumalı; hedefe özgü fiziksel yerleşim daha sonra runtime, OS adapter veya backend tarafından çözülmelidir.

## 2. Mantıksal kapsam

- **area:** Yakın/yerel erişim ve çekirdek yerelliği öncelikli bellek alanı sınıfı.
- **zone:** Daha geniş paylaşım alanı ve birden fazla yürütme biriminin erişebildiği bellek alanı sınıfı.

Bu tanım mantıksaldır. `area = L1/L2` ve `zone = L3/L4` bire bir donanım eşlemesi değildir; tasarım niyetini anlatan yaklaşık sınıflandırmadır.

## 3. Önbellek topolojisiyle ilişki

İşlemcilerde L1/L2 yerleşimi mimariye göre değişebilir:

- çekirdek başına özel L1 ve/veya L2,
- aynı çekirdek grubundaki çekirdeklerce paylaşılan L2,
- farklı çekirdek kümeleri arasında farklı paylaşım düzeyleri.

L3/L4 ise çoğu tasarımda daha geniş paylaşım alanına sahiptir; ancak bu da evrensel bir garanti değildir. Bu nedenle dil semantiği belirli cache seviyelerine veya belirli bir çekirdek/grup topolojisine bağlanmamalıdır.

Hedef tanımı, mantıksal alanı gerçek kaynaklara eşler. Örneğin bir hedefte `area` çekirdek-özel SRAM/L1'e yakın bir havuza; başka bir hedefte çekirdek grubu tarafından paylaşılan L2 benzeri bir havuza eşlenebilir. `zone` ise hedefin ortak cache/DRAM/NUMA benzeri paylaşım alanına eşlenebilir.

## 4. OS bağımsızlığı ve OS entegrasyonu

Aynı dil kontratı iki çalışma biçimini desteklemelidir:

1. **OS bağımsız:** Bare-metal veya özel runtime, alanları doğrudan fiziksel/NUMA/SRAM havuzlarına eşler.
2. **OS üzerinde:** Runtime veya OS adapter, uygun sanal bellek, allocator, NUMA policy ya da platform API'si üzerinden karşılık üretir.

OS desteği, `area`/`zone` anlamını değiştirmemeli; yalnızca kaynak ayırma ve eşleme mekanizmasını değiştirmelidir. Hedef kaynak sunamıyorsa davranış açıkça tanımlanmalıdır: kesin talepte hata, tercih talebinde belgelenmiş fallback veya derleme/çalışma zamanı tanısı. Sessiz ve belirsiz eşleme yapılmamalıdır.

## 5. Gelecekteki yerleşim politikası

Daha sonra ayırmanın nereden yapılacağını seçen bir politika eklenebilir. Olası boyutlar:

- tercih edilen mantıksal alan: area / zone / otomatik,
- kapsam: thread / core / core-group / device / shared,
- paylaşım: private / group-shared / global-shared,
- yerellik: preferred / required,
- fallback: izinli / yasak,
- hizalama ve minimum garanti.

Bunlar şimdilik kesin NEXSUS Flow sözdizimi değildir. Önce mevcut `area`/`zone` işlemlerinin AST ve semantic anlamı, kaynak sahipliği, yaşam süresi ve hata davranışı netleştirilecektir. Ardından bir dil sözdizimi mi, hedef konfigürasyonu mu, yoksa ikisinin birleşimi mi olacağı kararlaştırılacaktır.

## 6. Sahiplik, yaşam süresi ve eşzamanlılık — açık sözleşmeler

Alan seçimi tek başına sahiplik veya senkronizasyon sağlamaz. Aşağıdakiler ayrı ayrı tanımlanmalıdır:

- ayrılan belleğin sahibi ve sahiplik transferi,
- alanın ve tahsis edilen nesnenin yaşam süresi,
- zone içindeki eşzamanlı okuma/yazma kuralları,
- atomiklik ve memory ordering,
- alan dolduğunda / kaynak kullanılamadığında hata modeli,
- serbest bırakma ve scope sonu davranışı.

Özellikle `zone` ortak erişime izin verse bile, bu otomatik olarak atomik erişim veya data-race güvenliği anlamına gelmemelidir.

## 7. OCC / OIR için uygulama ilkeleri

- Parser, `area` ve `zone` intrinsic'lerini tanıyabilir; bu tek başına semantiğin tamamlandığı anlamına gelmez.
- Semantic artifact, alan sınıfı, erişim niyeti, sahiplik/ömür ve varsa hedef kısıtlarını açıkça taşımalıdır.
- OIR, fiziksel L1/L2/L3/L4 adlarına değil mantıksal bellek bölgesi ve erişim niteliklerine dayanmalıdır.
- Backend/OS adapter, hedef topolojisine göre eşleme yapar.
- Hedefin desteklemediği zorunlu bir nitelik sessizce düşürülmez; açık tanı üretilir.

## 8. Açık kararlar

1. Mevcut `area()` ve `zone()` çağrılarının tam argüman ve sonuç sözleşmesi nedir?
2. Bu intrinsic'ler tahsis mi yapar, mevcut belleği sınıflandırır mı, yoksa bir bellek alanı handle'ı mı döndürür?
3. Varsayılan yerleşim tercihi area mı zone mu, yoksa tür/erişim desenine göre otomatik mi?
4. Fallback yalnızca tercih kipinde mi serbest olacak?
5. Yerleşim politikası kaynak kodunda mı, target config'te mi, ikisinde mi tanımlanacak?

Bu sorular çözülmeden yeni sözdizimi veya fiziksel cache garantisi eklenmemelidir.
