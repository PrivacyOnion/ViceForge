# ViceForge Roadmap

## Phase 1 — Offline Foundation (Implemented)

- C++20 SDK and stable C ABI (`VfHostApi`, `VICEFORGE_DECLARE_MOD`)
- Strict JSON manifest parser (`viceforge.mod.json`) with SemVer constraints and topological DAG resolver
- Normalized VFS path overlay + 64-bit hash override table with conflict logging
- JOAAT-32 `ResourceId` and `DeferredRegister` / `Registry`
- Prioritized `EventBus` with cancellation support
- RAGE9 offline readers (`RPF9`, `TTPF` `RDS5`/`ETPF`, `MicrosoftGame.Config`)
- `vfcli` developer CLI and `viceforge_proxy.dll` safe thread bootstrap
- Automated test suite and Windows CI workflow

## Phase 2 — Asset Compilation & Packaging (Pre-Executable)

- Offline modpack bundler (`.vfpack`) with checksums and conflict pre-flight reports
- Automatic RDNA2 / D3D12 texture swizzler & BC7/BC5 `.ttpf` patch builder
- Declarative XML/meta diff-merging layer (`.meta` / `handling` / vehicle variation patches without whole-file replacement)

## Phase 3 — Runtime Attachment (Upon Executable Availability)

- Implement `GameRuntimeBridge : IRuntimeBridge`:
  - Import table inspection on the target executable to rename `viceforge_proxy.dll` (e.g. `version.dll` / `dinput8.dll`)
  - VFS file open & RPF/TTPF stream interception hooking `VfsMountTable::resolve` and `resolve_hash`
  - YSC script VM tick & native registration handler bridge (`VF_EVENT_SCRIPT_TICK`)
  - D3D12 `IDXGISwapChain::Present` / command queue hook (`VF_EVENT_DEVICE_CREATE`, `VF_EVENT_PRESENT`) for ImGui / custom overlay mods
