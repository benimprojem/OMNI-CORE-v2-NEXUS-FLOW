NexusFlow sisteminde Windows ve Linux için ABI (Uygulama İkili Arayüzü) tanımları, derleyicinin (OCC) hedef platforma özgü kod üretmesini sağlayan **.target** dosyaları üzerinden belirlenir 1, 2\. Bu iki platform arasındaki temel farklar çağrı standartları, dosya formatları ve sistem düzeyindeki çalışma mekanizmalarında yoğunlaşmaktadır.  
İşte kaynaklar doğrultusunda Windows ve Linux ABI tanımları arasındaki temel farklar:

### 1\. Çağrı Standartları (Calling Conventions)

Windows ve Linux, fonksiyon parametrelerini yazmaçlara (registers) dağıtırken farklı kurallar izler:

* **Windows (ms\_x64):** İlk dört parametre için sırasıyla rcx, rdx, r8 ve r9 yazmaçlarını kullanır 3\. En kritik farklardan biri, her fonksiyon çağrısı için yığında (stack) rezerve edilmesi zorunlu olan **32-byte'lık "shadow space" (gölge alanı)** kullanımıdır 3\. Dönüş değeri genellikle rax yazmacında tutulur 3\.  
* **Linux (sysv):** POSIX standartlarını takip eden bu yapı, genellikle Windows'tan farklı bir yazmaç dizilimi ve yığın hizalaması kullanır 4\.

### 2\. Dosya Formatları ve Uzantıları

Derleme çıktılarının yapısı ve uzantıları platforma göre değişir:

* **Windows:** Nihai ikili dosya formatı **PE (Portable Executable)** iken; nesne dosyaları .obj, dinamik kütüphaneler ise .dll uzantısını alır 3, 5\.  
* **Linux:** Nihai çıktı formatı **ELF**'dir; nesne dosyaları için .o, paylaşımlı kütüphaneler için ise .so uzantısı kullanılır 5, 6\.

### 3\. Giriş Noktaları (Entry Points)

Programın yürütülmeye başladığı ilk adres tanımı platform bazlıdır:

* **Windows:** Giriş noktası genellikle WinMain olarak tanımlanır 1, 3\.  
* **Linux:** Standart giriş noktası main fonksiyonudur 1\.

### 4\. Sistem Çağrıları ve Senkronizasyon

NexusFlow runtime'ının asenkron görevleri yönetirken kullandığı düşük seviyeli API'ler farklılık gösterir:

* **Windows:** Thread oluşturma için CreateThread, adres izleme ve uyku modu için **WaitOnAddress** API'lerini kullanır 7-9.  
* **Linux:** Benzer işlemler için pthread\_create ve kernel seviyesinde uyku sağlayan **futex (FUTEX\_WAIT)** sistem çağrılarını temel alır 4, 7, 8\.

### 5\. Bellek Yönetimi ve Standart Kütüphaneler

* **Windows:** Temel bellek tahsisi için HeapAlloc, VirtualAlloc veya msvcrt (C runtime) üzerinden malloc kullanılır 9\.  
* **Linux:** Bellek yönetimi genellikle POSIX uyumlu malloc veya doğrudan mmap sistem çağrıları ile gerçekleştirilir 4\.

### 6\. Platform Soyutlaması ve Direktifler

NexusFlow geliştiricileri, bu ABI farklarını yönetmek için **\!\!=\[target="..."\]** direktifini kullanarak platforma özel kod blokları yazabilirler 10\. Örneğin, bir ses çalma fonksiyonu Windows'ta kernel32 kütüphanesindeki Beep fonksiyonuna bağlanırken, Linux'ta asound kütüphanesine yönlendirilebilir 10\. Derleyici, .target dosyasındaki bilgilere bakarak doğru ABI kurallarını ve kütüphane bağlantılarını (link) otomatik olarak uygular 1, 3\.  
