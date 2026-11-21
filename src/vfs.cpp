#include "viceforge/vfs.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace viceforge {

namespace {

bool wins_over(uint32_t candidate_prio, bool candidate_pinned, uint32_t current_prio, bool current_pinned) {
    if (candidate_pinned != current_pinned) {
        return candidate_pinned;
    }
    return candidate_prio >= current_prio;
}

std::string format_hash64(uint64_t hash64) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << hash64;
    return ss.str();
}

} // namespace

std::string VfsMountTable::normalize_virtual_path(std::string_view raw_path) {
    while (!raw_path.empty() && std::isspace(static_cast<unsigned char>(raw_path.front()))) {
        raw_path.remove_prefix(1);
    }
    while (!raw_path.empty() && std::isspace(static_cast<unsigned char>(raw_path.back()))) {
        raw_path.remove_suffix(1);
    }

    std::string out;
    out.reserve(raw_path.size());
    bool prev_slash = false;
    for (char c : raw_path) {
        char ch = (c == '\\') ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ch == '/') {
            if (prev_slash) {
                continue;
            }
            prev_slash = true;
        } else {
            prev_slash = false;
        }
        out.push_back(ch);
    }
    while (!out.empty() && out.front() == '/') {
        out.erase(out.begin());
    }
    while (!out.empty() && out.back() == '/') {
        out.pop_back();
    }
    return out;
}

bool VfsMountTable::register_override(std::string_view virtual_path,
                                      const std::filesystem::path& host_file_path,
                                      std::string_view owner_mod_id,
                                      uint32_t priority,
                                      bool pinned,
                                      std::string* error) {
    const std::string key = normalize_virtual_path(virtual_path);
    if (key.empty() || host_file_path.empty() || owner_mod_id.empty()) {
        if (error) {
            *error = "VFS override requires non-empty virtual path, host file path, and mod id";
        }
        return false;
    }

    VfsOverrideRecord incoming{key, host_file_path, std::string(owner_mod_id), priority, pinned};
    auto it = path_overrides_.find(key);
    if (it == path_overrides_.end()) {
        path_overrides_.emplace(key, std::move(incoming));
        return true;
    }

    if (wins_over(incoming.priority, incoming.pinned, it->second.priority, it->second.pinned)) {
        conflicts_.push_back(VfsConflict{
            key,
            it->second.owner_mod_id,
            incoming.owner_mod_id,
            it->second.host_file_path,
            incoming.host_file_path
        });
        it->second = std::move(incoming);
    } else {
        conflicts_.push_back(VfsConflict{
            key,
            incoming.owner_mod_id,
            it->second.owner_mod_id,
            incoming.host_file_path,
            it->second.host_file_path
        });
    }
    return true;
}

bool VfsMountTable::register_hash_override(uint64_t hash64,
                                           const std::filesystem::path& host_file_path,
                                           std::string_view owner_mod_id,
                                           uint32_t priority,
                                           bool pinned,
                                           std::string* error) {
    if (hash64 == 0 || host_file_path.empty() || owner_mod_id.empty()) {
        if (error) {
            *error = "TTPF hash override requires non-zero hash, host file path, and mod id";
        }
        return false;
    }

    VfsHashOverrideRecord incoming{hash64, host_file_path, std::string(owner_mod_id), priority, pinned};
    auto it = hash_overrides_.find(hash64);
    if (it == hash_overrides_.end()) {
        hash_overrides_.emplace(hash64, std::move(incoming));
        return true;
    }

    if (wins_over(incoming.priority, incoming.pinned, it->second.priority, it->second.pinned)) {
        conflicts_.push_back(VfsConflict{
            format_hash64(hash64),
            it->second.owner_mod_id,
            incoming.owner_mod_id,
            it->second.host_file_path,
            incoming.host_file_path
        });
        it->second = std::move(incoming);
    } else {
        conflicts_.push_back(VfsConflict{
            format_hash64(hash64),
            incoming.owner_mod_id,
            it->second.owner_mod_id,
            incoming.host_file_path,
            it->second.host_file_path
        });
    }
    return true;
}

std::optional<VfsOverrideRecord> VfsMountTable::resolve(std::string_view virtual_path) const {
    const std::string key = normalize_virtual_path(virtual_path);
    auto it = path_overrides_.find(key);
    if (it == path_overrides_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<VfsHashOverrideRecord> VfsMountTable::resolve_hash(uint64_t hash64) const {
    auto it = hash_overrides_.find(hash64);
    if (it == hash_overrides_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<VfsOverrideRecord> VfsMountTable::list_path_overrides() const {
    std::vector<VfsOverrideRecord> list;
    list.reserve(path_overrides_.size());
    for (const auto& [k, v] : path_overrides_) {
        (void)k;
        list.push_back(v);
    }
    std::sort(list.begin(), list.end(), [](const VfsOverrideRecord& a, const VfsOverrideRecord& b) {
        return a.normalized_virtual_path < b.normalized_virtual_path;
    });
    return list;
}

std::vector<VfsHashOverrideRecord> VfsMountTable::list_hash_overrides() const {
    std::vector<VfsHashOverrideRecord> list;
    list.reserve(hash_overrides_.size());
    for (const auto& [k, v] : hash_overrides_) {
        (void)k;
        list.push_back(v);
    }
    std::sort(list.begin(), list.end(), [](const VfsHashOverrideRecord& a, const VfsHashOverrideRecord& b) {
        return a.hash64 < b.hash64;
    });
    return list;
}

const std::vector<VfsConflict>& VfsMountTable::conflicts() const noexcept {
    return conflicts_;
}

void VfsMountTable::clear() {
    path_overrides_.clear();
    hash_overrides_.clear();
    conflicts_.clear();
}

} // namespace viceforge
