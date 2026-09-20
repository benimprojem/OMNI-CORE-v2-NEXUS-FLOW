#ifndef OCC_SEMANTIC_H
#define OCC_SEMANTIC_H

#include "ast.h"

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
    struct Slot* next;
} Slot;

typedef struct {
    Slot* head;
    const char* filename;
    int error_count;
    Error* error_head;
    Error* error_tail;
    int current_depth;
    const char* current_func_return_type;
    Language lang;
} SemanticContext;

OCC_API void semantic_init(SemanticContext* ctx, const char* filename, const char* lang_file_path);
OCC_API void semantic_check(SemanticContext* ctx, ASTNode* node);
OCC_API void semantic_print_errors(SemanticContext* ctx);
OCC_API void semantic_destroy(SemanticContext* ctx);

#endif // OCC_SEMANTIC_H