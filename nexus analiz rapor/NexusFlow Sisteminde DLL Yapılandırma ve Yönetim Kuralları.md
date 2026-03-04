NexusFlow sisteminde dllexp: anahtar kelimesi kullanılarak bir DLL oluşturulurken uyulması gereken temel kurallar şunlardır:

* **Giriş Noktası (Entry Point) Tanımı:** DLL derleme sürecinde normal bir main() fonksiyonu kullanılamaz; bunun yerine DLL'in giriş noktası olan ana fonksiyon **dllexp:** ile tanımlanmalıdır 1, 2\.  
* **Görünürlük ve Erişim Kontrolü:** Fonksiyonlar ve gruplar varsayılan olarak **private** (özel) kabul edilir ve sadece tanımlandıkları kapsamda görünürler 3, 4\. Bir fonksiyonu DLL dışına public olarak açmak için **pup** (public function), bir grubu dışarı aktarmak için ise **exp** (export group) anahtar kelimesi kullanılmalıdır 2-4.  
* **Kullanım Kısıtlaması:** dllexp: anahtar kelimesi yalnızca geliştiricinin **kendi yazdığı kütüphaneler** için geçerlidir; dışarıdan temin edilen hazır DLL'ler için kullanılamaz 1, 2\.  
* **Grup Yapısı:** Yüklenen bir DLL içindeki tüm public fonksiyonlar bir **Group** gibi davranır. Bu fonksiyonlar, DLL'in yüklendiği masa (handler) veya belirlenen takma ad (alias) üzerinden çağrılır 5\.  
* **Platform Bağımsızlığı:** Derleyici, hedef platforma göre uygun çıktı formatını otomatik olarak belirler:  
* **Windows:** .dll 2, 5\.  
* **Linux:** .so 2, 5\.  
* **Bare-metal:** Link aşamasında .o veya .obj 2, 5\.  
* **Hata Yönetimi:** DLL'in yüklenememesi durumunda oluşabilecek hatalar **?-\>** operatörü ile yakalanmalıdır 5\. Ayrıca, bir DLL yükleme sonrasında masa (handler) üzerinden yönetilir ve kullanım bittiğinde **?;** (Halt) operatörü ile temizlenerek bellekten boşaltılmalıdır 5\.

