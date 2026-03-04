NexusFlow derleyicisi (OCC) için hazırlanan **.target** dosyalarında, ABI (Application Binary Interface) ve Calling Convention (Çağrı Standardı) bilgileri, fonksiyonların birbiriyle nasıl haberleşeceğini ve verilerin yazmaçlar (registers) ile yığın (stack) arasında nasıl paylaştırılacağını belirler 1, 2\.  
Bu bilgiler temel olarak **\[abi\_convention\]** başlığı altında ve genel hedef bilgilerinde şu parametrelerle tanımlanır:

### 1\. \[abi\_convention\] Bölümü İçeriği

Bu bölümde, hedef platformun spesifik çağrı kuralları detaylandırılır:

* **name**: Kullanılan standardın adını belirtir (Örn: Windows için "ms\_x64", Linux/Unix için "sysv") 2\.  
* **return\_reg**: Fonksiyon dönüş değerinin hangi yazmaçta saklanacağını tanımlar (Örn: x64 mimarisinde genellikle "rax") 2\.  
* **param\_regs**: Fonksiyon parametrelerinin hangi sırayla hangi yazmaçlara dizileceğini belirten bir listedir (Örn: Windows x64 için \["rcx", "rdx", "r8", "r9"\]) 2\.  
* **shadow\_space**: Özellikle Windows x64 mimarisinde her fonksiyon çağrısı için yığında (stack) rezerve edilmesi gereken 32-byte'lık alanı ifade eder 2\.

### 2\. Genel ABI Tanımları

Hedef dosyasının mimari ve sistem kısımlarında da ABI'yı etkileyen şu parametreler yer alır:

* **abi**: Genel çağrı standardı kategorisini belirler (Örn: win64, linux64, bare-metal, sysv) 3\.  
* **stack\_alignment**: Yığının kaç byte'lık sınırlarla hizalanması gerektiğini belirler (Örn: 16 byte). Bu, ABI uyumluluğu ve işlemci performansı için kritiktir 4\.  
* **pointer\_size**: Bellek adreslemesinin kaç bit (32 veya 64 bit) üzerinden yapılacağını tanımlar 3, 5\.

### Örnek Tanımlama (x86\_64-windows.target)

Kaynaklarda sağlanan örnek bir Windows yapılandırması şöyledir:  
\[abi\_convention\]  
name \= "ms\_x64"  
shadow\_space \= 32   \# Windows için zorunlu alan  
return\_reg \= "rax"  
param\_regs \= \["rcx", "rdx", "r8", "r9"\]  
Bu tanımlamalar sayesinde derleyici, bir fonksiyon çağrısı yaparken parametreleri doğru yazmaçlara yerleştirir, gerekli yığın alanını ayırır ve sonucu doğru yerden okur 2, 6\. Bare-metal (OS bağımsız) projelerde ise bu alanlar donanım kesme (interrupt) tablolarına veya özel sürücü gereksinimlerine göre özelleştirilebilir 7, 8\.  
