ABI (Uygulama İkili Arayüzü) tanımları, bir fonksiyon çağrısı sırasında verilerin hangi yazmaçlar (registers) üzerinden aktarılacağını ve hangi yazmaçların korunması gerektiğini belirleyerek derleyicinin ve donanımın çalışma şeklini kesin kurallara bağlar 1, 2\. NexusFlow sisteminde bu kısıtlamalar şu mekanizmalarla yönetilir:

### 1\. Parametre ve Dönüş Yazmaçlarının Belirlenmesi

ABI, fonksiyonlara gönderilen argümanların ve fonksiyonun döndürdüğü sonucun hangi yazmaçlarda bulunması gerektiğini dikkate alır. Örneğin, **ms\_x64** (Windows) çağrı standardında:

* **Parametre Yazmaçları (param\_regs):** İlk dört parametre sırasıyla rcx, rdx, r8 ve r9 yazmaçlarına yerleştirilmelidir 3\. Bu, derleyicinin bu yazmaçları başka amaçlar için serbestçe kullanmasını kısıtlar; çünkü fonksiyon çağrısı yapılmadan önce bu yazmaçların argümanlarla doldurulması zorunludur 3, 4\.  
* **Dönüş Yazmacı (return\_reg):** Fonksiyonun sonucu her zaman rax yazmacında döndürülmelidir 3\. Bu durum, fonksiyonun sonlanmadan önce sonucunu bu spesifik yazmaca yazmasını şart koşar.

### 2\. Yazmaç Kategorizasyonu ve Koruma Kuralları

Target dosyalarında tanımlanan yazmaç havuzu, derleyicinin kod üretimi (codegen) sırasında hangi yazmacı ne zaman kullanabileceğini belirler:

* **Geçici Yazmaçlar (scratch\_regs):** r10 ve r11 gibi yazmaçlar geçici işlemler için ayrılmıştır 2\. Fonksiyon çağrısı yapıldığında bu yazmaçların değerlerinin korunması garanti edilmez, bu nedenle çağrıyı yapan (caller) tarafın bu verileri saklaması gerekir.  
* **Rezerve Yazmaçlar (reserved\_regs):** rsp (yığın işaretçisi) ve rbp (taban işaretçisi) gibi yazmaçlar "dokunulmaz" olarak kabul edilir 2\. ABI kuralları gereği, bu yazmaçların fonksiyon çağrısı boyunca yığının bütünlüğünü bozmayacak şekilde yönetilmesi zorunludur 1, 2\.

### 3\. Shadow Space ve Hizalama Kısıtlamaları

Windows x64 gibi bazı ABI'lar, yazmaç kullanımının yanı sıra yığın üzerinde **"shadow space"** (gölge alanı) adı verilen 32 byte'lık bir alanın rezerve edilmesini zorunlu kılar 2, 3\. Ayrıca, **stack\_alignment** (genellikle 16 byte) kuralı, fonksiyon çağrıları sırasında yığının nasıl hizalanacağını belirleyerek dolaylı olarak yazmaçlardan yığına veri boşaltma (spilling) işlemlerini etkiler 2\.

### 4\. Inline Assembly ve Clobber Listesi Yönetimi

**fastexec** blokları içinde yazılan satır içi assembly kodlarında, ABI kısıtlamaları derleyici optimizer'ı tarafından denetlenir:

* **Clobber Listesi:** Geliştirici, \# clobber: rax, rbx gibi komutlarla hangi yazmaçların kod içinde "kirletileceğini" (değerinin değiştirileceğini) bildirir 5, 6\.  
* **Otomatik Koruma:** Derleyici, ABI kurallarını ihlal etmemek için bu yazmaçları otomatik olarak yığına iter (push) ve işlem sonunda geri yükler (pop) 5, 7\. Eğer kullanıcı yanlış bir yazmaç eşlemesi yaparsa, optimizer ABI uyumluluğunu sağlamak için bu durumu düzeltebilir 7, 8\.

Özetle ABI tanımları; yazmaçları "parametre taşıyıcılar", "sonuç taşıyıcılar" ve "geçici depolar" olarak bölümlere ayırarak, derleyicinin rastgele yazmaç tahsisi yapmasını engeller ve platformlar arası uyumluluğu garanti altına alır 1, 4\.  
