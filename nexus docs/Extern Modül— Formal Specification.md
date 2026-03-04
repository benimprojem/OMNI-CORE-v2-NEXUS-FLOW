
# 12. Extern Modül (`exf:` / !!=link)

## 12.1 Genel Kurallar

* `exf:` dış kütüphaneleri, `.dll`, `.so`, `.lib`, `.a` veya `.obj` dosyalarını **NexusFlow** içinde kullanılabilir hâle getirir.
* Modüller `use <module>` ile projeye dahil edilir.
* OS ve target bazlı conditional derleme ile **platform bağımsız arayüz** oluşturabilirsiniz.

### 12.1.1 Syntax

```nexus id="extern_syntax"
!!=[target = "<platform>"] {
    !!=link name = "<libname>";
    exf:fonksiyon_adi(params)!<type>;
}
```

* `!!=[target="windows"]` → Windows’a özel kod
* `!!=[target="linux"]` → Linux’a özel kod
* `!!=[target="bare-metal"]` → Bare-metal veya embedded platform
* `!=link name` → OS linker için kütüphane adı

---

## 12.2 Extern Modül Örneği (OS Abstraction)

```nexus id="extern_example"
# msvcrt.exf : OS-spesifik stdlib köprü modülü

!!=[target = "windows"] {
    !!=link name = "msvcrt";
    exf:_write(fd!i32, buf!*char, size!u32)!i32;
}

!!=[target = "linux"] {
    !!=link name = "libc";
    exf:write(fd!i32, buf!*char, size!u32)!i32;
}

# Bare-metal örnek: hiçbir OS, doğrudan donanım
!!=[target = "bare-metal"] {
    # Hardware-specific low-level I/O
}
```

---

## 12.3 Group ile OS Soyutlaması

* Aynı fonksiyon isimleri ile farklı OS’ler için arayüz sağlar.
* Kullanıcı OS detaylarını bilmek zorunda kalmaz.

```nexus id="extern_group_example"
group SysIO {

    # Static-like fonksiyon
    output => f:(msg!str) {
        v:len = len(msg);
        v:ptr = addr(msg);

        !!=[target = "windows"] {
            _write(1, ptr, len);
        }

        !!=[target = "linux"] {
            write(1, ptr, len);
        }

        !!=[target = "bare-metal"] {
            # VGA buffer veya UART
            loop(i, i < len, i++) {
                poke(0xB8000 + (i * 2), msg[i]);
                poke(0xB8000 + (i * 2) + 1, 0x07); # Gri renk
            }
        }
    }

}
```

---

## 12.4 Kullanım Örneği

```nexus id="extern_usage"
# Projeye ekleme
use msvcrt.exf;

f:main()!i32 {
    # OS detayları tamamen gizli
    SysIO.output("NexusFlow OS Bridge Active.");

    return 0;
}
```

* `use` → Modülü projeye ekler
* `SysIO.output(...)` → Tüm platformlarda çalışır
* Dışa bağımlı fonksiyonlar `exf:` ile tanımlanır, gruplar aracılığıyla kullanıcıya **uniform API** sunar

---

### 12.5 Notlar

1. `exf:` ile **sadece external fonksiyonlar** tanımlanabilir, kendi yazdığınız fonksiyonlar için `f:` veya `dllexp:` kullanılır.
2. Conditional derleme (`!!=[target="..."]`) ile **platform bağımsız kod yazılabilir**.
3. Extern modüller **public** sayılır; `use` ile projeye eklenmedikçe görünmezler.

---
