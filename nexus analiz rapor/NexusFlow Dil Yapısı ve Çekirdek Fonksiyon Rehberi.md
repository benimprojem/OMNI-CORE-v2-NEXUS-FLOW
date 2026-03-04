NexusFlow dili, donanım seviyesinden yüksek seviyeli asenkron akışlara kadar geniş bir yelpazede fonksiyon ve metot yapıları sunar. Bu yapılar temel olarak **Primordial Core (OS Bağımsız)** ve **CoreModule (OS Bağımlı)** olarak iki ana kategoriye ayrılır 1, 2\.

### 1\. Metot Yapıları ve Organizasyon

NexusFlow'da fonksiyonlar ve metotlar modülerliği ve güvenliği sağlamak için özel yapılar altında toplanır:

* **Group Metodu:** Fonksiyonları bir "namespace" gibi organize eder 3\. Bir struct veya enum ile aynı isme sahipse nesne yönelimli (OOP) tarzda kullanılır ve ilk parametre olarak **self** alır 4, 5\.  
* **Çoklu Kural İşleyici (Multi-Rule Engine):** Fonksiyonlara apply rules ile enjekte edilen kurallar; bariyer (:=\>) veya tetikleyici (-\>) olarak çalışır 6, 7\.  
* **Fonksiyon Türleri:** Dilde normal fonksiyonlar (f:), dış modül fonksiyonları (exf:), isimsiz lambdalar (f:(a,b)\> a\*b) ve generic (f:sum(a\!t, b\!t)\!t) yapılar mevcuttur 8, 9\.

### 2\. Çekirdek (Core Zero) Fonksiyonlar (OS Bağımsız)

Hiçbir kütüphane gerektirmeden, bare-metal veya kernel seviyesinde çalışan en temel fonksiyonlardır 10, 11\.  
Kategori,Öne Çıkan Fonksiyonlar,İşlev Özeti  
Bellek & Pointer,"read\_u8, write\_u8, offset, copy\_raw","Ham bellek adreslerine erişim ve pointer aritmetiği 10, 12."  
Hafıza Yönetimi,"area(size), zone(size), free, resize","Tekil (area) veya paylaşımlı (zone) bellek blokları ayırır 1, 13."  
Donanım & CPU,"out\_b, in\_b, halt, cli/sti, cpuid","Port I/O işlemleri, işlemciyi durdurma ve kesme yönetimi 14, 15."  
Sistem & Akış,"jump, call\_raw, set\_ptr, yield","Mutlak adrese atlama, yığın pointer ayarlama ve gönüllü işlemci bırakma 16, 17."  
Kripto & Güvenlik,"hash, verify, seal, random","Bellek alanlarını mühürleme (read-only) ve dijital imza doğrulama 18, 19."

### 3\. CoreModule Fonksiyonları (OS Bağımlı)

İşletim sistemi yüklendiğinde otomatik olarak devreye giren ve OS API'lerini sarmalayan fonksiyonlardır 2\.

* **I/O İşlemleri:** echo (standart çıktı), input (kullanıcı girişi) ve gelişmiş **styling** (error, warn, info renkli çıktıları) 20\.  
* **Dosya Yönetimi (file):** open, create, read\_all, write\_bytes, seek ve path gibi kapsamlı dosya sistemi araçları 20\.  
* **Sistem Arayüzü (sys):** env (ortam değişkeni), exit (güvenli kapatma), call (doğrudan syscall) ve info (donanım/OS bilgisi) 21, 22\.  
* **Zaman ve Ölçüm (time):** ticks (çevrim sayısı), now (epok zamanı), sleep (bekleme) ve clock (yüksek çözünürlüklü ölçüm) 23, 24\.

### 4\. Gelişmiş Akış ve Senkronizasyon (Flow)

Asenkron programlama ve thread yönetimi için kullanılan metotlardır:

* **spawn() / done():** Yeni hafif iş parçacığı başlatma ve bitiş bildirimi 24, 25\.  
* **listen() / \!listen():** Thread bitene kadar bekleme; \!listen bare-metal kesmelerine, listen ise OS event loop'una bağlıdır 24, 26\.  
* **send() / receive():** Masalar üzerinden kopyalamasız (zero-copy) sahiplik transferi ve veri takası 25, 27\.  
* **sync() / lock():** zone üzerindeki verilerin çekirdekler arası tutarlılığını sağlar ve kritik bölgeleri kilitler 28, 29\.

**Özetle;** NexusFlow, donanım portlarına erişen inb/outb gibi en alt seviye komutlardan, dosya sistemini ve asenkron iş parçacıklarını yöneten yüksek seviyeli file ve flow modüllerine kadar bütünleşik bir API seti sunar 30, 31\.  
