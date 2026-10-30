#ifndef NEXUS_TOKEN_HPP
#define NEXUS_TOKEN_HPP

// ============================================================
// Token.hpp — NexusFlow Unified Token Definitions
// (Token.hpp + AdvancedToken.hpp merged — single source of truth)
// ============================================================

#include <string>
#include <unordered_map>

// ------------------------------------------------------------
// TokenType Enum — All NexusFlow tokens
// ------------------------------------------------------------
enum class TokenType {

    // --- Prefixes (v: f: c: m: r: exf: nt:) ---
    PREFIX_VAR,     // v:
    PREFIX_FUNC,    // f:
    PREFIX_CONST,   // c:
    PREFIX_MUST,    // m:
    PREFIX_RULE,    // r:
    PREFIX_EXTERN,  // exf:
    PREFIX_NT,      // nt:

    // --- Primitive Types ---
    T_BIT, T_BYTE, T_HEX, T_CHAR, T_STR, T_BOOL,
    T_I8,  T_I16,  T_I32,  T_I64,
    T_U8,  T_U16,  T_U32,  T_U64,
    T_F32, T_F64,  T_D32,  T_D64,
    T_GENERIC,      // t (generic / template type)
    T_FPTR,         // fptr (function pointer type)

    // --- SIMD / Vector Types ---
    T_VEC2, T_VEC3, T_VEC4,

    // --- Collection Types ---
    T_MAP,          // map
    T_ARRAY_DYN,    // []
    T_ARRAY_STATIC, // [N]

    // --- Arithmetic Operators ---
    OP_ADD,         // +
    OP_SUB,         // -
    OP_MUL,         // *
    OP_DIV,         // /
    OP_MOD,         // %
    OP_POW,         // ^^
    OP_ADD_ASSIGN,  // +=
    OP_SUB_ASSIGN,  // -=
    OP_MUL_ASSIGN,  // *=
    OP_DIV_ASSIGN,  // /=
    OP_INC,         // ++
    OP_DEC,         // --

    // --- Comparison & Logic ---
    OP_EQ,          // ==
    OP_NEQ,         // !=
    OP_STRICT_EQ,   // === (value + memory size)
    OP_BIT_STRICT_EQ, // ==! (bit layout only)
    OP_GT,          // >
    OP_LT,          // <
    OP_GTE,         // >=
    OP_LTE,         // <=
    OP_AND,         // &&
    OP_OR,          // ||
    OP_NOT,         // ! (logical not — unary)
    OP_TYPE_IS,     // is (type identity)

    // --- Bitwise (Kernel & Driver) ---
    OP_BIT_AND,         // &
    OP_BIT_OR,          // |
    OP_BIT_XOR,         // ^
    OP_BIT_NOT,         // ~
    OP_LSHIFT,          // <<
    OP_RSHIFT,          // >>
    OP_BIT_OR_ASSIGN,   // |=
    OP_BIT_AND_ASSIGN,  // &=
    OP_BIT_XOR_ASSIGN,  // ^=

    // --- Nexus Flow Operators (The 15 Keys) ---
    OP_CAPTURE,         // <-   (priority 2)
    OP_RELOCATE,        // _>   (priority 3)
    OP_ROLLING,         // ?(   (priority 4) — rolling start
    OP_CATCH,           // ?->  (priority 5)
    OP_FALLBACK,        // ?=>  (priority 6)
    OP_PIPE,            // ->   (priority 7)
    OP_IGNORE,          // !->  (priority 8)
    OP_ZONE_WRITE,      // <<   NOTE: reuses LSHIFT token in flow context
    OP_FLOW_FEED,       // >>   NOTE: reuses RSHIFT token in flow context
    OP_HALT,            // ?;   (priority 12)
    OP_PANIC,           // !!   (priority 13)
    OP_INTENT,          // @    (priority 14)
    OP_DIRECTIVE,       // !!= (priority 15) — compiler directive

    // --- Special Nexus Operators ---
    OP_BIND,            // =>   (binding in group)
    OP_ASSIGN,          // =    (assignment)
    OP_BARRIER,         // :=> (Barrier stop rule)
    OP_FLOW_IF,         // =?> (Flow conditional)
    OP_SINGLE_LINE_RET, // >    (single-line function body return)
    OP_HARD_LOCK,       // !    (type lock — e.g. v:x!i32)
    OP_DOT,             // .    (member access)
    OP_RANGE,           // ..   (range 1..100)
    OP_VARIADIC,        // ...  (variadic params)
    OP_STR_CONCAT,      // .=   (string concatenation)
    OP_LABEL_DEF,       // :>   (label definition: out:>)
    OP_LABEL_JUMP,      // ?>   (conditional jump: out?>)
    OP_TERMINATOR,      // }?;  (handler cleanup + halt)

    // --- Control Flow Keywords ---
    KW_RETURN,    // return
    KW_BREAK,     // break
    KW_CONTINUE,  // continue
    KW_JUMP,      // jump

    // --- Boolean Literals ---
    KW_TRUE,   // true
    KW_FALSE,  // false

    // --- Structure Keywords ---
    KW_GROUP,   // group
    KW_STRUCT,  // struct
    KW_ENUM,    // enum
    KW_UNION,   // union
    KW_NT,      // nt (newtype)
    KW_DEFAULT, // default

    // --- Loop Keyword ---
    KW_LOOP,    // loop

