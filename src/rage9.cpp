#include "viceforge/rage9.hpp"
#include "viceforge/registry.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace viceforge {

namespace {

uint32_t read_u32_le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::string extract_xml_attr(std::string_view xml, std::string_view tag, std::string_view attr) {
    const std::string needle = "<" + std::string(tag);
    size_t search_from = 0;
    while (true) {
        const size_t pos = xml.find(needle, search_from);
        if (pos == std::string_view::npos) {
            return {};
        }
        const size_t after = pos + needle.size();
        if (after >= xml.size()) {
            return {};
        }
        const char next_ch = xml[after];
        if (!std::isspace(static_cast<unsigned char>(next_ch)) && next_ch != '>' && next_ch != '/') {
            search_from = after;
            continue;
        }
        const size_t end = xml.find('>', pos);
        if (end == std::string_view::npos) {
            return {};
        }
        const std::string_view tag_slice = xml.substr(pos, end - pos);
        const std::string attr_key = std::string(attr) + "=\"";
        const size_t a_pos = tag_slice.find(attr_key);
        if (a_pos != std::string_view::npos) {
            const size_t val_start = a_pos + attr_key.size();
            const size_t val_end = tag_slice.find('"', val_start);
            if (val_end != std::string_view::npos) {
                return std::string(tag_slice.substr(val_start, val_end - val_start));
            }
        }
        search_from = end + 1;
    }
}

std::string extract_xml_elem_text(std::string_view xml, std::string_view tag) {
    const std::string open = "<" + std::string(tag) + ">";
    const std::string close = "</" + std::string(tag) + ">";
    const size_t s = xml.find(open);
    if (s == std::string_view::npos) {
        return {};
    }
    const size_t start = s + open.size();
    const size_t e = xml.find(close, start);
    if (e == std::string_view::npos) {
        return {};
    }
    return std::string(xml.substr(start, e - start));
}

} // namespace

uint32_t joaat_lower(std::string_view text) {
    return joaat_lower_ct(text);
}

uint32_t joaat_exact(std::string_view text) {
    return joaat_exact_ct(text);
}

bool parse_rpf9_header(std::span<const uint8_t> bytes, Rpf9Header& out, std::string& error) {
    if (bytes.size() < 24) {
        error = "Truncated RPF9 header (expected at least 24 bytes)";
        return false;
    }
    const uint32_t magic = read_u32_le(bytes.data());
    if (magic != kRpf9Magic) {
        error = "Invalid RPF9 magic (expected ASCII '9FPR')";
        return false;
    }
    out.magic = magic;
    out.header_word_1 = read_u32_le(bytes.data() + 4);
    out.header_word_2 = read_u32_le(bytes.data() + 8);
    out.header_word_3 = read_u32_le(bytes.data() + 12);
    out.signature_lo = read_u32_le(bytes.data() + 16);
    out.signature_hi = read_u32_le(bytes.data() + 20);
    out.file_size = bytes.size();
    return true;
}

bool parse_rpf9_header(const std::filesystem::path& file_path, Rpf9Header& out, std::string& error) {
    std::error_code ec;
    const uint64_t fsize = std::filesystem::file_size(file_path, ec);
    if (ec) {
        error = "Cannot stat RPF9 file: " + ec.message();
        return false;
    }
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        error = "Cannot open RPF9 file: " + file_path.string();
        return false;
    }
    uint8_t buf[24]{};
    in.read(reinterpret_cast<char*>(buf), sizeof(buf));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(buf))) {
        error = "Truncated RPF9 header";
        return false;
    }
    if (!parse_rpf9_header(buf, out, error)) {
        return false;
    }
    out.file_size = fsize;
    return true;
}

