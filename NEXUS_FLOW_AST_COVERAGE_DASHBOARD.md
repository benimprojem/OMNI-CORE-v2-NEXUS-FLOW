# NEXUS Flow — 8 Başlıkta Syntax → AST → Verifier Durum Tablosu

**Dal:** `refactor/c-core-roadmap`  
**Güncelleme:** 2026-11-19  
**Semantic görev planı:** [SEMANTIC_COMPLETION_PLAN.md](SEMANTIC_COMPLETION_PLAN.md)  
**Amaç:** Dil referansında kayıtlı syntax yapılarını sekiz başlıkta görünür kılmak; her yapının Lexer, Parser, AST/Codec, Semantic, OIR Lowering ve bağımsız Verifier aşamalarında nerede olduğunu göstermek.

> Bu belge bir tamamlanma ilanı değildir. Durumlar aktif C dalındaki syntax referansı, syntax/AST matrisi, AST düğüm envanteri, OIR kayıtları, ilerleme kaydı ve mevcut test kanıtları birlikte değerlendirilerek verilmiştir. Bir aşamanın tamamlanması sonraki aşamaları otomatik olarak tamamlamaz.

## Durum anahtarı

| İşaret | Anlamı |
|---|---|
| ✓ | Bu satır/aşama için uygulama ve ilgili test kanıtı var |
| ~ | Kısmi; eksik kural, test, lowering veya entegrasyon var |
| ? | Satır bazında doğrulanmamış; tamamlanmış sayılmaz |
| D | Tasarım gereği ertelendi; aktif runtime syntax olarak sayılmaz |
| X | Core Parser tarafından açıkça desteklenmiyor/reddediliyor |
| — | Bu aşama için uygulanamaz |

**Sütunlar:** L = Lexer, P = Parser, A = AST payload + lifecycle + codec, S = Semantic, O = OIR lowering, V = bağımsız OIR Verifier.  
**Önemli:** Verifier ham AST almaz. AST'den Semantic AST'ye, oradan OIR'a indirgenen yürütülebilir davranışı doğrular. Bu nedenle O sütunu kapanmadan V sütunu da o syntax özelliği için kapanmış sayılamaz.

## CI verification checkpoint — 2026-11-19

- Branch: `refactor/c-core-roadmap`
- Verified commit: `3e15b23cef36fccb713b629738a48bd7138c77d6`
- GitHub Actions run: `44415847403`
- Result: all five jobs passed — Ubuntu build/tests, Windows build/tests, macOS build/tests, ASan/UBSan and Valgrind.
- CTest result: 73/73 passed on the Ubuntu test job.
- This checkpoint verifies the current pointer/reference, callback nullability, container-storage rejection and control-flow initialization regressions. It does not close the remaining Semantic rows, OIR or Verifier work.

