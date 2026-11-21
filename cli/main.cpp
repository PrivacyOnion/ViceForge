#include "viceforge/loader.hpp"
#include "viceforge/rage9.hpp"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage() {
    std::cout << "ViceForge CLI (vfcli)\n"
              << "Usage:\n"
              << "  vfcli init <mod-id> [--dir <path>]\n"
              << "  vfcli validate <mods-dir>\n"
              << "  vfcli run-headless <mods-dir>\n"
              << "  vfcli inspect-rpf <file.rpf>\n"
              << "  vfcli inspect-ttpf <file.ttpf> [--hash <hex64>]\n"
              << "  vfcli inspect-package <game-dir>\n";
}

bool is_valid_id(std::string_view id) {
    if (id.empty()) {
        return false;
    }
    for (char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string level_label(VfLogLevel level) {
    switch (level) {
        case VF_LOG_TRACE: return "TRACE";
        case VF_LOG_DEBUG: return "DEBUG";
        case VF_LOG_INFO:  return "INFO";
        case VF_LOG_WARN:  return "WARN";
        case VF_LOG_ERROR: return "ERROR";
        default:           return "LOG";
    }
}

int cmd_init(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli init requires <mod-id>\n";
        return 2;
    }
    const std::string mod_id = argv[2];
    if (!is_valid_id(mod_id)) {
        std::cerr << "Error: mod-id must match [a-z0-9_-]+\n";
        return 2;
    }

    std::filesystem::path target_dir = std::filesystem::current_path() / mod_id;
    for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --dir requires a path argument\n";
                return 2;
            }
            target_dir = std::filesystem::path(argv[++i]);
        } else {
            std::cerr << "Error: unknown argument '" << argv[i] << "'\n";
            return 2;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(target_dir / "src", ec);
    std::filesystem::create_directories(target_dir / "assets", ec);
    if (ec) {
        std::cerr << "Error creating directories: " << ec.message() << "\n";
        return 1;
    }

    {
        std::ofstream manifest(target_dir / "viceforge.mod.json", std::ios::binary);
        manifest << "{\n"
                 << "  \"id\": \"" << mod_id << "\",\n"
                 << "  \"name\": \"" << mod_id << "\",\n"
                 << "  \"version\": \"0.1.0\",\n"
                 << "  \"entry_dll\": \"" << mod_id << ".dll\",\n"
                 << "  \"dependencies\": [],\n"
                 << "  \"conflicts\": [],\n"
                 << "  \"load_before\": [],\n"
                 << "  \"load_after\": [],\n"
                 << "  \"asset_mounts\": []\n"
                 << "}\n";
    }

    {
        std::ofstream cmake(target_dir / "CMakeLists.txt", std::ios::binary);
        cmake << "cmake_minimum_required(VERSION 3.24)\n"
              << "project(" << mod_id << " LANGUAGES CXX)\n\n"
              << "set(CMAKE_CXX_STANDARD 20)\n"
              << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
              << "add_library(" << mod_id << " SHARED src/mod_main.cpp)\n"
              << "# Point target_include_directories at <ViceForge>/include\n";
    }

    {
        std::ofstream cpp(target_dir / "src" / "mod_main.cpp", std::ios::binary);
        cpp << "#include \"viceforge/mod.hpp\"\n\n"
            << "class ModMain final : public viceforge::Mod {\n"
            << "public:\n"
            << "    VfStatus on_init(const viceforge::ModContext& ctx) override {\n"
            << "        ctx.info(\"Initialized " << mod_id << "\");\n"
            << "        return VF_OK;\n"
            << "    }\n\n"
            << "    VfStatus on_event(const viceforge::ModContext& ctx, VfEventPayload& payload) override {\n"
            << "        if (payload.kind == VF_EVENT_SHUTDOWN) {\n"
            << "            ctx.info(\"Shutting down " << mod_id << "\");\n"
            << "        }\n"
            << "        return VF_OK;\n"
            << "    }\n"
            << "};\n\n"
            << "VICEFORGE_DECLARE_MOD(ModMain, \"" << mod_id << "\", \"" << mod_id << "\", \"0.1.0\")\n";
    }

    {
        std::ofstream notes(target_dir / "assets" / "README.txt", std::ios::binary);
        notes << "Place VFS replacement files for " << mod_id << " in this directory.\n";
    }

    std::cout << "Scaffolded mod '" << mod_id << "' in " << target_dir.string() << "\n";
    return 0;
}

