NexusFlow ve OmniCore Derleyicisi (OCC), **Shadow space** ve **stack alignment** (yığın hizalaması) gibi düşük seviyeli hataları yakalamak yerine, bu gereksinimleri derleme ve optimizasyon aşamalarında otomatik olarak yöneterek hataları en aza indirecek şekilde tasarlanmıştır 1, 2\.  
Bu hataların nasıl yönetildiği ve denetlendiğine dair mekanizmalar şunlardır:

### 1\. Codegen ve Optimizer Sorumluluğu

Inline assembly kullanımı sağlayan **fastexec** bloklarında, yığın ve yazmaç (register) yönetimi doğrudan derleyicinin **Codegen** (Kod Üretimi) biriminin sorumluluğundadır 3, 4\.

* **Clobber Listesi Denetimi:** Geliştirici, \# clobber: rax, rbx gibi komutlarla hangi yazmaçların kirletileceğine dair sadece "öneri" verir; ancak son kararı **optimizer** verir 1, 3-5.  
* **Otomatik Düzeltme:** Eğer kullanıcı yanlış bir clobber listesi önerirse veya yazmaçları hatalı eşlerse, optimizer bu durumu fark eder ve gerekli **push/pop** işlemlerini ekleyerek veya yazmaçları yeniden tahsis ederek hatayı düzeltir 1, 3, 5, 6\. Bu sayede register spill (yazmaç taşması) hataları minimize edilir 1\.

### 2\. .target Dosyası ve ABI Kuralları

Derleyici, her platformun kendine has yığın kurallarını ilgili **.target** dosyasından okur 7, 8\.

* **Shadow Space Tanımı:** Örneğin, bir Windows hedefi (x86\_64-windows.target) için shadow\_space \= 32 parametresi tanımlıdır 9\. Derleyici, bu platform için kod üretirken her fonksiyon çağrısında bu 32-byte'lık alanı otomatik olarak rezerve eder.  
* **Hizalama Kuralları:** Yine aynı dosyada yer alan stack\_alignment \= 16 (x86\_64 için) bilgisi, derleyicinin yığın işaretçisini her zaman bu sınırda tutmasını sağlar 10\.

### 3\. Semantik Analiz ve Güvenlik Denetimleri

Derleyicinin **Semantik Analiz** aşaması, bellek ve yığın güvenliğini korumak için şu kontrolleri yapar:

* **Ownership Check (Sahiplik Kontrolü):** Masaların (handler) ve değişkenlerin ömürlerini (lifetime) takip ederek, bir bloğun sonunda masaların otomatik olarak imha edilmesini (}?;) sağlar 3, 11, 12\. Bu, sahipsiz işaretçi (dangling pointer) riskini ve yığının bozulmasını engeller 3, 13\.  
* **İzinsiz Erişim:** Bir masanın (h) sadece tanımlandığı blokta geçerli olması, başka bloklardan yığına izinsiz erişilmesini ve dolayısıyla stack corruption hatalarını önler 3, 12\.

### 4\. Risk ve Sınır Durumlar

Doğrudan assembly yazmak (özellikle fastexec dışında veya kontrolsüz jump/call işlemleriyle) hâlâ **Undefined Behavior (UB)** riski taşır 1, 2\. Ancak OmniCore, **Error Aggregation** özelliği sayesinde tüm kaynak dosyalarını tarayarak sözdizimi ve sahiplik hatalarını bir araya getirip ayrıntılı raporlar sunar 14\.  
Özetle; Shadow space ve hizalama gibi konular geliştiricinin manuel takibinden çıkarılıp **optimizer ve target-aware codegen** katmanlarına devredilmiştir; derleyici bu kurallara uymayan bir kod üretilmesini engellemek için kullanıcı hatalarını otomatik olarak düzeltir 1, 15, 16\.  
