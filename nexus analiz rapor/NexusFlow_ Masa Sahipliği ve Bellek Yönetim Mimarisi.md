NexusFlow mimarisinde **Masa (Handler \- (h))** sahiplik mekanizması, hem sistem performansını maksimize etmek hem de bellek güvenliğini donanım seviyesinde garanti altına almak için tasarlanmış bir "istasyon" modelidir 1\.  
Bu mekanizmanın sistem üzerindeki etkileri şunlardır:

### 1\. Sistem Performansına Etkileri

Masa sahipliği transferi, veriyi fiziksel olarak taşımak yerine yetkiyi devrettiği için "sıfır maliyetli" bir yapı sunar.

* **Sıfır Kopyalama (Zero-Copy):** Veri bir masadan diğerine aktarılırken fiziksel bir kopyalama yapılmaz; sadece verinin bulunduğu **bellek adresi etiketi** değiştirilir 2, 3\. Bu durum, özellikle büyük veri bloklarının transferinde işlemci yükünü ortadan kaldırır.  
* **Sıfır Maliyetli Devir (Zero-Cost Handover):** Masalar arası transfer işlemi sadece bir adres etiketinin değişiminden ibarettir 2\. Bu sayede verinin bir iş parçacığından (thread) diğerine veya CPU'dan GPU belleğine geçişi "ışık hızında" gerçekleşir 4, 5\.  
* **Move-on-Flow:** Veri bir fonksiyona veya işleme aktarıldığında (-\> operatörü), derleyici veriyi kopyalamak yerine doğrudan **taşır** 2\. İşlem bittiğinde kaynak masa otomatik olarak sıfırlanır (nulled), böylece gereksiz bellek yönetimi yükü oluşmaz 2\.  
* **Cache Verimliliği:** Nexus Slot Matrix üzerindeki her masa alanı **32-byte hizalıdır (cache-line optimized)** 6, 7\. Bu hizalama, modern CPU'ların veriye en hızlı şekilde erişmesini sağlayarak performansı artırır 6, 8\.

### 2\. Bellek Güvenliğine Etkileri

Sahiplik mekanizması, bellek yönetimini deterministik bir hale getirerek yazılım hatalarından kaynaklanan güvenlik açıklarını derleme aşamasında engeller.

* **Otomatik Ömür Yönetimi:** Her masa sadece tanımlandığı blok (scope) içinde geçerlidir 9\. Blok sonunda kullanılan **}?; (Halt)** operatörü, o bloktaki tüm masaları ve kaynakları otomatik olarak imha eder, böylece bellek sızıntıları (memory leaks) önlenir 3, 10, 11\.  
* **Dangling Pointer Önleme:** Bir masa **\_\> (Relocate)** operatörü ile başka bir bloğa taşındığında, orijinal bloktaki masa anında silinir ve geçersiz kılınır 3, 9\. Eski bloktan bu veriye erişim denendiğinde derleyici (OCC) hata verir, bu da sahipsiz işaretçi (dangling pointer) riskini ortadan kaldırır 12, 13\.  
* **Yarış Koşulu (Race Condition) Koruması:** Verinin sahipliği her zaman tek bir masadadır. Bir veri taşındığında eski masada artık var olmadığı için, verinin aynı anda iki farklı yerden değiştirilmesi (mutable access) imkansız hale gelir 3, 12\.  
* **Üzerine Yazma Koruması (Table Occupied):** Eğer bir masa zaten doluysa ve üzerine yeni bir veri yazılmaya çalışılırsa (\<- Capture), derleyici bu durumu fark eder ve hata verir; böylece verilerin istem dışı bozulması engellenir 2, 3\.  
* **İzinsiz Erişimin Engellenmesi:** Masalar kapsam (scope) güvenliğine sahiptir; bir masa sadece kendi bloğu içinde okunabilir, dış bloklardan izinsiz erişim mümkün değildir 3, 9\.

Özetle; masa sahipliği sistemi, veriyi kopyalamadan hareket ettirerek **sıfır maliyetle performans** sağlarken, sahiplik devri kurallarıyla da **dangling pointer ve yarış koşulu gibi kritik bellek hatalarını** kökten çözer 14-16.  
