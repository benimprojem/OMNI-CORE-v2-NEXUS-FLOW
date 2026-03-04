NexusFlow mimarisinde masa (handler) sahipliği transferi, sistemin en yüksek performans kalemlerinden biri olarak tasarlanmıştır ve temel olarak **"Zero-Cost Handover" (Sıfır Maliyetli Devir)** prensibine dayanır 1, 2\.  
Masa sahipliği transferinin performansa etkileri şu şekilde detaylandırılabilir:

### 1\. Sıfır Kopyalama (Zero-Copy) Avantajı

Masa transferleri sırasında verinin kendisi fiziksel olarak bellekte bir yerden başka bir yere kopyalanmaz 1, 3\. Bunun yerine, sadece verinin bulunduğu **bellek adresinin etiketi (pointer)** bir masadan diğerine aktarılır 1, 4\.

* **İşlemci Seviyesinde Hız:** Örneğin x86\_64 mimarisinde bir relocate işlemi (\_\>), sadece bir register'daki adresin diğerine taşınması (mov) ve eski register'ın sıfırlanması (xor) kadar kısa süren, nanosaniye mertebesinde bir işlemdir 5\.  
* **Büyük Veri Blokları:** GB'larca boyuttaki bir veri setinin sahipliği, verinin boyutu ne olursa olsun aynı hızda (sadece adres değişimiyle) devredilir 1\.

### 2\. Move-on-Flow Mekanizması

Veri bir işlemden diğerine \-\> (Success) operatörü ile aktarıldığında veya bir fonksiyona paslandığında, derleyici bunu otomatik olarak bir **taşıma (move)** işlemi olarak ele alır 1, 3\.

* İşlem sonunda kaynak masa otomatik olarak null (sıfır) yapılır 1\.  
* Bu durum, gereksiz bellek tahsisi (allocation) ve serbest bırakma (deallocation) döngülerini ortadan kaldırarak çalışma zamanı (runtime) yükünü minimize eder 2\.

### 3\. Bölgeler Arası (Zone/GPU) Hızlı Geçiş

**\_\> (Relocate)** operatörü, bir masayı veya bağlamı (context) farklı bellek segmentlerine (örneğin CPU'dan GPU belleğine veya farklı bir thread handler'ına) taşımak için kullanılır 3, 6\.

* Bu taşıma, tüm mantıksal kararlardan önce yapılarak verinin işleneceği en verimli bölgeye en kısa yoldan ulaşması sağlanır 7\.  
* Thread'ler arası iletişimde (IPC) kullanılan send() ve receive() metotları da bu sahiplik transferi modelini kullanarak **kopyalamasız (copy-free) veri takası** sağlar 8, 9\.

### 4\. Deterministik Bellek Yönetimi ve Temizlik

Sahiplik her zaman tek bir masada (son blokta) olduğu için, derleyici bellek ömürlerini (lifetime) çalışma zamanında bir çöp toplayıcıya (Garbage Collector) ihtiyaç duymadan yönetebilir 2, 3\.

* Blok sonlarındaki **}?; (Halt)** operatörü, sadece o an sahipliği elinde bulunduran masaları temizler 3\. Bu, belleğin sürekli temiz kalmasını sağlayarak uzun süre çalışan sistemlerde performans düşüşünü (memory fragmentation/leak) engeller 3\.

### 5\. Derleme Zamanı Optimizasyonu (OCC)

Omni Core Compiler (OCC), sahiplik transferlerini derleme aşamasında (Semantic Analysis) denetler 10\.

* Sahipliği devredilmiş bir masaya erişim hataları derleme anında yakalandığı için, çalışma zamanında ek güvenlik kontrollerine veya "null check" mekanizmalarına ihtiyaç duyulmaz, bu da saf makine kodu performansına yaklaşılmasını sağlar 3, 11\.

**Özetle;** NexusFlow'da masa transferi veriyi kopyalamak yerine **sadece yetkiyi devrettiği** için, bellek bant genişliği üzerinde hiçbir baskı oluşturmaz ve özellikle büyük veri işleme ile bare-metal sistemlerde sistem kaynaklarının %100 verimle kullanılmasını sağlar 1, 2, 12\.  
