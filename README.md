# OMNI CORE v2 NEXUS FLOW - REFERENCE GUIDE
**Version:** 1.1 (gama)  
**Architecture:** Nexus Flow & Handler-Oriented Memory  
**Compiler:** OCC (Omni Core Compiler)
**Language:** Nexus Flow


## OmniCore SDK — Geliştirme Takibi

Derleyicinin güncel geliştirme yönü ve çalışma durumu aşağıdaki kök belgelerde tutulur:

- [Proje durumu](PROJECT_STATUS.md)
- [OCC pipeline ve 99 AST düğümü ilerleme tablosu](OCC_PIPELINE_PROGRESS.md)
- [Geliştirme yol haritası](ROADMAP.md)
- [Güncel görevler](TASKS.md)
- [Mimari kararlar](ARCHITECTURE_DECISIONS.md)
- [AREA / ZONE mantıksal bellek modeli](AREA_ZONE_MEMORY_MODEL.md)
- [Önerilen klasör yapısı](PROPOSED_LAYOUT.md)
- [Target modeli ve seçim kuralları](TARGET_MODEL.md)
- [Nexus Flow spec inceleme raporu](SPEC_AUDIT.md)
- [Intent System — Tasarım Notu](INTENT_SPEC.md)
- [Modüler derleyici pipeline mimarisi](COMPILER_PIPELINE.md)
- [OIR v1 ortak artifact formatı](OIR_FORMAT.md)
- [OIR_MODULE — SSA tabanlı ara temsil taslağı](OIR_MODULE.md)
- [OIR_LOWERING_PLAN — IR Lowering aşamalı uygulama planı](OIR_LOWERING_PLAN.md)
- [FRONTEND_COMPLETENESS_AUDIT — Lexer, Parser, AST ve Semantic tamlık denetimi](FRONTEND_COMPLETENESS_AUDIT.md)
- [NEXSUS syntax ↔ AST coverage matrix](NEXSUS_AST_SYNTAX_MATRIX.md)
- [NEXUS Flow Syntax Reference](docs/language/syntax-reference.md)
- [OIR SOURCE_UNIT, TOKEN_STREAM ve DIAGNOSTICS payload şemaları](OIR_PAYLOAD_SCHEMAS.md)
- [OIR token ID registry](OIR_TOKEN_REGISTRY.md)
- [OIR AST node ve field ID registry](OIR_AST_NODE_REGISTRY.md)
- [Diagnostic ve compilation context sözleşmesi](DIAGNOSTICS_AND_CONTEXT.md)
- [Mevcut pipeline geçiş denetimi](PIPELINE_MIGRATION_AUDIT.md)
- [Derleyici kaynak ve kapasite sınırları](COMPILER_RESOURCE_LIMITS.md)

**Uygulama kararı:** Bu çalışma dalında tek etkin derleyici uygulaması C dilindedir. Kaynak kodu, build sistemi ve testler C tabanlıdır. OmniCore; Windows, Linux ve macOS için, seçilebilir CPU mimarisi ve isteğe bağlı CPU yetenekleriyle hedef üretmeyi amaçlar. Açık hedef seçilmezse host OS + host architecture varsayılan alınır. Gerçek platform desteği ilgili backend ve testlerle doğrulanır.

---

## Burada Bir dilin ve o dile ait bir Derleyicinin tam tasarım planları bulunmaktadadır. Umarım bu bilgiler kendi dil ve derleyicilerini tasarlayan kişiler için öğretici ve bilgilendirici olur. Eğer buradaki bilgiler içerisinde herhangi bir öneriniz veya eleştiriniz var ise paylaşmaktan çekinmeyin.
---

## Nexus Flow dil belgeleri

Nexus Flow sözdizimi ve dil-semantik kuralları tek bir başvuru belgesinde tutulur: [NEXUS FLOW — Syntax Reference Guide](docs/language/syntax-reference.md).

Bu README proje tanımı, OCC mimarisi, geliştirme durumu ve yol haritası için kullanılır. Dil referansı burada kopya olarak tutulmaz; böylece README ile syntax belgesi arasında zamanla farklılaşan iki ayrı normatif kaynak oluşmaz.

Compiler geliştirme sırası AST → Parser → Lexer → Semantic → OIR → Verifier olarak korunur. Codegen, önceki aşamaların gereksinimleri ve doğrulamaları kapanmadan genişletilmez.
