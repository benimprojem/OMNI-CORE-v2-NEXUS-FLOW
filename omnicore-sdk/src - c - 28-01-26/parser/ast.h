#ifndef OCC_AST_H
#define OCC_AST_H

#include "parser.h"
#include "lexer.h"

// Omni Core v2: Abstract Syntax Tree (AST)

typedef enum {
    // --- Program Yapısı ---
    AST_PROGRAM,
    AST_BLOCK,

    // --- Bildirimler ---
    AST_VAR_DECL,       // v:x, c:y
    AST_FUNC_DECL,      // f:name 
    AST_STRUCT_DECL,    // struct:Name
    AST_ENUM_DECL,      // enum:Name
    AST_UNION_DECL,     // union:Name
    AST_NEWTYPE_DECL,   // nt:Name
    AST_ENUM_VARIANT,   // Enum seçeneği
    AST_GROUP_DECL,     // group:Name
    AST_METHOD,         // name => f:
    AST_EXTERN_DECL,    // exf:name

    // --- İfadeler (Expressions) ---
    AST_LITERAL,        // Sayı, String, Char
    AST_IDENTIFIER,     // Değişken ismi
    AST_BINARY_EXPR,    // + - * / ==
    AST_UNARY_EXPR,     // - ! ++
    AST_POSTFIX_EXPR,   // i++, i--
    AST_ASSIGNMENT,     // = +=
    AST_FUNC_CALL,      // foo()
    AST_MEMBER_ACCESS,  // obj.prop
    AST_INDEX_ACCESS,   // arr[0]
    AST_CAST,           // cast(v, type)
    AST_TUPLE,          // (a, b)

    // --- Akış Kontrolü ---
    AST_IF,             // =?>
    AST_LOOP,           // (loop)
    AST_RETURN,         // ret
    AST_BREAK,          // break
    AST_CONTINUE,       // continue
    AST_ASM_BLOCK,      // fastexec

    // --- Nexus Flow (Özel) ---
    AST_NEXUS_CAPTURE,  // (h) <- expr
    AST_NEXUS_PIPE,     // ->, >>, <<
    AST_NEXUS_FLOW,     // ?->, !->, ?=>
    AST_NEXUS_LABEL,    // :>
    AST_NEXUS_JUMP,     // ?>
    AST_NEXUS_HALT,     // ?;
    AST_NEXUS_INTENT,   // @
    AST_AI_LOGIC,       // ??
    AST_UNPACK,         // ...>
    AST_PANIC,          // !!
    AST_SPAWN,          // spawn()
    AST_LISTEN,         // !(listen)
    AST_ECHO,           // echo()
    AST_SEND,           // send()
    AST_RECEIVE,        // receive()
    AST_DONE,           // done()
    AST_AREA,           // area()
    AST_ADDR,           // addr()
    AST_LEN,            // len()
    AST_PEEK,           // peek()
    AST_POKE,           // poke()
    AST_INB,            // inb()
    AST_OUTB,           // outb()
    AST_IRQ,            // irq()
    AST_INTR,           // intr()
    AST_REG,            // reg()
    AST_PRNT,
    AST_PRMT,
    AST_WRITE,
    AST_FWRITE,
    AST_TYPEOF,
    AST_SIZEOF,
    AST_IS_OK,
    AST_IS_ERR,
    AST_IS_SOME,
    AST_IS_NONE,
    AST_IS_NULL,
    AST_EXIT,
    AST_SWAP,
    AST_DEFER,
    AST_THREAD_BLOCK,
    AST_USE,
    AST_ZONE,
    AST_DATE,
    AST_DATENOW,
    AST_TIME,
    AST_CLOCK,
    AST_FREE,
    AST_ZONE_SYNC,
    AST_ZONE_VIEW,
    AST_ZONE_LOCK

} ASTNodeType;

// Temel Düğüm
typedef struct ASTNode {
    ASTNodeType type;
    int line;
    int col;
    struct ASTNode* next; // Bağlı liste (Statement listesi vb.)
} ASTNode;

// --- İfade Yapıları ---

typedef struct {
    ASTNode base;
    OmniTokenType op;
    ASTNode* left;
    ASTNode* right;
} ASTBinaryExpr;

typedef struct {
    ASTNode base;
    OmniTokenType op;
    ASTNode* operand;
} ASTUnaryExpr;

typedef struct {
    ASTNode base;
    OmniTokenType type;     // TOKEN_NUMBER, TOKEN_STRING
    int lit_type;       // 0: Int, 1: Float, 2: String, 3: Char, 4: Bool
    long long int_value;
    double float_value;
    char* string_value; // Ham metin değeri
} ASTLiteral;

typedef struct {
    ASTNode base;
    char* name;
} ASTIdentifier;

typedef struct {
    ASTNode base;
    ASTNode* callee;
    ASTNode* args;      // Argüman listesi
} ASTCall;

