### OIR (Omni Intermediate Representation) Mimarisi ve Teknik Spesifikasyon Raporu

#### 1\. OIR Giriş ve Tasarım Felsefesi

Omni Intermediate Representation (OIR), NexusFlow ekosisteminin düşük seviyeli yürütme katmanını hem işletim sistemi (OS) hem de  **Bare-metal**  hedefler için standartlaştıran donanım bağımsız bir ara dildir. OIR'nin temel tasarım felsefesi; yürütme süresince  **deterministik**  davranışı garanti altına almak, veriyi  **atomik**  operasyonlarla mühürlemek ve  **donanım bağımsızlığı**  sağlayarak aynı mantıksal akışın mikrokontrolcülerden bulut sunucularına kadar taşınabilmesini sağlamaktır. Sistem, kaynak yönetiminde "Zero-cost abstraction" prensibiyle çalışarak asenkron görevleri ve donanım kesmelerini O(1) karmaşıklığında yönetir.

#### 2\. Nexus Slot Matrix ve Durum Yönetimi

Sistem, her asenkron görevin durumunu ve metadata bilgisini merkezi bir  **Nexus\_Slot**  yapısı üzerinden takip eder. Önbellek hattı (cache-line) verimliliği için yapı 32-byte olarak hizalanmıştır.| Alan Adı | Açıklama / Boyut || \------ | \------ || **thread\_handle** | OS Tutamacı (HANDLE/TID) veya Bare-metal Stack Pointer (8 Byte) || **status** | Görev Durumu: 0 (EMPTY), 1 (RUNNING), 2 (DONE), 3 (TIMEOUT) (1 Byte) || **priority** | Görev önceliği (0-255) (1 Byte) || **timeout** | Milisaniye cinsinden zaman aşımı (0 \= Sonsuz) (2 Byte) || **payload\_ptr** | Veri veya alan işaretçisi (8 Byte) || **pldstatus** | Yük tipi: 1 (Raw), 2 (Pointer), 3 (File Path) (1 Byte) || **\_padding** | 32-byte hizalaması için ayrılmış boşluk (11 Byte) |

#### 3\. OIR Talimat Seti (Instruction Set)

OIR, asenkron görev yönetimi ve senkronizasyon için optimize edilmiş, platform backend'lerine doğrudan eşlenen (lowering) bir talimat setine sahiptir:

* **spawn slot\_id, func, arg, timeout** : Belirtilen slotu RUNNING durumuna getirir, giriş fonksiyonunu hazırlar ve zamanlayıcıya ekler.  
* **done slot\_id, type, payload** : Görevi tamamlandı olarak işaretler, tipi ve veriyi slota yazar ve hazır haritasını günceller.  
* **timeout\_check slot\_id** : Slotun süresini kontrol eder; dolmuşsa durumu atomik olarak TIMEOUT yapar.  
* **listen layer\_mask, max\_slots** : Belirtilen öncelik katmanlarını dinler ve hazır slotları tüketir (OS modunda kernel uyku desteğiyle).  
* **\!listen layer\_mask** : Bare-metal modunda donanım kesmesi (IRQ) veya WFI ile hazır slotları bekleyen yoklama mekanizması.  
* **atomic\_cas addr, old, new** : Bellek adresinde atomik "karşılaştır ve değiştir" işlemini yürütür.  
* **atomic\_fetch\_or addr, mask** : Hazır haritası güncellemeleri için adreste atomik "veya" işlemi yapar.  
* **advance\_timer level** : Hiyerarşik zaman çarkını (Timing Wheel) ilgili seviye için bir tick ilerletir.  
* **context\_save tcb** : Mevcut yazılım bağlamını Görev Kontrol Bloğu'na (TCB) saklar.  
* **context\_load tcb** : TCB üzerinden yeni bir bağlam yükleyerek yürütmeyi devam ettirir.  
* **yield** : Bare-metal hedeflerde işbirlikçi (cooperative) zamanlama için yürütmeyi gönüllü bırakır.

#### 4\. SSA (Static Single Assignment) Yapısı ve IR Grafiği

