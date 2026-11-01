Farklı mimariler için özel yığın hizalama kuralları, NexusFlow mimarisinde her platforma özgü olarak oluşturulan **.target** yapılandırma dosyaları aracılığıyla tanımlanır 1, 2\. Bu dosyalar, derleyicinin (OCC) hedef donanımın fiziksel sınırlarını ve yığın (stack) yapısını anlamasını sağlayan bir kılavuz görevi görür 1, 3\.  
Yığın hizalamasını tanımlamak için izlenen adımlar ve kurallar şunlardır:

### 1\. Parametre Tanımlama

Hizalama kuralı, .target dosyasının Genellikle **\[memory\_model\]** başlığı altında **stack\_alignment** anahtarı ile belirtilir 4, 5\. Bu değer, bayt (byte) cinsinden tanımlanmalıdır 4, 6\.

* **Örnek Tanımlama (x86\_64-windows):**  
* \[memory\_model\]  
* page\_size \= 4096  
* stack\_alignment \= 16  \# Yığın 16-byte sınırlarına hizalanır  
* Bu örnekte görüldüğü gibi, x86\_64 mimarisi için standart olan 16-byte hizalama kuralı derleyiciye bildirilmiştir 5, 7\.

### 2\. Derleyici ve Optimizasyon Süreci

Tanımlanan bu değer, derleme sürecinin şu aşamalarında kritik rol oynar:

* **Kod Üretimi (Codegen):** Derleyici, fonksiyon giriş (prologue) ve çıkışlarında (epilogue) yığın işaretçisini (stack pointer) bu hizalama kuralına göre ayarlar 8, 9\.  
* **Vektör Optimizasyonu:** Özellikle SIMD (AVX512, NEON vb.) komutlarının doğru çalışması için yığındaki verilerin bu hizalamaya uygun olması gerekir 10, 11\.  
* **ABI Uyumluluğu:** stack\_alignment değeri, hedef platformun ABI (Application Binary Interface) standartlarıyla (örneğin Windows için ms\_x64) doğrudan eşleşmelidir 1, 5\.

### 3\. Yeni Target Oluştururken Zorunluluk

Geliştiriciler yeni bir mimari veya özel bir donanım (FPGA, özel kernel vb.) için kendi .target dosyalarını oluştururken, stack\_alignment parametresini doldurmak **zorunludur** 6, 12\. Eksik tanımlama durumunda derleyici hata raporlayarak platforma özgü yığın yönetimini gerçekleştiremez 13, 14\.

### 4\. Hizalama Bilgisinin Programatik Erişimi

Kod içerisinde belirli bir tipin veya yapının hizalama sınırını kontrol etmek için Primordial Core (OS bağımsız) katmanında yer alan **core.mem.align\_of(\[T\])** fonksiyonu kullanılabilir 15, 16\. Bu, özellikle düşük seviyeli bellek manipülasyonları ve donanım sürücüleri yazarken struct padding işlemlerini optimize etmek için kullanılır 15, 17\.  
Özetle; yığın hizalaması, mimariye özel **.target** dosyasındaki **stack\_alignment** alanı ile bayt bazlı olarak sabitlenir ve bu bilgi derleyicinin tüm kod üretim ve optimizasyon katmanlarında bir otorite olarak kabul edilir 11, 18\.  