int cmd_validate(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli validate requires <mods-dir>\n";
        return 2;
    }
    const std::filesystem::path mods_dir(argv[2]);
    viceforge::ModLoader loader;
    std::vector<std::string> errors;
    if (!loader.load_all(mods_dir, true, errors)) {
        for (const auto& err : errors) {
            std::cerr << "Validation error: " << err << "\n";
        }
        return 1;
    }

    std::cout << "Validated " << loader.loaded_mods().size() << " mod(s) in topological load order:\n";
    for (size_t i = 0; i < loader.loaded_mods().size(); ++i) {
        const auto& m = loader.loaded_mods()[i].manifest;
        std::cout << "  [" << (i + 1) << "] " << m.id << " v" << m.version.to_string()
                  << " (" << m.name << ")\n";
    }

    const auto path_overrides = loader.vfs().list_path_overrides();
    std::cout << "VFS path overrides: " << path_overrides.size() << "\n";
    for (const auto& rec : path_overrides) {
        std::cout << "  " << rec.normalized_virtual_path << " -> " << rec.host_file_path.string()
                  << " [owner=" << rec.owner_mod_id << ", prio=" << rec.priority
                  << (rec.pinned ? ", pinned" : "") << "]\n";
    }

    const auto& conflicts = loader.vfs().conflicts();
    std::cout << "VFS conflicts resolved: " << conflicts.size() << "\n";
    for (const auto& c : conflicts) {
        std::cout << "  target=" << c.target_key << " replaced=" << c.replaced_mod_id
                  << " winner=" << c.winning_mod_id << "\n";
    }
    return 0;
}

int cmd_run_headless(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli run-headless requires <mods-dir>\n";
        return 2;
    }
    const std::filesystem::path mods_dir(argv[2]);
    viceforge::ModLoader loader;
    loader.set_log_sink([](const viceforge::LogEntry& entry) {
        std::cout << "[" << level_label(entry.level) << "][" << entry.source << "] "
                  << entry.message << "\n";
    });

    std::vector<std::string> errors;
    if (!loader.load_all(mods_dir, false, errors)) {
        for (const auto& err : errors) {
            std::cerr << "Load error: " << err << "\n";
        }
        return 1;
    }

    std::cout << "Bridge: " << loader.bridge().backend_name()
              << " (game_attached=" << (loader.bridge().is_game_attached() ? "true" : "false") << ")\n";

    VfEventPayload asset_mount{VF_EVENT_ASSET_MOUNT, 0, 0.0, 0, 0, "headless asset mount"};
    loader.dispatch_event(asset_mount);

    VfEventPayload pre_tick{VF_EVENT_PRE_TICK, 1, 0.016, 1, 0, "headless tick"};
    loader.dispatch_event(pre_tick);

    VfEventPayload post_tick{VF_EVENT_POST_TICK, 1, 0.016, 0, 0, "headless tick"};
    loader.dispatch_event(post_tick);

    std::cout << "Loaded mods: " << loader.loaded_mods().size()
              << ", VFS path overrides: " << loader.vfs().list_path_overrides().size()
              << ", VFS hash overrides: " << loader.vfs().list_hash_overrides().size()
              << ", conflicts: " << loader.vfs().conflicts().size() << "\n";

    loader.shutdown();
    return 0;
}

int cmd_inspect_rpf(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli inspect-rpf requires <file.rpf>\n";
        return 2;
    }
    viceforge::Rpf9Header hdr{};
    std::string err;
    if (!viceforge::parse_rpf9_header(std::filesystem::path(argv[2]), hdr, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }
    std::cout << "RPF9 archive: " << argv[2] << "\n"
              << "  magic:         0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << hdr.magic << " (9FPR)\n"
              << "  file_size:     " << std::dec << hdr.file_size << " bytes\n"
              << "  header_words:  0x" << std::hex << hdr.header_word_1
              << ", 0x" << hdr.header_word_2
              << ", 0x" << hdr.header_word_3 << "\n"
              << "  signature_tag: 0x" << std::setw(8) << std::setfill('0') << hdr.signature_hi
              << std::setw(8) << std::setfill('0') << hdr.signature_lo << std::dec << "\n";
    return 0;
}