std::string_view dxgi_format_name(uint16_t dxgi) noexcept {
    switch (static_cast<TtpfDxgiFormat>(dxgi)) {
        case TtpfDxgiFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
        case TtpfDxgiFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case TtpfDxgiFormat::R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
        case TtpfDxgiFormat::BC1_UNORM: return "BC1_UNORM";
        case TtpfDxgiFormat::BC1_SRGB: return "BC1_SRGB";
        case TtpfDxgiFormat::BC2_UNORM: return "BC2_UNORM";
        case TtpfDxgiFormat::BC2_SRGB: return "BC2_SRGB";
        case TtpfDxgiFormat::BC3_UNORM: return "BC3_UNORM";
        case TtpfDxgiFormat::BC3_SRGB: return "BC3_SRGB";
        case TtpfDxgiFormat::BC4_UNORM: return "BC4_UNORM";
        case TtpfDxgiFormat::BC5_UNORM: return "BC5_UNORM";
        case TtpfDxgiFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case TtpfDxgiFormat::B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
        case TtpfDxgiFormat::BC6H_UF16: return "BC6H_UF16";
        case TtpfDxgiFormat::BC6H_SF16: return "BC6H_SF16";
        case TtpfDxgiFormat::BC7_UNORM: return "BC7_UNORM";
        case TtpfDxgiFormat::BC7_SRGB: return "BC7_SRGB";
        default: return "UNKNOWN";
    }
}

uint32_t bc_bytes_per_block(uint16_t dxgi) noexcept {
    switch (static_cast<TtpfDxgiFormat>(dxgi)) {
        case TtpfDxgiFormat::BC1_UNORM:
        case TtpfDxgiFormat::BC1_SRGB:
        case TtpfDxgiFormat::BC4_UNORM:
            return 8;
        case TtpfDxgiFormat::BC2_UNORM:
        case TtpfDxgiFormat::BC2_SRGB:
        case TtpfDxgiFormat::BC3_UNORM:
        case TtpfDxgiFormat::BC3_SRGB:
        case TtpfDxgiFormat::BC5_UNORM:
        case TtpfDxgiFormat::BC6H_UF16:
        case TtpfDxgiFormat::BC6H_SF16:
        case TtpfDxgiFormat::BC7_UNORM:
        case TtpfDxgiFormat::BC7_SRGB:
            return 16;
        default:
            return 0;
    }
}

