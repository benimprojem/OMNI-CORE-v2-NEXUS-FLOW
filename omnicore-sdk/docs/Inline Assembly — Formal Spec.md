
# 9. Gelişmiş Inline Assembly (`fastexec{asm{}}`) — Formal Specification

## 1. Tanım

**`fastexec`**, inline assembly bloklarını kapsayan bir **yüksek performanslı çalışma konteyneri**dir.
Amaç:

* ASM kodlarını doğrudan çalıştırmak,
* NexusFlow değişkenlerini **stack/heap/register mapping** ile yönetmek,
* Call ve Jump mekanizmalarını entegre etmek,
* Optimize edilmiş ve güvenli bir **asm çalışma ortamı** sağlamak.

> Önemli: ASM blokları `fastexec` dışında tanımlanırsa derleyici **hata verir**.

---

## 2. Temel Kurallar

1. **ASM Tanımı:**

```nexus
fastexec {
    asm: <Label> { ... }
}
```

* `<Label>` → ASM bloğunun etiketi, çağrılabilir (`asmcall`, `asmjmp`).
* Etiket isimleri `_`, `-`, `$` gibi özel karakterlerle başlayamaz.

2. **Değişken Geçişi:**

* `%degisken` → NexusFlow değişkeninin **bellek adresi (stack offset)**
* `%degisken:reg` → Belirtilen register'a yükleme ve işlem sonunda geri yazma

3. **Clobber Listesi:**

* `# clobber: rax, rbx` → ASM bloğunun kirlettiği register’ları otomatik push/pop ile korur.

4. **Call & Jump Fonksiyonları:**

* `asmcall("LABEL")` → ASM bloğunu **call** ile çağırır.
* `asmjmp("LABEL")` → ASM bloğuna **koşulsuz jmp** yapar.

---

## 3. Değişken Yönetimi

| Tür             | Kullanım  | Açıklama                                               |
| --------------- | --------- | ------------------------------------------------------ |
| `%degisken`     | Bellek    | Değişkenin stack offset adresine doğrudan erişim       |
| `%degisken:reg` | Register  | Belirli register’a yükleme ve işlem sonrası geri yazma |
| v:degisken      | NexusFlow | ASM dışında tanımlanan normal değişken                 |

* Derleyici, değişkenlerin ASM blokları içinde doğru offset ve register ile eşlemesini otomatik yapar.
* Geliştirici `%` ile referans verdiğinde, ASM kodunda doğru memory veya register kullanılır.

---

## 4. ASM Call & Jump Mantığı

1. **ASM Call (`asmcall`)**

* Tanımlı ASM bloğunu **call** ile çağırır.
* Fonksiyon gibi davranır, dönüş adresi saklanır.

2. **ASM Jump (`asmjmp`)**

* Koşulsuz sıçrama (jmp) yapar.
* Geri dönüş adresi saklanmaz; dikkat edilmelidir.

> Uyarı: ASM bloklarında **return** veya jump yoksa, kodunuz hiç geri dönmeyebilir ve uygulama çöker.

---

## 5. Örnek Kullanım

### 5.1 Fastexec + Inline ASM

```nexus
f:main()!i32 {
    fastexec {
        v:a!i32 = 10;
        v:b!i32 = 20;
        v:total!i32 = 0;
        
        asm: CRITICAL_ADD { 
            mov rax, %a
            add rax, %b
            mov %total, rax
        }
        
        echo(total); # Çıktı: 30
    }

    asmcall(CRITICAL_ADD);  # ASM bloğunu çağırır
    asmjmp("MY_ASM_BLOCK"); # ASM bloğuna zıplama
    return 0;
}
```

---

### 5.2 Fonksiyon İçinde ASM Kullanımı

```nexus
f:asm_func(a, b)!i32{
    fastexec {
        v:tot = 0;

        asm: CRITICAL_ADD {
            mov rax, %a
            add rax, %b
            mov %tot, rax
        }
        
        return tot;
    }
}

f:main()!i32 {
    v:a!i32 = 10;
    v:b!i32 = 20;

    v:total!i32 = asm_func(a, b); # Doğrudan fonksiyon gibi çağrı
    echo(total); # 30
    return 0;
}
```

* Fonksiyon içinde tanımlanan ASM bloğu, **direct call veya jump gerekmeden** çalıştırılabilir.
* Değişkenler (`a, b, tot`) **stack/register mapping** ile otomatik yönetilir.

---

## 6. Özellikler ve Optimizasyon

1. **Agressif Optimize:**

   * Derleyici, ASM bloklarını **inline** yerleştirir.
   * Gereksiz memory access ve register yüklemeleri minimize edilir.

2. **Değişken Substitution:**

   * `%degisken` ile direkt memory offset kullanımı
   * `%degisken:reg` ile register mapping

3. **Register Clobbering:**

   * `# clobber` ile ASM bloğu tarafından kullanılacak register’lar otomatik push/pop ile korunur.

4. **Bağlantılı ASM Blokları:**

   * ASM blokları birbirini **call veya jmp** ile çağırabilir.
   * Etiketler fonksiyon veya kod segmenti gibi davranır.

---

## 7. Özet

* `fastexec` → Inline ASM kapsayıcısı
* `%degisken` → Değişkenleri stack/register ile eşleme
* `asmcall` → ASM call
* `asmjmp` → ASM jump
* `# clobber` → Register koruma
* Derleyici, ASM bloklarını **optimize edilmiş ve güvenli** şekilde stack/register mapping ile oluşturur.

---
