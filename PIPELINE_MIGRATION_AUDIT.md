# Pipeline Migration Audit

**Date:** 31 October 2026  
**Branch:** `refactor/c-core-roadmap`  
**Scope:** Static inspection of the current C implementation. No source code was changed by this audit.

## Confirmed current structure

| Area | Current implementation | Target architecture |
|---|---|---|
| Orchestration | `main.c` directly calls parser, semantic and codegen | OCC pipeline runner loads stage modules |
| Frontend build | `occ_frontend` is one static library containing AST, lexer, parser and semantic sources | Independently loadable stage modules |
| Codegen | Separate shared library with a direct `codegen_run(ir, target, output)` entry point | Common versioned module ABI |
| Parser state | `ParserContext` combines source buffer, cursor, AST root and parser error list | Per-job compilation context plus stage-local state |
| Semantic state | `SemanticContext` owns a separate error list and symbol table | Shared diagnostic sink and explicit artifact/context ownership |
| Error flow | Parser returns failure on parser errors; OCC skips semantic analysis if parser failed | Recoverable diagnostics accumulate and eligible stages continue |
| Lexer errors | Lexer can return `TOKEN_ERROR`; the current interface does not provide a common diagnostic collector | Lexer reports structured diagnostics and continues where safe |
| Intermediate output | AST serialization is written through `parser_save_ir()` | Versioned artifacts and user-selected checkpoints |
| Multi-job execution | No general per-job pipeline scheduler is present | Independent contexts, reusable modules and bounded concurrent pipelines |

## Specific observations

1. `parser.h` defines `ParserContext` with filename, file handle, source buffer, cursor, line/column, AST root and linked-list errors in one structure.
2. `semantic.h` defines a separate `SemanticContext` with its own error count/list, symbol table and language state.
3. `parser_run()` returns non-zero when its parser instance has recorded an error. The current OCC path calls semantic analysis only when parsing succeeds.
4. `lexer_next_token()` can return `TOKEN_ERROR`; lexer diagnostics are not yet integrated into a job-wide diagnostic sink.
5. CMake builds AST, lexer, parser and semantic together as static target `occ_frontend`. Only `occ_codegen` is a shared library at present.
6. OCC loads codegen using platform-specific loader calls and resolves one function symbol. This is not yet the common module descriptor/ABI defined by the pipeline design.

## Migration risks to address

- Moving `ParserContext` across DLL boundaries before defining ABI and ownership would freeze an unsafe coupling.
- Sharing `Error*` nodes between parser and semantic would make cleanup ownership ambiguous.
- Reusing the current AST serialization as the permanent pipeline artifact would conflate an internal AST checkpoint with the not-yet-designed canonical OIR.
- Continuing after parser errors requires explicit partial-AST validity and parser recovery guarantees; it must not mean semantic analysis blindly trusts a damaged tree.
- A shared module loaded once must not keep per-job source, AST, symbol or diagnostic state in globals.

## Recommended implementation order

1. Introduce a job-owned diagnostic collector and structured diagnostic record.
2. Introduce a compilation context that owns diagnostics, options, source identities and artifact handles.
3. Adapt lexer/parser/semantic to report into that collector while preserving safe cleanup.
4. Add parser recovery tests and partial-AST validity rules; decide which semantic checks can run on recovered AST regions.
5. Define the stable module descriptor and C ABI using opaque handles and explicit release functions.
6. Split modules only after their contracts can be tested without relying on internal struct layout.

## Verification boundary

This is a static code audit. It is not a build/test result. The modular pipeline and diagnostic behavior are design targets, not yet implemented behavior.
