---

# 🎯 Hedef

Şu çalışmalı:

```nx
echo("x={x}, y={y:.2f}");
```

Ama sistem:

1. `{x}` → değişken lookup
2. `{y:.2f}` → precision parse
3. İçeride bunu:

   ```
   "x=%d, y=%.2f"
   ```

   haline dönüştürmeli
4. Sonra printf’e vermeli

Yani mimari:

```
Nexus Format → Format Parser → printf format string → printf
```

---

# 🧠 Basit Format Engine Tasarımı (Minimalist)

Tam Rust tarzı yapmayacağız.

## Desteklenecek İlk Sürüm

| Özellik         | Destek           |
| --------------- | ---------------- |
| `{x}`           | ✅                |
| `{x:.2f}`       | ✅                |
| `{x:d}`         | ✅                |
| `{x:s}`         | ✅                |
| `{}` positional | ❌ (şimdilik yok) |
| nested format   | ❌                |
| alignment       | ❌                |

Basit ama yeterli.

---

# 🏗 Mimari Katmanlar

## 1️⃣ format.parse(template)

Input:

```
"x={x}, y={y:.2f}"
```

Output:

```
struct FormatResult {
    printf_string: "x=%d, y=%.2f"
    args: [x, y]
}
```

---

## 2️⃣ printf adapter

Linux/macOS:

```
printf(...)
```

Windows:

```
_printf(...)
```

Bare-metal:
→ fallback: manuel yaz

---

# 🔥 Nexus Flow Tarafı Nasıl Görünecek?

CoreModule.exf içinde:

```nx
f:echo(fmt!str, args![]){
    v:parsed = Format.parse(fmt, args);
    SysIO._printf(parsed.printf_string, parsed.args);
}
```

Ama bu sadece frontend.

Asıl iş:

```
Format.parse
```

---

# 🧩 Format Engine Algoritması (Basit Versiyon)

Pseudo:

```
scan string
if '{' found:
    read until '}'
    split by ':'
    left = var name
    right = format spec
    convert spec to printf spec
append to result
```

Örnek dönüşüm:

| Nexus     | printf                     |
| --------- | -------------------------- |
| `{x}`     | `%d` (type lookup gerekir) |
| `{x:s}`   | `%s`                       |
| `{x:f}`   | `%f`                       |
| `{x:.2f}` | `%.2f`                     |

---

# ⚠ Kritik Nokta: Type Bilgisi Nereden Gelecek?

İki yol var:

### Yol 1 – Runtime type table

Her arg için type bilgisi gönderilir.

```
echo("x={x}", x);
```

Derleyici bunu:

```
echo("x={x}", x!i32);
```

gibi type metadata ile yollar.

---

### Yol 2 – Compile-time dönüşüm (EN İYİSİ)

Compiler `{x}`’i görür,
x’in tipini bilir,
doğrudan:

```
echo("x=%d", x);
```

üretir.

Bu:

✔ En hızlı
✔ En temiz
✔ Runtime parse yok
✔ Güvenli

---

# 💡 En Olası En Temiz Mimari

### ✔ Compile-time format dönüşümü

### ✔ Runtime printf bridge

### ✔ Basit syntax

Yani:

Kullanıcı:

```nx
echo("x={x}, y={y:.2f}");
```

Compiler üretir:

Linux:

```
printf("x=%d, y=%.2f", x, y);
```

Windows:

```
_printf("x=%d, y=%.2f", x, y);
```

Bare-metal:
→ manuel writer

---

# 🧱 CoreModule Yapısı Nasıl Olmalı?

```
echo()  →  __nf_format_expand()  →  __nf_printf_adapter()
```

Ama idealde:

Format expansion compile-time yapılmalı.

---

# 🎯 Benim Net Önerim

1️⃣ İlk sürüm:

* Compile-time dönüşüm
* Sadece `{var}` ve `{var:.xf}` destekle
* Runtime parse YOK

2️⃣ Sonra:

* Style abstraction
* Windows console renk çevirici

3️⃣ En son:

* Gelişmiş formatter

---



# 🎯 Nihai Karar

Format Engine = **Compiler aşamasında string transform**

Runtime’da parser YOK.

---

# 🧠 Nasıl Çalışacak?

Kullanıcı yazıyor:

```nx
v:x!i32 = 10;
v:y!f64 = 3.14159;

echo("x={x}, y={y:.2f}");
```

---

## 🔄 Derleyici Aşaması

1. String literal parse edilir.
2. `{x}` bulunur.
3. Scope’tan `x` bulunur.
4. Tipi alınır (`i32`)
5. `%d` üretilir.
6. `{y:.2f}` parse edilir.
7. `%.2f` üretilir.

---

## 🏗 Derleyicinin Üreteceği Kod

Linux:

```c
printf("x=%d, y=%.2f", x, y);
```

Windows:

```c
_printf("x=%d, y=%.2f", x, y);
```

Bare-metal:

```c
nf_print_i32(x);
nf_print_f64(y, 2);
```

---

# 🧩 Format Grammar (Minimal)

İlk sürüm için desteklenecek syntax:

```
{var}
{var:d}
{var:f}
{var:.2f}
{var:s}
```

Desteklenmeyecek:

