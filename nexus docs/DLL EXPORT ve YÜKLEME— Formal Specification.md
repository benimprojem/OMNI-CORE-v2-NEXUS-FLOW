
# 11. DLL EXPORT ve YÜKLEME (`dllexp:` / `import`)

## 11.1 DLL Export (`dllexp:`)

* `dllexp:` ana fonksiyon yerine **DLL olarak derlenecek entry point**’i belirler.
* Sadece kendi yazdığınız DLL’ler için geçerlidir; dışarıdan alınan DLL’ler için kullanılamaz.
* Syntax:

```nexus id="dll_exp1"
dllexp: run_optimizer(*oir_path, *nxir_path, *target) {
    # DLL ana fonksiyonu
    ...
}
```

* `dllexp:` ile derlenen fonksiyonlar, DLL dışına public olarak açılır.
* DLL içinde **normal main()** kullanılamaz; entry point `dllexp:` ile tanımlanır.

---

## 11.2 DLL Yükleme (`import(path)`)

* `import(path)` ile DLL, `.dll`, `.so`, `.o`, `.obj` gibi dosyalar yüklenebilir.
* Yüklenen DLL, **masa (handler)** gibi davranır: fonksiyonlar ve gruplar **`(h)` veya alias üzerinden çağrılır**.

### Kullanım:

```nexus id="dll_import1"
# DLL yükleme
(Optimizer) <- import("optimizer.dll") ?-> !!("DLL Bulunamadı!");

# Yüklenen DLL’in fonksiyonlarını çağırma
v:sonuc = Optimizer.run_optimizer(oir_path, nxir_path, target_info);

# DLL masa serbest bırakma
(Optimizer)?;  # Bellek temizlenir, handler sıfırlanır
```

---

## 11.3 Notlar ve Kurallar

1. **DLL Fonksiyonları Group gibi davranır**

   * Yüklenen DLL’in tüm public fonksiyonları, yüklenen masa üzerinden çağrılır:
     `Optimizer.<fonksiyon_adi>(...)`

2. **Hata ve Exception Yönetimi**

   * `?-> !!("Mesaj")` ile yükleme hataları yakalanır.
   * Masa boşaltılmazsa, DLL bellekte kalır → tekrar yükleme hatalı olabilir.

3. **Handler / Masa Kullanımı**

   * DLL yükleme sonrası `(h)` mantığı ile aynı şekilde davranır.
   * Bellek yönetimi ve cleanup `(Handler)?;` ile yapılır.

4. **DLL Reload / Alias**

   ```nexus id="dll_alias"
   use Optimizer as Opt;  # Alias ile daha kısa kullanım
   v:sonuc = Opt.run_optimizer(...);
   ```

5. **Cross-Platform DLL / SO**

   * Windows → `.dll`
   * Linux → `.so`
   * Bare-metal → `.o` veya `.obj` (link aşamasında)

---

## 11.4 Örnek Full Akış

```nexus id="dll_full_example"
# DLL Yükleme
(Optimizer) <- import("optimizer.dll") ?-> !!("DLL Bulunamadı!");

# Fonksiyon çağrısı
v:result = Optimizer.run_optimizer(oir_path, nxir_path, target_info);

# Alias ile kullanım
use Optimizer as Opt;
v:result2 = Opt.run_optimizer(oir_path, nxir_path, target_info);

# DLL boşaltma / serbest bırakma
(Optimizer)?; 
```

---
