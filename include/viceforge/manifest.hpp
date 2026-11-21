#pragma once

#include "viceforge/export.h"

#include <compare>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace viceforge {

struct SemVer {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    auto operator<=>(const SemVer&) const = default;

    VICEFORGE_API static bool parse(std::string_view text, SemVer& out);
    VICEFORGE_API std::string to_string() const;
};

class VersionConstraint {
public:
    enum class Kind { Any, Exact, GreaterEqual, Caret };

    VICEFORGE_API static bool parse(std::string_view text, VersionConstraint& out);
    VICEFORGE_API bool matches(const SemVer& version) const noexcept;
    VICEFORGE_API std::string to_string() const;

    Kind kind() const noexcept { return kind_; }
    const SemVer& baseline() const noexcept { return baseline_; }

private:
    Kind kind_ = Kind::Any;
    SemVer baseline_{};
    std::string raw_ = "*";
};

struct ModDependency {
    std::string id;
    VersionConstraint constraint;
    bool optional = false;
};

struct AssetMountSpec {
    std::string virtual_path;
    std::string source_relative_path;
    bool pinned = false;
};

struct ModManifest {
    std::string id;
    std::string name;
    SemVer version{};
    std::string entry_dll;
    std::filesystem::path directory;
    std::vector<ModDependency> dependencies;
    std::vector<std::string> conflicts;
    std::vector<std::string> load_before;
    std::vector<std::string> load_after;
    std::vector<AssetMountSpec> asset_mounts;
};

struct ResolutionReport {
    bool ok = false;
    std::vector<ModManifest> ordered_mods;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

VICEFORGE_API bool parse_manifest_json(std::string_view json_text,
                                       const std::filesystem::path& mod_directory,
                                       ModManifest& out,
                                       std::string& error);

VICEFORGE_API bool discover_manifests(const std::filesystem::path& mods_root,
                                      std::vector<ModManifest>& out_manifests,
                                      std::vector<std::string>& out_errors);

VICEFORGE_API ResolutionReport resolve_load_order(const std::vector<ModManifest>& manifests);

} // namespace viceforge
