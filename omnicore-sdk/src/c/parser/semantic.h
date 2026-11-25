#ifndef OCC_SEMANTIC_H
#define OCC_SEMANTIC_H

#include "ast.h"
#include "semantic_result.h"

typedef enum {
    SLOT_VAR,
    SLOT_FUNC,
    SLOT_STRUCT,
    SLOT_UNION,
    SLOT_ENUM,
    SLOT_NEWTYPE,
    SLOT_LABEL,
    SLOT_GROUP
} SlotKind;

typedef struct FieldInfo {
    char* name;
    char* type_name;
    char* array_type_name; /* Encoded inline-array shape, independent of nominal element type. */
    int is_const;
    int is_hard_typed;
    struct FieldInfo* next;
} FieldInfo;

// Masa (Slot) - Sembol Tablosu Kaydı
typedef struct Slot Slot;

typedef struct ActiveAllocation {
    uint64_t identity;
    const ASTNode* origin; /* Allocation expression identity; never a numeric address. */
    uint32_t function_depth; /* Halt only invalidates allocations owned by this execution context. */
    uint32_t scope_depth; /* Lexical owner depth; shorter-lived bindings cannot retain this allocation. */
    struct ActiveAllocation* next;
} ActiveAllocation;

typedef struct BorrowOrigin {
    Slot* owner;             /* Non-owning storage-owner reference. */
    uint64_t borrower_identity; /* Stable identity of the binding that owns this origin. */
    uint64_t parent_identity;   /* Immediate parent borrow identity; zero for direct borrows. */
    char* path;              /* Owned projection path relative to storage owner. */
    uint8_t kind;            /* 1 shared, 2 exclusive mutable. */
    struct BorrowOrigin* next;
} BorrowOrigin;

struct Slot {
    char* name;
    SlotKind kind;
    int is_const;
    int is_moved;          /* Ownership transferred by TOKEN_RELOCATE. */
    uint32_t shared_borrow_count;
    uint8_t mutable_borrow_active;
    uint8_t borrow_kind;    /* 0 none, 1 shared, 2 exclusive. */
    uint8_t is_parameter;   /* Function parameter storage; may carry a returned reference. */
    uint8_t is_initialized; /* Definite initialization state for pointer/reference/callback bindings. */
    uint8_t is_nullable;    /* Binding may currently contain null (pointer/callback values). */
    uint64_t borrow_identity; /* Stable for this Slot across rebinding and branch snapshots. */
    uint64_t allocation_identity; /* Allocation provenance; zero means no tracked allocation. */
    uint64_t borrow_suspended_by_identity; /* Active exclusive child; zero when accessible. */
    BorrowOrigin* borrow_origins; /* Conservative set of possible active borrow origins. */
    struct Slot* borrowed_from; /* Compatibility view of the first origin during migration. */
    struct Slot* borrow_origin; /* Non-owning parameter/global anchor for untracked reference provenance. */
    char* borrow_path;       /* Projection path relative to borrowed_from. */
    int depth; // Scope derinliği
    uint32_t function_depth; /* Owning execution frame; relocation may transfer it. */
    char* type_name;      // Değişken tipi, Fonksiyon dönüş tipi veya NewType hedefi
    ASTNode* declaration_node; // Non-owning AST declaration for signature validation
    ASTNode* callable_declaration; // Known callback target; non-owning AST reference
    int param_count;      // Fixed function parameter count
    int required_param_count; // Parameters before the trailing default-value suffix
    int is_variadic;       // Final AST_VARIADIC_PARAM permits additional arguments
    char** param_types;   // Fonksiyon parametre tipleri
    FieldInfo* fields;    // Struct alanları veya Enum varyantları
    int is_packed;        // Struct packed durumu
    uint32_t symbol_id;
    uint32_t scope_id;
    const ASTNode* group_owner; /* Non-owning Group declaration that owns private nominal types. */
    const ASTNode* fast_exec_owner; /* Non-owning FASTEXEC block that owns ASM labels. */
    uint32_t type_id;
    struct Slot* next;
};

struct OccSemanticImportEnvironment; /* Borrowed, immutable import catalog. */

typedef uint32_t (*SemanticNodeIdResolver)(
    void* user_context, const ASTNode* node
);

typedef struct SemanticTypeName {
    char* name;
    struct SemanticTypeName* next;
} SemanticTypeName;

