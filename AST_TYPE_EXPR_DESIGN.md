# AST Type Expression Model — Design Draft

**Status:** Implementation phase 1 — recursive node, declaration attachment fields, AST codec v4 and OIR AST handoff are in progress; Parser population for named/pointer/reference and function/callback types is implemented; array-wrapper population and full integration tests remain open  
**Branch:** `refactor/c-core-roadmap`  
**Scope:** C AST + parser representation only. Semantic meaning, ABI validation and lowering remain later stages.

## 1. Problem

The current AST stores type information in text fields (`ASTVarDecl.type_name`, `ASTFuncDecl.return_type`, `ASTNewTypeDecl.target_type`). Callback/function signatures are normalized strings. `AST_ARRAY_TYPE` and `AST_ARRAY_DIMENSION` already model array dimensions, but are not a unified recursive type representation.

This makes nested combinations hard to represent structurally and forces consumers to reparse strings.

## 2. Design constraints

- Preserve all existing AST node numeric IDs (append-only).
- Keep existing text fields during migration for source/API compatibility; new structured fields are authoritative when populated.
- No Semantic decisions in Parser: type syntax is represented, not validated for ABI compatibility or borrow/lifetime correctness.
- No implicit copying or ownership changes. Type-expression child nodes have unique ownership like the rest of AST.
- Source spans must cover the exact type syntax, including modifiers and nested delimiters.
- AST serialization/deserialization, graph validation, destruction, debug print and OIR AST field schema must all recognize added nodes/edges before adoption is considered complete.
- Existing syntax remains authoritative; do not invent a new surface grammar in this task.

## 3. Proposed recursive node family (append-only)

Introduce a common `AST_TYPE_EXPR` root tag and explicit kinds represented by append-only tags:

- **Named / primitive:** identifier or qualified type name (e.g. `i32`, `MyType`, `module.Type`).
- **Pointer:** one owned child type; preserve pointer depth structurally (`*T`, `**T`).
- **Reference:** one owned child type; preserve reference form (`&T`, `&&T` if syntax permits).
- **Array:** one owned element type plus ordered dimension nodes; dimension expression may be absent for dynamic/unspecified extent.
- **Function type:** ordered parameter type list, optional variadic marker, and one return type child. Callback/function type must remain distinct from callable expression/lambda nodes.
- **Qualifier/modifier:** preserve hard-typing/coercion markers and any type-local syntax modifiers without conflating them with the underlying named type.

Implemented append-only `AST_TYPE_EXPR` with `ASTTypeExprKind` values `NAMED`, `POINTER`, `REFERENCE`, `ARRAY`, `FUNCTION`, and `QUALIFIED`. Primitive names stay in the named-type payload; wrapper nodes own their child/parameter/dimension subtrees. OIR kind 108 is reserved for this node.

## 4. Integration targets

Structured type expressions are being attached to:

- variable declarations and parameters (`ASTVarDecl.type_expr`);
- function and EXF return signatures (`ASTFuncDecl.return_type_expr`); parameters use `ASTVarDecl.type_expr`;
- callback/function type declarations and nested callbacks;
- NewType targets (`ASTNewTypeDecl.target_type_expr`);
- array element types and dimensions;
- casts and other explicit type operands where current AST uses text or expression-shaped placeholders.

EXF additionally needs external symbol name and the declared linkage source relationship. The existing `AST_EXTERN_DECL`/`ASTFuncDecl` reuse must be assessed before adding fields or nodes; `linkname` directive parsing is separate from merely having an EXF tag.

## 5. Migration sequence

1. Audit all type-bearing fields and parser functions; create a field-to-type-expression inventory.
2. Freeze the C representation and ownership table; append node tags only after this review.
3. Add constructors/destructor/validator/serializer/deserializer/debug-printer/schema support. **Node core and declaration attachment fields are now implemented; Parser population is still pending.**
4. Add parser construction for the existing syntax (no grammar redesign).
5. Add positive, malformed, nested, ownership and allocation-failure tests.
6. Keep legacy string fields synchronized during transition and verify codec round-trip.
7. Update syntax reference, AST matrix and `OCC_PIPELINE_PROGRESS.md`.
8. Mark this work complete only after CI and parser/codec parity pass.

## 6. Explicitly out of scope

- Semantic type resolution, overload selection, coercion rules and compatibility checks.
- Pointer safety, nullability, mutability, aliasing, borrow/lifetime and dangling-reference analysis.
- External ABI/calling convention/object-format checks and OCL linker implementation.
- Codegen/backend representation.

## 7. Implementation checkpoint (2026-11-08)

- Added append-only `AST_TYPE_EXPR` and the six type-expression kinds. Ownership, destruction, validation, debug printing and AST stream v4 serialization/deserialization are implemented.
- Added optional structured type slots to `ASTVarDecl`, `ASTFuncDecl` and `ASTNewTypeDecl`; legacy string fields remain intact. AST stream v1–v3 readers remain supported, and pre-v4 streams reject the new node tag.
- Added OIR node kind 108, schema fields, child traversal, writer/reader reconstruction and an OIR round-trip test using a pointer-to-`i32` variable type.
- **Not complete:** Parser populates named/pointer/reference and function/callback type trees, but array declarators still use the legacy `AST_ARRAY_TYPE` side field rather than an `AST_TYPE_ARRAY` wrapper. Function-type child spans currently use the encompassing signature span rather than individual parameter token spans. Malformed nested-type tests, type-builder allocation-failure coverage and full CI must pass before this design can be marked complete.

## 8. Parser integration checkpoint (2026-11-08)

- `parser_build_type_expr` now creates `NAMED`, `POINTER`, `REFERENCE`, and hard-type `QUALIFIED` nodes for variable declarations, function parameters/returns and NewType targets.
- Callback/function type spellings `f:(name!Type, other:Type)!ReturnType` and legacy `f:(...)>!ReturnType` now produce `FUNCTION` nodes with parameter `ASTVarDecl` children and a recursive return-type child. The original canonical strings remain populated for compatibility.
- Parser regression coverage now asserts pointer/reference trees, hard qualifiers, callback parameter trees and function-type aliases. OIR round-trip coverage includes a callback tree.
- Array type wrappers and precise per-parameter spans remain the next gaps; do not mark recursive type expressions complete yet.