OIR programları, derleme aşamasında Yönlendirilmiş Devirsiz Grafik (DAG) yapısında temsil edilir. Her operasyonun SSA (Static Single Assignment) formunda olması, veri bağımlılıklarının statik olarak analiz edilmesini ve yarış durumlarının derleme anında tespit edilmesini sağlar.| Düğüm Tipi | Açıklama || \------ | \------ || **spawn\_node** | Görev oluşturma ve slot\_id üretim noktası. || **done\_node** | Görev tamamlama ve hazır haritası (Ready Map) güncelleme noktası. || **timeout\_node** | Zaman aşımı kontrolü; deterministik öncelik kurallarına bağlıdır. || **listen\_node** | Hazır slotların öncelik hiyerarşisine göre tüketimini temsil eder. || **atomic\_node** | Bellek bariyerleri ile korunan atomik operasyon düğümü. || **context\_node** | Bare-metal üzerinde yazılımsal bağlam değiştirme (save/load) noktası. || **yield\_node** | İşbirlikçi zamanlama için açık bırakma noktası. |

#### 5\. Deterministik Çalışma Prensipleri ve Yarış (Race) Çözümü

Sistem, asenkron yarış durumlarını (race conditions) çözmek için katı kurallar ve mimari bariyerler uygular:

* **DONE \> TIMEOUT Kuralı:**  Bir slot için done() ve timeout() aynı anda yarışırsa, RUNNING (1) \-\> DONE (2) geçişi atomik  **CAS (Compare-and-Swap)**  ile kontrol edilir. Eğer durum halihazırda DONE ise, zaman aşımı işlemi başarısız sayılarak görmezden gelinir.  
* **Bellek Çitleri (Memory Fences):**  done() çağrısı, verinin görünürlüğünü garanti etmek için "Release-Acquire" semantiği kullanır.  
* **Mimari Bazlı Bellek Sıralama:**  
* **x86 TSO (Total Store Order):**  Çoğunlukla sadece "Release Store" yeterlidir.  
* **ARM (Weak Ordering):**  DONE durumunun ve payload verisinin tüm çekirdeklerde tutarlı görülmesi için explicit  **DMB (Data Memory Barrier)**  talimatları enjekte edilir.

#### 6\. Hiyerarşik Zamanlama Çarkı (Hierarchical Timing Wheel) Mekanizması

Zaman aşımı yönetimi, O(1) karmaşıklığında çalışan çok seviyeli bir çark yapısı ile optimize edilmiştir.| Seviye | Tick Aralığı (Bucket) | Toplam Menzil || \------ | \------ | \------ || **Level 0** | 1 ms | 256 ms || **Level 1** | 256 ms | \~65 saniye || **Level 2** | \~65 saniye | \~4.6 saat |  
**Cascade Mekanizması:**  Bir üst seviyedeki (örneğin Level 1\) çark bir turu tamamladığında, o bucket içindeki görevler alt seviyeye (Level 0\) "re-hash" edilerek indirilir. Bu, binlerce zamanlayıcının aynı anda minimum CPU yüküyle takip edilmesini sağlar.

#### 7\. Kelime Boyutu Uyumlu (Word-size Adaptive) Ready Map

Hazır haritası, işlemcinin doğal kelime boyutuna (32/64-bit) göre dinamik olarak şekillenir. "Scanless" (taramasız) slot seçimi için iki katmanlı bir hiyerarşi kullanılır:

1. **global\_ready\_mask:**  Her bitin bir öncelik katmanını (priority layer) temsil ettiği merkezi maske. Zamanlayıcı önce bu maskeye bakarak hangi katmanlarda iş olduğunu O(1) sürede belirler.  
2. **ready\_page:**  Her katman içinde, slotların durumunu tutan bitmask sayfaları.  
3. **Hızlı Seçim:**  İşlemcinin donanımsal CLZ (Count Leading Zeros) veya BSF talimatları kullanılarak en yüksek öncelikli slot O(1) karmaşıklığında seçilir.

#### 8\. Platform Backend Karşılaştırması: OS vs. Bare-metal

