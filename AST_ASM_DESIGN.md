# Inline ASM AST Design Note (C frontend)

**Status:** design-only; Parser implementation is blocked on this contract.  
**Reference syntax:** `omnicore-sdk/docs/syntax.md`, Section 9.

## Confirmed source forms

```nxf
fastexec {
    v:total = 0;
    asm: CRITICAL_ADD {
        mov rax, %a
        add rax, %b
        mov %total, rax
    }
}
asmcall(CRITICAL_ADD);
asmjmp("MY_ASM_BLOCK");
```

- `fastexec` is a source-level container, not a raw instruction itself.
- `asm: LABEL { ... }` stores raw target-specific instruction text; it is not ordinary Nexus Flow statement syntax.
- `asmcall` and `asmjmp` refer to declared ASM labels and must be validated by Semantic/target stages.
- ASM is valid only in its documented `fastexec` context.
- ASM label names must follow the syntax reference restrictions; no target-specific assumptions belong in Parser.

## Current gap

The Lexer defines `TOKEN_KW_FASTEXEC`, `TOKEN_KW_ASM`, `TOKEN_KW_ASMCALL`, and `TOKEN_KW_ASMJMP`, but `parser.c` does not consume them. `AST_ASM_BLOCK` currently uses the generic `ASTBlock` layout and cannot preserve an ASM label plus raw source text. Therefore the syntax is **not implemented** despite the AST kind existing.

## Proposed AST extension (append-only)

Do not overload `AST_FUNC_CALL` or discard ASM source text.

- Append `AST_ASM_LABEL` after current AST ID 99.
  - C payload: `ASTNode base; char *label; char *source_text;`
  - Both strings owned by AST; no child nodes.
  - Parser captures the exact instruction-body byte span (excluding braces) without tokenizing its contents as Nexus Flow.
- Keep `AST_ASM_BLOCK` as a wrapper/container using an explicit documented child chain for its body; if the existing `ASTBlock.statements` contract is retained, only ordinary statement nodes and `AST_ASM_LABEL` may occur there.
- Represent `asmcall(label)` and `asmjmp(label)` as dedicated AST kinds (append-only) or explicitly define a stable intrinsic-call encoding. Dedicated nodes are preferred to avoid confusing normal function calls with control transfer.
- Update factory allocation, destroy, binary AST codec, OIR AST node registry and round-trip tests together. Do not reuse IDs 1–99.
- Semantic validation: labels are unique in the containing `fastexec`, references resolve, calls/jumps are in legal context, and target constraints are checked before lowering.
- Codegen remains responsible only for target-specific assembly validation/emission after semantic and OIR gates.

## Parser acceptance tests required

1. Valid `fastexec` containing ordinary statements and one/multiple ASM labels.
2. Raw instruction body preserves whitespace/newlines/comments and `%variable` placeholders byte-for-byte.
3. Unterminated ASM block and empty/invalid label produce source-positioned diagnostics.
4. ASM outside `fastexec` rejected.
5. `asmcall`/`asmjmp` syntax and malformed argument cases.
6. No accidental Lexer tokenization inside raw ASM body.
7. AST codec round-trip preserves label and exact raw text.
8. Semantic duplicate/unknown label and invalid-context tests.

**Implementation gate:** confirm this shape against the normative syntax and target ASM requirements before adding Parser branches. No behavior is currently claimed as implemented.
