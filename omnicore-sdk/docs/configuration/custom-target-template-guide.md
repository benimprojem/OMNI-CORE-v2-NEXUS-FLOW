Evet, NexusFlow mimarisi geliştiricilerin farklı mimariler, işletim sistemleri veya özel donanımlar için kendi **.target** şablonlarını oluşturmalarına ve bunları derleyiciye tanıtmalarına tam destek verir 1-3. Sistem, yeni bir donanım çıktığında derleyiciyi değiştirmek yerine sadece yeni bir yapılandırma dosyası ekleyerek genişleyebilecek şekilde tasarlanmıştır 3, 4\.  
Özel bir .target şablonu oluşturmak için şu adımları izleyebilirsiniz:

### 1\. Dosya Konumlandırması

Yeni oluşturduğunuz şablonu (örneğin: ozel\_donanim.target), SDK dizini içerisinde yer alan **targets/** klasörüne kaydetmelisiniz 5, 6\. Derleyici (occ.exe) bu klasörü otomatik olarak tarayarak yeni hedefi listesine ekler 5\.

### 2\. Şablon İçeriği ve Zorunlu Alanlar

Bir .target dosyası, derleyicinin makine kodu üretirken ihtiyaç duyduğu kritik donanım bilgilerini içermelidir. Şablonunuzda şu alanların bulunması zorunludur:

* **arch**: İşlemci mimarisi (örn: x86\_64, armv7, riscv64) 4, 6\.  
* **abi**: Hedef işletim sistemi çağrı standardı (örn: win64, sysv) 4, 6\.  
* **pointer\_size ve stack\_alignment**: Bayt cinsinden bellek ve yığın hizalama değerleri 4, 6\.  
* **scalar\_regs ve vector\_regs**: Kullanılabilir toplam yazmaç (register) sayıları 4, 6\.  
* **os\_name**: Hedef ortam (örn: Windows, Linux, Bare-metal) 4, 6\.

### 3\. Gelişmiş Yapılandırma Seçenekleri

Şablonunuzu daha detaylı hale getirerek derleyici optimizasyonlarını artırabilirsiniz:

* **Donanım Özellikleri (features)**: SSE, AVX veya NEON gibi işlemciye özel yetenekleri listeleyebilirsiniz 4, 7\.  
* **Sistem Desteği**: atomic\_support (atomik işlem desteği) ve std\_lib\_available (standart kütüphane varlığı) gibi parametrelerle derleyicinin davranışını belirleyebilirsiniz 4, 8, 9\.  
* **Nexus Primitifleri**: relocate\_op veya handler\_init gibi düşük seviyeli NexusFlow işlemlerinin hedef mimarideki assembly karşılıklarını tanımlayabilirsiniz 10\.

### 4\. Kullanım ve Derleme

Oluşturduğunuz özel şablonu kullanmak için derleme komutuna \-t parametresini eklemeniz yeterlidir:occ \-t ozel\_donanim.target main.nx 11, 12\.  
Bu yapı sayesinde, yazdığınız **aynı kaynak kodu** hiçbir değişiklik yapmadan sadece farklı .target şablonları seçerek Windows, Linux veya bare-metal bir Raspberry Pi gibi tamamen farklı sistemlerde derleyebilirsiniz 13, 14\. İlerleyen aşamalarda OmniCore Editör'ün (OCE), bu dosyaları form tabanlı olarak otomatik oluşturacak bir **Target Wizard** sistemine sahip olması planlanmaktadır 11, 15\.  
