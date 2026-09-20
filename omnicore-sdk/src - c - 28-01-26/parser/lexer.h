#ifndef OCC_LEXER_H
#define OCC_LEXER_H

#include "parser.h"

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    // --- Tanımlayıcılar ve Literaller ---
    TOKEN_IDENTIFIER,   // Değişken ve fonksiyon isimleri
    TOKEN_NUMBER,       // 100, 3.14, 0xFF
    TOKEN_STRING,       // "Merhaba"
    TOKEN_CHAR,         // 'A'
    TOKEN_TRUE,         // true
    TOKEN_FALSE,        // false

    // --- Omni Core v2 Anahtar Kelimeler ---
    TOKEN_KW_RET,       // ret
    TOKEN_KW_STRUCT,    // struct
    TOKEN_KW_ENUM,      // enum
    TOKEN_KW_UNION,     // union
    TOKEN_KW_GROUP,     // group
    TOKEN_KW_FN,        // f:
    TOKEN_KW_EXTERN,    // exf:
    TOKEN_KW_USE,       // use
    TOKEN_KW_NO,        // no
    TOKEN_KW_ASM,       // asm
    TOKEN_KW_FASTEXEC,  // fastexec
    TOKEN_KW_ASMCALL,   // asmcall
    TOKEN_KW_ASMJMP,    // asmjmp
    TOKEN_KW_BREAK,     // break
    TOKEN_KW_CONTINUE,  // continue
    TOKEN_KW_LOOP,      // loop
    TOKEN_KW_PRNT,      // prnt
    TOKEN_KW_PRMT,      // prmt
    TOKEN_KW_WRITE,     // write
    TOKEN_KW_FWRITE,    // fwrite
    TOKEN_KW_TYPEOF,    // typeof
    TOKEN_KW_SIZEOF,    // sizeof
    TOKEN_KW_LEN,       // len
    TOKEN_KW_IS_OK,     // is_ok
    TOKEN_KW_IS_ERR,    // is_err
    TOKEN_KW_IS_SOME,   // is_some
    TOKEN_KW_IS_NONE,   // is_none
    TOKEN_KW_IS_NULL,   // is_null
    TOKEN_KW_PANIC,     // panic
    TOKEN_KW_EXIT,      // exit
    TOKEN_KW_CAST,      // cast
    TOKEN_KW_SWAP,      // swap
    TOKEN_KW_DEFER,     // defer
    TOKEN_KW_SPAWN,     // spawn
    TOKEN_KW_PEEK,      // peek
    TOKEN_KW_POKE,      // poke
    TOKEN_KW_INB,       // inb
    TOKEN_KW_OUTB,      // outb
    TOKEN_KW_IRQ,       // irq
    TOKEN_KW_INTR,      // intr
    TOKEN_KW_REG,       // reg
    TOKEN_KW_AREA,      // area
    TOKEN_KW_ZONE,      // zone
    TOKEN_KW_FREE,      // free
    TOKEN_KW_ADDR,      // addr
    TOKEN_KW_DATE,      // date
    TOKEN_KW_DATENOW,   // datenow
    TOKEN_KW_TIME,      // time
    TOKEN_KW_CLOCK,     // clock
    TOKEN_KW_EXP,       // exp
    TOKEN_KW_PUP,       // pup
    TOKEN_KW_AS,        // as
    TOKEN_KW_DONE,
    TOKEN_KW_LISTEN,
    TOKEN_KW_ECHO,
    TOKEN_KW_SEND,
    TOKEN_KW_RECEIVE,
    TOKEN_THREAD_BLOCK, // >{

    // --- Tanımlayıcı Önekleri (Prefixes) ---
    TOKEN_PRE_VAR,      // v:
    TOKEN_PRE_CONST,    // c:
    TOKEN_PRE_MUST,     // m:
    TOKEN_PRE_RES,      // r:
    TOKEN_PRE_TYPE,     // t:
    TOKEN_PRE_OBJ,      // o:
    TOKEN_PRE_NEWTYPE,  // nt:
    TOKEN_PRE_FUNC,     // f:
    TOKEN_PRE_STRUCT,   // struct:
    TOKEN_PRE_ENUM,     // enum:
    TOKEN_PRE_UNION,    // union:

    // --- Veri Tipleri ---
    TOKEN_TYPE_U8, TOKEN_TYPE_U16, TOKEN_TYPE_U32, TOKEN_TYPE_U64,
    TOKEN_TYPE_I8, TOKEN_TYPE_I16, TOKEN_TYPE_I32, TOKEN_TYPE_I64,
    TOKEN_TYPE_F32, TOKEN_TYPE_F64,
    TOKEN_TYPE_STR, TOKEN_TYPE_CHAR, TOKEN_TYPE_BOOL, TOKEN_TYPE_BIT, TOKEN_TYPE_BYTE, TOKEN_TYPE_HEX,
    TOKEN_TYPE_VOID, TOKEN_TYPE_ANY,

    // --- Aritmetik ve Atama Operatörleri ---
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_POWER,        // ^^
    TOKEN_ASSIGN,       // =
    TOKEN_PLUS_EQ,      // +=
    TOKEN_MINUS_EQ,     // -=
    TOKEN_STAR_EQ,      // *=
    TOKEN_SLASH_EQ,     // /=
    TOKEN_INC,          // ++
    TOKEN_DEC,          // --

    // --- Karşılaştırma ve Mantık ---
    TOKEN_EQ, TOKEN_NEQ, TOKEN_GT, TOKEN_LT, TOKEN_GTE, TOKEN_LTE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,

    // --- Bitwise ---
    TOKEN_BIT_AND, TOKEN_BIT_OR, TOKEN_BIT_XOR, TOKEN_BIT_NOT, // & | ^ ~
    TOKEN_BIT_AND_EQ, TOKEN_BIT_OR_EQ, TOKEN_BIT_XOR_EQ,       // &= |= ^=

    // --- Nexus Flow & Özel Operatörler (21 Adet) ---
    TOKEN_CAPTURE,      // <-
    TOKEN_UNPACK,       // ...>
    TOKEN_RELOCATE,     // _>
    TOKEN_IF_FLOW,      // =?>
    TOKEN_ELSE_FLOW,    // ?->
    TOKEN_FALLBACK,     // ?=>
    TOKEN_PIPE,         // ->
    TOKEN_IGNORE,       // !->
    TOKEN_ITERATE,      // <--
    TOKEN_JUMP,         // ?>
    TOKEN_NEXUS_LABEL,  // :>
    TOKEN_HALT,         // ?;
    TOKEN_QMARK,        // ? (Tek başına veya rolling başlangıcı)
    TOKEN_INTENT,       // @
    TOKEN_AI,           // ??
    TOKEN_STRING_APPEND,// .=
    TOKEN_RANGE,        // ..
    TOKEN_ELLIPSIS,     // ...
    TOKEN_FORCE,        // !!
    TOKEN_FORCE_ASSIGN, // !!=
    TOKEN_STRICT_EQ,    // ===
    TOKEN_FORCE_EQ,     // ==!
    TOKEN_LSHIFT,       // << (Stream In)
    TOKEN_RSHIFT,        // >> (Stream Out)
    TOKEN_ERR_HANDLER,   // (e)
    TOKEN_FAT_ARROW,     // =>

    // --- Noktalama ---
    TOKEN_DOT, TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET

} OmniTokenType;

typedef struct {
    OmniTokenType type;
    const char* start; // Kaynak koddaki başlangıç adresi (pointer)
    int length;
    int line;
    int col;
} Token;

Token lexer_next_token(ParserContext* ctx);
const char* token_type_to_string(OmniTokenType type);

#endif // OCC_LEXER_H