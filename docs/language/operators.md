
# 3. NEXUS FLOW OPERATÖRLERİ (THE 15 KEYS) — Formal Specification

## 1. Tanım

**Nexus Akış Operatörleri**, veri ve kontrol akışını **deterministik, atomik ve güvenli** şekilde yöneten operatörler kümesidir. Her operatörün bir **öncelik sırası** vardır ve bu sıralama akışın nasıl işleneceğini belirler. Operatörler, **masa (handler)** tabanlı çalışma modeli ile entegre edilmiştir.

---

## 2. Operatör Listesi ve Açıklaması

| Öncelik | Operatör  | Adı               | İşlevi / Açıklaması                                                                        |
| ------- | --------- | ----------------- | ------------------------------------------------------------------------------------------ |
| **1**   | `(h)`     | Handler (Masa)    | Bellek adresini veya kaynak rezervasyonunu temsil eder. İşlem öncesi masa hazır olmalıdır. |
| **2**   | `<-`      | Capture (Yakala)  | Değer veya sonucu masaya kilitler. Başarısız ise hata akışına yönlendirilebilir.           |
| **3**   | `_>`      | Relocate          | Masayı veya context’i başka bir bölgeye taşır (GPU, farklı bellek segmenti).               |
| **4**   | `?(n,ms)` | Rolling           | İşlem hatalı ise `n` kez `ms` ms aralıkla tekrar dener.                                    |
| **5**   | `?->`     | Catch (Saptırıcı) | Deneme başarısız olursa akışı belirlenen hata bloğuna yönlendirir.                         |
| **6**   | `?=>`     | Fallback          | Hata saptırıcı yoksa veya veri `null` ise alternatif veri enjekte eder.                    |
| **7**   | `->`      | Success (Başarı)  | Veriyi bir sonraki işleme veya fonksiyona taşır.                                           |
| **8**   | `!->`     | Ignore (Sessiz)   | Hata olsa bile akışı zorla devam ettirir.                                                  |
| **9**   | `<<`      | Zone Write        | Veriyi atomik olarak paylaşımlı belleğe yazar.                                             |
| **10**  | `>>`      | Flow Feed         | Veriyi atomik olarak akışa besler (pipeline ready).                                        |
| **11**  | `(e){}`   | Error Block       | Hata durumunda devreye giren blok; saptırılan hatayı işler.                                |
| **12**  | `?;`      | Halt (Son)        | Akışı durdurur; yerel masalar ve kaynaklar imha edilir.                                    |
| **13**  | `!!`      | Panic             | Kritik hata; tüm akışı durdurur ve runtime uyarısı verir.                                  |
| **14**  | `@`       | Intent            | Derleyiciye veya runtime’a işlem niyetini bildirir (optimization hints).                   |
| **15**  | `!!=`     | Directive         | .target sistemi ve derleyici talimatlarını verir.                                          |

---

## 3. Masa (Handler) ve Sahiplik Mekanizması

### 3.1 Masa Kuralları

* **Table Occupied:** `(h)` doluysa ve `<-` ile yeni veri yazılmaya çalışılırsa **derleyici hata verir**. Masa boşaltılmalıdır.
* **Move-on-Flow:** `(h) -> write()` işlemi, veriyi kopyalamaz; sadece taşır. İşlem sonunda `(h)` sıfırlanır (nulled).
* **Zero-Cost Handover:** Masalar arası transfer sadece **adres etiketinin değişimi** ile gerçekleştirilir; veri kopyası gerekmez.

### 3.2 Standart Masa Takma Adları (Shims)

| Takma Adı | Tür            | Açıklama                     |
| --------- | -------------- | ---------------------------- |
| `(h)`     | Handle/Header  | Dosya veya sistem kaynakları |
| `(e)`     | Handle/Header  | Hata işleme masası           |
| `(ok)`    | Boolean/Result | Kontrol mekanizması          |
| `(v)`     | Value          | Ham veri                     |
| `(v,val)` | Value          | Ham veri (parametre ile)     |
| `(it)`    | Iterator       | Döngü akışı için iterator    |

---

## 4. Nexus Flow Mantığı

### 4.1 Ternary Akış Operatörü

Kural:

```nexus
(h) <- fn() ?-> (e) { ... }
```

* `fn()` çalıştırılır.
* Eğer sonuç **false, null, none veya unknown** ise akış `(e){}` bloğuna yönlendirilir.
* Başarılı ise `(h)` masasına değer yakalanır ve akış normal devam eder.

#### Örnek:

```nexus
(h) <- file.open("a.txt", r) ?-> (e){ echo(e); };
(h) -> process(h);
```

* Hata varsa `(e)` bloğu çalışır.
* Başarı varsa `(h)` ile process fonksiyonu devam eder.

---

### 4.2 Örnek Akış: Dosya İşleme

```nexus
(in)  <- file.open("input.txt", r)  ?-> !!("Giriş dosyası yok");
(out) <- file.open("output.txt", w) ?-> !!("Çıkış dosyası yok");

(in, out) -> {
    v:data <- file.read(in);
    file.write(out, data);
}?; # Akış biter, masalar imha edilir.
```

* `(in, out)` masaları ile kaynaklar rezerve edilir.
* `file.read` ile veri alınır ve `v:data` masasına capture edilir.
* `file.write` ile çıktı dosyasına veriyi taşır.
* `?;` operatörü ile akışın sonunda tüm masalar serbest bırakılır.

---

## 5. Akış Operatörleri Davranış Kuralları

1. **Handler Önceliği:** `(h)` her zaman ilk sırada olmalıdır.
2. **Başarı Akışı:** `->` operatörü, yalnızca bir önceki operasyon başarıyla tamamlanırsa devreye girer.
3. **Hata Akışı:** `?->` veya `!!` operatörleri hata durumunda çalışır.
4. **Fallback Mekanizması:** `?=>` ile hata saptırıcı veya `null` veriler için alternatif veri enjekte edilebilir.
5. **Atomik Bellek:** `<<` ve `>>` operatörleri ile veriler **thread-safe** ve **atomic** olarak yazılır.
6. **Akış Sonlandırma:** `?;` veya `!!` ile akış kesin olarak durdurulur.
7. **Compiler Hints:** `@` ve `!!=` operatörleri derleyici ve runtime optimizasyonları için kullanılır.

---

## 6. Özet ve Öneriler

* **Nexus Flow**, **15 operatör** ile **deterministik veri ve kontrol akışı** sağlar.
* Masalar `(h)` temelinde **sahiplik ve transfer mekanizması** ile optimize edilmiştir.
* Operatörler, **hata yönetimi, fallback, başarı ve atomik paylaşımlı bellek** gibi tüm durumları kapsar.
* Bu model, hem **OS tabanlı hem Bare-metal** akışlar için uyumludur.
* Örnekler, **dosya işlemleri** üzerinden gösterilmiş olup, aynı mantık ağ, IO ve thread yönetimi gibi farklı alanlarda uygulanabilir.

---

