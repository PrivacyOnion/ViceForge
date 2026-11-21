#include "viceforge/manifest.hpp"
#include "json_mini.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace viceforge {

namespace {

bool parse_u32_part(std::string_view part, uint32_t& out) {
    if (part.empty()) {
        return false;
    }
    for (char c : part) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    const auto* first = part.data();
    const auto* last = first + part.size();
    uint32_t value = 0;
    const auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

bool is_valid_mod_id(std::string_view id) {
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

bool read_text_file(const std::filesystem::path& path, std::string& content, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Cannot open file: " + path.string();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    content = ss.str();
    if (!in && !in.eof()) {
        error = "Failed reading file: " + path.string();
        return false;
    }
    return true;
}

bool read_string_array(const json::Object& obj,
                       const std::string& key,
                       std::vector<std::string>& out,
                       std::string& error) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return true;
    }
    if (!it->second.is_array()) {
        error = "'" + key + "' must be an array of strings";
        return false;
    }
    for (const auto& item : it->second.as_array()) {
        if (!item.is_string() || item.as_string().empty()) {
            error = "Every element in '" + key + "' must be a non-empty string";
            return false;
        }
        out.push_back(item.as_string());
    }
    return true;
}

} // namespace

bool SemVer::parse(std::string_view text, SemVer& out) {
    text = trim(text);
    const size_t dot1 = text.find('.');
    if (dot1 == std::string_view::npos) {
        return false;
    }
    const size_t dot2 = text.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos || text.find('.', dot2 + 1) != std::string_view::npos) {
        return false;
    }
    SemVer parsed{};
    if (!parse_u32_part(text.substr(0, dot1), parsed.major) ||
        !parse_u32_part(text.substr(dot1 + 1, dot2 - dot1 - 1), parsed.minor) ||
        !parse_u32_part(text.substr(dot2 + 1), parsed.patch)) {
        return false;
    }
    out = parsed;
    return true;
}

std::string SemVer::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool VersionConstraint::parse(std::string_view text, VersionConstraint& out) {
    text = trim(text);
    if (text.empty() || text == "*") {
        out.kind_ = Kind::Any;
        out.baseline_ = SemVer{};
        out.raw_ = "*";
        return true;
    }
    Kind k = Kind::Exact;
    std::string_view ver_part = text;
    if (text.starts_with(">=")) {
        k = Kind::GreaterEqual;
        ver_part = trim(text.substr(2));
    } else if (text.starts_with('^')) {
        k = Kind::Caret;
        ver_part = trim(text.substr(1));
    } else if (text.starts_with('=')) {
        k = Kind::Exact;
        ver_part = trim(text.substr(1));
    }
    SemVer v{};
    if (!SemVer::parse(ver_part, v)) {
        return false;
    }
    out.kind_ = k;
    out.baseline_ = v;
    out.raw_ = std::string(text);
    return true;
}

bool VersionConstraint::matches(const SemVer& version) const noexcept {
    switch (kind_) {
        case Kind::Any:
            return true;
        case Kind::Exact:
            return version == baseline_;
        case Kind::GreaterEqual:
            return version >= baseline_;
        case Kind::Caret: {
            if (version < baseline_) {
                return false;
            }
            if (baseline_.major > 0) {
                return version < SemVer{baseline_.major + 1, 0, 0};
            }
            if (baseline_.minor > 0) {
                return version < SemVer{0, baseline_.minor + 1, 0};
            }
            return version < SemVer{0, 0, baseline_.patch + 1};
        }
    }
    return false;
}

std::string VersionConstraint::to_string() const {
    return raw_;
}