uint32_t unswizzle_bc_block_index(uint32_t x,
                                  uint32_t y,
                                  uint32_t width_blocks,
                                  uint32_t height_blocks,
                                  uint32_t bytes_per_block,
                                  uint8_t tile_mode) noexcept {
    if (bytes_per_block == 16) {
        if (tile_mode == 0 && width_blocks >= 64 && height_blocks >= 64 &&
            (width_blocks % 64 == 0) && (height_blocks % 64 == 0)) {
            const uint32_t tx = x >> 6;
            const uint32_t ty = y >> 6;
            const uint32_t lx = x & 63u;
            const uint32_t ly = y & 63u;
            const uint32_t in_t =
                (lx & 1u) | ((ly & 1u) << 1) |
                (((lx >> 1) & 1u) << 2) | (((ly >> 1) & 1u) << 3) |
                (((ly >> 2) & 1u) << 4) | (((lx >> 2) & 1u) << 5) |
                (((ly >> 3) & 1u) << 6) | (((lx >> 3) & 1u) << 7) |
                (((ly >> 4) & 1u) << 8) | (((lx >> 4) & 1u) << 9) |
                (((ly >> 5) & 1u) << 10) | (((lx >> 5) & 1u) << 11);
            return (ty * (width_blocks >> 6) + tx) * 4096u + in_t;
        }
        if (width_blocks >= 16 && height_blocks >= 16 &&
            (width_blocks % 16 == 0) && (height_blocks % 16 == 0)) {
            const uint32_t tx = x >> 4;
            const uint32_t ty = y >> 4;
            const uint32_t lx = x & 15u;
            const uint32_t ly = y & 15u;
            const uint32_t in_t =
                (ly & 1u) | (((ly >> 1) & 1u) << 1) |
                ((lx & 1u) << 2) | (((lx >> 1) & 1u) << 3) |
                (((ly >> 2) & 1u) << 4) | (((lx >> 2) & 1u) << 5) |
                (((ly >> 3) & 1u) << 6) | (((lx >> 3) & 1u) << 7);
            return (ty * (width_blocks >> 4) + tx) * 256u + in_t;
        }
        return y * width_blocks + x;
    }

    if (bytes_per_block == 8) {
        if (tile_mode == 0 && width_blocks >= 128 && height_blocks >= 64 &&
            (width_blocks % 128 == 0) && (height_blocks % 64 == 0)) {
            const uint32_t tx = x >> 7;
            const uint32_t ty = y >> 6;
            const uint32_t lx = x & 127u;
            const uint32_t ly = y & 63u;
            const uint32_t in_t =
                (lx & 1u) | ((ly & 1u) << 1) |
                (((ly >> 1) & 1u) << 2) | (((lx >> 1) & 1u) << 3) |
                (((lx >> 2) & 1u) << 4) | (((ly >> 2) & 1u) << 5) |
                (((lx >> 3) & 1u) << 6) | (((ly >> 3) & 1u) << 7) |
                (((lx >> 4) & 1u) << 8) | (((ly >> 4) & 1u) << 9) |
                (((lx >> 5) & 1u) << 10) | (((ly >> 5) & 1u) << 11) |
                (((lx >> 6) & 1u) << 12);
            return (ty * (width_blocks >> 7) + tx) * 8192u + in_t;
        }
        if (width_blocks >= 16 && height_blocks >= 16 &&
            (width_blocks % 16 == 0) && (height_blocks % 16 == 0)) {
            const uint32_t tx = x >> 4;
            const uint32_t ty = y >> 4;
            const uint32_t lx = x & 15u;
            const uint32_t ly = y & 15u;
            const uint32_t in_t =
                (lx & 1u) | ((ly & 1u) << 1) |
                (((ly >> 1) & 1u) << 2) | (((lx >> 1) & 1u) << 3) |
                (((lx >> 2) & 1u) << 4) | (((ly >> 2) & 1u) << 5) |
                (((lx >> 3) & 1u) << 6) | (((ly >> 3) & 1u) << 7);
            return (ty * (width_blocks >> 4) + tx) * 256u + in_t;
        }
        return y * width_blocks + x;
    }

    return y * width_blocks + x;
}

