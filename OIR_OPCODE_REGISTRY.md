# OIR_MODULE Opcode Registry

**Registry:** 1.2 (integer bit-shift opcode extension)  
**Status:** Initial stable wire-ID assignment; type/effect verification remains in the module verifier  
**Implementation:** `src/c/oir/oir_module_opcode.h/.c`

Opcode IDs are explicit wire identifiers. They are not C enum ordinals and must never be renumbered or reused. New opcodes are appended in unused numeric ranges. An incompatible change to an existing opcode's operand or result meaning requires a registry-major version change.

## Operand and result model

Each instruction operand carries exactly one explicit kind:

| Kind | Meaning |
|---|---|
| `VALUE` | Reference to an OIR value |
| `BLOCK` | Reference to a basic block |
| `FUNCTION` | Reference to a function |
| `TYPE` | Reference to a module-local type |
| `INTEGER_IMMEDIATE` | Schema-defined integer immediate |
| `FLOAT_IMMEDIATE` | Schema-defined floating-point immediate |

The registry descriptor defines the allowed operand kinds and arity. It also defines whether an instruction must produce a result, must not produce a result, or may produce one. Each instruction result is a typed SSA value.

The registry currently validates structural shape only. It does not replace module verification: operand/result type compatibility, function signatures, SSA definition/use rules, block ownership and dominance remain verifier responsibilities.

## Initial opcode IDs

| ID | Symbol | Name | Operands | Result | Flags |
|---:|---|---|---|---|---|
| 1 | `OIR_OP_CONST_INT` | `const.int` | integer immediate | Required | Pure |
| 2 | `OIR_OP_CONST_FLOAT` | `const.float` | float immediate (IEEE-754 binary64 bits) | Required | Pure |
| 3 | `OIR_OP_CONST_BOOL` | `const.bool` | integer immediate (0 or 1) | Required | Pure |
| 16 | `OIR_OP_ADD` | `add` | value, value | Required | Pure |
| 17 | `OIR_OP_SUB` | `sub` | value, value | Required | Pure |
| 18 | `OIR_OP_MUL` | `mul` | value, value | Required | Pure |
| 19 | `OIR_OP_DIV` | `div` | value, value | Required | Pure, may trap |
| 20 | `OIR_OP_REM` | `rem` | value, value | Required | Pure, may trap |
| 21 | `OIR_OP_NEG` | `neg` | value | Required | Pure |
| 22 | `OIR_OP_BIT_AND` | `bit.and` | value, value | Required | Pure |
| 23 | `OIR_OP_BIT_OR` | `bit.or` | value, value | Required | Pure |
| 24 | `OIR_OP_BIT_XOR` | `bit.xor` | value, value | Required | Pure |
| 25 | `OIR_OP_BIT_NOT` | `bit.not` | value | Required | Pure |
| 26 | `OIR_OP_BIT_SHL` | `bit.shl` | value, value | Required | Pure, may trap |
| 27 | `OIR_OP_BIT_SHR` | `bit.shr` | value, value | Required | Pure, may trap |
| 32–37 | `OIR_OP_CMP_*` | `cmp.eq/ne/lt/le/gt/ge` | value, value | Required | Pure |
| 48 | `OIR_OP_CAST` | `cast` | value, type | Required | Pure |
| 64 | `OIR_OP_CALL` | `call` | function, zero or more values | Optional | May trap |
| 80 | `OIR_OP_LOAD` | `load` | value (address) | Required | Reads memory, may trap |
| 81 | `OIR_OP_STORE` | `store` | value (address), value | Forbidden | Writes memory, may trap |
| 96 | `OIR_OP_BRANCH` | `branch` | block | Forbidden | Terminator |
| 97 | `OIR_OP_COND_BRANCH` | `cond.branch` | value, block, block | Forbidden | Terminator |
| 98 | `OIR_OP_RETURN` | `return` | zero or one value | Forbidden | Terminator |
| 99 | `OIR_OP_UNREACHABLE` | `unreachable` | 0 | Forbidden | Terminator, may trap |

The comparison family uses consecutive IDs: 32 equality, 33 inequality, 34 less-than, 35 less-or-equal, 36 greater-than, and 37 greater-or-equal.

## Semantics and deliberate boundaries

- Constant opcodes carry exactly one explicitly typed immediate operand. Integer immediates use the 64-bit two's-complement bit pattern; floating immediates use IEEE-754 binary64 bits; boolean immediates are integer 0 or 1. The result value's module type defines the interpreted scalar width. No native C struct or host-endian floating representation is serialized.
- Arithmetic opcode IDs are type-neutral. The module verifier must enforce the supported integer/floating type combinations and result type; the registry does not silently apply C arithmetic conversions.
- `cast` names an explicit conversion to a module-local type. It must not be used to invent an implicit conversion absent from the semantic artifact.
- `call` names a function in operand zero. Remaining operands are argument values in declared parameter order. Its result is absent only for a void-returning function.
- `load` and `store` are explicit memory operations. Address space, alignment, volatility, atomic ordering and ownership constraints require additional typed metadata and are not inferred by this initial registry.
- `branch`, `cond.branch`, `return` and `unreachable` terminate a basic block. A block must not contain another instruction after its terminator.

The 1.1 extension adds bitwise AND/OR/XOR/NOT with stable IDs 22–25. The 1.2 extension adds integer left/right shifts with stable IDs 26–27. Shift operands are both integer values; the left operand type must exactly match the result type, while the shift-count integer type may differ. The shift opcodes are marked PURE and MAY_TRAP because a negative dynamic count is a runtime trap under the language contract. Counts greater than or equal to the left operand width produce zero. Bitwise AND/OR/XOR/NOT accept integer and bit/byte representation types only when operand and result type IDs match exactly; bool and floating-point values are rejected. The current Semantic-to-OIR expression lowerer has a limited scalar configuration and does not yet provide general mixed-width operand lowering; 128-bit constant payload encoding is not defined by the current immediate record.

This set still supports only the scalar-function foundation, bitwise instruction contracts and integer shift contracts. Switch, phi/block-parameter incoming-edge records, allocation/lifetime operations, aggregate operations, exceptions, NEXSUS Flow ownership operations and concurrency/hardware intrinsics are intentionally not registered until their semantics are specified.