    // --- Control Keywords ---
    KW_SELECT,  // select (pattern match)
    KW_SCAN,    // scan (router/server)
    KW_IF,      // if 
    KW_ELSIF,   // elsif
    KW_ELS,     // els
    KW_ELSE,    // else
    KW_UNROLL,  // unroll
    KW_ON,      // on
    KW_ALWAYS,  // always

    // --- Function / Module Keywords ---
    KW_RULES,     // rules
    KW_APPLY,     // apply (rule application)
    KW_FASTEXEC,  // fastexec (inline asm block)
    KW_ASM_LABEL, // asm: label
    KW_MACRO,     // :macro!
    KW_USE,       // use (import)
    KW_IMPORT,    // import
    KW_AS,        // as (alias)

    // --- Target / Build Keywords ---
    KW_TARGET, // target (in directives)
    KW_LINK,   // link (library linking)

    // --- Thread / Async Keywords ---
    KW_SPAWN,   // spawn
    KW_DONE,    // done
    KW_LISTEN,  // listen
    KW_NOISY_LISTEN, // !listen (bare-metal poll)
    KW_YIELD,   // yield

    // --- Delimiters ---
    L_PAREN,    // (
    R_PAREN,    // )
    L_BRACE,    // {
    R_BRACE,    // }
    L_BRACKET,  // [
    R_BRACKET,  // ]

    // --- Punctuation ---
    COMMA,     // ,
    COLON,     // :
    SEMICOLON, // ;
    HASH,      // # (comment start)
    AT,        // @ (intent / attribute)

    // --- Literals ---
    IDENTIFIER, // name, myVar, etc.
    NUMBER,     // 42, 3.14, 0xFF, 1010b
    STRING,     // "hello"
    T_CHAR_LIT, // 'a'

    // --- Sentinel ---
    EOF_TOKEN,
    UNKNOWN      // Unrecognized character
};

// ------------------------------------------------------------
// Token Struct — carries type, value, and source location
// ------------------------------------------------------------
struct Token {
    TokenType   type;
    std::string value;
    int         line = 1;
    int         col  = 1;
};

// ------------------------------------------------------------
// Keyword Map — used by Lexer to classify identifiers
// ------------------------------------------------------------
inline const std::unordered_map<std::string, TokenType>& nexusKeywords() {
    static const std::unordered_map<std::string, TokenType> kw = {
        // Primitive Types
        {"bit",   TokenType::T_BIT},  {"byte",  TokenType::T_BYTE},
        {"hex",   TokenType::T_HEX},  {"char",  TokenType::T_CHAR},
        {"str",   TokenType::T_STR},  {"bool",  TokenType::T_BOOL},
        {"i8",    TokenType::T_I8},   {"i16",   TokenType::T_I16},
        {"i32",   TokenType::T_I32},  {"i64",   TokenType::T_I64},
        {"u8",    TokenType::T_U8},   {"u16",   TokenType::T_U16},
        {"u32",   TokenType::T_U32},  {"u64",   TokenType::T_U64},
        {"f32",   TokenType::T_F32},  {"f64",   TokenType::T_F64},
        {"d32",   TokenType::T_D32},  {"d64",   TokenType::T_D64},
        {"t",     TokenType::T_GENERIC},
        {"fptr",  TokenType::T_FPTR},
        {"map",   TokenType::T_MAP},
        // SIMD
        {"Vec2",  TokenType::T_VEC2}, {"Vec3",  TokenType::T_VEC3},
        {"Vec4",  TokenType::T_VEC4},
        // Booleans
        {"true",  TokenType::KW_TRUE}, {"false", TokenType::KW_FALSE},
        // Control Flow
        {"return",   TokenType::KW_RETURN},   {"break",    TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE}, {"jump",     TokenType::KW_JUMP},
        // Structures
        {"group",   TokenType::KW_GROUP},   {"struct",  TokenType::KW_STRUCT},
        {"enum",    TokenType::KW_ENUM},    {"union",   TokenType::KW_UNION},
        {"nt",      TokenType::KW_NT},      {"default", TokenType::KW_DEFAULT},
        // Loop & Selection
        {"loop",   TokenType::KW_LOOP},
        {"select", TokenType::KW_SELECT},
        {"scan",   TokenType::KW_SCAN},
        {"if",     TokenType::KW_IF},
        {"elsif",  TokenType::KW_ELSIF},
        {"els",    TokenType::KW_ELS},
        {"else",   TokenType::KW_ELSE},
        {"unroll", TokenType::KW_UNROLL},
        {"on",     TokenType::KW_ON},
        {"always", TokenType::KW_ALWAYS},
        {"default",TokenType::KW_DEFAULT},
        // Functions & Modules
        {"rules",    TokenType::KW_RULES},
        {"apply",    TokenType::KW_APPLY},
        {"fastexec", TokenType::KW_FASTEXEC},
        {"use",      TokenType::KW_USE},
        {"import",   TokenType::KW_IMPORT},
        {"as",       TokenType::KW_AS},
        // Build
        {"target",  TokenType::KW_TARGET},
        {"link",    TokenType::KW_LINK},
        // Thread / Async
        {"spawn",  TokenType::KW_SPAWN},
        {"done",   TokenType::KW_DONE},
        {"listen", TokenType::KW_LISTEN},
        {"yield",  TokenType::KW_YIELD},
        // Type check
        {"is",     TokenType::OP_TYPE_IS},
    };
    return kw;
}

#endif // NEXUS_TOKEN_HPP