#ifndef OCC_AST_H
#define OCC_AST_H

#include "parser.h"
#include "lexer.h"
#include "../core/diagnostics.h"

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
    AST_SPAWN,          // Legacy serialized tag; new spawn syntax belongs to the Thread module
    AST_LISTEN,         // Legacy serialized Thread listener tag; new Parser emits AST_FUNC_CALL
    AST_ECHO,           // echo()
    AST_SEND,           // Legacy serialized Thread API tag; new Parser emits AST_FUNC_CALL
    AST_RECEIVE,        // Legacy serialized Thread API tag; new Parser emits AST_FUNC_CALL
    AST_DONE,           // Legacy serialized Thread API tag; new Parser emits AST_FUNC_CALL
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
    AST_ZONE_LOCK,

    /* Pattern matching / structured selection (append-only AST IDs). */
    AST_SELECT,
    AST_SELECT_ARM,
    AST_SCAN,
    AST_SCAN_ARM,
    AST_PATTERN_BINDING,
    AST_PATTERN_CONSTRUCTOR,
    AST_RULESET_DECL,
    AST_RULE_BARRIER,
    AST_RULE_TRIGGER,
    AST_RULE_ALWAYS,
    AST_RULE_DEFAULT,
    AST_APPLY_RULES,
    AST_ON_BLOCK,
    AST_LAMBDA,
    AST_RELOCATE,
    AST_UNROLL,
    AST_ARRAY_LITERAL,
    AST_ARRAY_TYPE,
    AST_ARRAY_DIMENSION,
    /* Block-scope resource cleanup marker emitted by the closing `}?;`. */
    AST_BLOCK_HALT,
    /* Append-only: variadic parameter-list marker and named call argument. */
    AST_VARIADIC_PARAM,
    AST_NAMED_ARG,
    /* Immutable value declaration introduced by `m:`. */
    AST_MUST_DECL,
    /* FASTEXEC scope and raw inline-ASM declaration; append-only. */
    AST_FASTEXEC_BLOCK,
    AST_ASM_LABEL,
    /* Append-only: Struct aggregate expression and named field initializer. */
    AST_STRUCT_LITERAL,
    AST_STRUCT_FIELD_INIT,
    AST_TYPE_EXPR
} ASTNodeType;

// Temel Düğüm
typedef struct ASTNode {
    ASTNodeType type;
    int line;
    int col;
    /*
     * Owned sibling chain (statement/parameter/member lists).
     * AST nodes and child subtrees have unique ownership: do not attach the
     * same node to more than one parent or sibling chain.
     */
    struct ASTNode* next;
    /* Full source range; line/col remain compatibility mirrors. */
    OccSourceSpan source_span;
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

/* cast(value, type): value and target type are separate AST children. */
typedef struct {
    ASTNode base;
    ASTNode* value;
    ASTNode* target_type;
} ASTCast;

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

/* arr[index]: target and index are independent owned expression children. */
typedef struct {
    ASTNode base;
    ASTNode* target;
    ASTNode* index;
} ASTIndexAccess;

typedef struct {
    ASTNode base;
    ASTNode* elements;  // Eleman listesi
} ASTTuple;

/* Pattern nodes preserve matching semantics; constructor patterns are not calls. */
typedef struct { ASTNode base; char* name; } ASTPatternBinding;
typedef struct { ASTNode base; char* name; ASTNode* bindings; } ASTPatternConstructor;
typedef struct { ASTNode base; ASTNode* selector; ASTNode* arms; } ASTSelect;
typedef struct { ASTNode base; ASTNode* pattern; ASTNode* body; int is_default; } ASTSelectArm;
typedef struct { ASTNode base; ASTNode* subject; ASTNode* arms; } ASTScan;
typedef struct { ASTNode base; ASTNode* pattern; ASTNode* body; } ASTScanArm;
typedef struct { ASTNode base; char* name; ASTNode* params; ASTNode* rules; } ASTRulesetDecl;
typedef struct { ASTNode base; ASTNode* condition; ASTNode* message; } ASTRuleBarrier;
typedef struct { ASTNode base; ASTNode* condition; ASTNode* action; } ASTRuleTrigger;
typedef struct { ASTNode base; ASTNode* action; } ASTRuleAlways;
typedef struct { ASTNode base; ASTNode* action; } ASTRuleDefault;
typedef struct { ASTNode base; char* ruleset_name; ASTNode* args; } ASTApplyRules;
typedef struct { ASTNode base; char* ruleset_name; char* event_name; ASTNode* body; } ASTOnBlock;
typedef struct { ASTNode base; ASTNode* params; ASTNode* body; int body_is_expression; } ASTLambda;
typedef struct { ASTNode base; ASTNode* source; ASTNode* target; } ASTRelocate;
typedef struct { ASTNode base; ASTNode* count; ASTNode* loop; } ASTUnroll;
typedef struct { ASTNode base; ASTNode* elements; } ASTArrayLiteral;
/* Struct literal preserves nominal type path and ordered named field values. */
typedef struct { ASTNode base; char* type_name; ASTNode* fields; } ASTStructLiteral;
/* One owned field initializer: name = value. */
typedef struct { ASTNode base; char* name; ASTNode* value; } ASTStructFieldInit;

/* Recursive type syntax; all child pointers have unique AST ownership. */
typedef enum {
    AST_TYPE_NAMED = 1, AST_TYPE_POINTER = 2, AST_TYPE_REFERENCE = 3,
    AST_TYPE_ARRAY = 4, AST_TYPE_FUNCTION = 5, AST_TYPE_QUALIFIED = 6
} ASTTypeExprKind;
enum { AST_TYPE_FLAG_HARD_TYPED = 1u << 0 };
typedef struct {
    ASTNode base;
    uint32_t kind;
    uint32_t flags;
    char* name;
    ASTNode* child;
    ASTNode* params;
    ASTNode* dimensions;
} ASTTypeExpr;
/* Array type declarator: dimensions are ordered AST_ARRAY_DIMENSION siblings.
 * A NULL dimension length means a dynamic/unspecified dimension. The element
 * type is carried by the owning ASTVarDecl.type_name (NULL means inferred).
 */
typedef struct { ASTNode base; ASTNode* dimensions; } ASTArrayType;
typedef struct { ASTNode base; ASTNode* length; } ASTArrayDimension;
/* A variadic marker has no payload and must be final in a function parameter list. */
typedef struct { ASTNode base; } ASTVariadicParam;
/* Named call argument: owned label and one owned value expression. */
typedef struct { ASTNode base; char* name; ASTNode* value; } ASTNamedArg;
/* `m:name = expression;` immutable value; source owns its initializer expression. */
typedef struct { ASTNode base; char* name; ASTNode* source; } ASTMustDecl;
/* FASTEXEC owns an ordered statement chain. */
typedef struct { ASTNode base; ASTNode* statements; } ASTFastExecBlock;
/* Raw target assembly is opaque to the Nexus lexer/parser. source_length is authoritative. */
typedef struct {
    ASTNode base;
    char* label;
    char* source_text;
    size_t source_length;
} ASTAsmLabel;

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
    ASTNode* inline_type; // Inline aggregate or structured type payload (e.g. AST_ARRAY_TYPE)
    ASTNode* type_expr;   // Optional recursive type syntax; appended for compatibility.
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
    /* Optional owned AST_ARRAY_TYPE describing return dimensions. Keep appended
     * for source/ABI layout compatibility; return_type remains base-type text. */
    ASTNode* return_array_type;
    /* Optional source-level linkage file inherited from the preceding linkname directive. */
    char* linkname;
    ASTNode* return_type_expr; // Optional recursive return type syntax; appended for compatibility.
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
    /* Optional selected namespace root introduced by USE `ns`. */
    char* namespace_name;
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
    ASTNode* target_type_expr; // Optional recursive target type syntax; appended for compatibility.
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
    /* Program policy: suppress the implicit OS module only when explicitly set. */
    uint8_t disable_osmodule;
    OccSourceSpan disable_osmodule_span;
} ASTProgram;

