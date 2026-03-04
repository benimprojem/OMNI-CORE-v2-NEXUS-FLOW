NexusFlow’un asenkron görev yönetiminin kalbi olan **Nexus Slot Matrix**, deterministik ve kilitlenmesiz (lock-free) bir çalışma düzeni sağlamak için katı **atomik kurallar** üzerine inşa edilmiştir. Bu yapı, 32-byte hizalı slotlardan oluşur ve her bir görev için ayrılan bu alanlardaki durum değişimleri donanım seviyesindeki atomik talimatlarla yönetilir 1, 2\.  
Nexus Slot Matrix yapısındaki temel atomik işleyiş kuralları şunlardır:

### 1\. Durum Geçişlerinde CAS (Compare-and-Swap) Mekanizması

Slotların durumu (EMPTY, RUNNING, DONE, TIMEOUT) arasındaki geçişler asla doğrudan bir yazma işlemiyle değil, her zaman **atomik CAS** talimatı ile yapılır 3, 4\.

* **EMPTY → RUNNING:** Bir görev başlatılırken (spawn), boş bir slotun durumu 0'dan 1'e CAS ile çekilir. Bu, iki farklı işlemin aynı slotu kapmasını engeller 4\.  
* **Çift Bitiş Kontrolü:** Bir görevin hem done() ile bitmesi hem de aynı anda timeout olması durumunda yarış koşulunu (race condition) önlemek için CAS kullanılır. Sadece durumu RUNNING olan bir slot güncellenebilir 4, 5\.

### 2\. DONE \> TIMEOUT Öncelik Kuralı

Sistemin determinizmini sağlayan en kritik kural budur. Eğer bir görev başarıyla biterken (DONE) aynı anda zaman aşımına uğrarsa (TIMEOUT), **DONE durumu her zaman kazanır** 6, 7\.

* timeout mekanizması bir slotu TIMEOUT durumuna getirmeye çalıştığında, CAS işlemi mevcut durumun hala RUNNING olup olmadığını kontrol eder 4, 6\.  
* Eğer done() fonksiyonu durumu zaten DONE (2) olarak işaretlediyse, timeout CAS işlemi başarısız olur ve zaman aşımı etkisiz hale getirilir 6, 8\.

### 3\. Bellek Çitleri (Memory Fences) ve Görünürlük

"Durum DONE ise veri (payload) kesinlikle yazılmıştır" garantisini vermek için atomik işlemlerle birlikte bellek çitleri kullanılır 9, 10:

* **Release-Acquire Semantiği:** Veriyi üreten taraf (producer), payload'u yazdıktan sonra bir **Release Fence** (x86'da mfence, ARM'da dmb ish) uygular ve ardından durumu DONE yapar 3, 10\.  
* **Tüketici Tarafı:** Listener (listen), durumu DONE olarak gördüğünde bir **Acquire Fence** kullanarak payload'un tüm işlemci çekirdeklerinde güncel ve görünür olduğundan emin olur 10, 11\.

### 4\. Ready Map ve Counter Atomiği

* **Word-Size Adaptive Ready Map:** Bir görev tamamlandığında, o görevin hazır olduğu bilgisi atomic\_fetch\_or talimatı ile kelime boyutundaki (32/64 bit) bir bitmask haritasına işlenir. Bu, tarama işleminin O(1) karmaşıklıkla ve lock-free yapılmasını sağlar 12, 13\.  
* **Active Counter:** Aktif görev sayısı fetch\_add ve fetch\_sub fonksiyonları ile atomik olarak artırılıp azaltılır, böylece birden fazla thread güvenli bir şekilde merkezi tabloyu güncelleyebilir 3\.

### 5\. Lost Wake (Kayıp Uyanma) Koruması

OS modunda trigger\_flag atomik olarak güncellenir 3\. WaitOnAddress (Windows) veya futex (Linux) gibi mekanizmalar kullanılırken, adres izleme başlamadan hemen önce atomik bir kontrol yapılır. Eğer sinyal (trigger) uyanma öncesinde gelmişse, sistem uykuya geçmek yerine hemen akışı devam ettirerek deadlock oluşmasını engeller 5, 14\.  