bool TtpfArchive::open(const std::filesystem::path& path, std::string& error) {
    path_ = path;
    file_size_ = 0;
    total_chunk_slots_ = 0;
    active_entries_ = 0;
    etpf_block_ = 0;
    vpages_.clear();
    disk_offsets_.clear();
    codecs_.clear();
    vpage_to_chunk_.clear();
    descriptors_.clear();
    hashes_.clear();
    hash_to_index_.clear();

    std::error_code ec;
    const uint64_t fsize = std::filesystem::file_size(path, ec);
    if (ec || fsize < 0x18u) {
        error = "Cannot read TTPF file or file smaller than 24-byte RDS5 header";
        return false;
    }
    file_size_ = fsize;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Cannot open TTPF file: " + path.string();
        return false;
    }

    uint8_t hdr[24]{};
    in.read(reinterpret_cast<char*>(hdr), sizeof(hdr));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(hdr))) {
        error = "Truncated RDS5 header";
        return false;
    }

    const uint32_t magic = read_u32_le(hdr);
    if (magic != kRds5Magic) {
        error = "Invalid TTPF magic (expected RDS5)";
        return false;
    }

    total_chunk_slots_ = read_u32_le(hdr + 4);
    etpf_block_ = read_u32_le(hdr + 16);

    const uint64_t chunk_table_bytes = static_cast<uint64_t>(total_chunk_slots_) * 8ull;
    if (0x18ull + chunk_table_bytes > fsize) {
        error = "RDS5 chunk table exceeds file bounds";
        return false;
    }

    std::vector<uint64_t> raw_entries(total_chunk_slots_);
    if (!raw_entries.empty()) {
        in.read(reinterpret_cast<char*>(raw_entries.data()), static_cast<std::streamsize>(chunk_table_bytes));
        if (in.gcount() != static_cast<std::streamsize>(chunk_table_bytes)) {
            error = "Failed reading RDS5 chunk table";
            return false;
        }
    }

    uint32_t n_act = total_chunk_slots_;
    while (n_act > 0 && raw_entries[n_act - 1u] == 0) {
        --n_act;
    }
    active_entries_ = n_act;
    vpages_.resize(n_act);
    disk_offsets_.resize(n_act);
    codecs_.resize(n_act);
    if (n_act > 1u) {
        vpage_to_chunk_.reserve(n_act);
    }
    for (uint32_t i = 0; i < n_act; ++i) {
        const uint64_t entry = raw_entries[i];
        vpages_[i] = static_cast<uint32_t>((entry >> 30) & 0x3FFFFFull);
        disk_offsets_[i] = static_cast<uint64_t>(entry & 0x3FFFFFFull) * 0x100ull;
        codecs_[i] = static_cast<uint8_t>((entry >> 26) & 0xFull);
        if (i + 1u < n_act) {
            vpage_to_chunk_.emplace(vpages_[i], i);
        }
    }

    const uint64_t etpf_offset = static_cast<uint64_t>(etpf_block_) * 0x100ull;
    if (etpf_offset + 16ull > fsize) {
        error = "ETPF offset outside file bounds";
        return false;
    }

    in.seekg(static_cast<std::streamoff>(etpf_offset), std::ios::beg);
    uint8_t etpf_hdr[16]{};
    in.read(reinterpret_cast<char*>(etpf_hdr), sizeof(etpf_hdr));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(etpf_hdr))) {
        error = "Failed reading ETPF header";
        return false;
    }
    if (read_u32_le(etpf_hdr) != kEtpfMagic) {
        error = "Invalid ETPF table magic";
        return false;
    }

    const uint32_t ntex = read_u32_le(etpf_hdr + 8);
    const uint64_t table_bytes = static_cast<uint64_t>(ntex) * 24ull;
    if (etpf_offset + 16ull + table_bytes > fsize) {
        error = "ETPF descriptor/hash table exceeds file bounds";
        return false;
    }

    static_assert(sizeof(RawDescriptor) == 16 && alignof(RawDescriptor) == 4,
                  "RawDescriptor layout must match packed ETPF table");
    descriptors_.resize(ntex);
    hashes_.resize(ntex);
    if (ntex > 0) {
        const std::streamsize desc_bytes = static_cast<std::streamsize>(ntex) * 16;
        in.read(reinterpret_cast<char*>(descriptors_.data()), desc_bytes);
        if (in.gcount() != desc_bytes) {
            error = "Failed reading ETPF descriptors";
            return false;
        }

        const std::streamsize hash_bytes = static_cast<std::streamsize>(ntex) * 8;
        in.read(reinterpret_cast<char*>(hashes_.data()), hash_bytes);
        if (in.gcount() != hash_bytes) {
            error = "Failed reading ETPF texture hashes";
            return false;
        }

        hash_to_index_.reserve(ntex);
        for (uint32_t i = 0; i < ntex; ++i) {
            hash_to_index_.emplace(hashes_[i], i);
        }
    }

    return true;
}