OIR talimatları, hedef platformun yeteneklerine göre en verimli düşük seviye mekanizmalara "lower" edilir.| Özellik | OS Mode | Bare-metal Mode || \------ | \------ | \------ || **Bekleme Mekanizması** | WaitOnAddress (Win), futex (Lin), ulock\_wait (macOS) | HLT / WFI (Wait For Interrupt) || **Uyanma Kaynağı** | Yazılımsal done() sinyali ve Kernel uyarısı | Donanım Kesmesi (IRQ) ve Kesme Vektörü || **Zamanlayıcı** | OS Timer API (timerfd, SetWaitableTimer) | Donanım Timer (SysTick, PIT, HPET) || **Bağlam Yönetimi** | OS Scheduler (Preemptive) | Soft Context Switch (TCB \+ PendSV) |

#### 9\. Yumuşak Bağlam Değiştirme (Soft Context Switch) ve TCB

Bare-metal hedeflerde, RTOS gereksinimi olmadan çoklu görev yürütmek için Task Control Block (TCB) tabanlı yazılımsal bağlam değiştirme uygulanır:

1. **Kesme Tetikleme:**  Zaman aşımı veya yield ile donanım kesmesi (örn: ARM  **PendSV** ) oluşturulur.  
2. **Bağlam Saklama:**  Mevcut register seti (R0-R15, PSR) mevcut görevin yığınına itilir ve yığın işaretçisi (SP) TCB'ye kaydedilir.  
3. **Yeni Görev Seçimi:**  Ready Map üzerinden en yüksek öncelikli slotun TCB'si belirlenir.  
4. **Bağlam Yükleme:**  Yeni görevin SP'si yüklenir, register'lar yığından geri çekilerek yürütme devam ettirilir.

#### 10\. Çekirdek (Core) Fonksiyonlar ve Güvenlik Referansı

Hiçbir kütüphane gerektirmeyen primordial API'ler, Nexus (Nx) sözdizimi ile doğrudan donanım kontrolü sağlar.| Kategori | Fonksiyon İmzası (Nx Syntax) | Return | Açıklama || \------ | \------ | \------ | \------ || **Bellek** | core.ptr.read\_u8(addr\!u64)\!u8 | \!u8 | Belirtilen adresten 1 byte okur. || **Bellek** | mem.area(size\!u64)\!\*u8 | \!\*u8 | Tekil mülkiyetli ham bellek ayırır. || **Donanım** | core.cpu.out\_b(port\!u16, val\!u8)\!void | \!void | Donanım portuna byte gönderir. || **İşlemci** | core.cpu.halt()\!void | \!void | İşlemciyi durdurur (HLT/WFI). || **Güvenlik** | crypto.seal(h\!\*u8, size\!u64)\!void | \!void | Alanı "read-only" olarak mühürler. || **Akış** | flow.sync(z\!\*u8)\!void | \!void | Bekleyen yazmaları fiziksel belleğe işler. |

#### 11\. Risk Kontrol Listesi ve Güvenlik Önlemleri

Sistemin bütünlüğünü korumak adına tanımlanan kritik riskler ve OCC derleyici çözümleri:| Risk Alanı | Güvenlik Önlemi / Çözüm || \------ | \------ || **Handler (h) Yaşam Ömrü** | Masalar sadece tanımlandığı blokta (scope) geçerlidir; blok sonunda ?; ile otomatik imha edilir. || **ASM Register Spill & Clobber** | Geliştirici \# clobber önerisi sunar; ancak  **OCC Optimizer**  son kararı vererek register mapping ve spill yönetimini UB (Undefined Behavior) oluşmayacak şekilde otomatik yapar. || **Paylaşımlı Bellek (Zone) Yarışları** | zone erişimleri atomik operatörler (\<\<, \>\>) ve donanım seviyesi memory barrier'lar ile zorunlu olarak korunur. || **Dangling Pointers** | Donanım register erişimleri için güvenli offset kontrolleri ve mühürleme (crypto.seal) mekanizmaları uygulanır. || **Sahiplik Transferi** | \_\> (Relocate) operatörü kullanıldığında, veri eski blokta geçersiz kılınır; double-free ve eşzamanlı mutable erişim engellenir. |  
