#include "target_profile.h"

#include <stdio.h>
#include <string.h>

static void set_error(char* out, size_t size, const char* message) {
    if (out && size) {
        (void)snprintf(out, size, "%s", message);
    }
}

static int valid_id(const char* id, size_t capacity) {
    return id && capacity > 1u && id[0] != '\0' &&
        memchr(id, '\0', capacity) != NULL;
}

static int device_index(const OccTargetProfile* profile, const char* id) {
    uint32_t i;
    for (i = 0u; i < profile->device_count; ++i) {
        if (strcmp(profile->devices[i].id, id) == 0) return (int)i;
    }
    return -1;
}

static int memory_index(const OccTargetProfile* profile, const char* id) {
    uint32_t i;
    for (i = 0u; i < profile->memory_count; ++i) {
        if (strcmp(profile->memories[i].id, id) == 0) return (int)i;
    }
    return -1;
}

static int duplicate_device_id(const OccTargetProfile* profile, uint32_t at) {
    uint32_t i;
    for (i = 0u; i < at; ++i) {
        if (strcmp(profile->devices[i].id, profile->devices[at].id) == 0) return 1;
    }
    return 0;
}

static int duplicate_memory_id(const OccTargetProfile* profile, uint32_t at) {
    uint32_t i;
    for (i = 0u; i < at; ++i) {
        if (strcmp(profile->memories[i].id, profile->memories[at].id) == 0) return 1;
    }
    return 0;
}

static int duplicate_link_id(const OccTargetProfile* profile, uint32_t at) {
    uint32_t i;
    for (i = 0u; i < at; ++i) {
        if (strcmp(profile->interconnects[i].id, profile->interconnects[at].id) == 0) return 1;
    }
    return 0;
}

OccTargetProfileStatus occ_target_profile_validate(
    const OccTargetProfile* profile,
    char* error_message,
    size_t error_message_size
) {
    uint32_t i, j;

    if (error_message && error_message_size) error_message[0] = '\0';
    if (!profile) {
        set_error(error_message, error_message_size, "Target profile is null.");
        return OCC_TARGET_PROFILE_INVALID_ARGUMENT;
    }
    if (profile->schema_major != OCC_TARGET_PROFILE_SCHEMA_MAJOR ||
        profile->schema_minor > OCC_TARGET_PROFILE_SCHEMA_MINOR) {
        set_error(error_message, error_message_size, "Unsupported target profile schema version.");
        return OCC_TARGET_PROFILE_UNSUPPORTED_SCHEMA;
    }
    if (!valid_id(profile->profile_id, sizeof(profile->profile_id)) ||
        (profile->profile_kind != OCC_TARGET_PROFILE_FAMILY &&
         profile->profile_kind != OCC_TARGET_PROFILE_EXACT_MACHINE)) {
        set_error(error_message, error_message_size, "Target profile identity or profile kind is invalid.");
        return OCC_TARGET_PROFILE_INVALID_ID;
    }
    if (profile->device_count > OCC_TARGET_MAX_DEVICES ||
        profile->memory_count > OCC_TARGET_MAX_MEMORIES ||
        profile->interconnect_count > OCC_TARGET_MAX_INTERCONNECTS) {
        set_error(error_message, error_message_size, "Target profile table count exceeds its configured limit.");
        return OCC_TARGET_PROFILE_INVALID_COUNT;
    }

    for (i = 0u; i < profile->device_count; ++i) {
        const OccTargetDevice* device = &profile->devices[i];
        if (!valid_id(device->id, sizeof(device->id)) ||
            device->kind < OCC_TARGET_DEVICE_CPU ||
            (device->kind > OCC_TARGET_DEVICE_STORAGE_CONTROLLER &&
             device->kind != OCC_TARGET_DEVICE_OTHER) ||
            !valid_id(device->architecture, sizeof(device->architecture)) ||
            device->capability_count > OCC_TARGET_MAX_DEVICE_CAPABILITIES) {
            set_error(error_message, error_message_size, "Target device record is invalid.");
            return OCC_TARGET_PROFILE_INVALID_DEVICE;
        }
        if (duplicate_device_id(profile, i)) {
            set_error(error_message, error_message_size, "Target profile contains duplicate device IDs.");
            return OCC_TARGET_PROFILE_DUPLICATE_ID;
        }
        for (j = 0u; j < device->capability_count; ++j) {
            if (!valid_id(device->capabilities[j], sizeof(device->capabilities[j]))) {
                set_error(error_message, error_message_size, "Target device contains an invalid capability name.");
                return OCC_TARGET_PROFILE_INVALID_DEVICE;
            }
        }
    }

    for (i = 0u; i < profile->memory_count; ++i) {
        const OccTargetMemory* memory = &profile->memories[i];
        if (!valid_id(memory->id, sizeof(memory->id)) ||
            memory->kind < OCC_TARGET_MEMORY_RAM ||
            memory->kind > OCC_TARGET_MEMORY_PERSISTENT ||
            memory->capacity_bytes == 0u) {
            set_error(error_message, error_message_size, "Target memory record is invalid.");
            return OCC_TARGET_PROFILE_INVALID_MEMORY;
        }
        if (duplicate_memory_id(profile, i) || device_index(profile, memory->id) >= 0) {
            set_error(error_message, error_message_size, "Target profile contains duplicate resource IDs.");
            return OCC_TARGET_PROFILE_DUPLICATE_ID;
        }
        if (memory->owner_device_id[0] &&
            device_index(profile, memory->owner_device_id) < 0) {
            set_error(error_message, error_message_size, "Target memory references an unknown owner device.");
            return OCC_TARGET_PROFILE_DANGLING_REFERENCE;
        }
    }

    for (i = 0u; i < profile->interconnect_count; ++i) {
        const OccTargetInterconnect* link = &profile->interconnects[i];
        if (!valid_id(link->id, sizeof(link->id)) ||
            !valid_id(link->from_device_id, sizeof(link->from_device_id)) ||
            !valid_id(link->to_device_id, sizeof(link->to_device_id)) ||
            !valid_id(link->protocol, sizeof(link->protocol)) ||
            strcmp(link->from_device_id, link->to_device_id) == 0 ||
            link->width_bits == 0u || link->lane_count == 0u) {
            set_error(error_message, error_message_size, "Target interconnect record is invalid.");
            return OCC_TARGET_PROFILE_INVALID_INTERCONNECT;
        }
        if (duplicate_link_id(profile, i) ||
            device_index(profile, link->from_device_id) < 0 ||
            device_index(profile, link->to_device_id) < 0) {
            set_error(error_message, error_message_size, "Target interconnect has a duplicate ID or dangling device reference.");
            return OCC_TARGET_PROFILE_DANGLING_REFERENCE;
        }
    }

    if (profile->profile_kind == OCC_TARGET_PROFILE_EXACT_MACHINE &&
        profile->machine_binding.mode == OCC_TARGET_BINDING_NONE) {
        set_error(error_message, error_message_size,
            "Exact-machine profile requires an explicit machine-binding policy.");
        return OCC_TARGET_PROFILE_INVALID_ARGUMENT;
    }
    if (profile->machine_binding.mode > OCC_TARGET_BINDING_TPM_KEY) {
        set_error(error_message, error_message_size, "Target machine-binding mode is invalid.");
        return OCC_TARGET_PROFILE_INVALID_ARGUMENT;
    }
    return OCC_TARGET_PROFILE_OK;
}
