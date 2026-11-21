#include "test_assert.hpp"
#include "viceforge/events.hpp"
#include "viceforge/rage9.hpp"
#include "viceforge/registry.hpp"
#include "viceforge/vfs.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace viceforge;

struct TestEvent {
    bool cancellable = true;
    bool cancelled = false;
    std::vector<int> trace;
};

int main() {
    // 1. Case-insensitive and exact JOAAT-32 checks
    VF_CHECK(joaat_lower("Player_Story000_00") == joaat_lower("player_story000_00"));
    VF_CHECK(joaat_exact("Player_Story000_00") != joaat_lower("player_story000_00"));
    VF_CHECK(joaat_lower("handling.meta") != 0u);

    // 2. Registry and DeferredRegister checks
    Registry<int> vehicles("vehicles");
    DeferredRegister<int> reg(vehicles, "sample_mod");
    reg.add("coupe", 42);
    reg.add("sedan", 77);
    std::string err;
    VF_CHECK(reg.register_all(&err));
    VF_CHECK(vehicles.find("sample_mod:coupe") != nullptr);
    VF_CHECK(*vehicles.find("sample_mod:coupe") == 42);
    VF_CHECK(vehicles.find_by_hash(joaat_lower("sample_mod:sedan")) != nullptr);
    VF_CHECK(*vehicles.find_by_hash(joaat_lower("sample_mod:sedan")) == 77);

    // Duplicate registration rejected
    VF_CHECK(!vehicles.register_entry(ResourceId::make("sample_mod", "coupe"), 99, &err));

    // Freeze prevents further writes
    vehicles.freeze();
    VF_CHECK(!vehicles.register_entry(ResourceId::make("sample_mod", "bike"), 10, &err));

    // 3. VFS normalization, conflict resolution, pinning, and hash64 overrides
    VfsMountTable vfs;
    VF_CHECK(VfsMountTable::normalize_virtual_path("common:\\DATA//handling.meta") == "common:/data/handling.meta");
    VF_CHECK(vfs.register_override("common:\\Data\\handling.meta", "mods/a/handling.meta", "mod_a", 1, false, &err));
    VF_CHECK(vfs.register_override("common:/data/handling.meta", "mods/b/handling.meta", "mod_b", 2, false, &err));

    const auto resolved = vfs.resolve("COMMON:/DATA/HANDLING.META");
    VF_CHECK(resolved.has_value());
    VF_CHECK(resolved->owner_mod_id == "mod_b");
    VF_CHECK(vfs.conflicts().size() == 1);
    VF_CHECK(vfs.conflicts()[0].replaced_mod_id == "mod_a");
    VF_CHECK(vfs.conflicts()[0].winning_mod_id == "mod_b");

    // Pinned override cannot be displaced by a non-pinned higher priority layer
    VF_CHECK(vfs.register_override("common:/data/pinned.meta", "mods/pin/pinned.meta", "pin_mod", 1, true, &err));
    VF_CHECK(vfs.register_override("common:/data/pinned.meta", "mods/late/pinned.meta", "late_mod", 99, false, &err));
    const auto pinned = vfs.resolve("common:/data/pinned.meta");
    VF_CHECK(pinned.has_value() && pinned->owner_mod_id == "pin_mod");

    VF_CHECK(vfs.register_hash_override(0xCAFEBABE12345678ull, "mods/a/tex.dds", "mod_a", 1, false, &err));
    VF_CHECK(vfs.register_hash_override(0xCAFEBABE12345678ull, "mods/b/tex.dds", "mod_b", 2, false, &err));
    const auto hres = vfs.resolve_hash(0xCAFEBABE12345678ull);
    VF_CHECK(hres.has_value() && hres->owner_mod_id == "mod_b");
    VF_CHECK(vfs.conflicts().size() == 3);

    // 4. EventBus priority ordering and cancellation
    EventBus bus;
    bus.subscribe<TestEvent>(EventPriority::Low, [](TestEvent& ev) { ev.trace.push_back(3); });
    bus.subscribe<TestEvent>(EventPriority::High, [](TestEvent& ev) {
        ev.trace.push_back(1);
        ev.cancelled = true;
    });
    bus.subscribe<TestEvent>(EventPriority::Lowest, [](TestEvent& ev) { ev.trace.push_back(4); });

    TestEvent ev;
    bus.post(ev);
    VF_CHECK((ev.trace == std::vector<int>{1}));

    std::cout << "test_vfs_registry passed\n";
    return 0;
}
