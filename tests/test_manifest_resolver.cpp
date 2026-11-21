#include "test_assert.hpp"
#include "viceforge/manifest.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace viceforge;

namespace {

ModManifest parse_ok(std::string_view json, const char* dir) {
    ModManifest out;
    std::string err;
    const bool ok = parse_manifest_json(json, std::filesystem::path(dir), out, err);
    if (!ok) {
        std::cerr << "Unexpected parse failure: " << err << "\n";
    }
    VF_CHECK(ok);
    return out;
}

} // namespace

int main() {
    // 1. SemVer and VersionConstraint tests
    SemVer v100{}, v125{}, v200{}, v023{}, v030{};
    VF_CHECK(SemVer::parse("1.0.0", v100));
    VF_CHECK(SemVer::parse("1.2.5", v125));
    VF_CHECK(SemVer::parse("2.0.0", v200));
    VF_CHECK(SemVer::parse("0.2.3", v023));
    VF_CHECK(SemVer::parse("0.3.0", v030));
    VF_CHECK(v100 < v125 && v125 < v200);

    SemVer bad_ver{};
    VF_CHECK(!SemVer::parse("1.2", bad_ver));
    VF_CHECK(!SemVer::parse("1.2.x", bad_ver));

    VersionConstraint c_ge{}, c_caret{}, c_zero_caret{}, c_exact{};
    VF_CHECK(VersionConstraint::parse(">=1.2.0", c_ge));
    VF_CHECK(c_ge.matches(v125) && c_ge.matches(v200) && !c_ge.matches(v100));

    VF_CHECK(VersionConstraint::parse("^1.2.0", c_caret));
    VF_CHECK(c_caret.matches(v125) && !c_caret.matches(v200) && !c_caret.matches(v100));

    VF_CHECK(VersionConstraint::parse("^0.2.0", c_zero_caret));
    VF_CHECK(c_zero_caret.matches(v023) && !c_zero_caret.matches(v030));

    VF_CHECK(VersionConstraint::parse("1.2.5", c_exact));
    VF_CHECK(c_exact.matches(v125) && !c_exact.matches(v100));

    // 2. Valid DAG with dependencies, load_before, and load_after
    const auto core = parse_ok(R"({
        "id": "core_lib",
        "name": "Core Lib",
        "version": "1.2.0"
    })", "mods/core_lib");
    const auto ext = parse_ok(R"({
        "id": "ext_pack",
        "version": "0.5.0",
        "dependencies": [{"id": "core_lib", "version": "^1.0.0"}],
        "load_before": ["ui_tweaks"]
    })", "mods/ext_pack");
    const auto ui = parse_ok(R"({
        "id": "ui_tweaks",
        "version": "2.0.0",
        "load_after": ["core_lib"]
    })", "mods/ui_tweaks");

    const auto dag_report = resolve_load_order({ui, ext, core});
    VF_CHECK(dag_report.ok);
    VF_CHECK(dag_report.ordered_mods.size() == 3);
    VF_CHECK(dag_report.ordered_mods[0].id == "core_lib");
    VF_CHECK(dag_report.ordered_mods[1].id == "ext_pack");
    VF_CHECK(dag_report.ordered_mods[2].id == "ui_tweaks");

    // 3. Missing required dependency rejected
    const auto missing_report = resolve_load_order({ext});
    VF_CHECK(!missing_report.ok);
    VF_CHECK(!missing_report.errors.empty());

    // 4. Incompatible dependency version rejected
    const auto old_core = parse_ok(R"({"id": "core_lib", "version": "0.9.0"})", "mods/core_lib");
    const auto ver_fail = resolve_load_order({old_core, ext});
    VF_CHECK(!ver_fail.ok);

    // 5. Explicit conflict rejected
    const auto conflict_mod = parse_ok(R"({
        "id": "conflicting",
        "version": "1.0.0",
        "conflicts": ["core_lib"]
    })", "mods/conflicting");
    const auto conflict_report = resolve_load_order({core, conflict_mod});
    VF_CHECK(!conflict_report.ok);

    // 6. Cycle detection
    const auto cyc_a = parse_ok(R"({"id": "mod_a", "version": "1.0.0", "dependencies": ["mod_b"]})", "mods/a");
    const auto cyc_b = parse_ok(R"({"id": "mod_b", "version": "1.0.0", "dependencies": ["mod_a"]})", "mods/b");
    const auto cyc_report = resolve_load_order({cyc_a, cyc_b});
    VF_CHECK(!cyc_report.ok);

    // 7. Reject path escape in manifest
    ModManifest bad_manifest{};
    std::string err;
    VF_CHECK(!parse_manifest_json(R"({"id": "bad", "version": "1.0.0", "entry_dll": "../evil.dll"})",
                                "mods/bad", bad_manifest, err));

    std::cout << "test_manifest_resolver passed\n";
    return 0;
}
