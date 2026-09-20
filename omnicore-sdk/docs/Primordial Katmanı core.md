

# Bölm 4 Nexus Flow Primordial Katmanı "core"  Modülü

Bu katman `core` takısı ile ifade edilir ve hiçbir `import` gerektirmez. Bu modül "no:CoreModule"  ile otomatik import edilir. Os bağımlılığı kaldırıdığında çalışır.

## 1. Bellek ve İşaretçi Primitifleri (Memory & Pointers)

Bir kernel yazmak için belleğin her hücresine erişim şarttır.

| Fonksiyon/Metot | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.ptr.read_u8(addr: u64)` | Belirtilen adresten 1 byte okur. | Donanım register'larını okumak. |
| `core.ptr.write_u8(addr, val)` | Belirtilen adrese 1 byte yazar. | Video belleğine (VGA) veri göndermek. |
| `core.ptr.offset(ptr, n)` | İşaretçiyi `n` kadar kaydırır. | Dizi ve veri yapısı navigasyonu. |
| `core.mem.size_of([T])` | Bir tipin bellekteki boyutunu döner. | Manuel bellek tahsisi (malloc benzeri). |
| `core.mem.align_of([T])` | Tipin hizalama (alignment) sınırını döner. | Veri yapısı optimizasyonu. |
| `core.mem.copy_raw(src, dst, n)` | Denetimsiz bellek kopyalama. | Kernel seviyesi veri transferi. |

---

## 2. Donanım ve CPU Komutları (CPU Intrinsics)

Bu fonksiyonlar doğrudan CPU talimatlarına (Instructions) derlenir.

| Fonksiyon/Metot | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.cpu.out_b(port, val)` | Donanım portuna byte gönderir. | Donanım cihazlarını (Klavye, Disk) kontrol etmek. |
| `core.cpu.in_b(port)` | Donanım portundan byte okur. | Donanımdan veri almak. |
| `core.cpu.halt()` | İşlemciyi durdurur (HLT). | Güç tasarrufu ve kernel boşta döngüsü. |
| `core.cpu.nop()` | İşlemciyi 1 çevrim boşta bırakır. | Hassas zamanlama/gecikme. |
| `core.cpu.cli()` / `sti()` | Interrupt'ları kapatır/açar. | Yarış durumlarını (Race conditions) engellemek. |
| `core.cpu.load_gdt(ptr)` | Global Descriptor Table yükler. | Bellek segmentasyonu ve korumalı mod geçişi. |
| `core.cpu.cpuid(leaf)` | İşlemci özelliklerini sorgular. | Donanım tanıma. |

---

## 3. Akış Kontrolü ve Yığın Yönetimi (Stack & Control)

| Fonksiyon | Tanımı | Kullanım Amacı |
| --- | --- | --- |
| `core.stack.set_ptr(addr)` | Yığın işaretçisini (ESP/RSP) ayarlar. | Bootloader'dan Kernel'a geçerken yığın oluşturma. |
| `core.stack.get_ptr()` | Mevcut yığın adresini döner. | Hata ayıklama (Stack trace). |
| `core.exec.jump(addr)` | Belirtilen adrese mutlak atlama yapar. | Kernel giriş noktasına (Entry point) gitmek. |
| `core.exec.call_raw(addr)` | Bir adresi fonksiyon gibi çağırır. | Dinamik yüklenen modülleri çalıştırmak. |

---

## 4. Tip ve Veri Tanımlama Metotları (Meta-Core)

Sıfırdan bir dil inşa ederken tiplerin nasıl davranacağını bu metotlar belirler.

* `core.type.cast(T, val)`: Veriyi güvenli/güvensiz olarak T tipine dönüştürür (Bit bazlı).
* `core.type.is_primitive(val)`: Verinin temel tip (int, bool vs.) olup olmadığını döner.

---

## 5. Uygulamalı Örnek: Sıfırdan "Hello World" Kernel (x86)

Hiçbir kütüphane (`io`, `os` vs.) kullanmadan, sadece çekirdek fonksiyonlarla ekrana (VGA Text Buffer) yazı yazan boot aşaması:

```
// OmniCore Kernel Entry
f:_start() {
    # VGA metin belleği adresi 0xB8000'dir.
    v:vga_buffer!u64 = 0xB8000
    v:message = "OmniCore Booted!"
    
    v:i!u64 = 0
    print_loop:> {
        (i < message.len()) =?> {
            v:char = message.at(i)
            
            # Karakteri yaz (Hizalama: Char, Color)
            core.ptr.write_u8(vga_buffer + (i * 2), char)
            core.ptr.write_u8(vga_buffer + (i * 2) + 1, 0x0F) // Beyaz renk
            
            i = i + 1
            (print_loop)?>
        }
    }

    # Kernel'ı sonsuz döngüde tut
    (loop) {
        core.cpu.halt()
    }
}

```

---

----
