#ifndef OCC_CODEGEN_H
#define OCC_CODEGEN_H

#include "../parser/ast.h"

#ifdef _WIN32
    #ifdef BUILD_CODEGEN_DLL
        #define CODEGEN_API __declspec(dllexport)
    #else
        #define CODEGEN_API __declspec(dllimport)
    #endif
#else
    #define CODEGEN_API
#endif

typedef enum {
    ABI_NONE = 0,
    ABI_WIN64,
    ABI_SYSV,
    ABI_BAREMETAL,
    ABI_ILP32,
    ABI_AAPCS64
} TargetABI;

typedef enum {
    FEAT_NONE  = 0,
    FEAT_MMX   = 1 << 0,
    FEAT_SSE   = 1 << 1,
    FEAT_SSE2  = 1 << 2,
    FEAT_SSE3  = 1 << 3,
    FEAT_SSE4  = 1 << 4,
    FEAT_AVX   = 1 << 5,
    FEAT_AVX2  = 1 << 6,
    FEAT_AVX512= 1 << 7,
    FEAT_NEON  = 1 << 8
} TargetFeatures;

typedef enum {
    MEM_FLASH = 0,
    MEM_RAM,
    MEM_IO
} MemoryType;

typedef struct {
    char name[32];
    MemoryType type;
    unsigned long long start;
    unsigned long long size;
} MemoryRegion;

typedef struct {
    char name[32];
    unsigned long long address;
} Peripheral;

typedef struct {
    char identifier[64];
    char arch[32];
    char os[32];
    char format[32];
    char endian[16];
    char abi_name[32];
    unsigned int bits;
    TargetABI abi;
    unsigned int features;
    unsigned long long base_addr;
    unsigned long long stack_size;

    MemoryRegion regions[16];
    int region_count;
    Peripheral ports[64];
    int port_count;
} TargetConfig;

CODEGEN_API int codegen_run(const char* ir_path, const char* target_path, const char* out_path);

#endif