typedef struct {
    ASTNode base;
    ASTNode* object;    // Erişilen nesne (örn: p)
    char* member_name;  // Üye ismi (örn: x)
} ASTMemberAccess;

typedef struct {
    ASTNode base;
    ASTNode* elements;  // Eleman listesi
} ASTTuple;

// --- Bildirim Yapıları ---

typedef struct {
    ASTNode base;
    char* name;
    char* type_name;    // "u8", "i32" vb.
    int is_const;       // 1 ise c:, 0 ise v:
    int is_hard_typed;  // ! operatörü var mı?
    int is_public;      // pup
    int is_exported;    // exp
    ASTNode* initializer;
    ASTNode* inline_type; // For v:x!struct { ... }
} ASTVarDecl;

typedef struct {
    ASTNode base;
    char* name;
    ASTNode* params;    // Parametre listesi
    char* return_type;
    ASTNode* body;      // Fonksiyon gövdesi (Block)
    int is_extern;
    int is_public;
    int is_exported;
} ASTFuncDecl;

typedef struct {
    ASTNode base;
    char* name;
    int is_packed; // !struct
    int is_union;  // union
    int is_public;
    int is_exported;
    ASTNode* fields; // ASTVarDecl listesi
} ASTStructDecl;

typedef struct {
    ASTNode base;
    char* name;
    ASTNode* data; // Opsiyonel veri (Struct gibi)
} ASTEnumVariant;

typedef struct {
    ASTNode base;
    char* name;
    ASTNode* variants; // ASTEnumVariant listesi
} ASTEnumDecl;

typedef struct {
    ASTNode base;
    char* public_name; // "yazdir"
    ASTNode* implementation; // ASTFuncDecl
} ASTMethod;

typedef struct {
    ASTNode base;
    char* name;
    int is_public;
    int is_exported;
    ASTNode* members; // ASTVarDecl, ASTFuncDecl, ASTStructDecl, ASTMethod vb.
} ASTGroupDecl;

typedef struct {
    ASTNode base;
    char* module_name;
    char* alias;
} ASTUse;

typedef struct {
    ASTNode base;
    ASTNode* body;
} ASTDefer;

typedef struct {
    ASTNode base;
    char* name;
    ASTNode* body;
    ASTNode* mode;
    ASTNode* timeout;
} ASTThreadBlock;

typedef struct {
    ASTNode base;
    char* name;
    char* target_type; // Hedef tip ismi
} ASTNewTypeDecl;

// --- Akış Yapıları ---

typedef struct {
    ASTNode base;
    ASTNode* statements;
} ASTBlock;

typedef struct {
    ASTNode base;
    ASTNode* condition;
    ASTNode* then_branch;
    ASTNode* else_branch; // ?=> bloğu
} ASTIf;

typedef struct {
    ASTNode base;
    ASTNode* init;
    ASTNode* condition;
    ASTNode* increment;
    ASTNode* body;
    // Foreach / Range extensions
    ASTNode* collection; // List or Range
    ASTNode* variable;   // Iterator variable
    int loop_type;       // 0: Infinite, 1: While, 2: For, 3: Foreach
} ASTLoop;

// --- Nexus Flow Yapıları ---

typedef struct {
    ASTNode base;
    char* handler_name; // (h)
    ASTNode* expr;      // Yakalanan ifade
} ASTNexusCapture;

typedef struct {
    ASTNode base;
    OmniTokenType op;       // ->, ?->, !->
    ASTNode* left;      // Sol taraf (Genelde bir handler veya ifade)
    ASTNode* right;     // Sağ taraf (Block veya ifade)
    int retry_count;    // Rolling için tekrar sayısı (0 ise yok)
    int retry_delay;    // Rolling için bekleme süresi (ms)
    ASTNode* catch_block; // !-> catch bloğu
} ASTNexusFlow;

typedef struct {
    ASTNode base;
    char* label_name;
    ASTNode* statement;
} ASTNexusLabel;

typedef struct {
    ASTNode base;
    char* label_name;
} ASTNexusJump;

typedef struct {
    ASTNode base;
    ASTNode* expr; // Opisyonel ifade
} ASTHalt;

typedef struct {
    ASTNode base;
    char* intent_name;
    ASTNode* target;
} ASTIntent;

typedef struct {
    ASTNode base;
    char* query;
} ASTAiLogic;

typedef struct {
    ASTNode base;
    ASTNode* expr;
} ASTUnpack;

typedef struct {
    ASTNode base;
    char* message;
} ASTPanic;

// --- Program Kökü ---

typedef struct {
    ASTNode base;
    ASTNode* declarations; // Global tanımlar
} ASTProgram;

// Fonksiyonlar
OCC_API ASTNode* ast_create(ASTNodeType type);
OCC_API void ast_print(ASTNode* root);
OCC_API void ast_destroy(ASTNode* root);
OCC_API void ast_serialize(ASTNode* root, FILE* out);
OCC_API ASTNode* ast_deserialize(FILE* in);

#endif // OCC_AST_H