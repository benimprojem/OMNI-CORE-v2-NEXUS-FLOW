NexusFlow mimarisinde dllexp: ve import(path) arasındaki temel fark, birinin **DLL oluşturma (export)**, diğerinin ise **DLL yükleme (import)** süreçlerini yönetmesidir 1, 2\.

### dllexp: (DLL Export)

* **Amacı:** Bir projenin ana fonksiyonu (main) yerine, **DLL olarak derlenecek giriş noktasını (entry point)** belirlemek için kullanılır 1, 3\.  
* **Kapsamı:** Sadece geliştiricinin **kendi yazdığı DLL'ler** için geçerlidir; dışarıdan temin edilen hazır DLL'ler için bu ifade kullanılamaz 1, 3\.  
* **İşleyişi:** dllexp: ile tanımlanan fonksiyonlar, DLL dışına **public (açık)** olarak sunulur 1\. Bir DLL projesinde normal bir main() fonksiyonu bulunmaz; onun yerini bu giriş noktası alır 1\.

### import(path) (DLL Yükleme)

* **Amacı:** Derlenmiş olan .dll, .so, .o veya .obj gibi dosyaları projeye dahil etmek için kullanılır 2, 3\.  
* **İşleyişi:** Yüklenen kütüphane, NexusFlow'un **masa (handler \- (h))** mekanizması gibi davranır 2\. Kütüphane içindeki tüm fonksiyonlar ve gruplar, yüklenen bu masa veya belirlenen bir alias (takma ad) üzerinden çağrılır 2\.  
* **Yönetim:** DLL yükleme işlemi sonrasında bellek yönetimi ve kaynak temizliği (Handler)?; operatörü ile yapılır 2\. Yükleme sırasında oluşabilecek hatalar ?-\> operatörü ile denetlenebilir 2\.

**Temel Fark Özeti:**

* **dllexp:**, bir kodu kütüphane olarak **paketleyip dışa açmak** için kullanılır 1\.  
* **import(path)**, paketlenmiş bir kütüphaneyi **projeye dahil edip içindeki işlevleri kullanmak** için kullanılır 2\.

