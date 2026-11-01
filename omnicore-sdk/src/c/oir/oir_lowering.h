#ifndef OMNICORE_OIR_LOWERING_H
#define OMNICORE_OIR_LOWERING_H

#include "oir_semantic_ast_artifact.h"
#include "oir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * First deliberately narrow lowering slice:
 * one non-extern, parameterless function; one block; optional initialized
 * scalar locals; one return of a supported scalar expression.
 *
 * All arrays and output storage are caller-owned. AST, TYPES and ANNOTATIONS
 * views must be validated and remain alive; production callers should obtain
 * the complete view through oir_semantic_ast_artifact_open().
 */
typedef struct OirLoweringLocal {
    uint32_t symbol_id;
    uint32_t value_id;
} OirLoweringLocal;

typedef struct OirLoweringWorkspace {
    OirModuleType* types;
    uint32_t type_capacity;
    OirModuleFunction* functions;
    uint32_t function_capacity;
    OirModuleBlock* blocks;
    uint32_t block_capacity;
    OirModuleValue* values;
    uint32_t value_capacity;
    OirModuleInstruction* instructions;
    uint32_t instruction_capacity;
    OirModuleOperand* operands;
    uint32_t operand_capacity;
    uint8_t* strings;
    uint32_t string_capacity;
    OirLoweringLocal* locals;
    uint32_t local_capacity;
} OirLoweringWorkspace;

/*
 * Extended scalar return lowering: supports integer/float/bool literals and
 * recursively nested +, -, *, / expressions and initialized local variables
 * referenced by later expressions. Local variables are promoted directly to
 * SSA values; this slice does not emit memory loads/stores. Unsupported AST nodes,
 * operators and type combinations fail closed.
 */
OirStatus oir_lower_semantic_ast_return(
    const OirSemanticAstArtifactView* artifact,
    OirLoweringWorkspace* workspace,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
);

OirStatus oir_lower_semantic_ast_literal_return(
    const OirSemanticAstArtifactView* artifact,
    OirLoweringWorkspace* workspace,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