bool parse_manifest_json(std::string_view json_text,
                         const std::filesystem::path& mod_directory,
                         ModManifest& out,
                         std::string& error) {
    json::Value root;
    json::Parser parser(json_text);
    if (!parser.parse(root, error)) {
        return false;
    }
    if (!root.is_object()) {
        error = "Manifest root must be a JSON object";
        return false;
    }
    const auto& obj = root.as_object();

    auto id_it = obj.find("id");
    if (id_it == obj.end() || !id_it->second.is_string() || !is_valid_mod_id(id_it->second.as_string())) {
        error = "Manifest requires lowercase 'id' matching [a-z0-9_-]+";
        return false;
    }

    auto ver_it = obj.find("version");
    if (ver_it == obj.end() || !ver_it->second.is_string()) {
        error = "Manifest requires string 'version'";
        return false;
    }

    ModManifest m{};
    m.id = id_it->second.as_string();
    m.directory = mod_directory;

    if (!SemVer::parse(ver_it->second.as_string(), m.version)) {
        error = "Manifest 'version' must follow MAJOR.MINOR.PATCH SemVer";
        return false;
    }

    auto name_it = obj.find("name");
    if (name_it != obj.end()) {
        if (!name_it->second.is_string() || name_it->second.as_string().empty()) {
            error = "Manifest 'name' must be a non-empty string";
            return false;
        }
        m.name = name_it->second.as_string();
    } else {
        m.name = m.id;
    }

    auto dll_it = obj.find("entry_dll");
    if (dll_it != obj.end() && !dll_it->second.is_null()) {
        if (!dll_it->second.is_string() || dll_it->second.as_string().empty()) {
            error = "Manifest 'entry_dll' must be a non-empty relative path";
            return false;
        }
        const std::filesystem::path rel(dll_it->second.as_string());
        if (rel.is_absolute()) {
            error = "Manifest 'entry_dll' must be a relative path";
            return false;
        }
        for (const auto& part : rel) {
            if (part == "..") {
                error = "Manifest 'entry_dll' cannot escape mod directory";
                return false;
            }
        }
        m.entry_dll = dll_it->second.as_string();
    }

    auto deps_it = obj.find("dependencies");
    if (deps_it != obj.end()) {
        if (!deps_it->second.is_array()) {
            error = "Manifest 'dependencies' must be an array";
            return false;
        }
        for (const auto& item : deps_it->second.as_array()) {
            ModDependency dep{};
            if (item.is_string()) {
                dep.id = item.as_string();
                VersionConstraint::parse("*", dep.constraint);
            } else if (item.is_object()) {
                const auto& dobj = item.as_object();
                auto did = dobj.find("id");
                if (did == dobj.end() || !did->second.is_string() || !is_valid_mod_id(did->second.as_string())) {
                    error = "Dependency object requires valid 'id'";
                    return false;
                }
                dep.id = did->second.as_string();
                auto dver = dobj.find("version");
                if (dver != dobj.end()) {
                    if (!dver->second.is_string() || !VersionConstraint::parse(dver->second.as_string(), dep.constraint)) {
                        error = "Invalid dependency version constraint for '" + dep.id + "'";
                        return false;
                    }
                } else {
                    VersionConstraint::parse("*", dep.constraint);
                }
                auto dopt = dobj.find("optional");
                if (dopt != dobj.end()) {
                    if (!dopt->second.is_bool()) {
                        error = "Dependency 'optional' must be boolean";
                        return false;
                    }
                    dep.optional = dopt->second.as_bool();
                }
            } else {
                error = "Dependency entry must be a string or object";
                return false;
            }
            if (dep.id == m.id) {
                error = "Mod '" + m.id + "' cannot depend on itself";
                return false;
            }
            m.dependencies.push_back(std::move(dep));
        }
    }

    if (!read_string_array(obj, "conflicts", m.conflicts, error) ||
        !read_string_array(obj, "load_before", m.load_before, error) ||
        !read_string_array(obj, "load_after", m.load_after, error)) {
        return false;
    }

    auto mounts_it = obj.find("asset_mounts");
    if (mounts_it != obj.end()) {
        if (!mounts_it->second.is_array()) {
            error = "Manifest 'asset_mounts' must be an array";
            return false;
        }
        for (const auto& item : mounts_it->second.as_array()) {
            if (!item.is_object()) {
                error = "Each 'asset_mounts' entry must be an object";
                return false;
            }
            const auto& mobj = item.as_object();
            auto vp = mobj.find("virtual_path");
            auto src = mobj.find("source");
            if (vp == mobj.end() || !vp->second.is_string() || vp->second.as_string().empty() ||
                src == mobj.end() || !src->second.is_string() || src->second.as_string().empty()) {
                error = "Asset mount requires non-empty 'virtual_path' and 'source'";
                return false;
            }
            const std::filesystem::path rel(src->second.as_string());
            if (rel.is_absolute()) {
                error = "Asset mount 'source' must be a relative path";
                return false;
            }
            for (const auto& part : rel) {
                if (part == "..") {
                    error = "Asset mount 'source' cannot escape mod directory";
                    return false;
                }
            }
            bool pinned = false;
            auto pin_it = mobj.find("pinned");
            if (pin_it != mobj.end()) {
                if (!pin_it->second.is_bool()) {
                    error = "Asset mount 'pinned' must be boolean";
                    return false;
                }
                pinned = pin_it->second.as_bool();
            }
            m.asset_mounts.push_back(AssetMountSpec{vp->second.as_string(), src->second.as_string(), pinned});
        }
    }

    out = std::move(m);
    return true;
}

bool discover_manifests(const std::filesystem::path& mods_root,
                        std::vector<ModManifest>& out_manifests,
                        std::vector<std::string>& out_errors) {
    out_manifests.clear();
    out_errors.clear();
    std::error_code ec;
    if (!std::filesystem::exists(mods_root, ec) || !std::filesystem::is_directory(mods_root, ec)) {
        out_errors.push_back("Mods directory does not exist: " + mods_root.string());
        return false;
    }

    std::vector<std::filesystem::path> dirs;
    for (const auto& entry : std::filesystem::directory_iterator(mods_root, ec)) {
        if (entry.is_directory(ec)) {
            dirs.push_back(entry.path());
        }
    }
    if (ec) {
        out_errors.push_back("Failed iterating mods directory: " + ec.message());
        return false;
    }
    std::sort(dirs.begin(), dirs.end());

    for (const auto& dir : dirs) {
        const auto manifest_path = dir / "viceforge.mod.json";
        if (!std::filesystem::exists(manifest_path, ec)) {
            continue;
        }
        std::string text;
        std::string err;
        if (!read_text_file(manifest_path, text, err)) {
            out_errors.push_back(err);
            continue;
        }
        ModManifest parsed;
        if (!parse_manifest_json(text, dir, parsed, err)) {
            out_errors.push_back(manifest_path.string() + ": " + err);
            continue;
        }
        out_manifests.push_back(std::move(parsed));
    }

    return out_errors.empty();
}

