OmniCore Derleyicisi (OCC), NexusFlow kaynak kodunu yüksek performanslı makine koduna dönüştürmek için modern derleme tekniklerini ve dile özgü "Flow" (akış) mantığını birleştiren çok aşamalı bir süreç kullanır 1, 2\.  
Derleyicinin işleyiş biçimi temel olarak şu aşamalardan oluşur:

### 1\. Ön Analiz (Lexical & Parsing)

* **Lexical Analysis (Lexer):** Kaynak kod, çok iş parçacıklı (multi-threaded) ve **SIMD destekli** bir tokenizasyon sürecinden geçer 3\. Bu aşamada yorum satırları ayrıştırılarak dokümantasyon belleğine alınır 3\.  
* **Parsing:** **LALR(1)** gramer yapısı kullanılarak kaynak koddan bir Soyut Sözdizimi Ağacı (**AST**) üretilir 1, 3\.

### 2\. Desugar (Sadeleştirme) Katmanı

NexusFlow'un yüksek seviyeli akış operatörleri (\<-, \-\>, ?-\>, \_\>), bu aşamada derleyicinin daha kolay işleyebileceği **"Core Language" (Çekirdek Dil)** yapılarına dönüştürülür 4, 5\.

* **Örnek:** Bir (h) \<- fn() (yakalama) işlemi, AST seviyesinde tmp \= fn(); h \= move tmp; gibi standart atama ve taşıma işlemlerine indirgenir 5\. Bu sayede karmaşık akış mantığı, SSA-friendly (Static Single Assignment) bir yapıya kavuşur 4, 5\.

### 3\. Semantik Analiz ve Güvenlik Denetimi

Bu aşamada kodun mantıksal doğruluğu denetlenir:

* **Sahiplik Denetimi (Ownership Check):** Masaların (handler) ve değişkenlerin ömürleri takip edilir 6, 7\. Bir masanın dolu olup olmadığı ("Table Occupied") veya bir verinin taşındıktan sonra eski yerinde kullanılıp kullanılmadığı burada kontrol edilir 7, 8\.  
* **Tip Çıkarımı:** Değişkenlerin tipleri (örneğin v:x \= 5 ifadesinden i32 olduğu) otomatik olarak belirlenir 6, 9\.

### 4\. Omni-IR (Ara Kod) Üretimi

Derleyici, mimariden bağımsız bir ara dil olan **Omni-IR (OIR)** üretir 1, 6\.

* **SSA Yapısı:** OIR, her değişkenin sadece bir kez atandığı **Static Single Assignment** yapısını kullanır 6, 10\.  
* **DAG Yapısı:** Program, yürütme ve veri bağımlılıklarını temsil eden bir **Yönlendirilmiş Çevrimsiz Grafik (DAG)** olarak modellenir 11\. Bu yapı, yarış koşullarını ve kilitlenmeleri derleme anında tespit etmeyi sağlar 11, 12\.

### 5\. Optimizasyon Katmanları

Kod, performans artırımı için üç seviyede optimize edilir 6:

1. **HLO (High-Level):** Satır içi genişletme (inlining), sabit katlama (constant folding) ve döngü açma (loop unrolling) 6\.  
2. **Middle-Level:** Ölü kod eleme (DCE) ve ortak alt ifade eleme 6\.  
3. **LTO:** Modüller arası çapraz optimizasyon 6\.

### 6\. Hedef Kod Üretimi (Codegen) ve Linker

* **Yazmaç Tahsisi (Register Allocation):** **Graph Coloring** algoritması kullanılarak, sanal değişkenler fiziksel işlemci yazmaçlarına en verimli şekilde atanır 6\.  
* **Target Sistemi:** Derleyici, **.target** dosyalarını okuyarak hedef mimarinin (x86, ARM, RISC-V vb.) yazmaç havuzu, yığın hizalaması (stack alignment) ve ABI kurallarına uygun makine kodu üretir 13-15.  
* **Linker (ocl.exe):** Fonksiyon seviyesinde bağlama (function-level linking) yaparak kullanılmayan kodları temizler ve nihai binary'yi (.exe, .so, .obj vb.) oluşturur 2, 13, 16\.

Derleyicinin bu aşamalı yapısını bir **İnfografik** veya daha detaylı bir **Özel Rapor** ile görselleştirmemi ister misiniz?  
