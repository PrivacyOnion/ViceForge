#pragma once

#include "viceforge/abi.h"
#include "viceforge/events.hpp"
#include "viceforge/export.h"
#include "viceforge/manifest.hpp"
#include "viceforge/runtime_bridge.hpp"
#include "viceforge/vfs.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace viceforge {

struct LogEntry {
    VfLogLevel level = VF_LOG_INFO;
    std::string source;
    std::string message;
};

struct LoadedModHandle {
    ModManifest manifest;
    VfModInfo queried_info{};
    void* native_handle = nullptr;
    VfModQueryFn fn_query = nullptr;
    VfModInitFn fn_init = nullptr;
    VfModEventFn fn_event = nullptr;
    VfModShutdownFn fn_shutdown = nullptr;
    bool initialized = false;
};

class ModLoader {
public:
    VICEFORGE_API explicit ModLoader(std::shared_ptr<IRuntimeBridge> bridge = nullptr);
    VICEFORGE_API ~ModLoader();

    ModLoader(const ModLoader&) = delete;
    ModLoader& operator=(const ModLoader&) = delete;

    VICEFORGE_API ResolutionReport discover_and_resolve(const std::filesystem::path& mods_root);
    VICEFORGE_API bool load_all(const std::filesystem::path& mods_root,
                                bool mount_only,
                                std::vector<std::string>& out_errors);
    VICEFORGE_API void dispatch_event(VfEventPayload& payload);
    VICEFORGE_API void shutdown();

    VICEFORGE_API void set_log_sink(std::function<void(const LogEntry&)> sink);

    VICEFORGE_API VfsMountTable& vfs() noexcept;
    VICEFORGE_API const VfsMountTable& vfs() const noexcept;
    VICEFORGE_API EventBus& event_bus() noexcept;
    VICEFORGE_API const IRuntimeBridge& bridge() const noexcept;
    VICEFORGE_API const std::vector<LoadedModHandle>& loaded_mods() const noexcept;
    VICEFORGE_API const std::vector<LogEntry>& logs() const noexcept;
    VICEFORGE_API bool is_mod_loaded(std::string_view mod_id) const;

private:
    static void host_log(void* user_data, uint32_t level, const char* mod_id, const char* message);
    static int32_t host_register_asset_override(void* user_data,
                                                const char* mod_id,
                                                const char* virtual_path,
                                                const char* host_file_path);
    static int32_t host_register_hash_override(void* user_data,
                                               const char* mod_id,
                                               uint64_t hash64,
                                               const char* host_file_path);
    static uint32_t host_get_mod_count(void* user_data);
    static uint8_t host_is_mod_loaded(void* user_data, const char* mod_id);
    static uint8_t host_is_runtime_attached(void* user_data);

    void record_log(VfLogLevel level, std::string source, std::string message);

    std::shared_ptr<IRuntimeBridge> bridge_;
    VfsMountTable vfs_;
    EventBus event_bus_;
    VfHostApi host_api_{};
    std::vector<LoadedModHandle> loaded_mods_;
    std::vector<LogEntry> logs_;
    std::function<void(const LogEntry&)> log_sink_;
};

} // namespace viceforge
