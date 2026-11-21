#pragma once

#include "viceforge/abi.h"
#include "viceforge/export.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace viceforge {

class ModContext {
public:
    ModContext(const VfHostApi* host, std::string mod_id)
        : host_(host), mod_id_(std::move(mod_id)) {}

    void log(VfLogLevel level, std::string_view message) const {
        if (host_ != nullptr && host_->log != nullptr) {
            const std::string copy(message);
            host_->log(host_->user_data, static_cast<uint32_t>(level), mod_id_.c_str(), copy.c_str());
        }
    }

    void info(std::string_view message) const { log(VF_LOG_INFO, message); }
    void warn(std::string_view message) const { log(VF_LOG_WARN, message); }
    void error(std::string_view message) const { log(VF_LOG_ERROR, message); }

    VfStatus register_asset_override(std::string_view virtual_path,
                                     std::string_view host_file_path) const {
        if (host_ == nullptr || host_->register_asset_override == nullptr) {
            return VF_ERR_INVALID_ARG;
        }
        const std::string vpath(virtual_path);
        const std::string hpath(host_file_path);
        return static_cast<VfStatus>(
            host_->register_asset_override(host_->user_data, mod_id_.c_str(), vpath.c_str(), hpath.c_str()));
    }

    VfStatus register_hash_override(uint64_t hash64, std::string_view host_file_path) const {
        if (host_ == nullptr || host_->register_hash_override == nullptr) {
            return VF_ERR_INVALID_ARG;
        }
        const std::string hpath(host_file_path);
        return static_cast<VfStatus>(
            host_->register_hash_override(host_->user_data, mod_id_.c_str(), hash64, hpath.c_str()));
    }

    uint32_t mod_count() const {
        return (host_ != nullptr && host_->get_mod_count != nullptr) ? host_->get_mod_count(host_->user_data) : 0;
    }

    bool is_mod_loaded(std::string_view id) const {
        if (host_ == nullptr || host_->is_mod_loaded == nullptr) {
            return false;
        }
        const std::string copy(id);
        return host_->is_mod_loaded(host_->user_data, copy.c_str()) != 0;
    }

    bool is_runtime_attached() const {
        return (host_ != nullptr && host_->is_runtime_attached != nullptr)
                   ? (host_->is_runtime_attached(host_->user_data) != 0)
                   : false;
    }

    const std::string& mod_id() const noexcept { return mod_id_; }
    const VfHostApi* raw() const noexcept { return host_; }

private:
    const VfHostApi* host_;
    std::string mod_id_;
};

class Mod {
public:
    virtual ~Mod() = default;
    virtual VfStatus on_init(const ModContext& ctx) = 0;
    virtual VfStatus on_event(const ModContext& ctx, VfEventPayload& payload) {
        (void)ctx;
        (void)payload;
        return VF_OK;
    }
    virtual void on_shutdown(const ModContext& ctx) { (void)ctx; }
};

namespace detail {

template <typename ModT>
struct ModInstanceHolder {
    static inline std::unique_ptr<ModT> instance;
    static inline std::unique_ptr<ModContext> context;
};

} // namespace detail

} // namespace viceforge

#define VICEFORGE_DECLARE_MOD(ModClass, ModId, ModName, ModVersion)                                \
    VICEFORGE_MOD_EXPORT int32_t vf_mod_query(VfModInfo* out_info) {                               \
        if (!out_info) {                                                                           \
            return VF_ERR_INVALID_ARG;                                                             \
        }                                                                                          \
        out_info->id = (ModId);                                                                    \
        out_info->name = (ModName);                                                                \
        out_info->version = (ModVersion);                                                          \
        out_info->abi_version = VICEFORGE_ABI_VERSION;                                             \
        return VF_OK;                                                                              \
    }                                                                                              \
    VICEFORGE_MOD_EXPORT int32_t vf_mod_init(const VfHostApi* host) {                              \
        if (!host || host->abi_version != VICEFORGE_ABI_VERSION) {                                 \
            return VF_ERR_ABI_MISMATCH;                                                            \
        }                                                                                          \
        using Holder = ::viceforge::detail::ModInstanceHolder<ModClass>;                           \
        Holder::context = std::make_unique<::viceforge::ModContext>(host, (ModId));                \
        Holder::instance = std::make_unique<ModClass>();                                           \
        return Holder::instance->on_init(*Holder::context);                                        \
    }                                                                                              \
    VICEFORGE_MOD_EXPORT int32_t vf_mod_on_event(VfEventPayload* payload) {                        \
        if (!payload) {                                                                            \
            return VF_ERR_INVALID_ARG;                                                             \
        }                                                                                          \
        using Holder = ::viceforge::detail::ModInstanceHolder<ModClass>;                           \
        if (!Holder::instance || !Holder::context) {                                               \
            return VF_ERR_NOT_FOUND;                                                               \
        }                                                                                          \
        return Holder::instance->on_event(*Holder::context, *payload);                             \
    }                                                                                              \
    VICEFORGE_MOD_EXPORT void vf_mod_shutdown(void) {                                            \
        using Holder = ::viceforge::detail::ModInstanceHolder<ModClass>;                           \
        if (Holder::instance && Holder::context) {                                                 \
            Holder::instance->on_shutdown(*Holder::context);                                       \
        }                                                                                          \
        Holder::instance.reset();                                                                  \
        Holder::context.reset();                                                                   \
    }
