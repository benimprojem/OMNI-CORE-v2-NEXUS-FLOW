NexusFlow’un Omni-IR (OIR) katmanı, **%0 CPU kullanımı** hedefine ulaşmak için modern işletim sistemi çekirdeklerinin sunduğu askıya alma mekanizmalarını ve işlemci mimarilerinin düşük güç tüketimi talimatlarını kullanarak aktif bekleme (busy-waiting) döngülerini ortadan kaldırır 1, 2\.  
Bu hedefe ulaşılmasını sağlayan temel optimizasyon mekanizmaları şunlardır:

### 1\. İşletim Sistemi Seviyesinde Uyku (OS Mode)

OIR, yüksek seviyeli asenkron akışları derlerken, bekleme anlarını işletim sisteminin planlayıcısına (scheduler) devreden spesifik sistem çağrılarına dönüştürür 3, 4:

* **Windows:** WaitOnAddress API'si kullanılır 3\. Bu fonksiyon, belirli bir bellek adresindeki (slotun durum alanı) değer değişene kadar ilgili iş parçacığını (thread) tamamen askıya alır 5\.  
* **Linux:** futex (FUTEX\_WAIT) sistem çağrısı enjekte edilir 3, 4\. Kernel seviyesinde sağlanan bu uyku modu, CPU'nun ilgili görev için döngü yapmasını engelleyerek kaynak tüketimini sıfıra indirir 6\.  
* **macOS:** ulock\_wait veya benzeri sistem seviyesi kilitleri ile iş parçacığı askıya alınır 3, 6\.

### 2\. Donanım Seviyesinde Uyku (Bare-Metal Mode)

İşletim sistemi olmayan (kernel/embedded) ortamlarda, OIR doğrudan işlemciyi durduran düşük seviyeli assembly talimatlarını kullanır 7, 8:

* **x86 Mimarisi:** HLT (Halt) talimatı kullanılır 6, 7\. İşlemci, bir donanım kesmesi (IRQ) gelene kadar tamamen durur ve bu süreçte %0 CPU tüketimi ile bekler 2, 8\.  
* **ARM Mimarisi:** WFI (Wait For Interrupt) talimatı devreye girer 7, 9\. İşlemci düşük güç moduna geçer ve sadece bir sinyal (SEV) veya kesme ile uyanır 7, 10\.

### 3\. OIR Lowering ve Tetikleme Mekanizması

OIR, bu uyuma ve uyanma sürecini **Nexus Slot Matrix** adı verilen merkezi bir tablo üzerinden yönetir 1, 11:

* **done() Tetiklemesi:** Bir görev bittiğinde çağrılan done talimatı, slotun durumunu günceller ve trigger\_flag alanını değiştirir 3, 12\.  
* **Wake-up (Uyandırma):** OS modunda WakeByAddressSingle (Windows) veya futex(FUTEX\_WAKE) (Linux) çağrıları ile uyuyan iş parçacığı uyandırılır 13\. Bare-metal modda ise donanım kesmesi (IRQ) işlemciyi otomatik olarak uyandırarak tablonun tekrar taranmasını sağlar 4, 10\.

### 4\. Akış Optimizasyonu

* **O(1) Karmaşıklık:** OIR, her tick'te tüm görevleri taramak yerine **Word-Size Adaptive Ready Map** ve **Hiyerarşik Zamanlama Çarkı (Timing Wheel)** kullanır 14, 15\. Bu, CPU'nun uyanık kaldığı sürede yapacağı iş miktarını minimize ederek genel verimliliği artırır 16, 17\.  
* **Belirlenmiş Öncelik:** DAG (Yönlendirilmiş Çevrimsiz Grafik) yapısı sayesinde veri bağımlılıkları netleştirilir ve CPU'nun gereksiz yere uyanması veya kilitlenmesi (deadlock) engellenir 18, 19\.

Özetle; NexusFlow, işlemciyi bir döngüde çalıştırmak yerine, iş bittiğinde onu **donanım veya kernel seviyesinde uyutarak** sadece yeni bir olay gerçekleştiğinde uyanmasını garanti eder 20, 21\.  
