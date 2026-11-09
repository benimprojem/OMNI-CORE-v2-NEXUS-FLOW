# AST Type Expression Model — Design Draft

**Status:** Implementation phase 3 — recursive node, Parser construction for named/pointer/reference/qualified/function/array types, AST codec v4 and OIR AST handoff are implemented; CI run `42497533312` passed all five jobs (Ubuntu CTest 60/60); callback-array composition, precise parameter spans and allocation-failure coverage remain open  
**Branch:** `refactor/c-core-roadmap`  
**Scope:** C AST + parser representation only. Semantic meaning, ABI validation and lowering remain later stages.

## 1. Problem

The current AST stores type information in text fields (`ASTVarDecl.type_name`, `ASTFuncDecl.return_type`, `ASTNewTypeDecl.target_type`). Callback/function signatures are normalized strings. `AST_ARRAY_TYPE` and `AST_ARRAY_DIMENSION` already model array dimensions, but are not a unified recursive type representation.

This makes nested combinations hard to represent structurally and forces consumers to reparse strings.

## 2. Design constraints

- Preserve all existing AST node numeric IDs (append-only).
- Keep existing text fields during migration for source/API compatibility; new structured fields are authoritative when populated.
- No Semantic decisions in Parser: type syntax is represented, not validated for ABI compatibility or borrow/lifetime correctness.
- No implicit copying or ownership changes. Type-expression child nodes have unique ownership like the rest of AST. Legacy array fields are borrowed aliases only when the recursive array wrapper owns the same `AST_ARRAY_TYPE` node.
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
3. Add constructors/destructor/validator/serializer/deserializer/debug-printer/schema support. **Node core and declaration attachment fields are implemented.**
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

- `parser_build_type_expr` creates `NAMED`, `POINTER`, `REFERENCE`, and hard-type `QUALIFIED` nodes for variable declarations, function parameters/returns and NewType targets.
- Callback/function type spellings `f:(name!Type, other:Type)!ReturnType` and legacy `f:(...)>!ReturnType` produce `FUNCTION` nodes with ordered parameter `ASTVarDecl` children and a recursive return-type child. Nested callback parameter signatures are recursively represented. The syntax reference now documents `nt:` aliases, direct callback parameters, nested callback signatures, and the distinction between a function type and a `>` lambda body.
- Array declarators now receive an `AST_TYPE_ARRAY` wrapper. Its `child` is the element type expression and its `dimensions` owns the legacy `AST_ARRAY_TYPE` dimension subtree.
- During migration, `ASTVarDecl.inline_type` and `ASTFuncDecl.return_array_type` remain available as borrowed aliases to the array node owned by the recursive type tree. They are not independent ownership edges:
  - AST destruction and validation skip the legacy edge when it aliases the type-expression array node.
  - AST stream v4 writes the array subtree once and reconstructs the borrowed alias on decode; v1–v3 compatibility remains unchanged.
  - OIR frontend indexing/writing omits the duplicate legacy reference; OIR reading restores the alias after all owned edges are linked.
- Parser tests assert array wrappers and legacy alias identity through AST codec round-trip. OIR round-trip tests assert array wrapper/alias identity for local arrays and function array returns.
- CI run `42497533312` passed Windows, macOS, Ubuntu, ASan/UBSan and Valgrind (5/5); Ubuntu CTest 60/60.
- Remaining: callback function-type array parameter/return composition, precise per-parameter type spans, malformed nested-type and deterministic type-builder allocation-failure coverage. Recursive Type Expression is not yet declared complete.

## 9. Callback array composition checkpoint (2026-11-09)

- Function-type aliases containing array dimensions now build recursive AST_TYPE_EXPR trees instead of skipping target_type_expr when the signature contains '['.
- Callback parameters support fixed dimensions, multidimensional dimensions and dynamic [] dimensions. Callback return arrays are represented as a hard-qualified return type whose child is AST_TYPE_ARRAY.
- Fixed dimension AST_LITERAL payloads preserve both int_value and NUL-terminated string_value. Invalid nonnumeric dimensions, missing closing brackets and signed-64-bit overflow are rejected with parser diagnostics.
- AST graph validation accepts AST_VARIADIC_PARAM under AST_TYPE_FUNCTION.params and continues to require the variadic marker to be final.
- Parser tests cover callback alias/parameter arrays, return arrays, AST codec round-trip, malformed dimensions and variadic callback round-trip. OIR AST kind 108 continues to carry type-expression trees.
- CI run 42695649641 passed Windows, macOS, Ubuntu, ASan/UBSan and Valgrind (5/5); Ubuntu CTest 60/60. Follow-up removal of temporary test diagnostics is tracked in CI run for commit b5e03f2.
- Remaining: exact per-parameter source spans, deterministic allocation-failure coverage for nested type construction, and Semantic callback compatibility. This design and AST stage remain open; no Codegen scope is started.