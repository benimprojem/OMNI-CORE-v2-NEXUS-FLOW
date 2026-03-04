# OMNI CORE v2 NEXUS FLOW - STDLIBS REFERENCE GUIDE
**Version:** 1.0 (gama)  
**Architecture:** Nexus Flow & Handler-Oriented Memory  
**Compiler:** OCC (Omni Core Compiler)
**Language:** Nexus Flow

----

# STDLIBS - Kütüphaler


## Bölüm 1. CoreModule "Çekirdek modül"
** Genişletilmiş Fonksiyon listesini barındırır. Os bağımlıdır ve (CoreModule) otomatik yüklenir. 
** Ana kod sayfası başına ( no:CoreModule; ) kullanılarak iptal edilebilir. Iptal ettiğinizde: Nexus Flow Primordial Katmanı "core" Modülü yüklenir.

### Fonksiyon Listesi
#### I/O - girdi/çıktı.

* `echo(data)`, `echo("Sonuç: {data} /n")`: Herhangi bir veriyi standart çıktıya basar.
* `input("Bir sayı giriniz: ")`: Kullanıcıdan giriş yakalar.

> Styling System (Renkli Çıktı)
* Yerleşik Stiller: error (Kırmızı), warn (Turuncu), info (Mavi), success (Yeşil)
* Stil Tanımlama (Global Kapsamda yapılmalıdır) Opsiyoneldir.
```
style Dikkat = "\033[31;1m";  # Kırmızı ve Kalın
style Bilgi  = "\033[36m";    # Cyan

# Kullanım (İkinci parametre olarak stil adı veya ANSI kodu verilir) opsiyonel

println("Bu kritik bir hatadır!: {e}", Dikkat);
print("İşlem tamamlandı.", Bilgi);
print("Doğrudan ANSI kodu.", "\033[32m"); # Yeşil
print("Normal yazı:{val}");

# Dışarıdan sitil almayanlar

echo("Normal yazı:{val} /n");  # dışarıdan sitil almaz
eprint("[E10010] Hata:.... "); # error stili ile basar 
```

#### File dosya işlemleri.

* `file.open(path,mode)`:  (mode: r, w, rw, a)
* `file.create(path)`: 
* `file.exists(path)`: 
* `file.delete(path)`: 
* `file.copy(src, dest)`: 
* `file.read()`: 
* `file.read_all()`: 
* `file.write()`: 
* `file.write_bytes()`: 
* `file.seek()`: 
* `file.tell()`: 
* `file.size()`: 
* `file.flush()`: 
* `file.close()`: 
* `file.path(path)`:  
---

-# path="C:\mingw64\x86_64-w64-mingw32\include\sec_api\stdio_s.h"  
-/->  0[C:] 1[mingw64] 2[x86_64-w64-mingw32] 3[include] 4[sec_api]  5[stdio_s]  6[.h] 
-` v: fname = file.path(path).last(-1);` # stdio_s  # .start C:   .last .h

#### A. Bellek ve Bölge Yönetimi (Area & Zone)**

* `mem.area(size)`: Tekil mülkiyetli alan açar.
* `mem.zone(size)`: Paylaşımlı bölge açar.
* `mem.resize(h, new_size)`: Masadaki alanın boyutunu (mümkünse yerinde) değiştirir.
* `mem.copy(dest_h, src_h)`: Sadece zorunlu durumlarda (explicit) derin kopyalama yapar.
* `mem.stat(h)`: Alanın doluluk oranını ve bilgisini döner.
* `mem.peek(ptr, type)` / `mem.poke(ptr, val)`: Ham adrese erişim.
* `mem.offset(ptr, n)`: Adres aritmetiği.
* `mem.move(dest, src, size)`: Ham blok taşıma.


#### B. Veri Akışı ve Senkronizasyon (Piping)**

* `flow.push(h, data)`: `<<` operatörünün fonksiyonel karşılığıdır.
* `flow.pull(h)`: `>>` Masadan veri çeker (pop).
* `flow.sync(z)` / `flow.lock(z)`: Bir `zone` üzerindeki tüm bekleyen yazmaları fiziksel belleğe işler (Memory Barrier).
* `flow.delay(ms)`: Akışı asenkron olarak bekletir (OS'te `sleep`, Bare-metal'de `timer_wait`).

#### C. Donanım ve Sistem Arayüzü (Hardware & OS)**

* `sys.io_read(port)` / `sys.io_write(port, val)`: OS'da izinli sürücü erişimi.
* `sys.reg_r(name)` / `sys.reg_w(name, val)`: CPU register manipülasyonu.
* `sys.env(key)`: OS modunda ortam değişkenleri,
* `sys.exit(code)`: Programı güvenli kapatır.
* `sys.call(id, args)`: Doğrudan syscall/interrupt tetikleme.
* `sys.io_r/w(port)`: Donanım port erişimi.
* `sys.info(query_id)`: Donanım/İşletim sistemi hakkında ham bilgi (CPU core sayısı, sayfa boyutu vb.).
* `sys.yield()`: İşlemciyi gönüllü olarak bir sonraki iş parçacığına bırakır.


