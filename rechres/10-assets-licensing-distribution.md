# 10 — Asset Requirements, Licensing Compliance & Distribution

## 1. Asset Strategy & Footprint Minimization

A core design principle of SeamlessArena is **zero-asset bloat**:
- **Traditional Arena Mods:** Distribute hundreds of megabytes of modified `.dcx`, `.msb`, `.flver`, and custom `regulation.bin` files. This creates immediate mod conflicts when players try to combine them with Seamless Co-op, overhaul mods (e.g. Convergence), or weapon packs.
- **SeamlessArena Strategy:** 
  1. Operate as a **pure native C++ DLL** (`SeamlessArena.Native.dll`).
  2. Rely 100% on FromSoftware's built-in vanilla Colosseum scene geometry and textures.
  3. Avoid distributing a customized `regulation.bin`. If parameter adjustments are needed (such as SpEffect invulnerability flags), apply them **in memory** via libER param hooks rather than overriding binary disk files.

---

## 2. Intellectual Property & Licensing Compliance

### 2.1. Open Source & Third-Party Code
- **SeamlessArena Project:** Licensed under the MIT License (`LICENSE` in repository root).
- **libER:** MIT Licensed (Dasaav-dsv).
- **MinHook:** 2-Clause BSD License (Tsuda Kageyu).
- **Mod Engine 3:** MIT / Apache-2.0 License (Gary Tierney).
- **Better Multiplayer / DEN Maps:** Proprietary assets. No decompiled assets, custom map models, or reverse-engineered textures are incorporated into this codebase. All ideas are implemented via original, clean-room C++ logic.

---

## 3. Distribution Packaging and Folder Layout

When distributed to end users, SeamlessArena packages as a clean drop-in folder compatible with Mod Engine 3:

```text
EldenRing/Game/
├── me3.exe
├── SeamlessCoop/
│   ├── ersc.dll
│   └── ersc_settings.ini
└── SeamlessArena/
    ├── SeamlessArena.me3            (Mod Engine 3 profile)
    ├── SeamlessArena.toml           (Configurable settings)
    └── bin/
        └── SeamlessArena.Native.dll (Compiled C++ extension)
```

### 3.1. User Installation Steps
1. Ensure Elden Ring and Seamless Co-op are installed and working.
2. Extract the `SeamlessArena` folder into your game root or ME3 mods folder.
3. Launch the game using ME3 pointing to `SeamlessArena.me3`:
   ```powershell
   me3.exe launch --profile SeamlessArena/SeamlessArena.me3
   ```
4. To uninstall: Simply delete the `SeamlessArena` folder or remove its entry from your ME3 launcher profile. No game files or save files are permanently modified.
