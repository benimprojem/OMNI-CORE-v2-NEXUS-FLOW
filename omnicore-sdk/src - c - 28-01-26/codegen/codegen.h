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
    ABI_WIN64,      // Windows x64 (RCX, RDX, R8, R9)
    ABI_SYSV,       // System V / Linux x64 (RDI, RSI, RDX, RCX, R8, R9)
    ABI_BAREMETAL   // Custom / Minimal
} TargetABI;

typedef enum {
    FEAT_NONE = 0,
    FEAT_MMX   = 1 << 0,
    FEAT_SSE   = 1 << 1,
    FEAT_SSE2  = 1 << 2,
    FEAT_SSE3  = 1 << 3,
    FEAT_SSE4  = 1 << 4,
    FEAT_AVX   = 1 << 5,
    FEAT_AVX2  = 1 << 6,
    FEAT_AVX512= 1 << 7
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
    char arch[32];   // x86_64, arm32, risc-v
    char os[32];     // windows, linux, none (baremetal)
    char format[32]; // elf, pe, bin
    TargetABI abi;
    unsigned int features; // Bitmask of TargetFeatures
    unsigned long long base_addr;
    unsigned long long stack_size;
    
    // Hardware Abstraction (ULTRA)
    MemoryRegion regions[16];
    int region_count;
    Peripheral ports[64];
    int port_count;
} TargetConfig;

// Codegen Giriş Noktası
// ir_path: .oir dosyası yolu
// target_path: .target dosyası yolu
// out_path: Çıktı (asm/bin) dosyası yolu
CODEGEN_API int codegen_run(const char* ir_path, const char* target_path, const char* out_path);

#endif // OCC_CODEGEN_H
