OmniCore Derleyicisi (OCC) ve NexusFlow ekosistemi, modüler bir SDK yapısı ve deterministik bir proje organizasyonu üzerine inşa edilmiştir. Aşağıda, sistemin SDK düzeyindeki klasör yapısı, proje bazlı dosya düzeni ve kullanılan dosya uzantılarının kapsamlı bir dökümü yer almaktadır.

### 1\. SDK / Sistem Seviyesi Klasör Yapısı

OmniCore SDK kurulumu, derleyici araçlarını, standart kütüphaneleri ve hedef platform tanımlarını şu dizinler altında toplar 1-4:

* **/bin (Araçlar):** Çalıştırılabilir derleyici bileşenlerini barındırır.  
* occ.exe: Derleyici orkestratörü (Frontend).  
* ocm.exe: Paket ve bağımlılık yöneticisi.  
* ocl.exe: Linker (Backend).  
* ocvm.exe: JIT / Hızlı test aracı.  
* oce.exe: OmniCore IDE / Editör.  
* **/lib (Modüler Bileşenler):** Derleyicinin her aşaması ayrı DLL/so dosyaları olarak modülerdir.  
* occ\_parser.dll: Kaynak koddan AST üretimi.  
* occ\_analyzer.dll: Tip ve semantik kontrol.  
* occ\_opt.dll: Optimizasyon motoru.  
* occ\_codegen.dll: Makine kodu üretimi.  
* **/stdlib (Standart Kütüphane):** OS bağımsız API'ler ve çekirdek modüller (core, io, net, sys) burada yer alır.  
* **/targets (Hedef Tanımları):** Her platform için mimari ve ABI bilgilerini barındıran .target dosyalarının bulunduğu merkezi dizindir.

### 2\. Proje Seviyesi Klasör Yapısı

Bir geliştirici ocm.exe create-project komutunu çalıştırdığında, aşağıdaki standart klasör yapısı otomatik olarak oluşturulur 5-7:

* **src/:** NexusFlow kaynak kodları (.nx) bu dizinde tutulur.  
* **include/:** Dış kütüphane başlık dosyaları (.h, .oci) burada yer alır.  
* **build/:** Derleme sonucunda oluşan ara kodlar (.ocb) ve nihai dosyalar (.exe, .dll) bu klasöre çıkarılır.  
* **dpslib/:** Projenin kullandığı paylaşımlı bağımlılıkların deposudur. Bu yapı sayesinde paketler bir kez yüklenir ve tüm projelerde tekrar kullanılabilir.  
* **assets/:** Görseller, stiller ve diğer statik kaynaklar için ayrılmış alandır.  
* **targets/:** Projeye özel özelleştirilmiş hedef platform tanımları burada saklanabilir.

### 3\. Dosya Tipleri ve Görevleri

Sistemde kullanılan temel dosya uzantıları ve işlevleri şunlardır 2:  
Uzantı,Açıklama  
.nx,NexusFlow kaynak kodu.  
.ocf,Proje metadata ve bağımlılıklarını tutan dosya.  
.nxr,Derleme direktiflerini ve adımlarını yöneten build dosyası.  
.ocb,Derlenmiş ara kod veya nihai ikili (binary) dosya.  
.target,"Hedef platformun (mimarisi, ABI, register seti vb.) teknik dökümü 8, 9."  
.oci,Interface (Arayüz) dosyası.  
.ocmap,Hata ayıklama (debug) için kaynak haritası.  
.ocd / .ocs,Dokümantasyon ve görsel stil tanımları.

### 4\. Modül ve Bağımlılık Yönetimi

* **CoreModule:** Derleme sırasında otomatik olarak yüklenen ana modüldür. Eğer bare-metal bir çalışma yapılacaksa, kodun başına no:CoreModule; eklenerek OS bağımlılıkları iptal edilebilir ve primordial katman olan "core" modülü devreye girer 10, 11\.  
* **Import Mekanizması:** import(path) komutu ile DLL, .so, .o ve .obj gibi dosyalar sisteme dahil edilebilir ve bu dosyalar birer masa (handler) gibi davranarak fonksiyonlarını dışa açar 12\.  
* **Linker (ocl):** Fonksiyon düzeyinde (function-level) linkleme yaparak kullanılmayan kodları temizler ve binary boyutunu minimal tutar 13\.

Bu yapısal organizasyon, NexusFlow'un **deterministik, platform bağımsız ve kilitlenmesiz** çalışma felsefesini dosya sistemine de yansıtarak hem OS hem de Bare-metal projelerin aynı verimlilikle yönetilmesini sağlar 7, 14\.  
