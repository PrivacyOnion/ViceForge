#include "test_assert.hpp"
#include "viceforge/loader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace viceforge;

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: test_mod_loader_e2e <sample_mod.dll> <sample_mod.json> <sample_addon.dll> <sample_addon.json>\n";
        return 2;
    }

    const auto staging = std::filesystem::temp_directory_path() / "viceforge_test_loader_e2e";
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);

    const auto mod_a = staging / "02_sample_mod";
    const auto mod_b = staging / "01_sample_addon";
    std::filesystem::create_directories(mod_a / "assets");
    std::filesystem::create_directories(mod_b / "assets");

    std::filesystem::copy_file(argv[1], mod_a / "sample_mod.dll");
    std::filesystem::copy_file(argv[2], mod_a / "viceforge.mod.json");
    std::filesystem::copy_file(argv[3], mod_b / "sample_addon.dll");
    std::filesystem::copy_file(argv[4], mod_b / "viceforge.mod.json");
    {
        std::ofstream(mod_a / "assets" / "sample.meta") << "base";
        std::ofstream(mod_b / "assets" / "addon.meta") << "addon";
    }

    ModLoader loader;
    std::vector<std::string> errors;
    const bool loaded = loader.load_all(staging, false, errors);
    if (!loaded) {
        for (const auto& e : errors) {
            std::cerr << "Load error: " << e << "\n";
        }
    }
    VF_CHECK(loaded);
    VF_CHECK(loader.loaded_mods().size() == 2);

    VF_CHECK(loader.loaded_mods()[0].manifest.id == "sample_mod");
    VF_CHECK(loader.loaded_mods()[1].manifest.id == "sample_addon");

    const auto static_override = loader.vfs().resolve("common:/data/viceforge_sample.meta");
    VF_CHECK(static_override.has_value());
    VF_CHECK(static_override->owner_mod_id == "sample_addon");

    const auto dynamic_override = loader.vfs().resolve("common:/data/viceforge_dynamic.meta");
    VF_CHECK(dynamic_override.has_value());
    VF_CHECK(dynamic_override->owner_mod_id == "sample_addon");

    const auto hash_override = loader.vfs().resolve_hash(0x1122334455667788ull);
    VF_CHECK(hash_override.has_value());
    VF_CHECK(hash_override->owner_mod_id == "sample_mod");

    VF_CHECK(loader.vfs().conflicts().size() == 2);

    VfEventPayload asset_ev{VF_EVENT_ASSET_MOUNT, 0, 0.0, 0, 0, "mount"};
    loader.dispatch_event(asset_ev);
    VfEventPayload pre_ev{VF_EVENT_PRE_TICK, 1, 0.016, 1, 0, "tick"};
    loader.dispatch_event(pre_ev);
    VfEventPayload post_ev{VF_EVENT_POST_TICK, 1, 0.016, 0, 0, "tick"};
    loader.dispatch_event(post_ev);

    bool saw_mod_asset = false;
    bool saw_addon_post = false;
    for (const auto& entry : loader.logs()) {
        if (entry.source == "sample_mod" && entry.message.find("VF_EVENT_ASSET_MOUNT") != std::string::npos) {
            saw_mod_asset = true;
        }
        if (entry.source == "sample_addon" && entry.message.find("VF_EVENT_POST_TICK") != std::string::npos) {
            saw_addon_post = true;
        }
    }
    VF_CHECK(saw_mod_asset);
    VF_CHECK(saw_addon_post);

    loader.shutdown();
    VF_CHECK(loader.loaded_mods().empty());

    std::filesystem::remove_all(staging, ec);
    std::cout << "test_mod_loader_e2e passed\n";
    return 0;
}
