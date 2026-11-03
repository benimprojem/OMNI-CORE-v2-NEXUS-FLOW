# OmniCore SDK — Klasör Yapısı ve Taşıma Planı

31 Ekim 2026 tarihinde ilk fiziksel düzenleme yapıldı. Bu belge mevcut gerçek konumu ve sonraki hedef düzeni birbirinden ayırır.

## Şu anki yapı

```text
repository/
├── temp/
│   ├── legacy-c/                # Kullanılmayan eski standalone C entry point
│   ├── legacy-tests/            # CMake dışındaki eski .nx test girdileri
│   ├── legacy-docs/             # Eski komut notları ve tarihsel belgeler
│   ├── build-artifacts/         # Eski EXE/DLL/A/OBJ/OIR/ASM ürünleri
│   └── duplicate-docs/          # Byte-for-byte aynı fazladan belge kopyaları
└── omnicore-sdk/
    ├── src/
    │   ├── c/                   # Etkin ve kanonik C derleyici kaynakları
    │   │   ├── main.c
    │   │   ├── parser/
    │   │   ├── codegen/
    │   │   ├── lang/
    │   │   └── tests/
    │   └── oce/                 # Ayrı IDE/editor projesi
    ├── targets/
    ├── stdlib/
    ├── dpslib/
    ├── docs/
    ├── CMakeLists.txt
    ├── build_c.bat
    └── build_c.sh
```

CMake etkin kaynak kökü olarak omnicore-sdk/src/c/ kullanır. Çalışma dalında yalnızca C derleyici kaynakları ve C build/test belgeleri tutulur.

## Sonraki hedef düzen

```text
omnicore-sdk/
├── src/
│   ├── c/
│   │   ├── occ/                 # CLI ve derleme orkestrasyonu
│   │   ├── lexer/               # Lexer
│   │   ├── parser/              # Parser ve AST
│   │   ├── semantic/            # Scope, type, ownership, diagnostics
│   │   ├── ir/                  # Kanonik OIR reader/writer
│   │   ├── optimizer/           # OIR pass'ları
│   │   ├── codegen/             # Backend altyapısı
│   │   └── support/             # Config, path, logging
│   └── oce/                     # IDE/editor; compiler'dan ayrı
├── targets/
├── stdlib/
├── dpslib/
├── tests/                       # İleride modül bazlı test ayrımı
├── docs/
└── tools/
```

Bu modül ayrımı henüz uygulanmadı. Mevcut src/c/parser/ içinde lexer, parser, AST ve semantic dosyaları birlikte duruyor; bunları ayırmak include ilişkileri ve CMake hedefleri ayrıca düzenlenerek yapılacak.

## Taşıma ilkeleri

1. C tek kanonik derleyici uygulamasıdır.
2. C++ referans kaynakları temp/legacy-cpp/ altında korunur; silinmez.
3. temp/build-artifacts/ içindeki dosyalar geçmiş çıktılardır; aktif build tarafından yüklenmez.
4. IDE/editor src/oce/ compiler frontend ile karıştırılmaz.
5. Yalnızca byte-for-byte aynı belgelerin fazladan kopyaları arşivlenir. İçeriği farklı belgeler sırf adları benziyor diye birleştirilmez.
6. Her sonraki taşıma aynı değişiklikte CMake/build betikleri, runtime yolları ve testlerle birlikte güncellenir.

## Doğrulama kontrol listesi

- [x] C kaynak kökü src/c/ konumuna taşındı ve CMake yolu güncellendi.
- [x] C-only çalışma alanı temizliği yapıldı; C dışı compiler kaynakları ve arşivleri bu daldan kaldırıldı.
- [x] Eski derleme ürünleri kaynak klasörlerinden ayrıldı.
- [x] Aynı belge kopyaları içerik SHA değerleriyle tespit edilip arşivlendi.
- [x] Windows, Linux ve macOS temiz checkout build/test doğrulandı; üçünde de 10/10 CTest geçti.
- [x] Valgrind CI işi definite/indirect leak denetimiyle doğrulandı; 10/10 test geçti.
- [ ] C modüllerinin ayrı klasörlere bölünmesi sonraki aşamada yapılmalı.

## Target descriptor alanları

Hedef tanımları aşağıdaki kavramları birbirine karıştırmamalıdır:

- os: windows, linux, macos
- arch: x86_64, aarch64 vb.
- abi: platform ABI / calling convention
- format: PE/COFF, ELF, Mach-O
- cpu_model: isteğe bağlı CPU modeli
- features: isteğe bağlı SIMD/ISA yetenekleri
- assembler, linker: kullanılacak araçlar ve seçenekleri
- backend: bu kombinasyon için gerçek codegen uygulaması

Target descriptor'ın bulunması, ilgili backend'in uygulandığı veya test edildiği anlamına gelmez.
