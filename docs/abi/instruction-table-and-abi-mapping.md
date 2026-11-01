Cross-compiling (çapraz derleme) sürecinde, OmniCore Derleyicisi (OCC) hedef platformun tüm karakteristik özelliklerini anlamak için **.target** dosyalarını birer "platform kılavuzu" olarak kullanır 1, 2\. Talimat Tablosu ve ABI tanımlarının eşleşme süreci şu şekilde işler:

### 1\. Talimat Tablosunun Oluşturulması

Derleme süreci başladığında derleyici, belirtilen platformun .target dosyasını okuyarak bir **Talimat Tablosu** oluşturur 3\. Bu tablo, derleyicinin kaynak kodunu hedef makine koduna dönüştürürken kullanacağı "sözlük" görevini görür.

* **Donanım Parametreleri:** İşlemci mimarisi (arch), bit genişliği (bits) ve bayt sıralaması (endian) gibi bilgiler bu tabloya işlenir 4, 5\.  
* **Yazmaç (Register) Havuzu:** .target dosyasında tanımlanan registers, scratch\_regs ve reserved\_regs listeleri, derleyicinin **Graph Coloring** algoritması ile hangi değişkeni hangi fiziksel yazmaca atayacağını belirlemesini sağlar 5, 6\.

### 2\. ABI ve Çağrı Standartlarının (Calling Convention) Eşleşmesi

ABI tanımları, fonksiyonların birbirini nasıl çağıracağını ve verilerin nasıl aktarılacağını belirleyen katı kurallar bütünüdür. Derleyici, .target dosyasındaki \[abi\_convention\] bölümünü Talimat Tablosu ile şu şekilde eşleştirir:

* **Parametre ve Dönüş Değerleri:** Örneğin, Windows x64 hedefinde param\_regs olarak tanımlanan rcx, rdx, r8, r9 yazmaçları, fonksiyon argümanlarının sırasıyla bu donanım birimlerine yerleştirilmesini sağlar 7\. Dönüş değeri ise tabloda belirtilen return\_reg (genellikle rax) üzerinden alınır 7\.  
* **Stack (Yığın) Yönetimi:** ABI'nin gerektirdiği stack\_alignment (hizalama) ve Windows'a özgü shadow\_space (32-byte gölge alan) gibi değerler, derleyici tarafından fonksiyon girişlerinde (prologue) ve çıkışlarında (epilogue) otomatik olarak yığın üzerinde ayrılır 5, 7\.

### 3\. Nexus Primitiflerinin Düşük Seviyeli Eşlemesi

NexusFlow'un özgün "handler" (masa) mekanizması, Talimat Tablosu aracılığıyla doğrudan hedef mimarinin assembly komutlarına eşlenir:

* **handler\_init:** Masanın bellek üzerindeki başlangıç durumu (örneğin xor rax, rax) hedef mimariye göre Talimat Tablosu'ndan çekilir 7\.  
* **relocate\_op:** Sahiplik transferi (\_\>) işlemi sırasında verinin bir yazmaçtan diğerine taşınması ve eski yazmacın sıfırlanması (örneğin mov rdi, rsi; xor rsi, rsi), hedef platformun Talimat Tablosu'ndaki assembly karşılığı ile gerçekleştirilir 7\.

### 4\. Target-Aware (Hedef Duyarlı) Karar Verme

Bu eşleşme sayesinde derleyici, kod içindeki **\!\!=\[target="..."\]** direktiflerini veya **exf:** (extern) tanımlamalarını o anki Talimat Tablosu ile kıyaslar 8-10. Eğer aktif target ile kod bloğu uyuşuyorsa, ilgili ABI kurallarına (örneğin Windows için WinMain, Bare-metal için \_start) göre derleme tamamlanır 3, 7\.  
**Özetle;** .target dosyası platformun fiziksel ve mantıksal sınırlarını tanımlarken, **Talimat Tablosu** bu bilgileri derleyicinin anlayacağı bir yürütme planına dönüştürür ve **ABI tanımları** bu planın diğer sistem fonksiyonlarıyla nasıl "konuşacağını" mühürler 1, 3, 11\.  
