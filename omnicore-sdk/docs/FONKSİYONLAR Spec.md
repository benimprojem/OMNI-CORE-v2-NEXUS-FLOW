
# 6. FONKSİYONLAR (FUNCTIONS) — Final Spec v2.0

## 1. Temel Tanımlar

| Tür / Açıklama                    | Tanımlama                                   | Syntax / Örnek                                         |
| --------------------------------- | ------------------------------------------- | ------------------------------------------------------ |
| **Main Fonksiyon**                | `f:main(argc, argv)!i32 { ... }`            | `f:main()!i32 { ... }`                                 |
| **Normal Fonksiyon**              | `f:fonksiyon_adi(params)!tip { ... }`       | `f:topla(a!i32, b!i32)!i32 { return a + b; }`          |
| **Dönüşsüz Fonksiyon (void)**     | `f:fonksiyon_adi(params) { ... }`           | Dönüş tipi belirtilmezse `void` varsayılır             |
| **Array / Multiple Return**       | `f:fonksiyon_adi(a, ...) !i32[] { ... }`    | `f:get_values(n)!i32[] { ... }`                        |
| **İsimli Parametreler / Default** | `f:topla(a:10, b:20)!i32 { ... }`           | `topla(a:10, b:20) { ... }`                            |
| **Lambda / Tek Satır Fonksiyon**  | `f:(a,b)> a*b;`                             | `v:kare = f:(a,b)> a*b;`                               |
| **Anonim Fonksiyon**              | `f:(a,b){ ... }`                            | `open => f:(a,b){ ... }` (Group içinde kullanılabilir) |
| **Extern Fonksiyon**              | `exf:fonksiyon_adi(a!i32, ...)!i64 { ... }` | OS veya başka modüller için                            |
| **Generic Fonksiyon**             | `f:fonksiyon_adi(a!t, b!t)!t { ... }`       | Parametre tipine göre dönüş tipi infer edilir          |

---

## 2. Generic Fonksiyon Örnekleri

```nexus
f:sum(a!t, b!t)!t {
    return a + b;
}

f:main()!i32 {
    v:x!i32 = sum(10, 20);      # t → i32
    v:y!f32 = sum(1.5, 2.5);    # t → f32
}
```

---

## 3. Fonksiyon İşaretçileri ve Callback

### 3.1 Fonksiyon İşaretçisi

```nexus
v:fp!fptr = f:(a!i32,b!i32)!i32> a+b;
echo(fp(10, 20)); # 30
```

### 3.2 Callback Fonksiyon

```nexus
f:do_work(n!i32, callback!fptr)!i32 {
    v:result!i32 = n * 2;
    callback(result); 
    return result;
}

f:main()!i32 {
    f:my_cb(x!i32) { echo("Callback: {x}"); }
    do_work(10, my_cb);
}
```

---

## 4. Fonksiyon Scope ve Visibility

* Fonksiyonlar **default public**
* `group` veya `struct` içinde tanımlanan fonksiyonlar **sadece o kapsama görünür**
* Anonim fonksiyonlar yalnızca tanımlandıkları scope içinde kullanılabilir

---

## 5. Fonksiyon Kuralları ve Syntax

1. **Parametre Tipleri**

   * `a!i32` → zorunlu tip
   * `a:10` → default değerli parametre
2. **Dönüş Tipi**

   * Belirtilmezse → `void`
   * `!t` → generic tip
   * `!i32[]` → dizi dönüş
3. **Lambda ve Anonim Fonksiyon**

   * Tek satır → `>` kullanılır
   * Çok satır → `{ ... }` kullanılır
4. **Extern Fonksiyonlar**

   * OS veya modül bağımlı fonksiyonlar için `exf:` prefix

---

## 6. Örnek Kullanımlar

### 6.1 Lambda ve Generic

```nexus
v:add_i32 = f:(a!i32,b!i32)!i32> a+b;
v:add_f32 = f:(a!f32,b!f32)!f32> a+b;

echo(add_i32(10,20)); # 30
echo(add_f32(1.5,2.5)); # 4.0
```

### 6.2 Callback Fonksiyon

```nexus
f:process_value(x!i32, cb!fptr)!i32 {
    v:y = x*3;
    cb(y);
    return y;
}

f:main()!i32 {
    v:cb = f:(val!i32)> echo("Callback: {val}");
    process_value(10, cb);
}
```

---

## 7. Advanced Özellikler

* **Async / Coroutine:** spawn ve !listen ile kullanılabilir
* **Error Handling:** ?->, ?=>, !! operatörleri ile uyumlu
* **Recursion / Tail-call:** Desteklenir, derleyici optimize edebilir
* **Scope ve Visibility:** Group / struct ile kısıtlanabilir
* **Function Overloading:** Generic ile sınırlı; normal overloading opsiyonel

---
