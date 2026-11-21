#include "test_assert.hpp"
#include "viceforge/events.hpp"
#include "viceforge/hash_utils.hpp"
#include "viceforge/rage9.hpp"
#include "viceforge/registry.hpp"
#include "viceforge/vfs.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace viceforge;
using namespace viceforge::literals;

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

} // namespace

int main() {
    // 1. Compile-time constexpr JOAAT-32 vs runtime JOAAT-32 parity
    constexpr uint32_t ct_hash = "handling.meta"_joaat;
    constexpr uint32_t ct_upper = joaat_lower_ct("HANDLING.META");
    static_assert(ct_hash == ct_upper);
    VF_CHECK(ct_hash == joaat_lower("handling.meta"));
    VF_CHECK(joaat_exact_ct("Handling.Meta") == joaat_exact("Handling.Meta"));

    // 2. Copy-on-write EventBus mutation safety during post()
    struct MutEvent {
        int calls = 0;
        bool added_during_dispatch = false;
    };
    EventBus bus;
    SubscriptionId victim_id = 0;
    victim_id = bus.subscribe<MutEvent>(EventPriority::Normal, [](MutEvent& ev) {
        ev.calls += 10;
    });
    bus.subscribe<MutEvent>(EventPriority::High, [&](MutEvent& ev) {
        ev.calls += 1;
        // Mutating listeners during post() must not invalidate active iteration snapshot.
        bus.unsubscribe<MutEvent>(victim_id);
        bus.subscribe<MutEvent>(EventPriority::Low, [](MutEvent& inner) {
            inner.added_during_dispatch = true;
        });
    });

    MutEvent first_ev{};
    bus.post(first_ev);
    VF_CHECK(first_ev.calls == 11);
    VF_CHECK(!first_ev.added_during_dispatch);

    MutEvent second_ev{};
    bus.post(second_ev);
    VF_CHECK(second_ev.calls == 1);
    VF_CHECK(second_ev.added_during_dispatch);

    // 3. Zero-copy VFS & Registry lookups
    VfsMountTable vfs;
    vfs.reserve(8, 8);
    VF_CHECK(VfsMountTable::is_normalized_virtual_path("common:/data/handling.meta"));
    VF_CHECK(!VfsMountTable::is_normalized_virtual_path("common:/Data/handling.meta"));
    VF_CHECK(!VfsMountTable::is_normalized_virtual_path("common:\\data\\handling.meta"));
    VF_CHECK(!VfsMountTable::is_normalized_virtual_path("/common:/data/handling.meta"));

    std::string err;
    VF_CHECK(vfs.register_override("common:/Data/Vehicles.meta", "mods/a/vehicles.meta", "mod_a", 1, false, &err));
    VF_CHECK(vfs.register_hash_override(0x5566778899AABBCCull, "mods/a/tex.dds", "mod_a", 1, false, &err));

    const VfsOverrideRecord* p1 = vfs.find("common:/data/vehicles.meta");
    const VfsOverrideRecord* p2 = vfs.find("COMMON:/DATA/VEHICLES.META");
    VF_CHECK(p1 != nullptr && p2 == p1 && p1->owner_mod_id == "mod_a");
    VF_CHECK(vfs.find_hash(0x5566778899AABBCCull) != nullptr);

    Registry<uint32_t> reg("bench_reg");
    reg.reserve(16);
    VF_CHECK(reg.register_entry(ResourceId::make("mod_a", "item_0"), 1234u, &err));
    const uint32_t* found = reg.find(std::string_view("mod_a:item_0"));
    VF_CHECK(found != nullptr && *found == 1234u);

    // 4. TtpfArchive::read_chunk_raw on synthetic container
    const auto temp = std::filesystem::temp_directory_path() / "viceforge_test_opts";
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp);

    std::vector<uint8_t> ttpf(0x300, 0);
    ttpf[0] = 'R';
    ttpf[1] = 'D';
    ttpf[2] = 'S';
    ttpf[3] = '5';
    write_u32_le(ttpf, 4, 2u);
    write_u64_le(ttpf, 8, 4096u);
    write_u32_le(ttpf, 16, 2u); // ETPF at 0x200
    // Chunk 0 at disk block 1 (0x100..0x180 -> 0x80 bytes)
    write_u64_le(ttpf, 0x18, (uint64_t{1} << 30) | (uint64_t{8} << 26) | uint64_t{1});
    write_u64_le(ttpf, 0x20, (uint64_t{2} << 30) | (uint64_t{0} << 26) | uint64_t{2});
    ttpf[0x100] = 0x78;
    ttpf[0x101] = 0xDA;
    ttpf[0x17F] = 0xEF;
    ttpf[0x200] = 'E';
    ttpf[0x201] = 'T';
    ttpf[0x202] = 'P';
    ttpf[0x203] = 'F';
    write_u32_le(ttpf, 0x204, 1u);
    write_u32_le(ttpf, 0x208, 1u);
    write_u32_le(ttpf, 0x210, 31u | (31u << 14));
    write_u32_le(ttpf, 0x214, 98u << 3);
    write_u32_le(ttpf, 0x218, 1u);
    write_u64_le(ttpf, 0x220, 0x0102030405060708ull);

    const auto ttpf_path = temp / "raw_chunk.ttpf";
    std::ofstream(ttpf_path, std::ios::binary).write(reinterpret_cast<const char*>(ttpf.data()), ttpf.size());

    TtpfArchive archive;
    VF_CHECK(archive.open(ttpf_path, err));
    const auto chunks = archive.texture_chunks(0);
    VF_CHECK(chunks.size() == 1);
    std::vector<uint8_t> raw_bytes;
    VF_CHECK(archive.read_chunk_raw(chunks[0], raw_bytes, err));
    VF_CHECK(raw_bytes.size() == 0x100u);
    VF_CHECK(raw_bytes[0] == 0x78 && raw_bytes[1] == 0xDA && raw_bytes[0x7F] == 0xEF);

    // 5. Tight loop sanity test on an immutable-subscriber EventBus + VfsMountTable
    EventBus fast_bus;
    struct FastTick { uint64_t ticks = 0; };
    fast_bus.subscribe<FastTick>(EventPriority::High, [](FastTick& ev) { ev.ticks += 1u; });
    FastTick fast_ev{};
    uint64_t check_sum = 0;
    for (int i = 0; i < 50000; ++i) {
        fast_bus.post(fast_ev);
        const auto* hit = vfs.find("common:/data/vehicles.meta");
        check_sum += hit ? hit->priority : 0u;
    }
    VF_CHECK(fast_ev.ticks == 50000ull);
    VF_CHECK(check_sum == 50000ull);

    std::filesystem::remove_all(temp, ec);
    std::cout << "test_optimizations passed\n";
    return 0;
}
