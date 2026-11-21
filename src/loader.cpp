#include "viceforge/loader.hpp"

#include <windows.h>

namespace viceforge {

namespace {

std::wstring utf8_to_wide(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), wide.data(), needed);
    return wide;
}

} // namespace

std::string_view HeadlessRuntimeBridge::backend_name() const noexcept {
    return "HeadlessRuntimeBridge";
}

bool HeadlessRuntimeBridge::is_game_attached() const noexcept {
    return false;
}

RuntimeCapability HeadlessRuntimeBridge::capabilities() const noexcept {
    return RuntimeCapability::None;
}

bool HeadlessRuntimeBridge::install_file_interception(std::string& error) {
    (void)error;
    file_hook_ready_ = true;
    return true;
}

bool HeadlessRuntimeBridge::install_ttpf_interception(std::string& error) {
    (void)error;
    ttpf_hook_ready_ = true;
    return true;
}

bool HeadlessRuntimeBridge::install_script_bridge(std::string& error) {
    (void)error;
    script_bridge_ready_ = true;
    return true;
}

bool HeadlessRuntimeBridge::install_present_hook(std::string& error) {
    (void)error;
    present_hook_ready_ = true;
    return true;
}

void HeadlessRuntimeBridge::uninstall_all() {
    file_hook_ready_ = false;
    ttpf_hook_ready_ = false;
    script_bridge_ready_ = false;
    present_hook_ready_ = false;
}

ModLoader::ModLoader(std::shared_ptr<IRuntimeBridge> bridge)
    : bridge_(bridge ? std::move(bridge) : std::make_shared<HeadlessRuntimeBridge>()) {
    host_api_.abi_version = VICEFORGE_ABI_VERSION;
    host_api_.log = &ModLoader::host_log;
    host_api_.register_asset_override = &ModLoader::host_register_asset_override;
    host_api_.register_hash_override = &ModLoader::host_register_hash_override;
    host_api_.get_mod_count = &ModLoader::host_get_mod_count;
    host_api_.is_mod_loaded = &ModLoader::host_is_mod_loaded;
    host_api_.is_runtime_attached = &ModLoader::host_is_runtime_attached;
    host_api_.user_data = this;
}

ModLoader::~ModLoader() {
    shutdown();
}

ResolutionReport ModLoader::discover_and_resolve(const std::filesystem::path& mods_root) {
    std::vector<ModManifest> discovered;
    std::vector<std::string> errors;
    if (!discover_manifests(mods_root, discovered, errors)) {
        ResolutionReport failed{};
        failed.ok = false;
        failed.errors = std::move(errors);
        return failed;
    }
    return resolve_load_order(discovered);
}