typedef struct ThreadBlockCapture {
    char* name;
    char* type_name;
    uint32_t symbol_id;
    uint32_t source_scope_id;
    uint32_t source_depth;
    ASTNode* first_use; /* Borrowed from the AST. */
    ASTNode* source_declaration; /* Borrowed from the AST. */
    struct ThreadBlockCapture* next;
} ThreadBlockCapture;

typedef struct ThreadBlockEntry {
    char* name;
    ASTNode* declaration; /* Borrowed from the AST; owned by the Parser/AST. */
    uint32_t capture_base_depth;
    ThreadBlockCapture* captures;
    ThreadBlockCapture* capture_tail;
    size_t capture_count;
    struct ThreadBlockEntry* next;
} ThreadBlockEntry;

typedef struct {
    Slot* head;
    const char* filename;
    int error_count;
    Error* error_head;
    Error* error_tail;
    int current_depth;
    /* Scope depth at which Halt terminates statement traversal; UINT32_MAX means active. */
    uint32_t halted_function_depth;
    unsigned int loop_depth;
    unsigned int function_depth;
    const char* current_func_return_type;
    Language lang;
    /* Optional job-owned collector shared with the parser stage. */
    OccDiagnosticCollector* diagnostics;
    uint32_t diagnostic_stage_id;

    /* Durable result is populated only when a stable AST node resolver is attached. */
    SemanticResult result;
    SemanticResultStatus result_status;
    uint32_t current_scope_id;
    const ASTNode* current_group_owner; /* Active lexical Group; NULL outside a Group. */
    const ASTNode* current_fast_exec_owner; /* Active FASTEXEC lexical scope. */
    const char* current_method_receiver_type;
    const char* current_method_public_name;
    uint32_t current_method_receiver_type_id;
    uint8_t current_method_context;
    uint8_t current_method_is_object;
    uint8_t suppress_group_default_dispatch;
    SemanticNodeIdResolver node_id_resolver;
    void* node_id_context;
    /* Borrowed from the stage/job; valid only until semantic_check completes. */
    const struct OccSemanticImportEnvironment* import_environment;
    uint8_t result_enabled;
    /* Non-zero while validating a thread payload; nested payloads are invalid. */
    uint8_t thread_block_depth;
    /* Borrow-expression operands are not ordinary owner reads. */
    uint32_t borrow_operand_depth;
    /* Assignment targets are writes; do not diagnose their identifier walk as a read. */
    uint32_t assignment_target_depth;
    /* Avoid a duplicate read diagnostic when the same borrowed owner is also the assignment target. */
    Slot* assignment_borrow_guard_owner;
    /* The declaration currently being initialized is not visible in its own initializer. */
    Slot* initializing_slot;
    /* Registry of complete named payloads available to spawn() calls. */
    /* Expression-derived reference type spellings live until semantic_destroy. */
    SemanticTypeName* expression_type_names;
    ThreadBlockEntry* thread_blocks;
    ThreadBlockEntry* thread_block_tail;
    ThreadBlockEntry* active_thread_block;
    uint32_t thread_capture_copy_depth;
    uint64_t next_borrow_identity; /* Monotonic SemanticContext-local borrow identity source. */
    ActiveAllocation* active_allocations; /* Sole source of truth for live allocation identities. */
    uint64_t next_allocation_identity; /* Monotonic; overflow must fail closed. */

    /* Deterministic allocation-failure injection for semantic tests; zero disables. */
    uint32_t test_allocation_fail_at;
    uint32_t test_allocation_attempts;
} SemanticContext;

OCC_API void semantic_init(SemanticContext* ctx, const char* filename, const char* lang_file_path);
OCC_API void semantic_attach_diagnostics(
    SemanticContext* ctx, OccDiagnosticCollector* collector, uint32_t stage_id
);
OCC_API void semantic_attach_import_environment(
    SemanticContext* ctx,
    const struct OccSemanticImportEnvironment* environment
);
OCC_API void semantic_attach_node_id_resolver(
    SemanticContext* ctx, SemanticNodeIdResolver resolver, void* user_context
);
OCC_API SemanticResultStatus semantic_finalize_result(
    SemanticContext* ctx, uint32_t ast_node_count
);
OCC_API void semantic_check(SemanticContext* ctx, ASTNode* node);
OCC_API void semantic_print_errors(SemanticContext* ctx);
OCC_API void semantic_destroy(SemanticContext* ctx);
OCC_API void semantic_test_fail_allocation_at(
    SemanticContext* ctx, uint32_t allocation_index
);
OCC_API uint32_t semantic_test_get_allocation_attempts(
    const SemanticContext* ctx
);


#endif // OCC_SEMANTIC_H