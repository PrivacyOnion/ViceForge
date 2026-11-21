#pragma once

#include "viceforge/export.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace viceforge {

VICEFORGE_API uint32_t joaat_lower(std::string_view text);
VICEFORGE_API uint32_t joaat_exact(std::string_view text);

struct ResourceId {
    std::string namespace_name;
    std::string path;
    uint32_t hash = 0;

    static ResourceId parse(std::string_view qualified) {
        const size_t colon = qualified.find(':');
        if (colon == std::string_view::npos || colon == 0 || colon + 1 >= qualified.size()) {
            throw std::invalid_argument("ResourceId must have non-empty <namespace>:<path>");
        }
        ResourceId id;
        id.namespace_name = std::string(qualified.substr(0, colon));
        id.path = std::string(qualified.substr(colon + 1));
        id.hash = joaat_lower(id.to_string());
        return id;
    }

    static ResourceId make(std::string_view ns, std::string_view p) {
        if (ns.empty() || p.empty()) {
            throw std::invalid_argument("ResourceId namespace and path must be non-empty");
        }
        ResourceId id{std::string(ns), std::string(p), 0};
        id.hash = joaat_lower(id.to_string());
        return id;
    }

    std::string to_string() const { return namespace_name + ":" + path; }

    bool operator==(const ResourceId& other) const noexcept {
        return namespace_name == other.namespace_name && path == other.path;
    }
};

template <typename T>
class Registry {
public:
    explicit Registry(std::string registry_name) : name_(std::move(registry_name)) {}

    bool register_entry(const ResourceId& id, T value, std::string* error = nullptr) {
        if (frozen_) {
            if (error) {
                *error = "Registry '" + name_ + "' is frozen";
            }
            return false;
        }
        const std::string key = id.to_string();
        if (by_key_.find(key) != by_key_.end()) {
            if (error) {
                *error = "Duplicate key '" + key + "' in registry '" + name_ + "'";
            }
            return false;
        }
        if (by_hash_.find(id.hash) != by_hash_.end()) {
            if (error) {
                *error = "JOAAT hash collision for '" + key + "' in registry '" + name_ + "'";
            }
            return false;
        }
        const size_t index = entries_.size();
        entries_.push_back(Entry{id, std::move(value)});
        by_key_.emplace(key, index);
        by_hash_.emplace(id.hash, index);
        return true;
    }

    const T* find(std::string_view qualified_key) const {
        auto it = by_key_.find(std::string(qualified_key));
        if (it == by_key_.end()) {
            return nullptr;
        }
        return &entries_[it->second].value;
    }

    const T* find_by_hash(uint32_t hash) const {
        auto it = by_hash_.find(hash);
        if (it == by_hash_.end()) {
            return nullptr;
        }
        return &entries_[it->second].value;
    }

    struct Entry {
        ResourceId id;
        T value;
    };

    const std::vector<Entry>& entries() const noexcept { return entries_; }
    const std::string& name() const noexcept { return name_; }
    void freeze() noexcept { frozen_ = true; }
    bool is_frozen() const noexcept { return frozen_; }

private:
    std::string name_;
    bool frozen_ = false;
    std::vector<Entry> entries_;
    std::unordered_map<std::string, size_t> by_key_;
    std::unordered_map<uint32_t, size_t> by_hash_;
};

template <typename T>
class DeferredRegister {
public:
    DeferredRegister(Registry<T>& target, std::string mod_namespace)
        : target_(target), mod_namespace_(std::move(mod_namespace)) {}

    void add(std::string path, T value) {
        pending_.push_back(PendingEntry{ResourceId::make(mod_namespace_, path), std::move(value)});
    }

    bool register_all(std::string* error = nullptr) {
        for (auto& item : pending_) {
            if (!target_.register_entry(item.id, item.value, error)) {
                return false;
            }
        }
        return true;
    }

    size_t pending_count() const noexcept { return pending_.size(); }

private:
    struct PendingEntry {
        ResourceId id;
        T value;
    };

    Registry<T>& target_;
    std::string mod_namespace_;
    std::vector<PendingEntry> pending_;
};

} // namespace viceforge