ResolutionReport resolve_load_order(const std::vector<ModManifest>& manifests) {
    ResolutionReport report{};
    std::unordered_map<std::string, size_t> index_by_id;

    for (size_t i = 0; i < manifests.size(); ++i) {
        const auto& m = manifests[i];
        if (!index_by_id.emplace(m.id, i).second) {
            report.errors.push_back("Duplicate mod id: '" + m.id + "'");
        }
    }

    for (size_t i = 0; i < manifests.size(); ++i) {
        const auto& m = manifests[i];
        for (const auto& conflict_id : m.conflicts) {
            if (conflict_id == m.id) {
                report.errors.push_back("Mod '" + m.id + "' lists itself in conflicts");
            } else if (index_by_id.contains(conflict_id)) {
                report.errors.push_back("Mod conflict: '" + m.id + "' conflicts with '" + conflict_id + "'");
            }
        }
        for (const auto& dep : m.dependencies) {
            auto dep_it = index_by_id.find(dep.id);
            if (dep_it == index_by_id.end()) {
                if (!dep.optional) {
                    report.errors.push_back("Missing required dependency '" + dep.id + "' (" +
                                            dep.constraint.to_string() + ") for mod '" + m.id + "'");
                } else {
                    report.warnings.push_back("Optional dependency '" + dep.id + "' not present for '" + m.id + "'");
                }
                continue;
            }
            const auto& target = manifests[dep_it->second];
            if (!dep.constraint.matches(target.version)) {
                report.errors.push_back("Incompatible dependency version for '" + m.id + "': requires '" +
                                        dep.id + "' " + dep.constraint.to_string() + ", found " +
                                        target.version.to_string());
            }
        }
    }

    if (!report.errors.empty()) {
        report.ok = false;
        return report;
    }

    const size_t n = manifests.size();
    std::vector<std::set<size_t>> adj(n);
    std::vector<size_t> indegree(n, 0);

    auto add_edge = [&](size_t before_idx, size_t after_idx) {
        if (before_idx == after_idx) {
            report.errors.push_back("Self-ordering edge on mod '" + manifests[before_idx].id + "'");
            return;
        }
        if (adj[before_idx].insert(after_idx).second) {
            ++indegree[after_idx];
        }
    };

    for (size_t i = 0; i < n; ++i) {
        const auto& m = manifests[i];
        for (const auto& dep : m.dependencies) {
            auto it = index_by_id.find(dep.id);
            if (it != index_by_id.end()) {
                add_edge(it->second, i);
            }
        }
        for (const auto& after_target : m.load_before) {
            auto it = index_by_id.find(after_target);
            if (it != index_by_id.end()) {
                add_edge(i, it->second);
            }
        }
        for (const auto& before_target : m.load_after) {
            auto it = index_by_id.find(before_target);
            if (it != index_by_id.end()) {
                add_edge(it->second, i);
            }
        }
    }

    if (!report.errors.empty()) {
        report.ok = false;
        return report;
    }

    struct ReadyKey {
        std::string id;
        size_t idx = 0;
        bool operator>(const ReadyKey& other) const noexcept { return id > other.id; }
    };

    std::priority_queue<ReadyKey, std::vector<ReadyKey>, std::greater<ReadyKey>> ready;
    for (size_t i = 0; i < n; ++i) {
        if (indegree[i] == 0) {
            ready.push(ReadyKey{manifests[i].id, i});
        }
    }

    std::vector<bool> visited(n, false);
    while (!ready.empty()) {
        const auto top = ready.top();
        ready.pop();
        visited[top.idx] = true;
        report.ordered_mods.push_back(manifests[top.idx]);
        for (size_t next : adj[top.idx]) {
            if (--indegree[next] == 0) {
                ready.push(ReadyKey{manifests[next].id, next});
            }
        }
    }

    if (report.ordered_mods.size() != n) {
        std::string cycle_nodes;
        for (size_t i = 0; i < n; ++i) {
            if (!visited[i]) {
                if (!cycle_nodes.empty()) {
                    cycle_nodes += ", ";
                }
                cycle_nodes += manifests[i].id;
            }
        }
        report.errors.push_back("Dependency or load-order cycle involving: " + cycle_nodes);
        report.ordered_mods.clear();
        report.ok = false;
        return report;
    }

    report.ok = true;
    return report;
}

} // namespace viceforge