bool ModLoader::load_all(const std::filesystem::path& mods_root,
                         bool mount_only,
                         std::vector<std::string>& out_errors) {
    shutdown();
    out_errors.clear();

    const ResolutionReport report = discover_and_resolve(mods_root);
    if (!report.ok) {
        out_errors = report.errors;
        for (const auto& err : out_errors) {
            record_log(VF_LOG_ERROR, "viceforge", err);
        }
        return false;
    }

    for (const auto& warn : report.warnings) {
        record_log(VF_LOG_WARN, "viceforge", warn);
    }

    std::string bridge_err;
    if (!bridge_->install_file_interception(bridge_err) ||
        !bridge_->install_ttpf_interception(bridge_err)) {
        out_errors.push_back(bridge_err.empty() ? "Failed to prepare runtime bridge" : bridge_err);
        return false;
    }

    size_t total_mounts = 0;
    for (const auto& manifest : report.ordered_mods) {
        total_mounts += manifest.asset_mounts.size();
    }
    vfs_.reserve(total_mounts, 0);
    loaded_mods_.reserve(report.ordered_mods.size());
    mod_index_by_id_.reserve(report.ordered_mods.size());

    for (size_t i = 0; i < report.ordered_mods.size(); ++i) {
        const auto& manifest = report.ordered_mods[i];
        const uint32_t priority = static_cast<uint32_t>(i + 1u);

        LoadedModHandle handle{};
        handle.manifest = manifest;

        for (const auto& mount : manifest.asset_mounts) {
            const auto source_path = manifest.directory / mount.source_relative_path;
            std::string vfs_err;
            if (!vfs_.register_override(mount.virtual_path, source_path, manifest.id,
                                        priority, mount.pinned, &vfs_err)) {
                out_errors.push_back("Failed mounting asset for '" + manifest.id + "': " + vfs_err);
                shutdown();
                return false;
            }
        }

        if (!mount_only && !manifest.entry_dll.empty()) {
            const auto dll_path = manifest.directory / manifest.entry_dll;
            const std::wstring wide_dll = utf8_to_wide(dll_path.string());
            HMODULE module = LoadLibraryExW(wide_dll.c_str(), nullptr,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!module) {
                module = LoadLibraryW(wide_dll.c_str());
            }
            if (!module) {
                out_errors.push_back("LoadLibrary failed for mod '" + manifest.id + "': " + dll_path.string());
                shutdown();
                return false;
            }
            handle.native_handle = module;
            handle.fn_query = reinterpret_cast<VfModQueryFn>(GetProcAddress(module, "vf_mod_query"));
            handle.fn_init = reinterpret_cast<VfModInitFn>(GetProcAddress(module, "vf_mod_init"));
            handle.fn_event = reinterpret_cast<VfModEventFn>(GetProcAddress(module, "vf_mod_on_event"));
            handle.fn_shutdown = reinterpret_cast<VfModShutdownFn>(GetProcAddress(module, "vf_mod_shutdown"));

            if (!handle.fn_query || !handle.fn_init) {
                out_errors.push_back("Mod DLL '" + manifest.id + "' is missing vf_mod_query or vf_mod_init");
                loaded_mods_.push_back(std::move(handle));
                shutdown();
                return false;
            }

            if (handle.fn_query(&handle.queried_info) != VF_OK ||
                handle.queried_info.abi_version != VICEFORGE_ABI_VERSION) {
                out_errors.push_back("Mod DLL '" + manifest.id + "' ABI version mismatch");
                loaded_mods_.push_back(std::move(handle));
                shutdown();
                return false;
            }

            if (handle.queried_info.id == nullptr || manifest.id != handle.queried_info.id) {
                out_errors.push_back("Mod DLL id mismatch against manifest '" + manifest.id + "'");
                loaded_mods_.push_back(std::move(handle));
                shutdown();
                return false;
            }
        }

        mod_index_by_id_.emplace(handle.manifest.id, loaded_mods_.size());
        loaded_mods_.push_back(std::move(handle));
    }

    if (!mount_only) {
        active_event_callbacks_.reserve(loaded_mods_.size());
        for (auto& handle : loaded_mods_) {
            if (handle.fn_init) {
                const int32_t rc = handle.fn_init(&host_api_);
                if (rc != VF_OK) {
                    out_errors.push_back("vf_mod_init failed for '" + handle.manifest.id + "' with code " +
                                         std::to_string(rc));
                    shutdown();
                    return false;
                }
                handle.initialized = true;
                if (handle.fn_event) {
                    active_event_callbacks_.push_back(handle.fn_event);
                }
            }
        }

        VfEventPayload load_event{VF_EVENT_MOD_LOAD, 0, 0.0, 0, 0, "mods loaded"};
        dispatch_event(load_event);
    }

    return true;
}

void ModLoader::dispatch_event(VfEventPayload& payload) {
    event_bus_.post(payload);
    if (payload.cancellable && payload.cancelled) {
        return;
    }
    for (VfModEventFn fn : active_event_callbacks_) {
        fn(&payload);
        if (payload.cancellable && payload.cancelled) {
            break;
        }
    }
}

void ModLoader::shutdown() {
    if (!loaded_mods_.empty()) {
        VfEventPayload stop_event{VF_EVENT_SHUTDOWN, 0, 0.0, 0, 0, "shutdown"};
        dispatch_event(stop_event);
    }

    active_event_callbacks_.clear();
    mod_index_by_id_.clear();
    for (auto it = loaded_mods_.rbegin(); it != loaded_mods_.rend(); ++it) {
        if (it->initialized && it->fn_shutdown) {
            it->fn_shutdown();
            it->initialized = false;
        }
        if (it->native_handle) {
            FreeLibrary(static_cast<HMODULE>(it->native_handle));
            it->native_handle = nullptr;
        }
    }
    loaded_mods_.clear();
    vfs_.clear();
    event_bus_.clear();
    if (bridge_) {
        bridge_->uninstall_all();
    }
}

