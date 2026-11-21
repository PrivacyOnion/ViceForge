# ViceForge Architecture

## 1. Layering Overview

```text
+---------------------------------------------------------------+
|                         Mod DLLs                              |
|  (use include/viceforge/mod.hpp + VICEFORGE_DECLARE_MOD)      |
+-------------------------------+-------------------------------+
                                | C ABI (abi.h / VfHostApi)
+-------------------------------v-------------------------------+
|                      viceforge_core.dll                       |
|  ModLoader | Resolve DAG | VfsMountTable | EventBus | Registry|
+-------------------------------+-------------------------------+
                                | IRuntimeBridge
        +-----------------------+-----------------------+
        v                                               v
HeadlessRuntimeBridge                          GameRuntimeBridge (Future)
(CLI validation & CI tests)                    (VFS detours, D3D12 Present,
                                                YSC native bridge)
```

## 2. ABI & Lifecycle Guarantees

- Every native mod exports four `extern "C"` symbols (`vf_mod_query`, `vf_mod_init`, `vf_mod_on_event`, `vf_mod_shutdown`) generated automatically by `VICEFORGE_DECLARE_MOD`.
- Before calling `vf_mod_init`, `ModLoader` verifies `abi_version == VICEFORGE_ABI_VERSION` and ensures the DLL's reported `id` matches its `viceforge.mod.json`.
- Mods are initialized in topological order (`dependencies`, `load_after`, reversed `load_before`, lexicographic tie-breaker) and shut down in exact reverse order.
- `ModLoader` never loads mod DLLs or runs initialization inside `DllMain`; `viceforge_proxy.dll` spawns a dedicated bootstrap worker thread on `DLL_PROCESS_ATTACH`.

## 3. VFS Overlay Semantics

- Virtual asset paths are normalized (ASCII lowercase, `/` separators, duplicate slashes collapsed, leading/trailing slashes removed).
- Declarative `asset_mounts` in `viceforge.mod.json` and dynamic calls via `ModContext::register_asset_override` share one priority model: later mods in the resolved topological order override earlier ones unless an earlier mount sets `pinned: true`.
- 64-bit TTPF texture hashes can be registered alongside virtual paths via `ModContext::register_hash_override`.
- All displacements are recorded in `VfsMountTable::conflicts()` so modpacks stay transparent and debuggable.

## 4. RAGE9 Offline Layer

- `RPF9` (`9FPR`): Reads archive header words, signature tag, and total size safely from disk or memory spans.
- `TTPF` (`RDS5` / `ETPF`): Indexes chunk page tables and texture descriptors (`width`, `height`, DXGI BC1/BC3/BC4/BC5/BC7 format, `tile_mode`, 64-bit texture hash) in O(1) after initial index load, plus provides RDNA2/Xbox Series X swizzle block index translation.
- `MicrosoftGame.Config`: Identifies title metadata and tracks whether the declared executable (`game_scarlett_master.exe`) is present in the inspected directory.
