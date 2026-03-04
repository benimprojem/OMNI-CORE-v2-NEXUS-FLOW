NexusFlow mimarisinde **Masa (Handler \- (h))** sahiplik mekanizması, bellek yönetimini deterministik, atomik ve güvenli bir hale getirmek için tasarlanmış temel bir yapıdır 1\. Bu mekanizmanın bellek güvenliği üzerindeki etkileri şu ana başlıklar altında toplanabilir:

### 1\. Otomatik Ömür Yönetimi ve Temizlik (Lifetime & Cleanup)

Masalar, sadece tanımlandıkları blok (scope) içerisinde geçerlidir 2, 3\. Bir bloğun sonuna gelindiğinde kullanılan **}?; (Halt)** operatörü, o bloktaki tüm yerel masaları ve rezerve edilen kaynakları otomatik olarak imha eder 1, 2, 4\. Bu durum, bellek sızıntılarını önlediği gibi **"dangling pointer"** (sahipsiz işaretçi) riskini de minimize eder 2, 3\.

### 2\. Sahiplik Devri ve Relocation (\_\>)

Verinin bir bloktan başka bir bloğa (örneğin farklı bir thread'e veya GPU belleğine) taşınması **\_\> (Relocate)** operatörü ile yapılır 1, 4, 5\.

* **Sahiplik Transferi:** Veri taşındığında, orijinal bloktaki masa anında **sıfırlanır (nulled) ve imha edilir** 2, 3, 6\.  
* **Erişim Kontrolü:** Sahipliği devredilen bir masaya eski bloktan tekrar erişilmeye çalışılırsa, derleyici (OCC) derleme aşamasında hata verir 3, 7\. Bu, verinin aynı anda birden fazla blokta "mutable" (değiştirilebilir) olarak bulunmasını ve olası yarış durumlarını (race conditions) engeller 2, 8\.

### 3\. Derleme Zamanı Denetimleri (Compile-Time Safety)

NexusFlow derleyicisi, masaların durumunu sıkı kurallarla takip eder:

* **Table Occupied (Masa Dolu):** Eğer bir masa zaten veri içeriyorsa ve **\<- (Capture)** operatörü ile üzerine yeni bir veri yazılmaya çalışılırsa, derleyici hata vererek üzerine yazma (overwrite) hatalarını önler; masanın önce boşaltılması gerekir 3, 9\.  
* **Move-on-Flow:** Bir masadaki veri **\-\> (Success)** operatörü ile bir sonraki işleme aktarıldığında, masa kopyalama yapmaz, veriyi **taşır** ve işlem sonunda masa otomatik olarak null yapılır 3, 9\.

### 4\. İzolasyon ve Güvenli Erişim

Masalar, tanımlandıkları kapsamın dışından doğrudan okunamaz veya izinsiz erişilemez 3\. Bu yapı, bellek bölgeleri arasında tam bir izolasyon sağlar. Ayrıca **Zero-Cost Handover** prensibi sayesinde, masalar arası transferler sadece adres etiketinin değişimiyle yapılır; veri kopyalanmadığı için hem yüksek performans sağlanır hem de kopyalama sırasında oluşabilecek veri tutarsızlıklarının önüne geçilir 3, 9\.

### 5\. Atomik İşlemler ve Zone Güvenliği

Paylaşımlı bellek alanları (**zone**) için kullanılan **\<\< (Zone Write)** ve **\>\> (Flow Feed)** operatörleri, veriyi atomik olarak işler 1, 10\. Bu, çok çekirdekli sistemlerde aynı bellek alanına yapılan erişimlerde "race condition" riskini minimize eder 2, 11\.  
**Özetle;** Masa mekanizması, belleği fiziksel olarak yönetirken sahipliği her zaman son blokta tutar ve derleyici düzeyinde yapılan kontrollerle (ownership check) manuel bellek yönetiminden kaynaklanan hataların çoğunu ortadan kaldırır 12, 13\.  
