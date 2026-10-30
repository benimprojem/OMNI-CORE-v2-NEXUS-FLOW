# OmniCore SDK

OmniCore SDK, NexusFlow kaynak kodunu analiz etmek ve hedef platformlara taşınabilir çıktılar üretmek için geliştirilen araç zinciridir.

**Aktif karar:** C uygulaması ana kaynak ve birincil geliştirme hattıdır. C++ uygulaması silinmeyecek; karşılaştırma ve kontrollü özellik aktarımı için korunacaktır.

## Durum ve yön

Bu depo henüz tamamlanmış, çoklu hedef destekli bir derleyici değildir. Lexer, parser, AST, semantik analiz ve sınırlı codegen parçaları vardır. Modüllerin var olması, tüm dil özelliklerinin doğru işlendiğini veya üretim kalitesinde olduğunu kanıtlamaz.

- [Proje durumu](docs/PROJECT_STATUS.md)
- [Geliştirme yol haritası](docs/ROADMAP.md)
- [Mimari kararlar](docs/ARCHITECTURE_DECISIONS.md)
- [Hedef klasör yapısı](docs/PROPOSED_LAYOUT.md)

## Mevcut kaynaklar

| Yol | Rol | Karar |
|---|---|---|
| src - c - 28-01-26/ | C lexer/parser/AST/semantic/codegen | Ana uygulama adayı; kontrollü taşınma öncesi mevcut yol korunuyor |
| src/ | C++ parser/OIR/analyzer ve OCE | Referans ve karşılaştırma; yeni ana geliştirme hattı değil |
| targets/ | Hedef tanımları | Korunacak; gerçek codegen desteği ayrıca doğrulanacak |
| stdlib/, dpslib/ | Standart ve bağımlılık kütüphaneleri | Korunacak |
| test/ ve C tarafındaki tests/ | Test girdileri ve çıktıları | Tek test düzenine kademeli taşınacak |
| bin/, lib/ | Derleme çıktıları | Kaynak koddan ayrılacak; doğrulama öncesi silinmeyecek |

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

Bu akış hedef mimaridir; tüm adımların bugün uygulanmış olduğu iddia edilmez. OIR sözleşmesi, optimizer, ARM64 backend, x86-64 backend ve linker entegrasyonu tamamlanması gereken işlerdir.

## Geliştirme ilkeleri

1. Önce C ana hattı derlenebilir ve test edilebilir hale getirilir.
2. Dil davranışı ile hedef işlemci davranışı birbirinden ayrılır.
3. OIR; AST serileştirmesinden ve hedefe özel assembly'den bağımsız tanımlanır.
4. Her yeni özellik için pozitif, negatif ve regresyon testleri eklenir.
5. Tasarım kararı, mevcut uygulama ve gelecek hedefi dokümanlarda ayrı etiketlenir.
6. C++ kodu, eşdeğer C davranışı ve testleri olmadan ana hatta taşınmaz.

İlk doğrulama build komutları, yol haritasındaki Faz 0 tamamlanmadan geçerli/kararlı kabul edilmemelidir.
