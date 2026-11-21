#include "viceforge/mod.hpp"

class SampleBaseMod final : public viceforge::Mod {
public:
    VfStatus on_init(const viceforge::ModContext& ctx) override {
        ctx.info("sample_mod initialized");
        ctx.register_asset_override("common:/data/viceforge_dynamic.meta", "sample_mod_dynamic.meta");
        ctx.register_hash_override(0x1122334455667788ull, "sample_mod_albedo.dds");
        return VF_OK;
    }

    VfStatus on_event(const viceforge::ModContext& ctx, VfEventPayload& payload) override {
        if (payload.kind == VF_EVENT_ASSET_MOUNT) {
            ctx.info("sample_mod received VF_EVENT_ASSET_MOUNT");
        } else if (payload.kind == VF_EVENT_PRE_TICK) {
            ctx.info("sample_mod received VF_EVENT_PRE_TICK");
        }
        return VF_OK;
    }

    void on_shutdown(const viceforge::ModContext& ctx) override {
        ctx.info("sample_mod shutdown");
    }
};

VICEFORGE_DECLARE_MOD(SampleBaseMod, "sample_mod", "ViceForge Sample Base Mod", "1.0.0")