// Fonksiyonlar
OCC_API ASTNode* ast_create(ASTNodeType type);
/* Deterministic, process-global allocation fault injection for serialized AST tests.
 * Test-only seam: do not change the hook while AST operations run concurrently.
 * Passing zero disables injection and resets the allocation counter.
 */
OCC_API void ast_test_fail_allocation_at(size_t allocation_index);
OCC_API void ast_test_reset_allocation_failure(void);
OCC_API size_t ast_test_get_allocation_count(void);
/* Sets/clears a node source range. NULL clears it; line/col mirrors start. */
OCC_API int ast_set_source_span(ASTNode* node, const OccSourceSpan* span);
OCC_API void ast_print(ASTNode* root);
/* Serialization/deserialization reject AST nesting deeper than 512 nodes; sibling lists are iterative. */
/* Serialization accepts only a non-NULL AST satisfying ast_validate().
 * Invalid graphs are rejected before any bytes are written. Deserialization
 * rejects truncated, trailing, or structurally invalid AST streams. */
OCC_API int ast_serialize(ASTNode* root, FILE* out);
OCC_API ASTNode* ast_deserialize(FILE* in);

typedef enum {
    AST_VALIDATE_OK = 0,
    AST_VALIDATE_INVALID_KIND,
    AST_VALIDATE_CYCLE,
    AST_VALIDATE_SHARED_NODE,
    AST_VALIDATE_MISSING_REQUIRED,
    AST_VALIDATE_OUT_OF_MEMORY,
    AST_VALIDATE_DEPTH_LIMIT,
    AST_VALIDATE_INVALID_SPAN
} ASTValidationCode;

typedef struct {
    ASTValidationCode code;
    const ASTNode* node;
    const char* field; /* Static field name; NULL when not applicable. */
} ASTValidationError;

/* Top-level USE visitor. use_ordinal is a deterministic 1-based ordinal among
 * AST_USE declarations in source order (not a persistent AST node identity).
 * Return nonzero to continue; zero stops traversal.
 */
typedef int (*ASTUseVisitorFn)(const ASTUse* use, uint32_t use_ordinal, void* context);
typedef enum {
    AST_USE_VISIT_OK = 0,
    AST_USE_VISIT_INVALID_ARGUMENT,
    AST_USE_VISIT_INVALID_ROOT,
    AST_USE_VISIT_ORDINAL_OVERFLOW,
    AST_USE_VISIT_STOPPED
} ASTUseVisitStatus;
OCC_API ASTUseVisitStatus ast_visit_top_level_uses(
    const ASTNode* program,
    ASTUseVisitorFn visitor,
    void* context,
    size_t* out_count
);

/* Validates node kinds, unique ownership/cycles, and core required child fields.
 * Does not mutate or free the tree. Returns 1 when valid, otherwise 0.
 * error may be NULL; on success it is reset to AST_VALIDATE_OK.
 */
OCC_API int ast_validate(const ASTNode* root, ASTValidationError* error);

/* Destroys root, its complete sibling chain, and all exclusively owned children. */
OCC_API void ast_destroy(ASTNode* root);

#endif // OCC_AST_H