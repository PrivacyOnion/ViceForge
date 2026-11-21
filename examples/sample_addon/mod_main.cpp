#include "viceforge/mod.hpp"

class SampleAddonMod final : public viceforge::Mod {
public:
    VfStatus on_init(const viceforge::ModContext& ctx) override {
        if (!ctx.is_mod_loaded("sample_mod")) {
            ctx.error("Expected sample_mod to be loaded first");
            return VF_ERR_NOT_FOUND;
        }
        ctx.info("sample_addon initialized after sample_mod");
        ctx.register_asset_override("common:/data/viceforge_dynamic.meta", "sample_addon_dynamic.meta");
        return VF_OK;
    }

    VfStatus on_event(const viceforge::ModContext& ctx, VfEventPayload& payload) override {
        if (payload.kind == VF_EVENT_POST_TICK) {
            ctx.info("sample_addon received VF_EVENT_POST_TICK");
        }
        return VF_OK;
    }

    void on_shutdown(const viceforge::ModContext& ctx) override {
        ctx.info("sample_addon shutdown");
    }
};

VICEFORGE_DECLARE_MOD(SampleAddonMod, "sample_addon", "ViceForge Sample Addon", "1.1.0")
