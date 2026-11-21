#include "test_assert.hpp"
#include "viceforge/rage9.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace viceforge;

namespace {

void write_u32_le(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    buf[off + 0] = static_cast<uint8_t>(v & 0xFFu);
    buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    buf[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    buf[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

void write_u64_le(std::vector<uint8_t>& buf, size_t off, uint64_t v) {
    write_u32_le(buf, off, static_cast<uint32_t>(v & 0xFFFFFFFFull));
    write_u32_le(buf, off + 4, static_cast<uint32_t>(v >> 32));
}

std::filesystem::path make_temp_dir(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

int main() {
    const auto temp = make_temp_dir("viceforge_test_rage9");

    // 1. RPF9 valid + invalid tests
    {
        std::vector<uint8_t> rpf(64, 0);
        rpf[0] = '9';
        rpf[1] = 'F';
        rpf[2] = 'P';
        rpf[3] = 'R';
        write_u32_le(rpf, 4, 0x360u);
        write_u32_le(rpf, 8, 0x340u);
        write_u32_le(rpf, 12, 0x40u);
        write_u32_le(rpf, 16, 0x11223344u);
        write_u32_le(rpf, 20, 0x55667788u);

        const auto rpf_path = temp / "synthetic.rpf";
        {
            std::ofstream out(rpf_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(rpf.data()), static_cast<std::streamsize>(rpf.size()));
        }
        Rpf9Header hdr{};
        std::string err;
        VF_CHECK(parse_rpf9_header(rpf_path, hdr, err));
        VF_CHECK(hdr.magic == kRpf9Magic);
        VF_CHECK(hdr.header_word_1 == 0x360u);
        VF_CHECK(hdr.signature_lo == 0x11223344u);
        VF_CHECK(hdr.signature_hi == 0x55667788u);
        VF_CHECK(hdr.file_size == 64u);

        rpf[0] = 'X';
        VF_CHECK(!parse_rpf9_header(rpf, hdr, err));
        VF_CHECK(!parse_rpf9_header(std::span<const uint8_t>(rpf).subspan(0, 8), hdr, err));
    }

    // 2. Synthetic TTPF (RDS5 + ETPF) container parsing and lookup
    {
        std::vector<uint8_t> ttpf(0x300, 0);
        ttpf[0] = 'R';
        ttpf[1] = 'D';
        ttpf[2] = 'S';
        ttpf[3] = '5';
        write_u32_le(ttpf, 4, 4u);    // 4 slots (1 trailing empty slot)
        write_u64_le(ttpf, 8, 9u * 4096u);
        write_u32_le(ttpf, 16, 2u);   // ETPF block = 2 -> 0x200

        const uint64_t c0 = (uint64_t{1} << 30) | (uint64_t{8} << 26) | uint64_t{1};
        const uint64_t c1 = (uint64_t{5} << 30) | (uint64_t{8} << 26) | uint64_t{2};
        const uint64_t c2 = (uint64_t{9} << 30) | (uint64_t{0} << 26) | uint64_t{3};
        write_u64_le(ttpf, 0x18, c0);
        write_u64_le(ttpf, 0x20, c1);
        write_u64_le(ttpf, 0x28, c2);
        write_u64_le(ttpf, 0x30, 0ull); // trailing empty slot

        const size_t etpf_off = 0x200;
        ttpf[etpf_off + 0] = 'E';
        ttpf[etpf_off + 1] = 'T';
        ttpf[etpf_off + 2] = 'P';
        ttpf[etpf_off + 3] = 'F';
        write_u32_le(ttpf, etpf_off + 4, 1u);
        write_u32_le(ttpf, etpf_off + 8, 2u); // 2 textures
        write_u32_le(ttpf, etpf_off + 12, 0u);

        const uint32_t t0_d0 = 127u | (127u << 14);
        const uint32_t t0_d1 = (98u << 3) | (0u << 24);
        const uint32_t t0_d2 = 1u;
        write_u32_le(ttpf, etpf_off + 16, t0_d0);
        write_u32_le(ttpf, etpf_off + 20, t0_d1);
        write_u32_le(ttpf, etpf_off + 24, t0_d2);
        write_u32_le(ttpf, etpf_off + 28, 0u);

        const uint32_t t1_d0 = 63u | (63u << 14);
        const uint32_t t1_d1 = (83u << 3) | (1u << 24);
        const uint32_t t1_d2 = 5u;
        write_u32_le(ttpf, etpf_off + 32, t1_d0);
        write_u32_le(ttpf, etpf_off + 36, t1_d1);
        write_u32_le(ttpf, etpf_off + 40, t1_d2);
        write_u32_le(ttpf, etpf_off + 44, 0u);

        write_u64_le(ttpf, etpf_off + 48, 0xAAAA000011112222ull);
        write_u64_le(ttpf, etpf_off + 56, 0xBBBB000033334444ull);

        const auto ttpf_path = temp / "synthetic.ttpf";
        {
            std::ofstream out(ttpf_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(ttpf.data()), static_cast<std::streamsize>(ttpf.size()));
        }

        TtpfArchive archive;
        std::string err;
        VF_CHECK(archive.open(ttpf_path, err));
        VF_CHECK(archive.chunk_count() == 4);
        VF_CHECK(archive.active_chunk_entries() == 3);
        VF_CHECK(archive.texture_count() == 2);

        const auto idx0 = archive.find_by_hash(0xAAAA000011112222ull);
        VF_CHECK(idx0.has_value() && *idx0 == 0);
        const auto info0 = archive.texture_info(*idx0);
        VF_CHECK(info0.has_value());
        VF_CHECK(info0->width == 128 && info0->height == 128);
        VF_CHECK(info0->dxgi == 98 && dxgi_format_name(info0->dxgi) == "BC7_UNORM");
        VF_CHECK(bc_bytes_per_block(info0->dxgi) == 16);
        const auto chunks0 = archive.texture_chunks(0);
        VF_CHECK(chunks0.size() == 1);
        VF_CHECK(chunks0[0].virtual_size == 16384u);
        VF_CHECK(chunks0[0].codec == 8u);

        const auto idx1 = archive.find_by_hash(0xBBBB000033334444ull);
        VF_CHECK(idx1.has_value() && *idx1 == 1);
        const auto info1 = archive.texture_info(*idx1);
        VF_CHECK(info1.has_value() && info1->dxgi == 83 && info1->tile_mode == 1);
        VF_CHECK(!archive.find_by_hash(0x9999999999999999ull).has_value());

        const uint32_t swiz = unswizzle_bc_block_index(63, 63, 256, 256, 16, 0);
        VF_CHECK(swiz == 4095u);
    }

    // 3. MicrosoftGame.Config parser
    {
        const auto cfg = temp / "MicrosoftGame.Config";
        std::ofstream out(cfg, std::ios::binary);
        out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            << "<Game configVersion=\"1\">\n"
            << "  <TitleId>12345678</TitleId>\n"
            << "  <StoreId>9TESTSTORE01</StoreId>\n"
            << "  <Identity Name=\"TestPublisher.Title\" Version=\"1.0.2.1\" />\n"
            << "  <ShellVisuals DefaultDisplayName=\"Synthetic Game\" PublisherDisplayName=\"Test Studio\" />\n"
            << "  <ExecutableList><Executable Name=\"missing_game.exe\" TargetDeviceFamily=\"Scarlett\" /></ExecutableList>\n"
            << "</Game>\n";
        out.close();

        GamePackageInfo pkg{};
        std::string err;
        VF_CHECK(parse_microsoft_game_config(temp, pkg, err));
        VF_CHECK(pkg.title_id == "12345678");
        VF_CHECK(pkg.store_id == "9TESTSTORE01");
        VF_CHECK(pkg.identity_version == "1.0.2.1");
        VF_CHECK(pkg.executable_name == "missing_game.exe");
        VF_CHECK(!pkg.executable_present);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::cout << "test_rage9_containers passed\n";
    return 0;
}
