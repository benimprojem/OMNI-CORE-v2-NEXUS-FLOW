Omni-IR (OIR) aşamasındaki **DAG (Yönlendirilmiş Çevrimsiz Grafik)** yapısı, asenkron görevlerin ve veri akışının matematiksel bir modelini oluşturarak yarış koşullarını (race conditions) ve kilitlenmeleri (deadlock) derleme aşamasında veya IR seviyesinde engeller 1, 2\.  
Bu yapının yarış koşullarını önleme mekanizmaları şu temel unsurlara dayanır:

### 1\. Yürütme ve Veri Bağımlılıklarının Modellenmesi

OIR programı, düğümlerin (instructions) ve kenarların (dependencies) bulunduğu bir DAG olarak temsil edilir 1\.

* **Kontrol Kenarları (Control Edges):** Talimatlar arasındaki kesin yürütme sırasını belirler (örneğin; bir görev bitmeden done düğümü çalışamaz) 3\.  
* **Veri Kenarları (Data Edges):** Payload, slot\_id veya bellek konumları arasındaki bağımlılıkları temsil eder 3\. Bu sayede bir veri, ona bağlı olan işlem tamamlanmadan başka bir işlem tarafından tüketilemez 3\.

### 2\. Deterministik Öncelik Kuralları (DONE \> TIMEOUT)

DAG yapısı, aynı kaynak veya slot üzerinde yarışan işlemler için katı bir hiyerarşi tanımlar:

* **DONE Baskınlığı:** Bir slot için done (tamamlandı) ve timeout (zaman aşımı) düğümleri yarışırsa, **DONE düğümü her zaman kazanır** 2, 4\.  
* **CAS Mekanizması:** Zaman aşımı kontrolü (timeout\_node), yalnızca slot "RUNNING" durumundaysa başarılı olabilir 4\. Eğer done\_node daha önce çalışıp durumu değiştirdiyse, zaman aşımı işlemi etkisiz hale gelir 4, 5\.

### 3\. Atomik Operasyonlar ve Bellek Çitleri (Memory Fences)

DAG içindeki düğümler, bellek erişimlerini güvenli hale getirmek için atomik operasyonları (atomic\_node) içerir 2\.

* **Senkronizasyon:** done operasyonu, üretici ve tüketici arasındaki senkronizasyonu sağlamak için **release-acquire** bellek çitlerini kullanır 6\. Bu, done çağrıldığında tüm veri yazımlarının tüketici tarafından görünür olmasını garanti eder 6\.  
* **Kilitlenmesiz (Lock-free) Yapı:** Hazır haritasına (ready map) eklemeler gibi işlemler idempotent ve kilitlenmesizdir, bu da klasik kilit (lock) mekanizmalarından kaynaklanan yarış risklerini ortadan kaldırır 2\.

### 4\. Döngüsel Bağımlılıkların Engellenmesi

Yapının **"Acyclic" (Çevrimsiz)** olması, IR seviyesinde döngüsel bağımlılıkların oluşmasını imkansız kılar 2\. Bu özellik, görevler arasındaki karşılıklı bağımlılıklardan kaynaklanabilecek **ölümcül kilitlenmeleri (deadlocks)** ve kaynak açlığını (starvation) yapısal olarak engeller 2, 7\.

### 5\. Deterministik Tüketim Sırası

Graf, zamanlayıcının (scheduler) slotları hangi sırada tüketeceğini belirleyen bir akış sunar 2\. Tüketici düğümlerin yürütülmesi her zaman tanımlanmış öncelik katmanlarını takip eder, bu da belirsiz (non-deterministic) davranışların önüne geçer 2, 8\.  
Özetle; OIR'deki DAG yapısı, tüm asenkron işlemleri **belirlenmiş bir sıraya, atomik kurallara ve katı bir öncelik hiyerarşisine** bağlayarak sistemin yarış koşullarından arındırılmış ve deterministik bir şekilde çalışmasını sağlar 8, 9\.  
