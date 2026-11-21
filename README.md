# ViceForge

**ViceForge** is a modular, Forge-inspired modding framework, virtual filesystem (VFS) overlay, C++20 SDK, and RAGE9 asset toolkit built for *Grand Theft Auto VI* (`game_scarlett_master.exe` / future desktop builds).

Every core subsystem—manifest & SemVer dependency resolution, deterministic load ordering, VFS path & hash-64 override resolution, JOAAT-32 registries, prioritized event bus, native DLL ABI loading, CLI scaffolding/validation, and RPF9 / TTPF (`RDS5`/`ETPF`) container inspection—works and is verified offline today without requiring the game executable. Runtime detours are cleanly isolated behind `IRuntimeBridge` so game attachment can drop in directly once the executable becomes available.

## Features

- **Stable C ABI + Ergonomic C++20 SDK** (`include/viceforge/abi.h`, `include/viceforge/mod.hpp`):
  - Versioned host API table (`VICEFORGE_ABI_VERSION`), automatic `VICEFORGE_DECLARE_MOD(...)` entrypoint export macro (`vf_mod_query`, `vf_mod_init`, `vf_mod_on_event`, `vf_mod_shutdown`).
- **Declarative Mod Manifests & Topological Resolver** (`include/viceforge/manifest.hpp`):
  - Parses `viceforge.mod.json` (SemVer constraints `^`, `>=`, `=`, `*`, required/optional dependencies, `conflicts`, `load_before`, `load_after`, and `asset_mounts`).
  - Deterministic Kahn topological sort with duplicate-ID, version-mismatch, conflict, and cycle diagnostics.
- **VFS Overlay & Conflict Tracker** (`include/viceforge/vfs.hpp`):
  - Case-insensitive normalized virtual RAGE9 paths (`common:/data/...`) and 64-bit TTPF texture hash overrides with pinning and explicit conflict logging.
- **JOAAT-32 Registry & DeferredRegister** (`include/viceforge/registry.hpp`):
  - Deterministic `namespace:path` resource registration (`joaat_lower` / `joaat_exact`), collision checking, and post-init registry freezing.
- **Prioritized Event Bus** (`include/viceforge/events.hpp`):
  - Priority-ordered dispatch (`Highest` through `Lowest`) with cancellable events and lifecycle broadcasts (`ModLoad`, `AssetMount`, `PreTick`, `PostTick`, `ScriptTick`, `DeviceCreate`, `Present`, `Shutdown`).
- **RAGE9 Container Toolkit** (`include/viceforge/rage9.hpp`):
  - Read-only parsers for `RPF9` (`9FPR`) archive headers, `.ttpf` (`RDS5` / `ETPF`) texture database chunk tables & DXGI formats, RDNA2/Xbox Series X BCn swizzle block indexing, and `MicrosoftGame.Config` package descriptors.
- **Headless Loader, CLI & Proxy Bootstrap** (`cli/main.cpp`, `src/proxy_bootstrap.cpp`):
  - `vfcli` supports mod scaffolding, manifest validation, headless mod-pack simulation, and RAGE9/TTPF/package inspection.
  - `viceforge_proxy.dll` provides an out-of-loader-lock worker bootstrap ready to pair with the runtime bridge.

## Building & Testing (Windows x64)

Requirements: Visual Studio 2022 (MSVC C++20), CMake >= 3.24, Ninja.

```powershell
# Launch x64 Native Tools / VS Developer PowerShell, then from the repository root:
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure -V
```

## Quickstart (`vfcli`)

```powershell
# Scaffold a new C++20 mod project
.\out\build\dev\bin\vfcli.exe init my_handling_mod

# Validate mod load order, dependencies, and VFS conflicts
.\out\build\dev\bin\vfcli.exe validate .\mods

# Boot compiled mod DLLs under the headless host bridge
.\out\build\dev\bin\vfcli.exe run-headless .\mods

# Inspect RAGE9 archives, TTPF texture tables, or package layout
.\out\build\dev\bin\vfcli.exe inspect-rpf .\common.rpf
.\out\build\dev\bin\vfcli.exe inspect-ttpf .\data\ttdb\graphics\ttdb_graphics.ttpf --hash 0x1122334455667788
.\out\build\dev\bin\vfcli.exe inspect-package .
```

## Status & Capability Matrix

| Subsystem | Available Today (No `.exe` Needed) | Requires Game Executable (`game_scarlett_master.exe` / PC `.exe`) |
| --- | --- | --- |
| Mod Manifests & SemVer Dependency Resolver | Full parsing, DAG sorting, cycle/conflict detection | Game version gating against executable build string |
| C++20 Mod SDK & Versioned C ABI | Complete headers, macros, host callback table | Additional live gameplay/world host services |
| VFS Overlay & Hash-64 Override Table | Complete normalization, pinning, priority & conflict resolution | Detouring engine file open & TTPF lookup functions in memory |
| Prioritized Event Bus & Headless Simulation | Offline testing via `HeadlessRuntimeBridge` & `vfcli run-headless` | Frame tick, script tick & D3D12 `Present` hooks driving events live |
| JOAAT-32 Registry & `DeferredRegister` | Resource ID hashing, collision detection, freezing | Runtime injection into game pools / archetype managers |
| RAGE9 Toolkit (`RPF9`, `TTPF`, `MicrosoftGame.Config`) | Header/table parsing, hash lookup, swizzle math, package inspection | In-memory decryption/streaming keys & engine allocator integration |
| DLL Proxy Bootstrap (`viceforge_proxy.dll`) | Safe background thread init & mod loading | Wiring to an imported system DLL proxy table of the target `.exe` |
| Script & D3D12 Runtime Hooks | `IRuntimeBridge` contract & capability flags | Signature scanning, YSC native table bridge, D3D12 command queue hook |
