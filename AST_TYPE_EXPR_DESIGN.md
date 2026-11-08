# AST Type Expression Model — Design Draft

**Status:** Draft / AST implementation not yet started  
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

Exact C structs and node IDs will be finalized after reviewing AST ownership conventions and parser token grammar. Avoid one tag per primitive scalar; primitive names stay in named-type payload.

## 4. Integration targets

Structured type expressions should eventually be attached to:

- variable declarations and parameters;
- function and EXF parameter/return signatures;
- callback/function type declarations and nested callbacks;
- NewType targets;
- array element types and dimensions;
- casts and other explicit type operands where current AST uses text or expression-shaped placeholders.

EXF additionally needs external symbol name and the declared linkage source relationship. The existing `AST_EXTERN_DECL`/`ASTFuncDecl` reuse must be assessed before adding fields or nodes; `linkname` directive parsing is separate from merely having an EXF tag.

## 5. Migration sequence

1. Audit all type-bearing fields and parser functions; create a field-to-type-expression inventory.
2. Freeze the C representation and ownership table; append node tags only after this review.
3. Add constructors/destructor/validator/serializer/deserializer/debug-printer/schema support.
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
