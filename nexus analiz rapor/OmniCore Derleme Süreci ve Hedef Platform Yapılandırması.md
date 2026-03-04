OmniCore Derleyicisi (OCC), **.target** dosyalarını kaynak kodun hedef mimariye ve işletim sistemine tam uyumlu bir makine koduna dönüştürülmesi için bir "platform kılavuzu" olarak kullanır 1, 2\. Bu süreç, derleme öncesinden başlayıp nihai bağlama (linking) aşamasına kadar devam eden çok katmanlı bir akıştır.

### 1\. Hedef Belirleme ve Keşif (Discovery)

Derleme süreci başladığında, derleyici SDK dizini altındaki /targets klasörünü otomatik olarak tarayarak mevcut platform yapılandırmalarını listeler 3, 4\. Geliştirici, komut satırı üzerinden \-t parametresiyle (örneğin: occ \-t x86\_64-win) veya IDE arayüzünden ilgili hedefi seçtiğinde, derleyici bu dosyayı belleğe yükleyerek işleme başlar 5, 6\.

### 2\. .target Dosyasının Ayrıştırılması ve TargetInfo Oluşumu

Derleyici, .target dosyasındaki anahtar-değer çiftlerini (işlemci mimarisi, ABI, register sayıları, stack hizalaması vb.) okur 2, 7, 8\. Bu bilgilerden bir **TargetInfo** veri yapısı ve **Talimat Tablosu** oluşturulur 1, 9\. Bu yapılandırma şu kritik verileri içerir:

* **Mimari Detayları:** CPU ailesi, endianness (bayt sıralaması) ve pointer boyutu (32/64 bit) 10, 11\.  
* **Yazmaç (Register) Havuzu:** Genel amaçlı, SIMD ve kayan nokta (float) yazmaçlarının listesi 10, 11\.  
* **Bellek Modeli:** Sayfa boyutu, yığın hizalaması (stack alignment) ve varsayılan bellek alanı boyutları 10, 11\.  
* **ABI ve Çağrı Standartları:** Parametre ve dönüş yazmaçları ile Windows için gerekli olan "shadow space" gibi platforma özel alan tanımları 12\.

### 3\. Koşullu Derleme ve Makro Genişletme

Semantik analiz aşamasında derleyici, kod içindeki hedef bazlı direktifleri (\!\!=\[target="..."\]) işler 13, 14\. .target dosyasından alınan bilgiler sayesinde derleyici, o anki hedefe uygun olmayan kod bloklarını eler ve platform bağımsız arayüzlerin (OS Bridge) doğru backend'e bağlanmasını sağlar 15-17.

### 4\. Optimizasyon ve Ara Kod (IR) İşleme

Omni-IR aşamasında, optimizer (eniyileyici) .target dosyasındaki donanım sınırlarını dikkate alır 9:

* **Döngü Açma (Loop Unrolling):** Yazmaç sayısına göre döngülerin ne kadar açılacağına karar verilir 9, 18\.  
* **Vektörleştirme:** Hedefin SIMD genişliği (örn: AVX512) ve vektör yazmaç sayısı kullanılarak otomatik vektörleştirme yapılır 9, 11, 18\.

### 5\. Kod Üretimi (Codegen) ve Bağlama (Linking)

Bu aşama, .target dosyasındaki bilgilerin doğrudan makine koduna dönüştüğü yerdir:

* **Yazmaç Tahsisi:** occ\_codegen.dll modülü, dosyada tanımlanan yazmaç havuzunu kullanarak Graph Coloring algoritması ile değişkenleri yazmaçlara atar 19-21.  
* **Düşük Seviye Eşlemeler:** NexusFlow'a özgü operasyonlar (handler\_init, relocate\_op vb.), .target dosyasında belirtilen özel assembly komut dizilerine dönüştürülür 12\.  
* **Nihai Dosya Formatı:** Linker (ocl.exe), .target dosyasındaki çıktı formatına (Windows için PE/.exe, Linux için ELF vb.) göre dosyayı oluşturur ve uygun giriş noktasını (WinMain veya \_start) mühürler 1, 12, 22\.

### 6\. Bare-Metal ve OS Ayrımı

Eğer hedef platform bir işletim sistemi içermiyorsa (bare-metal), derleyici .target dosyasındaki talimatlara uyarak standart kütüphaneleri devre dışı bırakır ve sadece donanım kesmelerini (IRQ) ve fiziksel adreslemeyi kullanan minimal bir binary üretir 1, 23, 24\.  
