#ifndef OCC_TARGET_PROFILE_H
#define OCC_TARGET_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_TARGET_PROFILE_SCHEMA_MAJOR 2u
#define OCC_TARGET_PROFILE_SCHEMA_MINOR 0u

#define OCC_TARGET_MAX_DEVICES 32u
#define OCC_TARGET_MAX_MEMORIES 64u
#define OCC_TARGET_MAX_INTERCONNECTS 64u
#define OCC_TARGET_MAX_DEVICE_CAPABILITIES 24u
#define OCC_TARGET_ID_SIZE 32u
#define OCC_TARGET_NAME_SIZE 64u
#define OCC_TARGET_ARCH_SIZE 64u
#define OCC_TARGET_PROTOCOL_SIZE 48u

typedef enum OccTargetProfileKind {
    OCC_TARGET_PROFILE_FAMILY = 1,
    OCC_TARGET_PROFILE_EXACT_MACHINE = 2
} OccTargetProfileKind;

typedef enum OccTargetDeviceKind {
    OCC_TARGET_DEVICE_CPU = 1,
    OCC_TARGET_DEVICE_GPU = 2,
    OCC_TARGET_DEVICE_NPU = 3,
    OCC_TARGET_DEVICE_FAPU = 4,
    OCC_TARGET_DEVICE_DSP = 5,
    OCC_TARGET_DEVICE_FPGA = 6,
    OCC_TARGET_DEVICE_MEMORY_CONTROLLER = 7,
    OCC_TARGET_DEVICE_STORAGE_CONTROLLER = 8,
    OCC_TARGET_DEVICE_OTHER = 255
} OccTargetDeviceKind;

typedef enum OccTargetMemoryKind {
    OCC_TARGET_MEMORY_RAM = 1,
    OCC_TARGET_MEMORY_VRAM = 2,
    OCC_TARGET_MEMORY_SRAM = 3,
    OCC_TARGET_MEMORY_FLASH = 4,
    OCC_TARGET_MEMORY_ROM = 5,
    OCC_TARGET_MEMORY_PERSISTENT = 6
} OccTargetMemoryKind;

typedef enum OccTargetBindingMode {
    OCC_TARGET_BINDING_NONE = 0,
    OCC_TARGET_BINDING_SECURE_ELEMENT_KEY = 1,
    OCC_TARGET_BINDING_TPM_KEY = 2
} OccTargetBindingMode;

typedef struct OccTargetDevice {
    char id[OCC_TARGET_ID_SIZE];
    OccTargetDeviceKind kind;
    char vendor[OCC_TARGET_NAME_SIZE];
    char model[OCC_TARGET_NAME_SIZE];
    char architecture[OCC_TARGET_ARCH_SIZE];
    char revision[OCC_TARGET_NAME_SIZE];
    char isa_profile[OCC_TARGET_NAME_SIZE];
    uint32_t core_count;
    uint32_t execution_unit_count;
    uint32_t lane_count;
    uint32_t group_width;
    uint64_t local_memory_bytes;
    uint64_t cache_bytes;
    uint64_t vram_bytes;
    char capabilities[OCC_TARGET_MAX_DEVICE_CAPABILITIES][OCC_TARGET_NAME_SIZE];
    uint32_t capability_count;
} OccTargetDevice;

typedef struct OccTargetMemory {
    char id[OCC_TARGET_ID_SIZE];
    OccTargetMemoryKind kind;
    char owner_device_id[OCC_TARGET_ID_SIZE]; /* Empty means platform-owned/shared. */
    char address_space[OCC_TARGET_NAME_SIZE];
    uint64_t base_address;
    uint64_t capacity_bytes;
    uint32_t bank_count;
    uint32_t cache_line_bytes;
    uint32_t page_size_bytes;
    uint32_t flags; /* Reserved for coherency, ECC, persistence and access policy bits. */
} OccTargetMemory;

typedef struct OccTargetInterconnect {
    char id[OCC_TARGET_ID_SIZE];
    char from_device_id[OCC_TARGET_ID_SIZE];
    char to_device_id[OCC_TARGET_ID_SIZE];
    char protocol[OCC_TARGET_PROTOCOL_SIZE];
    uint32_t width_bits;
    uint32_t lane_count;
    uint64_t bandwidth_bytes_per_second;
    uint64_t latency_nanoseconds;
    uint8_t bidirectional;
    uint8_t coherent;
    uint8_t dma_capable;
    uint8_t reserved;
} OccTargetInterconnect;

typedef struct OccTargetMachineBinding {
    OccTargetBindingMode mode;
    uint8_t manifest_signature_required;
    uint8_t runtime_verification_required;
    uint8_t require_exact_topology;
    uint8_t reserved;
} OccTargetMachineBinding;

typedef struct OccTargetProfile {
    uint32_t schema_major;
    uint32_t schema_minor;
    char profile_id[OCC_TARGET_NAME_SIZE];
    OccTargetProfileKind profile_kind;
    char os[OCC_TARGET_NAME_SIZE];
    char runtime[OCC_TARGET_NAME_SIZE];
    char package_format[OCC_TARGET_NAME_SIZE];
    OccTargetDevice devices[OCC_TARGET_MAX_DEVICES];
    uint32_t device_count;
    OccTargetMemory memories[OCC_TARGET_MAX_MEMORIES];
    uint32_t memory_count;
    OccTargetInterconnect interconnects[OCC_TARGET_MAX_INTERCONNECTS];
    uint32_t interconnect_count;
    OccTargetMachineBinding machine_binding;
} OccTargetProfile;

typedef enum OccTargetProfileStatus {
    OCC_TARGET_PROFILE_OK = 0,
    OCC_TARGET_PROFILE_INVALID_ARGUMENT,
    OCC_TARGET_PROFILE_UNSUPPORTED_SCHEMA,
    OCC_TARGET_PROFILE_INVALID_ID,
    OCC_TARGET_PROFILE_DUPLICATE_ID,
    OCC_TARGET_PROFILE_INVALID_COUNT,
    OCC_TARGET_PROFILE_INVALID_DEVICE,
    OCC_TARGET_PROFILE_INVALID_MEMORY,
    OCC_TARGET_PROFILE_INVALID_INTERCONNECT,
    OCC_TARGET_PROFILE_DANGLING_REFERENCE
} OccTargetProfileStatus;

/* Structural validation only; backend support and physical identity are separate gates. */
OccTargetProfileStatus occ_target_profile_validate(
    const OccTargetProfile* profile,
    char* error_message,
    size_t error_message_size
);

#ifdef __cplusplus
}
#endif

#endif
