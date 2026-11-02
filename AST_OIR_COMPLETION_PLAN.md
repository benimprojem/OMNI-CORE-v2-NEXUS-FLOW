# AST / OIR tamamlanma planı — Codegen ön koşulu

**Durum:** Aktif plan  
**Kural:** AST, semantic AST ve OIR eksikleri kapanmadan Codegen kapsamı genişletilmeyecek. Mevcut Codegen yalnızca pipeline testleri ve dar prototip doğrulaması için kullanılabilir.

## Katmanlar ve mevcut bileşenler

| Katman | Mevcut bileşen | Tamamlanma ölçütü |
|---|---|---|
| Kaynak → AST | `parser/lexer.c`, `parser/parser.c`, `parser/ast.c` | Dil token/syntax envanteri ile AST tag/field sözleşmesi birebir eşleşir; pozitif/negatif parse testleri |
| AST güvenliği | AST serializer/deserializer, `ast_destroy` | Her node alanı round-trip; partial tree cleanup; ownership ve OOM testleri |
| AST sözleşmesi | `parser/ast.h`, AST field schema/TLV | Her node'un zorunlu/opsiyonel child ve sibling ilişkileri, source span ve ownership tanımı |
| Semantic | `parser/semantic.c`, `semantic_result.c` | Scope, symbol, type, conversion, control-flow ve ownership kuralları testli; hata halinde sonuç üretimi engellenir |
| Semantic AST artifact | `oir_semantic_ast_artifact.*`, `oir_semantic_ast.*` | AST graph + scope/type/symbol/annotation/relation/conversion tabloları karşılıklı tutarlı ve doğrulanmış |
| AST → OIR | `oir_lowering.*`, `oir_lowering_expr.c` | AST node/opcode coverage matrisi; desteklenmeyen yapı açıkça fail-closed; tüm desteklenen örneklerde round-trip/semantic test |
| OIR contract | `oir_module.*`, opcode registry, artifact codec | Canonical schema, operand/result/side-effect/control-flow kuralları ve sürüm politikası sabit |
| OIR Verifier | `oir_verify_stage_module.c` | Her OIR record/opcode invariantı ve çapraz referans doğrulanır; güvenli bağımsız hata aggregation testleri |
| Pipeline | Parser → Semantic → AST artifact → Lowering → Verifier → Optimizer → Verifier → Codegen | Hatalı ara ürün hiçbir sonraki aşamaya ulaşmaz; producer/consumer ve artifact immutability testleri |

## Öncelikli açık işler

### A0. AST field ownership baseline
- [x] Add `AST_NODE_FIELD_OWNERSHIP.md`: AST family/struct map, child and string ownership expectations, sibling-chain invariant and cross-component checklist.
- [ ] Generate a machine-checked per-tag coverage report from AST enum, allocation/destroy/print/codec, OIR schema and parser switches; inventory document is baseline, not proof of exhaustive parity.
- [ ] Add byte start/end spans and end line/column consistently to all nodes/artifacts (schema/version impact review required).
- [ ] Stress parser error/OOM cleanup and shared-child rejection for every owned node family.

### A. AST sözleşmesi ve kapsam
- [ ] `ASTNodeType` envanterini parser üretimi, serializer/deserializer, destroy, printer ve field schema ile karşılaştır.
- [ ] Her node için child field, list/sibling, ownership ve nullability tablosu oluştur.
- [ ] AST source span: byte offset + length ve line/column alanlarının tüm node'larda tutarlı taşınmasını sabitle.
- [ ] AST destroy ve parser hata/OOM yollarında tek sahiplik kurallarını doğrula.
- [ ] Parser recovery ve kısmi AST cleanup testlerini tamamla.

### A1. Array type declarators — implementation added, verification in progress
- [x] Append stable AST nodes `AST_ARRAY_TYPE` and `AST_ARRAY_DIMENSION` without changing existing node IDs.
- [x] Represent ordered dimensions as child nodes; a null length means dynamic/unspecified extent.
- [x] Parse declaration suffixes (`v:name[n][m]!T`) and parameter forms (`a![]`, `a!T[]`, `a:T[n]`).
- [x] Extend AST create/print/destroy/binary codec and OIR frontend map/schema/writer/reader/graph traversal.
- [x] Add semantic fixed-dimension validation and aggregate array type records with element base type and shape name.
- [x] Add parser, AST codec and OIR frontend round-trip fixtures.
- [ ] Confirm the full Windows/Linux/macOS CI matrix is green after the latest parser diagnostic and registry-test fixes.
- [x] Extend fixed dimensions to validated compile-time integer arithmetic expressions (`+`, `-`, `*`, `/`, `%`) with overflow and divide-by-zero rejection.
- [x] Validate array literal leaf element types against declared element types and check fixed shape/rank compatibility.
- [x] Add semantic-result integration assertions for array shape/base-type records.
- [ ] Add full semantic AST artifact decode assertions and function-call argument shape compatibility.
- [ ] Define executable array layout/indexing/allocation lowering; until then unsupported lowering must fail closed.

