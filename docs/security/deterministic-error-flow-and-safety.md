NexusFlow'un hata yönetimi, geleneksel dillerdeki "Exception" (İstisna) veya basit hata kodları mantığından tamamen ayrılarak, **"Handler-Oriented" (Masa Odaklı)** bir veri akışı ve deterministik operatörler üzerine inşa edilmiştir 1, 2\.  
NexusFlow'un hata yönetimini diğer dillerden ayıran temel özellikler şunlardır:

### 1\. Derleme Zamanında "Table Occupied" Kontrolü

Diğer dillerde bir değişkenin üzerine veri yazılması genellikle serbesttir veya çalışma zamanında (runtime) kontrol edilir. NexusFlow'da ise bir **masa (handler)** zaten veriyle doluysa ve \<- (Capture) operatörü ile yeni bir veri yazılmaya çalışılırsa, derleyici **"Table Occupied"** hatası vererek işlemi durdurur 3, 4\. Bu durum, bellek üzerine kazara yazma veya veri kilitlenmesi risklerini henüz kod derlenirken önler 3, 5\.

### 2\. Akış Tabanlı Hata Yönlendirme (Ternary Flow)

NexusFlow, hataları bir "akış sapması" olarak görür. Bir fonksiyon çalıştırıldığında sonuç **false, null, none veya unknown** ise akış otomatik olarak sağa, yani hata bloğuna ((e){}) yönlendirilir 6, 7\. Bu yapı, if-else yığınları yerine operatör seviyesinde bir kontrol sağlar:

* **?-\> (Catch/Saptırıcı):** İşlem başarısız olursa akışı belirlenen hata bloğuna yönlendirir 1\.  
* **?=\> (Fallback):** Hata oluştuğunda veya veri null ise akışa anında alternatif (yedek) bir veri enjekte eder 1, 8\.

### 3\. Sahiplik ve "Move-on-Flow" Güvenliği

C++ gibi dillerde sıkça görülen "dangling pointer" (boşta kalan işaretçi) hataları, NexusFlow'un sahiplik modeliyle engellenir. Veri bir masadan diğerine \-\> operatörü ile taşındığında, orijinal masa **sıfırlanır (nulled)** ve eski blokta bu veriye erişim imkansız hale gelir 3, 4\. Bu "sıfır maliyetli devir" (Zero-Cost Handover), verinin aynı anda iki yerde mutable (değiştirilebilir) olmasını yapısal olarak engeller 2, 3\.

### 4\. Deterministik Hata Operatörleri (The 15 Keys)

NexusFlow, hata durumlarını yönetmek için özelleşmiş 15 temel operatör sunar 1:

* **?(n,ms) (Rolling):** Bir hata durumunda işlemi belirtilen n kez ve ms milisaniye aralıklarla otomatik olarak tekrar dener 1\.  
* **\!-\> (Ignore):** Hata olsa bile akışı zorla devam ettirir; bu, kritik olmayan hataların akışı kesmesini önlemek için kullanılır 1, 9\.  
* **\!\! (Panic):** Kritik bir hata oluştuğunda tüm akışı anında durdurur ve bir runtime uyarısı tetikler 1, 8\.

### 5\. Tanı ve İzleme Araçları

Hata analizi için sistem çekirdeğinde yerleşik iki temel araç bulunur:

* **err.last():** Akışta oluşan en son hatanın detaylı objesini döndürür 10, 11\.  
* **err.trace():** Bir panic durumunda işlemci kayıtçılarının (register) ve yığının (stack) o anki izini (trace) vererek düşük seviyeli hata ayıklama imkanı sağlar 10, 11\.

Özetle; NexusFlow hataları çalışma zamanında yakalanması gereken beklenmedik olaylar olarak değil, **derleme aşamasında kuralları belirlenmiş bir veri akış rotası** olarak yönetir 2, 12\.  
