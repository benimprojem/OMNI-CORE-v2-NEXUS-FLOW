#ifndef OMNICORE_OIR_MODULE_H
#define OMNICORE_OIR_MODULE_H
#include "oir.h"
#include "oir_module_opcode.h"
#ifdef __cplusplus
extern "C" {
#endif
#define OIR_MODULE_SCHEMA_MAJOR 1u
#define OIR_MODULE_SCHEMA_MINOR 0u
#define OIR_MODULE_HEADER_SIZE 36u
#define OIR_MODULE_TABLE_COUNT_MAX 1000000u
#define OIR_MODULE_STRING_BYTES_MAX 16777216u
typedef struct OirModuleType { uint32_t id; uint16_t kind, bit_width; } OirModuleType;
typedef struct OirModuleFunction { uint32_t id,name_offset,name_length,function_type_id,entry_block_id; } OirModuleFunction;
typedef struct OirModuleBlock { uint32_t id,function_id,first_instruction,instruction_count; } OirModuleBlock;
typedef struct OirModuleValue { uint32_t id,type_id,defining_instruction_id,flags; } OirModuleValue;
typedef struct OirModuleInstruction { uint32_t id,block_id; uint16_t opcode_id,reserved; uint32_t result_value_id,first_operand,operand_count; } OirModuleInstruction;
typedef struct OirModuleOperand { uint32_t kind,reference_id; uint64_t immediate; } OirModuleOperand;
typedef struct OirModuleInput {
 const OirModuleType* types; uint32_t type_count;
 const OirModuleFunction* functions; uint32_t function_count;
 const OirModuleBlock* blocks; uint32_t block_count;
 const OirModuleValue* values; uint32_t value_count;
 const OirModuleInstruction* instructions; uint32_t instruction_count;
 const OirModuleOperand* operands; uint32_t operand_count;
 const uint8_t* strings; uint32_t strings_length;
} OirModuleInput;
typedef struct OirModuleView {
 uint32_t type_count,function_count,block_count,value_count,instruction_count,operand_count,strings_length;
 const uint8_t *types,*functions,*blocks,*values,*instructions,*operands,*strings;
} OirModuleView;
OirStatus oir_module_open(const uint8_t*,size_t,OirModuleView*);
OirStatus oir_module_write(uint8_t*,size_t,const OirModuleInput*,size_t*);
OirStatus oir_module_get_type(const OirModuleView*,uint32_t,OirModuleType*);
OirStatus oir_module_get_function(const OirModuleView*,uint32_t,OirModuleFunction*);
OirStatus oir_module_get_block(const OirModuleView*,uint32_t,OirModuleBlock*);
OirStatus oir_module_get_value(const OirModuleView*,uint32_t,OirModuleValue*);
OirStatus oir_module_get_instruction(const OirModuleView*,uint32_t,OirModuleInstruction*);
OirStatus oir_module_get_operand(const OirModuleView*,uint32_t,OirModuleOperand*);
#ifdef __cplusplus
}
#endif
#endif
