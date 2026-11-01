#ifndef OMNICORE_OIR_LOWERING_H
#define OMNICORE_OIR_LOWERING_H

#include "oir_semantic_ast_artifact.h"
#include "oir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * First deliberately narrow lowering slice:
 * one non-extern, parameterless function; one block; one return of a
 * semantically annotated integer, float or bool literal.
 *
 * All arrays and output storage are caller-owned. AST, TYPES and ANNOTATIONS
 * views must be validated and remain alive; production callers should obtain
 * the complete view through oir_semantic_ast_artifact_open().
 */
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
} OirLoweringWorkspace;

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
