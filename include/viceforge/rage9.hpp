#pragma once

#include "viceforge/export.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace viceforge {

inline constexpr uint32_t kRpf9Magic = 0x52504639u; // Little-endian '9FPR' ("RPF9" bytes reversed or ASCII '9','F','P','R')
inline constexpr uint32_t kRds5Magic = 0x35534452u; // "RDS5"
inline constexpr uint32_t kEtpfMagic = 0x46505445u; // "ETPF"

struct Rpf9Header {
    uint32_t magic = 0;
    uint32_t header_word_1 = 0;
    uint32_t header_word_2 = 0;
    uint32_t header_word_3 = 0;
    uint32_t signature_lo = 0;
    uint32_t signature_hi = 0;
    uint64_t file_size = 0;
};

VICEFORGE_API bool parse_rpf9_header(std::span<const uint8_t> bytes, Rpf9Header& out, std::string& error);
VICEFORGE_API bool parse_rpf9_header(const std::filesystem::path& file_path, Rpf9Header& out, std::string& error);

enum class TtpfDxgiFormat : uint16_t {
    Unknown = 0,
    R16G16B16A16_FLOAT = 10,
    R8G8B8A8_UNORM = 28,
    R8G8B8A8_SRGB = 29,
    BC1_UNORM = 71,
    BC1_SRGB = 72,
    BC2_UNORM = 74,
    BC2_SRGB = 75,
    BC3_UNORM = 77,
    BC3_SRGB = 78,
    BC4_UNORM = 80,
    BC5_UNORM = 83,
    B8G8R8A8_UNORM = 87,
    B8G8R8A8_SRGB = 91,
    BC6H_UF16 = 95,
    BC6H_SF16 = 96,
    BC7_UNORM = 98,
    BC7_SRGB = 99
};

VICEFORGE_API std::string_view dxgi_format_name(uint16_t dxgi) noexcept;
VICEFORGE_API uint32_t bc_bytes_per_block(uint16_t dxgi) noexcept;
VICEFORGE_API uint32_t unswizzle_bc_block_index(uint32_t x,
                                                uint32_t y,
                                                uint32_t width_blocks,
                                                uint32_t height_blocks,
                                                uint32_t bytes_per_block,
                                                uint8_t tile_mode) noexcept;

struct TtpfChunkInfo {
    uint32_t index = 0;
    uint32_t vpage = 0;
    uint32_t virtual_size = 0;
    uint64_t disk_offset = 0;
    uint8_t codec = 0;
};

struct TtpfTextureInfo {
    uint32_t index = 0;
    uint64_t hash64 = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t dxgi = 0;
    uint8_t tile_mode = 0;
    uint32_t vpage = 0;
};

class TtpfArchive {
public:
    VICEFORGE_API bool open(const std::filesystem::path& path, std::string& error);
    VICEFORGE_API std::optional<uint32_t> find_by_hash(uint64_t hash64) const noexcept;
    VICEFORGE_API std::optional<TtpfTextureInfo> texture_info(uint32_t index) const;
    VICEFORGE_API std::vector<TtpfChunkInfo> texture_chunks(uint32_t index) const;
    VICEFORGE_API bool read_chunk_raw(const TtpfChunkInfo& chunk,
                                      std::vector<uint8_t>& out,
                                      std::string& error) const;

    VICEFORGE_API uint32_t texture_count() const noexcept;
    VICEFORGE_API uint32_t chunk_count() const noexcept;
    VICEFORGE_API uint32_t active_chunk_entries() const noexcept;
    VICEFORGE_API const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
    uint64_t file_size_ = 0;
    uint32_t total_chunk_slots_ = 0;
    uint32_t active_entries_ = 0;
    uint32_t etpf_block_ = 0;
    std::vector<uint32_t> vpages_;
    std::vector<uint64_t> disk_offsets_;
    std::vector<uint8_t> codecs_;
    std::unordered_map<uint32_t, uint32_t> vpage_to_chunk_;

    struct RawDescriptor {
        uint32_t d0 = 0;
        uint32_t d1 = 0;
        uint32_t d2 = 0;
        uint32_t d3 = 0;
    };
    std::vector<RawDescriptor> descriptors_;
    std::vector<uint64_t> hashes_;
    std::unordered_map<uint64_t, uint32_t> hash_to_index_;
};

struct GamePackageInfo {
    std::string title_id;
    std::string store_id;
    std::string identity_name;
    std::string identity_version;
    std::string display_name;
    std::string publisher_display_name;
    std::string executable_name;
    std::string target_device_family;
    bool executable_present = false;
};

VICEFORGE_API bool parse_microsoft_game_config(const std::filesystem::path& game_root,
                                               GamePackageInfo& out,
                                               std::string& error);

} // namespace viceforge