### A2. Array index access — AST/semantic complete, OIR design bounded
- [x] Append dedicated `AST_INDEX_ACCESS` node with separate target/index child ownership; keep existing AST tag IDs stable.
- [x] Parse postfix `target[index]` and support chained syntax structurally.
- [x] Update AST allocation, printer, destroy, serialization and deserialization paths; add codec fixture using the dedicated node shape.
- [x] Semantic check: target must be array, index must be integer; index expression is annotated as an lvalue and element type is propagated.
- [x] Add valid/invalid parser-semantic fixtures and diagnostic assertions.
- [x] Decide checked bounds, left-to-right single evaluation, row-major offset and checked overflow in `OIR_LOWERING_PLAN.md`.
- [ ] Register checked-bounds/address-offset OIR instructions and verifier contracts before lowering indexed reads/writes.
- [ ] Implement full-rank index lowering to typed `LOAD`/`STORE`; keep partial-rank views disabled until ownership/lifetime is modeled.

### B. Semantic tamamlanması
- [ ] Token/dil spec ile semantic kurallarının tam matrisi.
- [ ] Scope açma/kapama, shadowing, duplicate declaration, visibility/export ve label kapsamı.
- [ ] Primitive/user/aggregate/function/tuple type çözümleme; recursive/forward declaration politikası.
- [ ] Numeric promotion, implicit/explicit conversion ve hard type kuralları.
- [ ] Function signature, argument/return uyumu; extern ve method/group kuralları.
- [ ] Struct/union/enum/newtype alan ve varyant ilişkileri.
- [ ] Handler/area/zone/ownership/transfer kuralları.
- [ ] Return/break/continue, unreachable code ve CFG ön koşulları.
- [ ] SemanticResult bütün referanslarının AST node ve scope tablolarına bağlanması.

### C. Lowering ve OIR
- [ ] AST tag → lowering desteği tablosu: supported / partial / intentionally unsupported.
- [ ] Semantic annotation/type/symbol bilgisi lowering'de eksiksiz kullanılır.
- [ ] Function parameters, calls, locals, assignment, memory, aggregate, cast ve control flow lowering kapsamı.
- [ ] NexusFlow özel AST node'ları için lowering/side-effect/control-flow sözleşmesi.
- [ ] OIR opcode registry: shape, types, effects, terminator, memory behavior, call signature ilişkisi.
- [ ] Source map/debug metadata politikası.
- [ ] OIR verifier opcode ve tablo invariantları için tam test matrisi.

### D. Codegen giriş kapısı
Codegen genişletmesi şu koşulların tümü sağlanmadan başlamaz:
- [ ] AST node coverage ve ownership testleri tamamlandı.
- [ ] Semantic feature matrix testleri tamamlandı.
- [ ] Lowering coverage ve fail-closed testleri tamamlandı.
- [ ] OIR schema ve opcode semantics onaylandı.
- [ ] Verifier tüm OIR invariantlarını kapsıyor.
- [ ] End-to-end testte invalid AST/semantic/OIR Codegen'e ulaşamıyor.
- [ ] Her desteklenen küçük program için OIR → Codegen çıktısı ve davranış testi mevcut.

## Çalışma sırası

1. AST node/field/ownership envanteri.
2. AST serializer/deserializer/destroy/parser tutarlılığı.
3. Semantic type/scope/function/ownership/control-flow eksikleri.
4. AST → semantic artifact referans bütünlüğü.
5. Lowering kapsamı ve OIR sözleşmesi.
6. Verifier tamlığı ve test matrisi.
7. Pipeline gate testleri.
8. Codegen backend kapsamı.

**Not:** Bir özellik henüz desteklenmiyorsa sessizce yanlış OIR üretmek yerine lowering/semantic aşamasında açık ve konumlu hata vermelidir.
