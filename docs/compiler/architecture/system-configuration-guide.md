OmniCore Derleyicisi (**OCC**) ve beraberindeki dosya sistemi, endüstriyel standartlarda, deterministik ve modüler bir geliştirme ortamı sağlamak üzere tasarlanmıştır 1, 2\. Aşağıda, OCC'nin teknik mimarisi ve SDK/Proje düzeyindeki klasör yapısı detaylandırılmıştır.

### 1\. OCC Derleyici Mimarisi ve İşleyişi

OCC, kaynak kodu makine koduna dönüştürürken çok aşamalı ve modüler bir boru hattı (pipeline) izler 1, 3:

* **Lexical Analysis (Lexer):** Çok iş parçacıklı ve **SIMD destekli** tokenizasyon yapar; yorumları ayıklayarak dokümantasyon belleğine alır 4\.  
* **Parsing (AST Üretimi):** **LALR(1)** gramer yapısını kullanarak kaynak koddan Soyut Sözdizimi Ağacı (AST) oluşturur 4\.  
* **Semantik Analiz ve Tip Kontrolü:** Kapsam çözünürlüğü (scope resolution), **sahiplik denetimi** (ownership check) ve tip çıkarımı (type inference) bu aşamada gerçekleştirilir 5\.  
* **Omni-IR (Ara Kod):** Mimariden bağımsız, **SSA (Static Single Assignment)** tabanlı bir ara dil üretilir 5, 6\. Bu yapı, kilitlenmeleri ve yarış koşullarını engelleyen bir **Yönlendirilmiş Çevrimsiz Grafik (DAG)** olarak modellenir 7, 8\.  
* **Optimizasyon Katmanları:** Kod; satır içi genişletme (Inlining), sabit katlama (Constant Folding) ve ölü kod eleme (DCE) gibi yüksek ve orta seviyeli optimizasyonlardan geçer 5\.  
* **Code Generation (Codegen):** Hedef mimariye özgü komut seçimi ve yazmaç tahsisi (**Graph Coloring** algoritması ile) yapılır 5\.  
* **Linker (ocl.exe):** Fonksiyon düzeyinde bağlama (Function-Level Linking) yaparak kullanılmayan kodları (Dead-Code) temizler ve nihai binary'yi oluşturur 9, 10\.

### 2\. SDK (Sistem) Klasör Yapısı

OmniCore SDK kurulumu, derleyici araçlarını ve kütüphaneleri merkezi bir yapıda organize eder 3, 11:

* **/bin (Araçlar):** Tüm çalıştırılabilir araçları barındırır.  
* occ.exe: Derleyici orkestratörü (frontend) 3, 11\.  
* ocm.exe: Paket ve bağımlılık yöneticisi 3, 11\.  
* ocl.exe: Linker (backend) 3, 11\.  
* ocvm.exe: JIT / Hızlı test aracı 3, 11\.  
* oce.exe: OmniCore IDE / Editör 3\.  
* **/lib (Modüller):** Derleyicinin modüler bileşenlerini (DLL/so) içerir.  
* occ\_parser.dll, occ\_analyzer.dll, occ\_opt.dll, occ\_codegen.dll 3, 12\.  
* **/stdlib (Standart Kütüphane):** core, io, net ve sys gibi OS bağımsız API modüllerini barındırır 11, 13\.  
* **/targets (Hedefler):** Farklı mimariler (x86, ARM, RISC-V vb.) için platform tanımlarını içeren **.target** dosyalarını tutar 13-15.

### 3\. Proje Klasör Yapısı

Yeni bir proje başlatıldığında (ocm.exe create-project), aşağıdaki deterministik yapı oluşturulur 1, 14, 16:

* **src/:** NexusFlow kaynak kodları (\*\* .nx\*\*) burada saklanır 14, 16\.  
* **include/:** Dış kütüphane başlık dosyaları (.h, .oci) için ayrılmıştır 14\.  
* **build/:** Derleme çıktıları (.ocb, .exe, .dll) ve debug haritaları (.ocmap) bu dizine çıkarılır 10, 14, 16\.  
* **dpslib/:** Projenin kullandığı **paylaşımlı bağımlılıkların** deposudur; bir kez yüklenen paketler tüm projelerde tekrar kullanılabilir 10, 11, 14, 17\.  
* **assets/:** Görseller ve stil dosyaları gibi statik kaynaklar burada yer alır 14\.

### 4\. Dosya Tipleri Referans Tablosu

Uzantı,Tanım ve Görev  
.nx,NexusFlow ana kaynak kod dosyası 13\.  
.ocf,"Proje metadata ve bağımlılık tanımları 13, 16."  
.nxr,"Derleme direktiflerini ve adımlarını yöneten build dosyası 13, 18."  
.ocb,"Derlenmiş ara kod veya platforma özel nihai binary 10, 13."  
.target,"Hedef platformun mimari ve ABI (yazmaç seti, hizalama vb.) bilgilerini tutan yapılandırma dosyası 13-15."  
.oci,Arayüz (Interface) tanımlama dosyası 13\.  
.ocmap,"Hata ayıklama için oluşturulan kaynak haritası 13, 16."  
Bu yapı, NexusFlow'un **"bir kez yaz, her yerde derle"** felsefesini destekleyerek hem Bare-metal hem de OS tabanlı projelerin aynı iş akışı üzerinden yönetilmesini sağlar 17, 19\.  
