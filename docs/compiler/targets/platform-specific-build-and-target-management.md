NexusFlow'da **\!\!=\[target="..."\]** direktifi, derleyiciye (OCC) belirli bir kod bloğunun yalnızca hedeflenen platform veya mimari için derlenmesi gerektiğini bildiren bir **koşullu derleme (conditional compilation)** aracıdır 1-3. Bu direktif, "15 Anahtar Operatör"den biri olup, .target sistemiyle entegre çalışarak platform bağımsız arayüzler oluşturulmasına olanak tanır 1, 3, 4\.  
Platforma özel kod yazma süreci ve kullanım yöntemleri şu şekildedir:

### 1\. Temel Sözdizimi ve Hedef Belirleme

Direktif, kod bloğunun başına eklenerek derleme aşamasında hangi platformun aktif olduğunu kontrol eder 1\. Yaygın kullanım şekilleri şunlardır:

* **\!\!=\[target="windows"\]**: Kodun yalnızca Windows işletim sistemi için derlenmesini sağlar 1\.  
* **\!\!=\[target="linux"\]**: Kodun yalnızca Linux sistemleri için derlenmesini sağlar 1\.  
* **\!\!=\[target="bare-metal"\]**: Kodun işletim sistemi olmayan, doğrudan donanım üzerinde çalışan (embedded/kernel) yapılar için derlenmesini sağlar 1\.

### 2\. Fonksiyon ve Grup İçinde Kullanım

Bu direktif genellikle farklı platformlarda farklı kütüphane çağrıları gerektiren durumları tek bir çatı altında toplamak için kullanılır 5\.

* **Platform Soyutlaması**: Geliştirici, dışa bağımlı fonksiyonları **exf:** ile tanımlayıp, platforma göre uygun olanı direktif yardımıyla tetikleyebilir 6\.  
* **Örnek Akış**:  
* pup f:uyari\_sesi(frekans\!u32) {  
*     \!\!=\[target="windows"\] \-\> Beep(frekans, 500); \# Windows API çağrısı \[6\]  
*     \!\!=\[target="linux"\]   \-\> snd\_beep(frekans);  \# Linux kütüphane çağrısı \[6\]  
* }

Bu yapı sayesinde kullanıcı işletim sistemi detaylarını bilmek zorunda kalmaz, sadece uyari\_sesi() fonksiyonunu çağırır 5, 6\.

### 3\. Makro Sistemi ile Entegrasyon

Makrolar içinde platforma özel kod blokları üretmek için de bu direktif kullanılır 7\. Örneğin, bir işletim sistemi köprüsü (**OS Bridge**) oluştururken makro şu şekilde yapılandırılabilir:  
:macro\! OS\_Bridge($name, $win\_fn, $lin\_fn) {  
    \!\!=\[target="windows"\] {  
        f:$name() { $win\_fn(); } \[7\]  
    }  
    \!\!=\[target="linux"\] {  
        f:$name() { $lin\_fn(); } \[7\]  
    }  
}

### 4\. Derleyici (OCC) İşleyişi

Derleme sırasında **OCC**, projenin hedef platformunu içeren .target dosyasını okur 8-10. Derleyici, semantik analiz aşamasında aktif hedefle eşleşmeyen \!\!=\[target="..."\] bloklarını eler (dead-code elimination) ve böylece nihai binary dosyası minimal ve platforma tam uyumlu kalır 8, 11\.  
**Özetle;** \!\!=\[target="..."\] direktifi, donanım register'larından OS API'lerine kadar her seviyede platform bağımlı kodları tek bir kaynak kod dosyasında yönetmenize ve **aynı kod tabanıyla farklı platformlar için çıktı üretmenize** imkan tanır 1, 12\.  