int cmd_inspect_ttpf(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli inspect-ttpf requires <file.ttpf> [--hash <hex64>]\n";
        return 2;
    }
    bool has_hash = false;
    uint64_t query_hash = 0;
    for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--hash") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --hash requires a 64-bit hex value\n";
                return 2;
            }
            std::string_view tok(argv[++i]);
            if (tok.starts_with("0x") || tok.starts_with("0X")) {
                tok.remove_prefix(2);
            }
            const auto res = std::from_chars(tok.data(), tok.data() + tok.size(), query_hash, 16);
            if (tok.empty() || res.ec != std::errc{} || res.ptr != tok.data() + tok.size()) {
                std::cerr << "Error: invalid hex64 hash '" << argv[i] << "'\n";
                return 2;
            }
            has_hash = true;
        } else {
            std::cerr << "Error: unknown argument '" << argv[i] << "'\n";
            return 2;
        }
    }

    viceforge::TtpfArchive archive;
    std::string err;
    if (!archive.open(std::filesystem::path(argv[2]), err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    std::cout << "TTPF archive: " << archive.path().string() << "\n"
              << "  chunk_slots:   " << archive.chunk_count() << "\n"
              << "  active_chunks: " << archive.active_chunk_entries() << "\n"
              << "  textures:      " << archive.texture_count() << "\n";

    if (archive.texture_count() > 0 && !has_hash) {
        const auto first = archive.texture_info(0);
        if (first.has_value()) {
            const auto chunks = archive.texture_chunks(0);
            std::cout << "  first_texture: hash=0x" << std::hex << std::uppercase
                      << std::setw(16) << std::setfill('0') << first->hash64
                      << std::dec << " " << first->width << "x" << first->height
                      << " format=" << viceforge::dxgi_format_name(first->dxgi)
                      << " tile_mode=" << static_cast<unsigned>(first->tile_mode)
                      << " chunks=" << chunks.size() << "\n";
        }
    }

    if (has_hash) {
        const auto idx = archive.find_by_hash(query_hash);
        if (!idx.has_value()) {
            std::cerr << "Texture hash 0x" << std::hex << std::uppercase
                      << std::setw(16) << std::setfill('0') << query_hash << " not found\n";
            return 1;
        }
        const auto info = archive.texture_info(*idx);
        const auto chunks = archive.texture_chunks(*idx);
        std::cout << "  matched_texture[" << *idx << "]: "
                  << info->width << "x" << info->height
                  << " format=" << viceforge::dxgi_format_name(info->dxgi)
                  << " (dxgi=" << info->dxgi << ")"
                  << " tile_mode=" << static_cast<unsigned>(info->tile_mode)
                  << " chunks=" << chunks.size() << "\n";
    }
    return 0;
}

int cmd_inspect_package(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: vfcli inspect-package requires <game-dir>\n";
        return 2;
    }
    viceforge::GamePackageInfo info{};
    std::string err;
    if (!viceforge::parse_microsoft_game_config(std::filesystem::path(argv[2]), info, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }
    std::cout << "Game package: " << argv[2] << "\n"
              << "  display_name:         " << info.display_name << "\n"
              << "  publisher:            " << info.publisher_display_name << "\n"
              << "  title_id:             " << info.title_id << "\n"
              << "  store_id:             " << info.store_id << "\n"
              << "  identity:             " << info.identity_name << " (" << info.identity_version << ")\n"
              << "  executable_slot:      " << info.executable_name << "\n"
              << "  target_device_family: " << info.target_device_family << "\n"
              << "  executable_present:   " << (info.executable_present ? "true" : "false") << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }
    const std::string_view cmd = argv[1];
    if (cmd == "init") {
        return cmd_init(argc, argv);
    }
    if (cmd == "validate") {
        return cmd_validate(argc, argv);
    }
    if (cmd == "run-headless") {
        return cmd_run_headless(argc, argv);
    }
    if (cmd == "inspect-rpf") {
        return cmd_inspect_rpf(argc, argv);
    }
    if (cmd == "inspect-ttpf") {
        return cmd_inspect_ttpf(argc, argv);
    }
    if (cmd == "inspect-package") {
        return cmd_inspect_package(argc, argv);
    }
    if (cmd == "--help" || cmd == "-h") {
        print_usage();
        return 0;
    }
    std::cerr << "Unknown command: " << cmd << "\n";
    print_usage();
    return 2;
}
