Bare-metal projeler için özel bir **.target** yapılandırması oluşturmak, OmniCore Derleyicisi'nin (OCC) donanımı doğrudan yönetebilmesi için gerekli olan mimari, bellek ve sistem parametrelerinin tanımlanmasını içerir 1, 2\. Bu süreç, işletim sistemi bağımlılıklarını devre dışı bırakıp donanım seviyesindeki primitifleri tanımlayarak gerçekleştirilir 3, 4\.  
Aşağıdaki adımları izleyerek bare-metal hedefinizi yapılandırabilirsiniz:

### 1\. Target Dosyasını Oluşturma ve Konumlandırma

Tüm yapılandırma dosyaları SDK dizini içerisindeki **targets/** klasöründe bulunmalıdır 5, 6\. Derleyici bu klasörü otomatik olarak tarar 6\. Yeni bir dosya oluşturun (örneğin: my\_embedded\_chip.target) 6, 7\.

### 2\. Temel Mimari ve Donanım Bilgilerini Tanımlama

Dosyanın başında işlemcinin fiziksel özelliklerini belirtmeniz gerekir:

* **arch ve bits:** İşlemci mimarisi (ör. armv7, riscv64) ve pointer boyutu (32/64 bit) 8, 7\.  
* **endian:** Bellek sıralaması (little veya big) 8, 9\.  
* **registers:** Kod üretimi (codegen) sırasında kullanılacak genel amaçlı yazmaçların listesi 9\.  
* **stack\_alignment:** Yığın hizalaması; donanım uyumluluğu için kritiktir (ör. 8 veya 16 byte) 8, 9\.

### 3\. Bare-Metal Özelliklerini Yapılandırma

İşletim sistemi olmayan bir ortam için aşağıdaki parametreler zorunludur:

* **os\_name:** Bu alan **"Bare-metal"** olarak ayarlanmalıdır 10, 7\.  
* **std\_lib\_available:** Standart kütüphane desteği olmadığı için **false** yapılmalıdır 10, 7\.  
* **entry\_point:** Programın başlangıç noktası genellikle \_start veya reset\_vector olarak tanımlanır 3, 6\.  
* **syscall\_support:** Bare-metal sistemlerde sistem çağrısı yapılamayacağı için **false** olmalıdır 10, 7\.

### 4\. Nexus Primitiflerini Eşleme

NexusFlow'un masa (handler) mantığının donanım seviyesinde nasıl çalışacağını belirleyen **nexus\_primitives** bölümünü doldurmalısınız 11\. Örneğin:

* **handler\_init:** Bir masanın (h) bellekteki başlangıç durumunu belirleyen assembly komutu 11\.  
* **relocate\_op:** Sahiplik transferi (relocate) sırasında verinin bir yazmaçtan diğerine taşınmasını ve eski yazmacın sıfırlanmasını sağlayan komut dizisi 11\.

### 5\. Örnek Bare-Metal Yapılandırma Dosyası

Kaynaklardaki spesifikasyonlara göre tipik bir bare-metal yapılandırması şuna benzer:  
\[target\_info\]  
identifier \= "arm-cortex-m4-bare"  
os\_name \= "Bare-metal"

\[cpu\_arch\]  
family \= "arm"  
bits \= 32  
registers \= \["r0", "r1", "r2", "r3", "r4", "r5"\] \# Havuz \[9\]  
stack\_alignment \= 8 \# \[8\]

\[instruction\_set\]  
atomic \= true \# Donanım atomik komut desteği varsa \[9\]  
std\_lib\_available \= false \# OS kütüphanelerini devre dışı bırakır \[7\]

\[output\_format\]  
binary\_type \= "bin" \# Ham binary çıktı \[3\]  
entry\_point \= "reset\_vector" \# \[3\]

\[nexus\_primitives\]  
relocate\_op \= "mov r1, r0; mov r0, \#0" \# Sahiplik devri assembly karşılığı \[11\]

### 6\. Derleme ve Kullanım

Oluşturduğunuz yapılandırmayı kullanmak için derleyiciyi **\-t** parametresiyle tetikleyin:occ \-t arm-cortex-m4-bare.target main.nx 12, 13\.  
Bu yapılandırma seçildiğinde, derleyici **listen()** (OS tabanlı) yerine donanım kesmelerini (IRQ) kullanan **\!listen()** mekanizmasını etkinleştirir ve kod üretimini doğrudan donanım portlarına (inb/outb) ve fiziksel adreslemeye (peek/poke) göre optimize eder 14, 15, 3\.  
