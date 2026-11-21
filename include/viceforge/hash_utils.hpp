#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace viceforge {

struct TransparentStringHash {
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }

    size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }

    size_t operator()(const char* s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

constexpr uint32_t joaat_lower_ct(std::string_view text) noexcept {
    uint32_t h = 0;
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        }
        h += c;
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    return h;
}

constexpr uint32_t joaat_exact_ct(std::string_view text) noexcept {
    uint32_t h = 0;
    for (unsigned char c : text) {
        h += c;
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    return h;
}

namespace literals {

constexpr uint32_t operator""_joaat(const char* str, size_t len) noexcept {
    return joaat_lower_ct(std::string_view(str, len));
}

} // namespace literals

} // namespace viceforge