std::optional<uint32_t> TtpfArchive::find_by_hash(uint64_t hash64) const noexcept {
    auto it = hash_to_index_.find(hash64);
    if (it == hash_to_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<TtpfTextureInfo> TtpfArchive::texture_info(uint32_t index) const {
    if (index >= descriptors_.size()) {
        return std::nullopt;
    }
    const auto& d = descriptors_[index];
    const uint32_t vp22 = d.d2 & 0x3FFFFFu;
    const uint32_t vp20 = d.d2 & 0xFFFFFu;
    const uint32_t vpage = vpage_to_chunk_.contains(vp22) ? vp22 : vp20;
    return TtpfTextureInfo{
        index,
        hashes_[index],
        (d.d0 & 0x3FFFu) + 1u,
        ((d.d0 >> 14) & 0x3FFFu) + 1u,
        static_cast<uint16_t>((d.d1 & 0xFFFFu) >> 3),
        static_cast<uint8_t>((d.d1 >> 24) & 0xFFu),
        vpage
    };
}

std::vector<TtpfChunkInfo> TtpfArchive::texture_chunks(uint32_t index) const {
    std::vector<TtpfChunkInfo> out;
    const auto info = texture_info(index);
    if (!info.has_value() || active_entries_ < 2) {
        return out;
    }
    auto it = vpage_to_chunk_.find(info->vpage);
    if (it == vpage_to_chunk_.end()) {
        return out;
    }
    const uint32_t start_chunk = it->second;
    uint32_t next_vp = vpages_.back();
    if (index + 1u < descriptors_.size()) {
        const auto next_info = texture_info(index + 1u);
        if (next_info.has_value()) {
            next_vp = next_info->vpage;
        }
    }
    for (uint32_t ci = start_chunk; ci + 1u < active_entries_ && vpages_[ci] < next_vp; ++ci) {
        const uint32_t vlen = (vpages_[ci + 1u] - vpages_[ci]) * 4096u;
        out.push_back(TtpfChunkInfo{
            ci,
            vpages_[ci],
            vlen,
            disk_offsets_[ci],
            codecs_[ci]
        });
    }
    return out;
}

bool TtpfArchive::read_chunk_raw(const TtpfChunkInfo& chunk,
                                 std::vector<uint8_t>& out,
                                 std::string& error) const {
    if (chunk.index + 1u >= active_entries_) {
        error = "Chunk index out of bounds";
        return false;
    }
    const uint64_t start = disk_offsets_[chunk.index];
    const uint64_t end = disk_offsets_[chunk.index + 1u];
    if (end < start || end > file_size_) {
        error = "Chunk disk range exceeds TTPF file bounds";
        return false;
    }
    const uint64_t size = end - start;
    out.resize(static_cast<size_t>(size));
    if (size == 0) {
        return true;
    }
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        error = "Cannot open TTPF file for chunk read";
        return false;
    }
    in.seekg(static_cast<std::streamoff>(start), std::ios::beg);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    if (in.gcount() != static_cast<std::streamsize>(size)) {
        error = "Truncated read for TTPF chunk";
        return false;
    }
    return true;
}

uint32_t TtpfArchive::texture_count() const noexcept {
    return static_cast<uint32_t>(descriptors_.size());
}

uint32_t TtpfArchive::chunk_count() const noexcept {
    return total_chunk_slots_;
}

uint32_t TtpfArchive::active_chunk_entries() const noexcept {
    return active_entries_;
}

const std::filesystem::path& TtpfArchive::path() const noexcept {
    return path_;
}

bool parse_microsoft_game_config(const std::filesystem::path& game_root,
                                 GamePackageInfo& out,
                                 std::string& error) {
    const auto cfg_path = game_root / "MicrosoftGame.Config";
    std::ifstream in(cfg_path, std::ios::binary);
    if (!in) {
        error = "Cannot open MicrosoftGame.Config at " + cfg_path.string();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string xml = ss.str();

    out = GamePackageInfo{};
    out.title_id = extract_xml_elem_text(xml, "TitleId");
    out.store_id = extract_xml_elem_text(xml, "StoreId");
    out.identity_name = extract_xml_attr(xml, "Identity", "Name");
    out.identity_version = extract_xml_attr(xml, "Identity", "Version");
    out.display_name = extract_xml_attr(xml, "ShellVisuals", "DefaultDisplayName");
    out.publisher_display_name = extract_xml_attr(xml, "ShellVisuals", "PublisherDisplayName");
    out.executable_name = extract_xml_attr(xml, "Executable", "Name");
    out.target_device_family = extract_xml_attr(xml, "Executable", "TargetDeviceFamily");

    if (out.executable_name.empty()) {
        error = "MicrosoftGame.Config is missing <Executable Name=\"...\"/>";
        return false;
    }

    std::error_code ec;
    out.executable_present = std::filesystem::exists(game_root / out.executable_name, ec);
    return true;
}

} // namespace viceforge
