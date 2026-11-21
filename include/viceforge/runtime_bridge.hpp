#pragma once

#include "viceforge/export.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace viceforge {

enum class RuntimeCapability : uint32_t {
    None = 0u,
    HookFunctions = 1u << 0,
    VfsInterception = 1u << 1,
    TtpfInterception = 1u << 2,
    ScriptNativeBridge = 1u << 3,
    D3D12PresentHook = 1u << 4
};

constexpr RuntimeCapability operator|(RuntimeCapability a, RuntimeCapability b) noexcept {
    return static_cast<RuntimeCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr bool has_capability(RuntimeCapability mask, RuntimeCapability flag) noexcept {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0u;
}

class IRuntimeBridge {
public:
    virtual ~IRuntimeBridge() = default;

    virtual std::string_view backend_name() const noexcept = 0;
    virtual bool is_game_attached() const noexcept = 0;
    virtual RuntimeCapability capabilities() const noexcept = 0;

    virtual bool install_file_interception(std::string& error) = 0;
    virtual bool install_ttpf_interception(std::string& error) = 0;
    virtual bool install_script_bridge(std::string& error) = 0;
    virtual bool install_present_hook(std::string& error) = 0;
    virtual void uninstall_all() = 0;
};

class HeadlessRuntimeBridge final : public IRuntimeBridge {
public:
    VICEFORGE_API std::string_view backend_name() const noexcept override;
    VICEFORGE_API bool is_game_attached() const noexcept override;
    VICEFORGE_API RuntimeCapability capabilities() const noexcept override;

    VICEFORGE_API bool install_file_interception(std::string& error) override;
    VICEFORGE_API bool install_ttpf_interception(std::string& error) override;
    VICEFORGE_API bool install_script_bridge(std::string& error) override;
    VICEFORGE_API bool install_present_hook(std::string& error) override;
    VICEFORGE_API void uninstall_all() override;

    bool file_hook_ready() const noexcept { return file_hook_ready_; }
    bool ttpf_hook_ready() const noexcept { return ttpf_hook_ready_; }
    bool script_bridge_ready() const noexcept { return script_bridge_ready_; }
    bool present_hook_ready() const noexcept { return present_hook_ready_; }

private:
    bool file_hook_ready_ = false;
    bool ttpf_hook_ready_ = false;
    bool script_bridge_ready_ = false;
    bool present_hook_ready_ = false;
};

} // namespace viceforge
