# OmniCore SDK — Önerilen Klasör Yapısı

Bu belge hedef düzeni tanımlar. 31 Ekim 2026 tarihinde ilk güvenli fiziksel düzenleme yapıldı: etkin C kaynağı `omnicore-sdk/src/c/` altına taşındı, eski C++ kodu `temp/legacy-cpp/` altına alındı ve CMake kaynak yolu aynı değişiklikte güncellendi. Modül bazlı C alt dizinlerine ayrıştırma henüz yapılmadı.

## Hedef yapı

~~~text
omnicore-sdk/
├── README.md
├── occ.ocf
├── occ.nxr
├── build/
│   ├── scripts/
│   └── cmake/
├── src/
│   ├── c/
│   │   ├── occ/          # CLI ve derleme orkestrasyonu
│   │   ├── lexer/        # Tokenizer
│   │   ├── parser/       # Parser ve AST üretimi
│   │   ├── semantic/     # Scope, type, ownership ve diagnostics
│   │   ├── ir/           # Kanonik OIR reader/writer
│   │   ├── optimizer/    # OIR pass'ları
│   │   ├── codegen/      # Ortak backend altyapısı
│   │   └── support/      # Config, path, logging, diagnostics
│   ├── cpp-reference/    # Korunan C++ referans uygulaması
│   └── oce/              # IDE/editor (ayrı ürün sınırı)
├── include/
│   └── omnicore/         # Dışarı açılan kararlı C API başlıkları
├── targets/
│   ├── windows/
│   │   ├── x86_64/
│   │   └── aarch64/
│   ├── linux/
│   │   ├── x86_64/
│   │   └── aarch64/
│   └── macos/
│       ├── x86_64/
│       └── aarch64/
├── stdlib/
├── dpslib/
├── tests/
│   ├── lexer/
│   ├── parser/
│   ├── semantic/
│   ├── ir/
│   ├── codegen/
│   └── integration/
├── examples/
├── docs/
├── tools/
│   └── scripts/
├── bin/                  # Üretilmiş araçlar; kaynak değil
└── lib/                  # Üretilmiş kütüphaneler; kaynak değil
~~~

## Taşıma kuralları

1. Mevcut src - c - 28-01-26/ önce arşiv/başlangıç kaynağı olarak korunur.
2. C kaynakları yeni src/c/ yapısına taşınırken include yolları ve build scriptleri aynı commit içinde güncellenir.
3. C++ kaynakları silinmez; src/cpp-reference/ konumuna ancak bağımlılıkları ve IDE kaynakları ayrıştırıldıktan sonra taşınır.
4. oce/ IDE dosyaları compiler frontend ile karıştırılmaz.
5. Binary dosyalar kaynak dizinlerinde tutulmaz. Ancak hangi kaynakla üretildiği bilinmeyen mevcut binary'ler doğrulama tamamlanana kadar silinmez.
6. Test girdileri, beklenen çıktılar ve gerçek derleme çıktıları farklı dizinlerde tutulur.
7. Eski yol için geçici uyumluluk notu bırakılır; tüm tüketiciler güncellenmeden eski klasör kaldırılmaz.

## İlk fiziksel taşıma öncesi kontrol listesi

- [ ] Tüm include ilişkileri çıkarıldı.
- [ ] DLL/EXE yükleme yolları çalışma dizininden bağımsız hale getirildi.
- [ ] Build betikleri yeni yollarla çalışıyor.
- [ ] Test dosyaları yeni konumda çalışıyor.
- [ ] Temiz checkout build ve test geçti.
- [ ] Eski yolun kaldırılması/arsivlenmesi için ayrı commit hazırlandı.


## Target descriptor alanları

Hedef dosyaları aşağıdaki kavramları birbirine karıştırmadan tarif etmelidir:

- `os`: windows, linux, macos
- `arch`: x86_64, aarch64 vb.
- `abi`: platform ABI / calling convention
- `format`: PE/COFF, ELF, Mach-O
- `cpu_model`: isteğe bağlı model/seri
- `features`: isteğe bağlı SIMD/ISA özellikleri
- `assembler`, `linker`: kullanılacak araçlar ve seçenekleri
- `backend`: bu kombinasyon için gerçek codegen uygulaması

Bu ağaç hedef düzen önerisidir; mevcut target dosyalarının bugün bu alanların tamamını içerdiği anlamına gelmez. Önce mevcut target dosyalarının şeması çıkarılacak, sonra geriye uyumlu geçiş tasarlanacaktır.
