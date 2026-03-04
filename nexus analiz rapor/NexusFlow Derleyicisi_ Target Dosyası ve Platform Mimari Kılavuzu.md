NexusFlow derleyicisi (OCC), **.target** dosyalarını kaynak kodun hedef platforma (mimari ve işletim sistemi) uyumlu hale getirilmesi için bir "platform kılavuzu" olarak kullanır 1, 2\. Bu dosyadaki bilgiler, derleyicinin **kod üretimi (codegen)** ve **ara kod (IR) eşleme** süreçlerini doğrudan şu şekillerde etkiler:

### 1\. Yazmaç Tahsisi ve Yönetimi (Register Allocation)

Derleyicinin occ\_codegen.dll modülü, hedef mimarinin sunduğu fiziksel yazmaçları (registers) yönetmek için bu dosyayı referans alır:

* **Yazmaç Havuzu:** .target dosyasında tanımlanan registers listesi (örneğin; rax, rbx vb.), Graph Coloring algoritması tarafından değişkenlerin hangi fiziksel yazmaçlara atanacağını belirlemek için kullanılır 3, 4\.  
* **Özel Yazmaçlar:** scratch\_regs (geçici işlemler için) ve reserved\_regs (yığın işaretçisi rsp gibi dokunulmazlar) tanımları, derleyicinin hangi yazmaçları özgürce kullanabileceğini, hangilerini ise koruması gerektiğini söyler 4\.

### 2\. Çağrı Standartları ve ABI Uyumluluğu (ABI Convention)

Fonksiyonların birbirini nasıl çağıracağı ve veri alışverişi yapacağı ABI bilgileriyle şekillenir:

* **Parametre ve Dönüş Yazmaçları:** param\_regs parametrelerin hangi sırayla yazmaçlara dizileceğini, return\_reg ise sonucun hangi yazmaçta döneceğini (örn: Windows x64 için rax) belirler 5\.  
* **Shadow Space:** Windows gibi platformlar için gerekli olan 32-byte'lık shadow space alanı, yığın (stack) üzerinde derleyici tarafından otomatik olarak rezerve edilir 5\.  
* **Stack Alignment:** Yığının kaç byte'lık sınırlarla hizalanacağı (stack\_alignment: 16), fonksiyon çağrıları sırasında bellek hatalarının önlenmesi ve performans için kritik bir kuraldır 4\.

### 3\. Talimat Seti ve Donanım Optimizasyonu

Derleyici, donanımın yeteneklerine göre kod üretir:

* **SIMD Desteği:** Hedefin avx512 veya neon gibi vektör komut setlerini destekleyip desteklemediği bilgisi, derleyicinin otomatik vektörleştirme yapmasını sağlar 4\.  
* **Atomik İşlemler:** atomic \= true ayarı, \<\< (Zone Write) gibi operatörlerin doğrudan işlemci düzeyinde atomik komutlarla mı yoksa yazılımsal kilitlerle mi üretileceğini belirler 4-6.

### 4\. Nexus Primitiflerinin Assembly Eşlemesi

NexusFlow'a özgü bazı düşük seviyeli operatörler, .target dosyasındaki doğrudan assembly karşılıklarına dönüştürülür:

* **Relocate (\_\>) Operasyonu:** Sahiplik transferi mantığı olan bu operatör, .target dosyasındaki relocate\_op tanımına göre derlenir (örneğin; mov rdi, rsi; xor rsi, rsi komut dizisiyle veriyi taşır ve eski masayı sıfırlar) 5\.  
* **Handler Başlatma:** handler\_init talimatı ile masaların (h) bellek üzerindeki başlangıç durumu (örn: xor rax, rax) ayarlanır 5\.

### 5\. Çıktı Formatı ve Linker Ayarları

Nihai binary dosyasının yapısı da bu dosyaya bağlıdır:

* **Dosya Formatı:** Çıktının Windows için pe (.exe), Linux için elf veya bare-metal için ham binary mi olacağı burada belirlenir 5, 7\.  
* **Entry Point:** Programın başlangıç noktasının main, WinMain veya \_start (bare-metal) olacağı .target dosyasındaki entry\_point tanımıyla netleşir 5, 7\.

Özetle; derleyici, yüksek seviyeli NexusFlow kodunu platform bağımsız bir ara dile (Omni-IR) çevirdikten sonra, bu **.target** verilerini kullanarak kodu hedef donanımın anladığı en verimli makine diline ve ABI standartlarına döker 3, 8, 9\.  