void ModLoader::set_log_sink(std::function<void(const LogEntry&)> sink) {
    log_sink_ = std::move(sink);
}

VfsMountTable& ModLoader::vfs() noexcept { return vfs_; }
const VfsMountTable& ModLoader::vfs() const noexcept { return vfs_; }
EventBus& ModLoader::event_bus() noexcept { return event_bus_; }
const IRuntimeBridge& ModLoader::bridge() const noexcept { return *bridge_; }
const std::vector<LoadedModHandle>& ModLoader::loaded_mods() const noexcept { return loaded_mods_; }
const std::vector<LogEntry>& ModLoader::logs() const noexcept { return logs_; }

bool ModLoader::is_mod_loaded(std::string_view mod_id) const {
    return mod_index_by_id_.find(mod_id) != mod_index_by_id_.end();
}

void ModLoader::record_log(VfLogLevel level, std::string source, std::string message) {
    logs_.push_back(LogEntry{level, std::move(source), std::move(message)});
    if (log_sink_) {
        log_sink_(logs_.back());
    }
}

void ModLoader::host_log(void* user_data, uint32_t level, const char* mod_id, const char* message) {
    auto* self = static_cast<ModLoader*>(user_data);
    if (!self) {
        return;
    }
    self->record_log(static_cast<VfLogLevel>(level),
                     mod_id ? mod_id : "unknown",
                     message ? message : "");
}

int32_t ModLoader::host_register_asset_override(void* user_data,
                                                const char* mod_id,
                                                const char* virtual_path,
                                                const char* host_file_path) {
    auto* self = static_cast<ModLoader*>(user_data);
    if (!self || !mod_id || !virtual_path || !host_file_path) {
        return VF_ERR_INVALID_ARG;
    }
    uint32_t priority = 1;
    if (auto it = self->mod_index_by_id_.find(std::string_view(mod_id)); it != self->mod_index_by_id_.end()) {
        priority = static_cast<uint32_t>(it->second + 1u);
    }
    std::string err;
    if (!self->vfs_.register_override(virtual_path, std::filesystem::path(host_file_path),
                                      mod_id, priority, false, &err)) {
        self->record_log(VF_LOG_ERROR, mod_id, err);
        return VF_ERR_INVALID_ARG;
    }
    return VF_OK;
}

int32_t ModLoader::host_register_hash_override(void* user_data,
                                               const char* mod_id,
                                               uint64_t hash64,
                                               const char* host_file_path) {
    auto* self = static_cast<ModLoader*>(user_data);
    if (!self || !mod_id || !host_file_path || hash64 == 0) {
        return VF_ERR_INVALID_ARG;
    }
    uint32_t priority = 1;
    if (auto it = self->mod_index_by_id_.find(std::string_view(mod_id)); it != self->mod_index_by_id_.end()) {
        priority = static_cast<uint32_t>(it->second + 1u);
    }
    std::string err;
    if (!self->vfs_.register_hash_override(hash64, std::filesystem::path(host_file_path),
                                           mod_id, priority, false, &err)) {
        self->record_log(VF_LOG_ERROR, mod_id, err);
        return VF_ERR_INVALID_ARG;
    }
    return VF_OK;
}

uint32_t ModLoader::host_get_mod_count(void* user_data) {
    const auto* self = static_cast<const ModLoader*>(user_data);
    return self ? static_cast<uint32_t>(self->loaded_mods_.size()) : 0u;
}

uint8_t ModLoader::host_is_mod_loaded(void* user_data, const char* mod_id) {
    const auto* self = static_cast<const ModLoader*>(user_data);
    if (!self || !mod_id) {
        return 0;
    }
    return self->is_mod_loaded(mod_id) ? 1u : 0u;
}

uint8_t ModLoader::host_is_runtime_attached(void* user_data) {
    const auto* self = static_cast<const ModLoader*>(user_data);
    return (self && self->bridge_ && self->bridge_->is_game_attached()) ? 1u : 0u;
}

} // namespace viceforge