#### D. Kripto ve Güvenlik (Security Guard)**

* `crypto.hash(h, algorithm)`: Bir `area` veya `zone` içeriğinin anlık hash'ini alır.
* `crypto.verify(h, signature)`: İçeriği dijital imza ile doğrular.
* `crypto.seal(h)`: Bir alanı "read-only" (sadece okunur) hale getirir, değiştirilemezlik mührü basar.

#### E. Hata ve Tanı (Diagnostic)**

* `err.last()`: Akışta oluşan son hatanın detaylı objesini döner.
* `err.trace()`: `panic` durumunda register dump ve stack izini verir.
* `log.echo(msg)`: Standart çıktı (OS: `stdout`.

#### F. Zaman ve Hassas Ölçüm (Timing)**

* `time.ticks()`: İşlemcinin çevrim (cycle) sayısını döner (RDTSC benzeri).
* `time.now()`: Epok zamanı veya sistem çalışma süresi.


----

## Bölüm 2.  Senkron, Asenkron  flow Modülü :

### Nexus Flow ile Asenkron Programlama: Kullanım Rehberi ve Örnekler

`use flow;` 

#### 1. Temel Operatörler

* `<- spawn(block, mode, Timeout)`: Yeni bir hafif iş parçacığı (green thread) başlatır. 
* (- spawn(th1, async, 5000) - mode: async, sync - Fonksiyon veya Blok: (th1)>{...} )
* `(blok_name)>{...}`: thread blokları..

* `!listen(th, intr)` : Thread işini bitirene kadar bekle/dinle, sonucu al ve akışı başlat. ( baremetal.) 
* `listen(th, w_u_time)`: Thread işini bitirene kadar bekle/dinle, sonucu al ve akışı başlat. ( os bağımlı.)

* `>>` : Veriyi bir sonraki işleme atomik aktar.
* `<<` : Veriyi bir paylaşımlı belleğe (zone) atomik aktar.
* `?->` : Hata durumunda yapılacakları tanımla.
* `done(type, val)` : Thread işlemlerinin bittiğini bildir.
* `?;` : Masayı temizle ve kapat.

`(done())` 3 şey yapar: 
- 1: datayı 1024 byte lık alana yazar. 
- 2: datanın ne olduğunu rzv tablosuna yazar. 1:data 2: bellek adresi + boyut 3: dosya yolu. 
- 3: işlemlerin bittiğini listen e dinleme bayrağını 1 yaparak bildirir. 

Thread'ler arası güvenli veri transferi:
- **`send(target_h, data)`**: Veriyi kopyalamadan sahipliğini hedefe taşır.
- **`receive(source_h)`**: Hedef masadan veri gelene kadar akışı bekleme moduna alır.
- **`done(type, val, addr)`**: İşlem sonucunu Nexus Slot Matrix'e yazar ve `listen`'ı tetikler.


#### 2. Kullanım Örnekleri


**Örnek 1: İşçi Tarafında Veri Üretimi (Done Kullanımı)**

```
use flow;


f:sensor_reader(port, *d) {
    v:raw = sys.io_read(port);
    
    # İş bitti: Veri tipi 1: (Raw) Max: 1024byte. 2: Verinin bulunduğu adres ve boyutu 8+8byte. 3: dosya yolu.
    done(1, raw, *d); 
}

f:main()!i32 {
    v:data = area(1024);
    (h) <- spawn(sensor_reader(0xAF, *data), async, 500);
    
    # listen() sadece trigger_flag'e bakar, 
    # flag 1 olunca tabloyu tarayıp (h) slotunu bulur.
    listen(h, 10) >> (val) {
        echo("Sensör: {val.payload}");
    }?;
    
    return 0;
}

```

**Örnek 2: Bare-Metal Olay Döngüsü (Power Saving)**
Bare-metal modunda `!listen()`, `active_tasks` varsa ve `trigger_flag` sıfırsa işlemciyi uyutur.

```
no:CoreModule;
use flow;

f:kernel_main() {
    (t1) <- spawn(check_disk(), async, 0);
    
    # Burada CPU 'HLT' moduna geçer. 
    # Donanım kesmesi (IRQ) gelip 'done' tetikleyince uyanır.
    !listen(t1, 0) >> (data) {
        process(data);
    }?;
}

```

---

### **1️⃣ Bare-metal vs OS farkı**

| Özellik                         | Bare-metal                                                   | OS Bağımlı                                                     |
| ------------------------------- | ------------------------------------------------------------ | -------------------------------------------------------------- |
| Dinleme (listen)                | `!listen()` → CPU kesme veya flag ile uyanır                 | `listen()` → OS event loop veya thread scheduler ile uyanır    |
| Görev bitiş bildirimi           | `done()` genellikle opsiyonel, çünkü donanım flag’i tetikler | `done()` gerekli, OS’ye görev tamamlandı bilgisini vermek için |
| Bekleme sırasında CPU kullanımı | 0% (HLT veya WFI)                                            | Thread askıya alınır, OS scheduler devrede                     |
| Sinyal / trigger                | Donanım kesmesi veya trigger flag                            | OS signaling / futex / WaitOnAddress                           |

---

### **2️⃣ Akış Şeması Örneği**

```text
          Bare-metal                      OS Bağımlı
--------------------------------    -------------------------------
 spawn task()                       spawn task()
     |                                   |
     v                                   v
 !(listen) blok bekle                listen() blok bekle
     |                                   |
   CPU uyur                          OS thread askıya alınır
     |                                   |
 done() opsiyonel                       done() gerekli
     |                                   |
   Veri hazır                           Veri hazır
     |                                   |
     v                                   v
 İşlemci veya task devam eder          OS scheduler uyarılır
```

---

### **3️⃣ Mini Örnek Kod**

**Bare-metal:**

```nxf
v:buffer = area(1024);

f:task(mem_addr) {
    # veriyi üret
    poke(mem_addr, 42);
    # done() opsiyonel
}

(th)<-spawn(task(buffer), async, 0);

!listen(th, 10) >> (val) {
    echo("Veri: {val.payload}");
}?;
```

**OS tarafı:**

```nxf
v:buffer = area(1024);

f:task(mem_addr) {
    poke(mem_addr, 42);
    done(1, mem_addr);  # OS’ye işin bittiğini bildir
}

(th)<-spawn(task(buffer), async, 0);

listen(th, 10) >> (val) {
    echo("Veri: {val.payload}");
}?;
```

---

✅ **Özet:**

* Bare-metal `!listen()` ile **direkt donanım trigger**, `done()` çoğu zaman gereksiz.
* OS tarafında `done()` görev bitişini scheduler’a bildirir, `listen()` onu alır.

---


#### 3. Programcı İçin İpuçları

* **Bellek:** `free()` veya `delete` kullanmanıza gerek yoktur; `?;` operatörü sizin için masayı siler.
* **Güvenlik:** Bir masayı `_>` ile taşıdıktan sonra eski masaya erişmeye çalışırsanız, derleyici (OCC) size derleme anında hata verir.
* **Timeout:** Her zaman mantıklı bir `timeout` değeri verin. Monitor(listen), kilitlenen thread'lerin sistemi çökertmesini engeller.

----

## Bölüm 3. Multi Procesing  ipc modülü

Yapı,Mantık,İşleyiş
fork(),Süreç Klonlama,Mevcut masayı (context) kopyalayarak yeni bir PID oluşturur.
process(path),Bağımsız Süreç,Belirtilen binary'yi tamamen izole bir bellekte başlatır.
cpu.affinity,Çekirdek Kilidi,Süreci fiziksel bir CPU çekirdeğine mühürler.
IPC (Inter-Process),Masa Paylaşımı,zone üzerinden süreçler arası ışık hızında veri takası.

```
# Standart uygulamalarda bunlar yüklenmez
use flow; # Threading ve Asenkron yetenekleri ekle
use ipc;  # Pipe ve Process yetenekleri ekle

f:main() {
    # Flow modülünden spawn kullanımı
    (h) <- flow.spawn(worker_task(), async, 100);
    
    # IPC modülünden pipe kullanımı
    (p1) <- ipc.process("./other_app");
    pipe(current, p1);
}

```


```
use ipc;
use flow;

f:main() {
    # 1. Yeni bir bağımsız süreç başlat (Multi-Processing)
    # Bu, spawn'dan farklı olarak kendi bellek alanına (Address Space) sahiptir.
    (p1) <- process.start("./optimizer.nx");

    # 2. İşlemci çekirdeğine mühürle (Core Affinity)
    # CPU 0 ve 1'i bu sürece ayır.
    cpu.affinity(p1, [0, 1]);

    # 3. IPC (Inter-Process Communication)
    # 'zone' bellek alanı, süreçler arası ortak "Masa" görevi görür.
    (shared) <- mem.zone(4096);
    
    # Veriyi sürece gönder (Sahiplik devri değil, paylaşım)
    p1.attach(shared);

    # 4. Sürecin bitmesini dinle
    listen(p1, 5000) >> (exit_code) {
        echo("Süreç tamamlandı. Kod: {exit_code}");
    }?;
}
```

* pipe kullanımı:
```
f:main() {
    # 1. Süreçleri başlat
    (p1) <- process.start("./producer.nx");
    (p2) <- process.start("./consumer.nx");

    # 2. Arada bir PIPE (Boru Hattı) kur
    # Bu direktif, işletim sistemi seviyesinde bir I/O stream oluşturur.
    pipe(p1, p2);

    # 3. p1'den gelen veriyi p2'ye AKIT (Pipe üzerinden)
    # '>>' operatörü burada atomik bir 'bridge' görevi görür.
    (p1.output) >> (p2.input);

    # 4. Alternatif: Paylaşımlı Bellek (Zone) Kullanımı
    (shared) <- mem.zone(1024); # 1KB paylaşımlı alan
    
    # p1 veriyi yazar (Atomic Write)
    v:sensor_data = 0xFF;
    (shared) << sensor_data; # Paylaşımlı belleğe atomik yazma

    # p2 veriyi çeker (Flow Feed)
    v:target_data <- flow.pull(shared); # Zone'dan veri çekme
}

```

----
