#pragma once

#include "viceforge/export.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace viceforge {

struct VfsOverrideRecord {
    std::string normalized_virtual_path;
    std::filesystem::path host_file_path;
    std::string owner_mod_id;
    uint32_t priority = 0;
    bool pinned = false;
};

struct VfsHashOverrideRecord {
    uint64_t hash64 = 0;
    std::filesystem::path host_file_path;
    std::string owner_mod_id;
    uint32_t priority = 0;
    bool pinned = false;
};

struct VfsConflict {
    std::string target_key;
    std::string replaced_mod_id;
    std::string winning_mod_id;
    std::filesystem::path replaced_path;
    std::filesystem::path winning_path;
};

class VfsMountTable {
public:
    VICEFORGE_API static std::string normalize_virtual_path(std::string_view raw_path);

    VICEFORGE_API bool register_override(std::string_view virtual_path,
                                         const std::filesystem::path& host_file_path,
                                         std::string_view owner_mod_id,
                                         uint32_t priority,
                                         bool pinned = false,
                                         std::string* error = nullptr);

    VICEFORGE_API bool register_hash_override(uint64_t hash64,
                                              const std::filesystem::path& host_file_path,
                                              std::string_view owner_mod_id,
                                              uint32_t priority,
                                              bool pinned = false,
                                              std::string* error = nullptr);

    VICEFORGE_API std::optional<VfsOverrideRecord> resolve(std::string_view virtual_path) const;
    VICEFORGE_API std::optional<VfsHashOverrideRecord> resolve_hash(uint64_t hash64) const;

    VICEFORGE_API std::vector<VfsOverrideRecord> list_path_overrides() const;
    VICEFORGE_API std::vector<VfsHashOverrideRecord> list_hash_overrides() const;
    VICEFORGE_API const std::vector<VfsConflict>& conflicts() const noexcept;
    VICEFORGE_API void clear();

private:
    std::unordered_map<std::string, VfsOverrideRecord> path_overrides_;
    std::unordered_map<uint64_t, VfsHashOverrideRecord> hash_overrides_;
    std::vector<VfsConflict> conflicts_;
};

} // namespace viceforge
