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
    SLOT_LABEL
} SlotKind;

typedef struct FieldInfo {
    char* name;
    char* type_name;
    struct FieldInfo* next;
} FieldInfo;

// Masa (Slot) - Sembol Tablosu Kaydı
typedef struct Slot {
    char* name;
    SlotKind kind;
    int is_const;
    int depth; // Scope derinliği
    char* type_name;      // Değişken tipi, Fonksiyon dönüş tipi veya NewType hedefi
    int param_count;      // Fonksiyon parametre sayısı
    char** param_types;   // Fonksiyon parametre tipleri
    FieldInfo* fields;    // Struct alanları veya Enum varyantları
    int is_packed;        // Struct packed durumu
    uint32_t symbol_id;
    uint32_t scope_id;
    uint32_t type_id;
    struct Slot* next;
} Slot;

typedef uint32_t (*SemanticNodeIdResolver)(
    void* user_context, const ASTNode* node
);

typedef struct {
    Slot* head;
    const char* filename;
    int error_count;
    Error* error_head;
    Error* error_tail;
    int current_depth;
    unsigned int loop_depth;
    const char* current_func_return_type;
    Language lang;
    /* Optional job-owned collector shared with the parser stage. */
    OccDiagnosticCollector* diagnostics;
    uint32_t diagnostic_stage_id;

    /* Durable result is populated only when a stable AST node resolver is attached. */
    SemanticResult result;
    SemanticResultStatus result_status;
    uint32_t current_scope_id;
    SemanticNodeIdResolver node_id_resolver;
    void* node_id_context;
    uint8_t result_enabled;

    /* Deterministic allocation-failure injection for semantic tests; zero disables. */
    uint32_t test_allocation_fail_at;
    uint32_t test_allocation_attempts;
} SemanticContext;

OCC_API void semantic_init(SemanticContext* ctx, const char* filename, const char* lang_file_path);
OCC_API void semantic_attach_diagnostics(
    SemanticContext* ctx, OccDiagnosticCollector* collector, uint32_t stage_id
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