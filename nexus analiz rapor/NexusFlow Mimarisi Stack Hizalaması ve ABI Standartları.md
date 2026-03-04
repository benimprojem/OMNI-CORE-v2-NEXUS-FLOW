NexusFlow mimarisinde **stack alignment (yığın hizalaması)**, fonksiyon çağrıları sırasında yığın işaretçisinin (stack pointer) belirli bayt sınırlarına (8, 16, vb.) tam bölünmesini zorunlu kılan bir kuraldır 1, 2\. Bu kurallar, seçilen **ABI (Application Binary Interface)** standardına ve donanım mimarisine bağlı olarak .target dosyalarında şu şekilde değişir:

### 1\. Windows x64 ABI (ms\_x64)

Windows işletim sisteminde kullanılan ms\_x64 standardı, katı hizalama ve alan kurallarına sahiptir:

* **Hizalama Sınırı:** Yığın her zaman **16-byte** sınırına hizalanmalıdır 2\. Bu, özellikle SIMD (AVX/SSE) talimatlarının performanslı çalışması ve donanım uyumluluğu için kritiktir 2, 3\.  
* **Shadow Space (Gölge Alan):** Windows ABI'sinde, yığın hizalamasına ek olarak her fonksiyon çağrısı için yığında **32-byte'lık bir "shadow space"** rezerve edilmesi zorunludur 2, 4\. Derleyici (OCC), fonksiyon girişinde bu alanı otomatik olarak ayırır 4\.

### 2\. Bare-Metal ve Gömülü Sistemler

İşletim sistemi olmayan ortamlarda hizalama kuralları doğrudan donanımın gereksinimlerine göre belirlenir:

* **Esnek Yapılandırma:** İşlemci mimarisine göre hizalama genellikle **8-byte** veya **16-byte** olarak yapılandırılır 2\. Örneğin, bazı ARM tabanlı sistemlerde 8-byte hizalama yeterli olabilirken, 64-bit mimarilerde genellikle 16-byte tercih edilir 2\.  
* **Donanım Zorunluluğu:** Bare-metal projelerde yanlış hizalama, işlemcinin "alignment fault" hatası vererek durmasına (panic) neden olabilir 2, 5\.

### 3\. Derleyici (OCC) Tarafından Yönetim

Farklı ABI standartlarındaki bu değişimler, derleme sürecini şu şekilde etkiler:

* **Target Dosyası Kontrolü:** Derleyici, .target dosyasındaki stack\_alignment parametresini okuyarak kod üretimi (codegen) sırasında yığın pointer'ını (rsp) bu değere göre ayarlar 1, 2\.  
* **Fonksiyon Prolog ve Epilogları:** Derleyici, fonksiyonun başında ve sonunda yığını temizlerken ABI'nin gerektirdiği hizalamayı ve ek alanları (shadow space gibi) hesaba katar 2, 4\.  
* **Inline Assembly (fastexec):** Geliştirici fastexec blokları içinde manuel assembly yazsa bile, optimizer yığın hizalamasını ve yazmaç eşlemelerini ABI kurallarına sadık kalarak otomatik olarak denetler veya düzeltir 6, 7\.

Özetle; **Windows** için 16-byte hizalama ve 32-byte shadow space standart iken, **Bare-metal** hedeflerde bu değerler donanım mimarisinin fiziksel sınırlarına göre .target dosyası üzerinden özelleştirilir 2, 8\.  
