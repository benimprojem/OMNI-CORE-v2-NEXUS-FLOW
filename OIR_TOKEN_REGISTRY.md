# OIR Token ID Registry v1

**Status:** Initial stable ID registry, derived from the current C frontend token inventory  
**Registry version:** 1  
**Current inventory:** 160 token kinds

OIR token IDs are an interchange-level namespace. They are **not** the numeric values of the C `OmniTokenType` enum. Producers and consumers must map by symbolic token identity. C enum insertion/reordering must never renumber these IDs.

Rules:
- IDs are unsigned 16-bit integers.
- IDs 1–160 are assigned below and are permanently reserved for these meanings.
- Removed token meanings remain reserved; do not reuse IDs.
- New token kinds receive new IDs at the end of the registry.
- A payload containing an unknown token ID is rejected unless a future schema version explicitly defines safe extension behavior.
- `TOKEN_ERROR` carries a diagnostic reference; it is not a source lexeme.
- The existence of an enum entry does not prove the current lexer emits it. Runtime coverage is tested separately.

| OIR token ID | Stable symbolic name |
|---:|---|
| 1 | `TOKEN_EOF` |
| 2 | `TOKEN_ERROR` |
| 3 | `TOKEN_IDENTIFIER` |
| 4 | `TOKEN_NUMBER` |
| 5 | `TOKEN_STRING` |
| 6 | `TOKEN_CHAR` |
| 7 | `TOKEN_TRUE` |
| 8 | `TOKEN_FALSE` |
| 9 | `TOKEN_KW_RET` |
| 10 | `TOKEN_KW_STRUCT` |
| 11 | `TOKEN_KW_ENUM` |
| 12 | `TOKEN_KW_UNION` |
| 13 | `TOKEN_KW_GROUP` |
| 14 | `TOKEN_KW_FN` |
| 15 | `TOKEN_KW_EXTERN` |
| 16 | `TOKEN_KW_USE` |
| 17 | `TOKEN_KW_NO` |
| 18 | `TOKEN_KW_ASM` |
| 19 | `TOKEN_KW_FASTEXEC` |
| 20 | `TOKEN_KW_ASMCALL` |
| 21 | `TOKEN_KW_ASMJMP` |
| 22 | `TOKEN_KW_BREAK` |
| 23 | `TOKEN_KW_CONTINUE` |
| 24 | `TOKEN_KW_LOOP` |
| 25 | `TOKEN_KW_PRNT` |
| 26 | `TOKEN_KW_PRMT` |
| 27 | `TOKEN_KW_WRITE` |
| 28 | `TOKEN_KW_FWRITE` |
| 29 | `TOKEN_KW_TYPEOF` |
| 30 | `TOKEN_KW_SIZEOF` |
| 31 | `TOKEN_KW_LEN` |
| 32 | `TOKEN_KW_IS_OK` |
| 33 | `TOKEN_KW_IS_ERR` |
| 34 | `TOKEN_KW_IS_SOME` |
| 35 | `TOKEN_KW_IS_NONE` |
| 36 | `TOKEN_KW_IS_NULL` |
| 37 | `TOKEN_KW_PANIC` |
| 38 | `TOKEN_KW_EXIT` |
| 39 | `TOKEN_KW_CAST` |
| 40 | `TOKEN_KW_SWAP` |
| 41 | `TOKEN_KW_DEFER` |
| 42 | `TOKEN_KW_SPAWN` |
| 43 | `TOKEN_KW_PEEK` |
| 44 | `TOKEN_KW_POKE` |
| 45 | `TOKEN_KW_INB` |
| 46 | `TOKEN_KW_OUTB` |
| 47 | `TOKEN_KW_IRQ` |
| 48 | `TOKEN_KW_INTR` |
| 49 | `TOKEN_KW_REG` |
| 50 | `TOKEN_KW_AREA` |
| 51 | `TOKEN_KW_ZONE` |
| 52 | `TOKEN_KW_FREE` |
| 53 | `TOKEN_KW_ADDR` |
| 54 | `TOKEN_KW_DATE` |
| 55 | `TOKEN_KW_DATENOW` |
| 56 | `TOKEN_KW_TIME` |
| 57 | `TOKEN_KW_CLOCK` |
| 58 | `TOKEN_KW_EXP` |
| 59 | `TOKEN_KW_PUP` |
| 60 | `TOKEN_KW_AS` |
| 61 | `TOKEN_KW_DONE` |
| 62 | `TOKEN_KW_LISTEN` |
| 63 | `TOKEN_KW_ECHO` |
| 64 | `TOKEN_KW_SEND` |
| 65 | `TOKEN_KW_RECEIVE` |
| 66 | `TOKEN_THREAD_BLOCK` |
| 67 | `TOKEN_PRE_VAR` |
| 68 | `TOKEN_PRE_CONST` |
| 69 | `TOKEN_PRE_MUST` |
| 70 | `TOKEN_PRE_RES` |
| 71 | `TOKEN_PRE_TYPE` |
| 72 | `TOKEN_PRE_OBJ` |
| 73 | `TOKEN_PRE_NEWTYPE` |
| 74 | `TOKEN_PRE_FUNC` |
| 75 | `TOKEN_PRE_STRUCT` |
| 76 | `TOKEN_PRE_ENUM` |
| 77 | `TOKEN_PRE_UNION` |
| 78 | `TOKEN_TYPE_U8` |
| 79 | `TOKEN_TYPE_U16` |
| 80 | `TOKEN_TYPE_U32` |
| 81 | `TOKEN_TYPE_U64` |
| 82 | `TOKEN_TYPE_I8` |
| 83 | `TOKEN_TYPE_I16` |
| 84 | `TOKEN_TYPE_I32` |
| 85 | `TOKEN_TYPE_I64` |
| 86 | `TOKEN_TYPE_F32` |
| 87 | `TOKEN_TYPE_F64` |
| 88 | `TOKEN_TYPE_STR` |
| 89 | `TOKEN_TYPE_CHAR` |
| 90 | `TOKEN_TYPE_BOOL` |
| 91 | `TOKEN_TYPE_BIT` |
| 92 | `TOKEN_TYPE_BYTE` |
| 93 | `TOKEN_TYPE_HEX` |
| 94 | `TOKEN_TYPE_VOID` |
| 95 | `TOKEN_TYPE_ANY` |
| 96 | `TOKEN_PLUS` |
| 97 | `TOKEN_MINUS` |
| 98 | `TOKEN_STAR` |
| 99 | `TOKEN_SLASH` |
| 100 | `TOKEN_PERCENT` |
| 101 | `TOKEN_POWER` |
| 102 | `TOKEN_ASSIGN` |
| 103 | `TOKEN_PLUS_EQ` |
| 104 | `TOKEN_MINUS_EQ` |
| 105 | `TOKEN_STAR_EQ` |
| 106 | `TOKEN_SLASH_EQ` |
| 107 | `TOKEN_INC` |
| 108 | `TOKEN_DEC` |
| 109 | `TOKEN_EQ` |
| 110 | `TOKEN_NEQ` |
| 111 | `TOKEN_GT` |
| 112 | `TOKEN_LT` |
| 113 | `TOKEN_GTE` |
| 114 | `TOKEN_LTE` |
| 115 | `TOKEN_AND` |
| 116 | `TOKEN_OR` |
| 117 | `TOKEN_NOT` |
| 118 | `TOKEN_BIT_AND` |
| 119 | `TOKEN_BIT_OR` |
| 120 | `TOKEN_BIT_XOR` |
| 121 | `TOKEN_BIT_NOT` |
| 122 | `TOKEN_BIT_AND_EQ` |
| 123 | `TOKEN_BIT_OR_EQ` |
| 124 | `TOKEN_BIT_XOR_EQ` |
| 125 | `TOKEN_CAPTURE` |
| 126 | `TOKEN_UNPACK` |
| 127 | `TOKEN_RELOCATE` |
| 128 | `TOKEN_IF_FLOW` |
| 129 | `TOKEN_ELSE_FLOW` |
| 130 | `TOKEN_FALLBACK` |
| 131 | `TOKEN_PIPE` |
| 132 | `TOKEN_IGNORE` |
| 133 | `TOKEN_ITERATE` |
| 134 | `TOKEN_JUMP` |
| 135 | `TOKEN_NEXUS_LABEL` |
| 136 | `TOKEN_HALT` |
| 137 | `TOKEN_QMARK` |
| 138 | `TOKEN_INTENT` |
| 139 | `TOKEN_AI` |
| 140 | `TOKEN_STRING_APPEND` |
| 141 | `TOKEN_RANGE` |
| 142 | `TOKEN_ELLIPSIS` |
| 143 | `TOKEN_FORCE` |
| 144 | `TOKEN_FORCE_ASSIGN` |
| 145 | `TOKEN_STRICT_EQ` |
| 146 | `TOKEN_FORCE_EQ` |
| 147 | `TOKEN_LSHIFT` |
| 148 | `TOKEN_RSHIFT` |
| 149 | `TOKEN_ERR_HANDLER` |
| 150 | `TOKEN_FAT_ARROW` |
| 151 | `TOKEN_DOT` |
| 152 | `TOKEN_COMMA` |
| 153 | `TOKEN_COLON` |
| 154 | `TOKEN_SEMICOLON` |
| 155 | `TOKEN_LPAREN` |
| 156 | `TOKEN_RPAREN` |
| 157 | `TOKEN_LBRACE` |
| 158 | `TOKEN_RBRACE` |
| 159 | `TOKEN_LBRACKET` |
| 160 | `TOKEN_RBRACKET` |

Source inventory: `omnicore-sdk/src/c/parser/lexer.h`. The symbolic mapping adapter is implemented in omnicore-sdk/src/c/oir/oir_token_map.c and is tested in test_oir_token_map.c. The full lexer emission audit remains open.
