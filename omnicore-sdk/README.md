# OmniCore SDK

OmniCore SDK, NexusFlow kaynak kodunu analiz etmek ve hedef platformlara taşınabilir çıktılar üretmek için geliştirilen araç zinciridir.

**Aktif karar:** Bu SDK çalışma alanı C-only derleyici uygulamasıdır. Kaynak kodu, build sistemi, testler ve compiler belgeleri C hattına aittir.

## Durum ve yön

Bu depo henüz tamamlanmış, çoklu hedef destekli bir derleyici değildir. Lexer, parser, AST, semantik analiz ve sınırlı codegen parçaları vardır. Modüllerin var olması, tüm dil özelliklerinin doğru işlendiğini veya üretim kalitesinde olduğunu kanıtlamaz.

- [Proje durumu](../PROJECT_STATUS.md)
- [Geliştirme yol haritası](../ROADMAP.md)
- [Mimari kararlar](../ARCHITECTURE_DECISIONS.md)
- [Hedef klasör yapısı](../PROPOSED_LAYOUT.md)
- [Target modeli](../TARGET_MODEL.md)
- [C derleyicisi build ve test talimatları](BUILDING.md)

## Mevcut kaynaklar

| Yol | Rol | Karar |
|---|---|---|
| src/c/ | C lexer/parser/AST/semantic/codegen | Etkin ve kanonik uygulama kaynağı |
| targets/ | Hedef tanımları | Korunacak; gerçek codegen desteği ayrıca doğrulanacak |
| occ/libs/ | Önceden tanımlı Nexus Flow standart modülleri | Kanonik standart kütüphane konumu |
| src/c/tests/ | Etkin CTest girdileri ve test programları | CMake tarafından kullanılıyor |
| ../temp/build-artifacts/ | Önceden izlenen derleme çıktıları | Kaynaklardan ayrıldı; silinmedi |

## Platform ve target yaklaşımı

OmniCore Windows, Linux ve macOS üzerinde çalışmayı; x86-64 ve AArch64 gibi işlemci mimarilerini hedeflemeyi amaçlar. Kullanıcı genel CPU mimarisini seçebilir, desteklenen ek CPU özelliklerini belirtebilir veya belirli bir CPU modeli seçebilir. Target verilmezse host OS + host architecture varsayılan olur. Host ve target farklı olduğunda cross-compilation uygulanır. Target tanımı gerçek backend desteğinin kanıtı değildir.

## Hedef derleyici akışı

~~~text
NexusFlow source
    -> Lexer -> Parser -> AST
    -> Semantic analysis
    -> canonical, target-independent OIR
    -> optimization passes
    -> target backend
    -> assembler / linker
    -> executable or library
~~~

Bu akış hedef mimaridir; tüm adımların bugün uygulanmış olduğu iddia edilmez. OIR sözleşmesi, optimizer, Windows/Linux/macOS için x86-64 ve AArch64 backend'leri ve linker entegrasyonu tamamlanması gereken işlerdir.

## Geliştirme ilkeleri

1. Önce C ana hattı derlenebilir ve test edilebilir hale getirilir.
2. Dil davranışı ile hedef işlemci davranışı birbirinden ayrılır.
3. OIR; AST serileştirmesinden ve hedefe özel assembly'den bağımsız tanımlanır.
4. Her yeni özellik için pozitif, negatif ve regresyon testleri eklenir.
5. Tasarım kararı, mevcut uygulama ve gelecek hedefi dokümanlarda ayrı etiketlenir.
6. Compiler değişiklikleri C API, modül sözleşmeleri ve otomatik testlerle doğrulanır.

İlk doğrulama build komutları, yol haritasındaki Faz 0 tamamlanmadan geçerli/kararlı kabul edilmemelidir.
