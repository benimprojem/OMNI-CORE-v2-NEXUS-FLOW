NexusFlow mimarisinde **"Table Occupied"** kuralı, Handler-Oriented (Masa Odaklı) bellek yönetim modelinin temel bir güvenlik mekanizmasıdır 1\. Bu kural, bir masa (handler) zaten veriyle doluyken veya kilitliyken, üzerine **\<- (Capture)** operatörü ile yeni bir veri yazılmaya çalışılması durumunda devreye girer 2, 3\.  
Kuralın işleyişi ve önemi şu temel noktalara dayanmaktadır:

* **Derleme Zamanı Güvenliği:** Derleyici, bir masanın doluluk durumunu takip eder ve dolu bir masaya yeni veri enjekte edilmesini çalışma zamanına (runtime) bırakmadan derleme aşamasında engeller 2, 4\. Bu durum, yanlış veri kilitleme veya verinin kazara üzerine yazılması (overwrite) risklerini ortadan kaldırır 3\.  
* **İstasyon Mantığı:** NexusFlow'da değişkenler sadece veri saklayan kutular değil, birer "istasyon" olarak görülür; eğer istasyon (masa) doluysa veri akışı durur ve sistem kilitlenir 1\. Bu kilitlenme, geliştiriciye masanın önce boşaltılması gerektiğini bildiren bir hata mesajı olarak yansır 2, 4\.  
* **Sahiplik ve Determinizm:** Bu kural, verinin sahipliğinin ve yaşam döngüsünün deterministik olmasını sağlar 1\. Masanın tekrar kullanılabilmesi için mevcut içeriğin **\-\> (Success/Move)** veya **\_\> (Relocate)** gibi operatörlerle başka bir bölgeye taşınmış olması veya masanın sıfırlanmış olması gerekir 2, 4\.  
* **Hata Yönetimi:** "Table Occupied" hatası alındığında, geliştiricinin masayı ya manuel olarak serbest bırakması ya da akışın masayı boşaltan bir operatörle (örneğin veriyi bir fonksiyona taşıyarak) devam etmesini sağlaması beklenir 2, 4\.

Özetle; bu kural, NexusFlow'un **"Masa doluysa kilitlenir"** felsefesinin bir sonucudur ve bellek yönetiminde beklenmedik davranışları önlemek için kullanılan kritik bir kısıtlamadır 1, 4\.  
