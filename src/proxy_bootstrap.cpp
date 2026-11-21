#include "viceforge/loader.hpp"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace {

std::unique_ptr<viceforge::ModLoader> g_loader;
HANDLE g_worker_thread = nullptr;

std::filesystem::path resolve_mods_directory() {
    wchar_t env_buf[MAX_PATH]{};
    const DWORD env_len = GetEnvironmentVariableW(L"VICEFORGE_MODS_DIR", env_buf, MAX_PATH);
    if (env_len > 0 && env_len < MAX_PATH) {
        return std::filesystem::path(env_buf);
    }
    wchar_t exe_buf[MAX_PATH]{};
    const DWORD exe_len = GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
    if (exe_len > 0 && exe_len < MAX_PATH) {
        return std::filesystem::path(exe_buf).parent_path() / L"mods";
    }
    return std::filesystem::current_path() / "mods";
}

DWORD WINAPI bootstrap_worker(LPVOID) {
    g_loader = std::make_unique<viceforge::ModLoader>();
    const auto mods_dir = resolve_mods_directory();
    std::error_code ec;
    if (std::filesystem::exists(mods_dir, ec)) {
        std::vector<std::string> errors;
        g_loader->load_all(mods_dir, false, errors);
    }
    return 0;
}

} // namespace

extern "C" __declspec(dllexport) uint32_t vf_proxy_abi_version(void) {
    return VICEFORGE_ABI_VERSION;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        // Never perform LoadLibrary or filesystem discovery directly under the OS loader lock.
        g_worker_thread = CreateThread(nullptr, 0, bootstrap_worker, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_worker_thread) {
            WaitForSingleObject(g_worker_thread, 2000);
            CloseHandle(g_worker_thread);
            g_worker_thread = nullptr;
        }
        if (g_loader) {
            g_loader->shutdown();
            g_loader.reset();
        }
    }
    return TRUE;
}
