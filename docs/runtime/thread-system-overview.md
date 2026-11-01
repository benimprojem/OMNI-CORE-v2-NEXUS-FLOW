# Thread Sistemi
#### 1. Merkezi Rezervasyon Tablosu (Nexus Slot Matrix)

Sistem, bellek üzerinde sabit, 32-byte hizalı bir tablo (`Nexus_Slot`) kullanır. Bu tablo, asenkron görevlerin durumunu takip eden tek otoritedir.

| Alan | Boyut | Açıklama |
| --- | --- | --- |
| `thread_handle` | 8 Byte | Windows: `HANDLE`, Linux: `TID`, Bare-metal: `Stack Pointer`. |
| `status` | 1 Byte | 0: Boş, 1: Çalışıyor, 2: Bitti, 3: Zaman Aşımı. |
| `priority` | 1 Byte | Görev önceliği (0-255). |
| `timeout` | 2 Byte | Cellat süresi (ms). 0 = Sonsuz. |
| `payload_ptr` | 8 Byte | 1024-byte Area bellek adresi. |
| `pldstatus` | 1 Byte | 1: Ham Veri, 2: Pointer, 3: Dosya Yolu. |
| `_padding` | 11 Byte | 32-byte Cache-line hizalaması için boşluk. |

#### 2. İşletim Sistemi ve Donanım Adaptasyonu

Derleyici (OCC), `spawn` ve `!(listen)` komutlarını hedef platforma göre şu şekilde derler:

* **Windows Modu:** `WaitOnAddress` API'sini kullanır. Bir bellek adresi (slotun status alanı) değişene kadar thread'i tamamen askıya alır. CPU kullanımı %0'dır.
* **Linux Modu:** `futex (FUTEX_WAIT)` sistem çağrısını kullanır. Benzer şekilde kernel seviyesinde uyku sağlar.
* **macOS Modu:** `ulock_wait` (Private API) veya sistem seviyesi kilitleri kullanır.
* **Bare-Metal Modu:** Hiçbir kütüphane kullanmaz. `asm { hlt }` ile işlemciyi durdurur, Donanım Kesmesi (Interrupt) gelince uyanıp tabloyu tarar.


---


## Derleyici (OCC) İçin "Listen" Optimizasyonu

x86/64 Mimarisi: PAUSE komutu (İşlemciye "bekle" sinyali verir, ısınmayı önler).

Bare Metal: Doğrudan WFI (Wait For Interrupt) veya en hızlı döngü.


### 1. CPU Dostu Çözüm (Wait-On-Address) donanım seviyesi baremetal
Bu tek thread'in de sürekli dönmemesi için modern işlemcilerin ve işletim sistemlerinin sunduğu "Wait-On-Address" (Adres İzleme) API'lerini kullanabiliriz.

Windows: WaitOnAddress fonksiyonu, bir bellek adresi değişene kadar thread'i tamamen askıya alır.

Linux: futex (Fast Userspace Mutex) ile benzer bir uyku modu sağlanır.

triger_flag ı izler ve değişiklik olursa listen uyanır.

Bu sayede, tek thread olmasına rağmen "bekleme" anında işlemci kullanımı sıfır olur. Sadece bir asenkron görev bayrağı değiştirdiğinde, CPU uyanır ve !listen bloğunu tetikler.



### 2. Akışın Basitliği (The Programmer's Power)

Programcı `!listen()` bloğunu yazdığında artık ne yapacağını biliyor:

```oc
v:nexus = area(1024); # 1024byte lık alanı otomatikleştirebilirmiyiz !!! 

download_task(down_adres, *mem_addr){
    ...
    ....
    
    done(3, path, mem_addr);  
    # done: payload_ptr(mem_addr) (1024 byte) adresine path ı yazar.
    # Rezervasyon Tablosunda (status) ü (2 bitti ) yapar. (pldstatus) ü 3 yapar.
    # Listen_Triger da triger_flag 0 ise 1 yapar. 
    # En son da Thread i kapatır.
}
done için Veri tipi 
**1: (Raw) Ham veri. Max: 1024byte. 
**2: Verinin bulunduğu adres ve boyutu 8+8byte. 
**3: dosya yolu.


(th1)<-spawn(download_task("https://file.zip", addr(nexus)), async, timeout_ms);

// ... ana uygulama işine devam eder ...

!listen(th1, 10)->{
    // 1. Duruma bak: Disk mi?
    (nexus.pldstatus == 3) =?>{
        // 2. Yolu al ve kendi yöntemlerinle oku!
        v:file_path = nexus.Path; 
        v:size = nexus.DataLength;
        
        // Programcı burada istediği kütüphaneyi veya ASM kodunu kullanır:
        v:data = io:read_all(file_path, size); 
        echo("Dosya teslim alındı: {file_path}");
    }
}

```

### 1. Mantıksal Akış (In-Depth Reasoning)

Monitor Thread uyandığında (100ms dolduğunda veya sinyal geldiğinde), öncelikle  (triger_flag == 1) ı kontrol eder eğer 1 ise thread_id ile listede tarar
ve bilgileri akışa iletir, (counter -1) yapar. Sonra başka biten varmı diye listeyi kontrol eder (event_code == 2)
Bir `while` döngüsüyle o an buffer'da ne kadar bitmiş görev varsa hepsini tüketene kadar çalışır. 
Ne zaman ki buffer boşalır,  (triger_flag) ı 0lar ve tekrar 100ms'lik uyku moduna geçer.


---

### 3. Listen triger area

```

nt:!struct {
    u8_t triger_flag,  # 0: Boş, 1: bitti.
    u16_t counter,     # Task Counter Aktif Thread sayısı 
    u32_t thread_id    # Son biten Threadin Kimliği
    u8_t empty,        # 8 byte a tamamla..
} Listen_Triger;


```

---


### 3. Kullanım Örnekleri


**Örnek 1: İşçi Tarafında Veri Üretimi (Done Kullanımı)**

```nxf
v:data = area(1024);
f:sensor_reader(port, mem_addr) {
    v:raw = sys.io_read(port);
    
    # İş bitti: Veri tipi 1: (Raw) Max: 1024byte. 2: Verinin bulunduğu adres ve boyutu 8+8byte. 3: dosya yolu.
    done(1, raw, mem_addr); 
}

f:main()!i32 {
    (h) <- spawn(sensor_reader(0xAF, data), async, 500);
    
    # !listen sadece trigger_flag'e bakar, 
    # flag 1 olunca tabloyu tarayıp (h) slotunu bulur.
    !listen(h, 10) >> (val) {
        echo("Sensör: {val.payload}");
    }?;
    
    ret 0;
}

```

**Örnek 2: Bare-Metal Olay Döngüsü (Power Saving)**
Bare-metal modunda `!listen()`, `active_tasks` varsa ve `trigger_flag` sıfırsa işlemciyi uyutur.

```nxf
no:CoreModule;

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


### **Send & Receive (IPC)**
Thread'ler arası iletişim masalar üzerinden atomik olarak gerçekleşir:

```nxf
# Thread A: Veri Gönderici
f:sender(mailbox!h) {
    send(mailbox, "Merhaba Dünya");
    #done(1, true); # İşi bitir
}

# Thread B: Veri Alıcı
f:receiver(mailbox!h) {
    (msg) <- receive(mailbox); # Veri gelene kadar bloklanır
    echo("Alındı: {msg}");
}
```

- **`done(type, payload, target)`**: Görevin bittiğini Nexus Slot Matrix'e bildirir.
- **`send()`**: Veriyi kopyalamaz, pointer sahipliğini anında hedefe aktarır.

