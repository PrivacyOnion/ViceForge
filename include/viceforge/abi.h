#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VICEFORGE_ABI_VERSION 0x00010000u

typedef enum VfLogLevel {
    VF_LOG_TRACE = 0,
    VF_LOG_DEBUG = 1,
    VF_LOG_INFO = 2,
    VF_LOG_WARN = 3,
    VF_LOG_ERROR = 4
} VfLogLevel;

typedef enum VfStatus {
    VF_OK = 0,
    VF_ERR_INVALID_ARG = 1,
    VF_ERR_ABI_MISMATCH = 2,
    VF_ERR_NOT_FOUND = 3,
    VF_ERR_CONFLICT = 4,
    VF_ERR_IO = 5,
    VF_ERR_FORMAT = 6,
    VF_ERR_INTERNAL = 7
} VfStatus;

typedef enum VfEventKind {
    VF_EVENT_MOD_LOAD = 0,
    VF_EVENT_ASSET_MOUNT = 1,
    VF_EVENT_PRE_TICK = 2,
    VF_EVENT_POST_TICK = 3,
    VF_EVENT_SCRIPT_TICK = 4,
    VF_EVENT_DEVICE_CREATE = 5,
    VF_EVENT_PRESENT = 6,
    VF_EVENT_SHUTDOWN = 7
} VfEventKind;

typedef struct VfModInfo {
    const char* id;
    const char* name;
    const char* version;
    uint32_t abi_version;
} VfModInfo;

typedef struct VfEventPayload {
    uint32_t kind;
    uint64_t tick_index;
    double delta_seconds;
    uint8_t cancellable;
    uint8_t cancelled;
    const char* detail_text;
} VfEventPayload;

typedef struct VfHostApi {
    uint32_t abi_version;
    void (*log)(void* user_data, uint32_t level, const char* mod_id, const char* message);
    int32_t (*register_asset_override)(void* user_data,
                                       const char* mod_id,
                                       const char* virtual_path,
                                       const char* host_file_path);
    int32_t (*register_hash_override)(void* user_data,
                                      const char* mod_id,
                                      uint64_t hash64,
                                      const char* host_file_path);
    uint32_t (*get_mod_count)(void* user_data);
    uint8_t (*is_mod_loaded)(void* user_data, const char* mod_id);
    uint8_t (*is_runtime_attached)(void* user_data);
    void* user_data;
} VfHostApi;

typedef int32_t (*VfModQueryFn)(VfModInfo* out_info);
typedef int32_t (*VfModInitFn)(const VfHostApi* host);
typedef int32_t (*VfModEventFn)(VfEventPayload* payload);
typedef void (*VfModShutdownFn)(void);

#ifdef __cplusplus
}
#endif
