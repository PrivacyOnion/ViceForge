#include "test_assert.hpp"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int run_cmd(const std::filesystem::path& exe, const std::string& args) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmdline = "\"" + exe.string() + "\" " + args;
    if (!CreateProcessA(exe.string().c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "CreateProcessA failed (" << GetLastError() << ") for: " << cmdline << "\n";
        return -1;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exit_code);
}

void write_u32_le(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    buf[off + 0] = static_cast<uint8_t>(v & 0xFFu);
    buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    buf[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    buf[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

void write_u64_le(std::vector<uint8_t>& buf, size_t off, uint64_t v) {
    write_u32_le(buf, off, static_cast<uint32_t>(v & 0xFFFFFFFFull));
    write_u32_le(buf, off + 4, static_cast<uint32_t>(v >> 32));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: test_cli_e2e <vfcli.exe> <sample_mod.dll> <sample_mod.json> <sample_addon.dll> <sample_addon.json>\n";
        return 2;
    }

    const std::filesystem::path vfcli(argv[1]);
    const auto temp = std::filesystem::temp_directory_path() / "viceforge_test_cli_e2e";
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp);

    // 1. vfcli init
    const auto scaffold_dir = temp / "my_custom_mod";
    VF_CHECK(run_cmd(vfcli, "init my_custom_mod --dir \"" + scaffold_dir.string() + "\"") == 0);
    VF_CHECK(std::filesystem::exists(scaffold_dir / "viceforge.mod.json"));
    VF_CHECK(std::filesystem::exists(scaffold_dir / "src" / "mod_main.cpp"));
    VF_CHECK(std::filesystem::exists(scaffold_dir / "CMakeLists.txt"));

    // 2. Stage valid mods directory -> validate + run-headless
    const auto mods = temp / "mods";
    const auto dir_a = mods / "sample_mod";
    const auto dir_b = mods / "sample_addon";
    std::filesystem::create_directories(dir_a / "assets");
    std::filesystem::create_directories(dir_b / "assets");
    std::filesystem::copy_file(argv[2], dir_a / "sample_mod.dll");
    std::filesystem::copy_file(argv[3], dir_a / "viceforge.mod.json");
    std::filesystem::copy_file(argv[4], dir_b / "sample_addon.dll");
    std::filesystem::copy_file(argv[5], dir_b / "viceforge.mod.json");
    std::ofstream(dir_a / "assets" / "sample.meta") << "a";
    std::ofstream(dir_b / "assets" / "addon.meta") << "b";

    VF_CHECK(run_cmd(vfcli, "validate \"" + mods.string() + "\"") == 0);
    VF_CHECK(run_cmd(vfcli, "run-headless \"" + mods.string() + "\"") == 0);

    // 3. Synthetic RPF9 + TTPF + MicrosoftGame.Config CLI inspections
    std::vector<uint8_t> rpf(32, 0);
    rpf[0] = '9';
    rpf[1] = 'F';
    rpf[2] = 'P';
    rpf[3] = 'R';
    const auto rpf_file = temp / "test.rpf";
    std::ofstream(rpf_file, std::ios::binary).write(reinterpret_cast<const char*>(rpf.data()), rpf.size());
    VF_CHECK(run_cmd(vfcli, "inspect-rpf \"" + rpf_file.string() + "\"") == 0);

    std::vector<uint8_t> ttpf(0x200, 0);
    ttpf[0] = 'R';
    ttpf[1] = 'D';
    ttpf[2] = 'S';
    ttpf[3] = '5';
    write_u32_le(ttpf, 4, 2u);
    write_u64_le(ttpf, 8, 4096u);
    write_u32_le(ttpf, 16, 1u); // ETPF at 0x100
    write_u64_le(ttpf, 0x18, (uint64_t{1} << 30) | (uint64_t{8} << 26) | 1u);
    write_u64_le(ttpf, 0x20, (uint64_t{2} << 30) | (uint64_t{0} << 26) | 2u);
    ttpf[0x100] = 'E';
    ttpf[0x101] = 'T';
    ttpf[0x102] = 'P';
    ttpf[0x103] = 'F';
    write_u32_le(ttpf, 0x104, 1u);
    write_u32_le(ttpf, 0x108, 1u);
    write_u32_le(ttpf, 0x110, 15u | (15u << 14));
    write_u32_le(ttpf, 0x114, 98u << 3);
    write_u32_le(ttpf, 0x118, 1u);
    write_u64_le(ttpf, 0x120, 0xDEAD0000BEEF1234ull);
    const auto ttpf_file = temp / "test.ttpf";
    std::ofstream(ttpf_file, std::ios::binary).write(reinterpret_cast<const char*>(ttpf.data()), ttpf.size());
    VF_CHECK(run_cmd(vfcli, "inspect-ttpf \"" + ttpf_file.string() + "\" --hash 0xDEAD0000BEEF1234") == 0);

    {
        std::ofstream cfg(temp / "MicrosoftGame.Config", std::ios::binary);
        cfg << "<Game><TitleId>7952987A</TitleId><Executable Name=\"game_scarlett_master.exe\" TargetDeviceFamily=\"Scarlett\" /></Game>";
    }
    VF_CHECK(run_cmd(vfcli, "inspect-package \"" + temp.string() + "\"") == 0);

    std::filesystem::remove_all(temp, ec);
    std::cout << "test_cli_e2e passed\n";
    return 0;
}
