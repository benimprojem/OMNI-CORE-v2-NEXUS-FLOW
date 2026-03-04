OmniCore Derleyicisi (OCC), LALR(1) parser katmanında hata mesajlarını yalnızca sözdizimi düzeyinde değil, NexusFlow’un **"Flow Sugar" (akış şekeri)** ve **handler (masa)** temelli sahiplik modeline göre şu mekanizmalarla özelleştirir:

### 1\. Hata Agregasyonu (Error Aggregation)

Derleyici, bir hata bulduğunda süreci durdurmak yerine tüm kaynak dosyalarını taramaya devam eder 1\. **Error Aggregation** özelliği sayesinde, tüm sözdizimi ve semantik hataları bir araya getirilerek kullanıcıya ayrıntılı bir rapor olarak sunulur 1\. Bu mesajlar hatanın dosyası, satırı ve sütun bilgisiyle birlikte tam konumunu belirtir 1\.

### 2\. Desugar Katmanı ve Akış Denetimi

NexusFlow'un yüksek seviyeli akış operatörleri (\<-, \-\>, ?-\> vb.), parser aşamasında veya hemen sonrasında **Core Language (Çekirdek Dil)** yapılarına dönüştürülür (Desugar) 2, 3\. Bu süreçte akışa özgü hatalar şöyle özelleştirilir:

* **Akış Mantığı Hataları:** Eğer bir ?-\> (Catch) operatörü yanlış bir hata bloğuna yönlendiriliyorsa veya akış kuralları ihlal edilmişse, derleyici bu operatörlerin çekirdek dile dönüşümü sırasında akışın doğasına uygun hata mesajları üretir 3\.  
* **Fastexec Kısıtlaması:** Eğer ASM blokları bir fastexec konteyneri dışında tanımlanmışsa, derleyici doğrudan bir yapılandırma hatası raporlar 4\.

### 3\. Masa (Handler) ve Sahiplik Hataları

LALR(1) parser ve ardından gelen semantik analiz, NexusFlow'un handler-oriented belleğini denetleyerek şu özel hata mesajlarını döndürür:

* **Table Occupied (Masa Dolu):** Eğer bir masa (h) halihazırda veri içeriyorsa ve \<- (Capture) operatörü ile üzerine yeni bir veri yazılmaya çalışılırsa, derleyici "Table Occupied" hatası vererek masanın önce boşaltılması gerektiğini bildirir 5\.  
* **Sahiplik İhlali (Relocate Check):** Bir masa \_\> (Relocate) ile başka bir bloğa taşındıktan sonra eski blokta ona erişilmeye çalışılırsa, derleyici bu sahiplik devrini takip eder ve derleme anında hata üretir 6\.

### 4\. Çoklu Kural İşleyici (Multi-Rule Engine) ve Bariyerler

Fonksiyonlara enjekte edilen kurallar, akışın başında durdurucu birer mesaj kaynağı olarak çalışabilir:

* **Barrier Rules (:=\>):** Eğer bir bariyer koşulu sağlanırsa, fonksiyonun çalışması durdurulur ve kullanıcıya/log sistemine bu kurala özgü özelleştirilmiş bir mesaj döner 7\.

### 5\. Tip ve Format Güvenliği

Format motoru (Format Engine) derleme aşamasında çalıştığı için, {x:.2f} gibi bir şablon ile değişken tipi (x) uyuşmuyorsa, derleyici **Type Mismatch** hatasını runtime crash yerine derleme anında özelleştirilmiş bir uyarı olarak verir 8\.  
Ayrıca, **m: (MUST)** ile işaretlenen zorunlu bağımlılıkların yüklenememesi durumunda derleyici, akışı devam ettirmeyerek spesifik bir "bağımlılık eksikliği" hatasıyla süreci sonlandırır 9\.  