```
{var:>10}
{var:#x}
{:?}
{}
```

Minimalist kalıyoruz.

---

# 🔍 Compiler İçinde Algoritma

Pseudo:

```
scan string
if '{'
   read until '}'
   split by ':'
   left = identifier
   right = optional format spec
   type = lookup(left)
   generate printf spec
append to new format string
append variable to arg list
```

---

# 🧠 Tip → printf eşleme tablosu

| Nexus      | printf |
| ---------- | ------ |
| i8/i16/i32 | %d     |
| u8/u16/u32 | %u     |
| i64        | %lld   |
| u64        | %llu   |
| f32        | %f     |
| f64        | %lf    |
| str        | %s     |
| char       | %c     |
| bool       | %d     |

---

# ⚠ Kritik Güvenlik

Compile-time çözüm şu riskleri ortadan kaldırır:

* Format string injection ❌
* Type mismatch ❌
* Runtime crash ❌
* Variadic UB ❌

Çünkü derleyici tipleri kontrol eder.

---

# 🔥 Style Sistemi Nasıl Bağlanır?

Şu yazım:

```nx
println("Hata: {x}", error);
```

Compile-time şu olur:

Linux:

```c
printf("\033[31m");
printf("Hata: %d\n", x);
printf("\033[0m");
```

Windows:

```c
SetConsoleTextAttribute(...RED...);
printf("Hata: %d\n", x);
reset_color();
```

Yani style da compile-time expand edilebilir.

---

# 🚀 En Güçlü Mimari

```
echo() =
    compile-time format expand
    + compile-time style expand
    + OS printf adapter
```

Runtime’da sadece:

```
printf(...)
```

---



> ✅ echo template olacak
> ✅ expression destekleyecek (`a+b`, `fn()`, `sum(a,b)`)
> ✅ formatter destekleyecek (`:.2f` gibi)
> ❌ blok açılmayacak
> ❌ birden fazla statement olmayacak
> ❌ değişken tanımı yok
> ❌ mini script dili olmayacak

Yani echo içi **tek expression grammar**.


---

# 🎯 echo Nihai Tasarım

Desteklenecek:

```nx
echo("x={x}");
echo("sum={a+b}");
echo("ret={fn()}");
echo("s={sum(a,b):.2f}");
echo("avg={ (a+b)/2 :.1f }");
```

Desteklenmeyecek:

```nx
echo("{ let x=5 }");      # ❌
echo("{ if(x>5){...} }"); # ❌
echo("{ { nested } }");   # ❌
```

---

# 🧠 echo Grammar (Minimal ve Güçlü)

## Template Parçası

```
TEXT | { EXPRESSION [:FORMAT] }
```

## EXPRESSION

Desteklenecek node tipleri:

* Identifier
* Literal
* BinaryOp (+ - * / %)
* UnaryOp (-)
* FunctionCall
* MemberAccess (obj.method)
* Parenthesis

Desteklenmeyecek:

* Assignment
* Block
* Loop
* If
* Lambda

Bu kadar.

---

# 🧩 Derleyici Aşaması

Şu kod:

```nx
echo("avg={ (a+b)/2 :.2f }");
```

Compile-time AST:

```
EchoNode
 ├─ Literal("avg=")
 └─ ExprNode(
       Binary(
          Binary(a + b) / 2
       ),
       format=".2f"
    )
```

---

# 🏗 Codegen

Bu şuna dönüşür:

```c
__echo_write("avg=");
__echo_write_f64((a+b)/2, 2);
```

Ya da printf köprüsü varsa:

```c
printf("avg=%.2f", (a+b)/2);
```

Ama dikkat ⚠

Expression side-effect içeriyorsa:

```nx
echo("x={fn()}");
```

Bunu şöyle üretmelisin:

```c
temp0 = fn();
printf("x=%d", temp0);
```

Çünkü expression 1 kez evaluate edilmeli.

---

# 🔥 echo Format Map

| Nexus     | printf      |
| --------- | ----------- |
| `{x}`     | type lookup |
| `{x:.2f}` | `%.2f`      |
| `{x:d}`   | `%d`        |
| `{x:s}`   | `%s`        |
| `{x:x}`   | `%x`        |

---

# 🎯 Kritik Karar

echo iki şekilde üretilebilir:

### Yol A – printf'e çevir

En basit ve güçlü.

### Yol B – Kendi writer fonksiyonları

Bare-metal için daha iyi.

Ben hibrit öneririm:

```
if target == os:
   printf dönüşümü
else:
   internal writer
```

---

# ⚠ Edge Case

Şu desteklenecek mi?

```nx
echo("{fn():.2f}");
```

Yani fonksiyon dönüşü float ise precision uygulanacak.

Bu compile-time type resolution gerektirir.
Ama zaten type checker var.

Bu yapılabilir.

---

# 💎 Güçlü ama Sade echo Özeti

* Expression var
* Format var
* Tek expression
* Side-effect safe
* Compile-time parse
* Runtime sadece evaluate

Bu mimari:

✔ Güçlü
✔ Basit
✔ Optimize edilebilir
✔ Güvenli

---

* echo için tam grammar
* AST node tasarımı
* Codegen pseudo
* Type resolver akışı
