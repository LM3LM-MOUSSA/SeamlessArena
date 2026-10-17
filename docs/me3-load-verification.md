# ME3 Profile & Dual DLL Load Order Verification

**Date:** 2026-10-17  
**ME3 Version:** `me3 0.12.1 (commit 6d7697b49e2e4067b68394c0d22fbba8c98065a2)`  
**Profile Location:** `F:\dev\SeamlessArena\SeamlessArena.me3`

---

## 1. Verified Native Load Order

Mod Engine 3 parses `SeamlessArena.me3` and establishes the sequential load order based on the `load_early` property:

1. **Step 1 (Early Native):** `ersc.dll` (Seamless Co-op)
   - Path: `F:/SteamLibrary/steamapps/common/ELDEN RING/Game/SeamlessCoop/ersc.dll`
   - Setting: `load_early = true`, `optional = false`
   - Purpose: Injects before main engine subsystems to establish Steam Networking P2P hooks and separate `.co2` save isolation.

2. **Step 2 (Standard Native):** `SeamlessArena.dll`
   - Path: `F:/dev/SeamlessArena/bin/x64/Release/SeamlessArena.dll`
   - Setting: `load_early = false`, `optional = false`
   - Initializer: `SeamlessArena_Initialize`
   - Finalizer: `SeamlessArena_Finalize`
   - Purpose: Starts after Seamless Co-op is mapped in process memory. `VersionGuard::VerifyGameVersion()` checks `GetModuleHandleW(L"ersc.dll")` to confirm Seamless is present before initiating arena hooks.

---

## 2. ME3 Profile Inspection Output

Command executed:
```powershell
& "C:\Users\Rayen_lm3alim\AppData\Local\Programs\garyttierney\me3\bin\me3.exe" profile show --file "f:\dev\SeamlessArena\SeamlessArena.me3"
```

Console Output:
```text
 INFO me3: version="0.12.1" commit_id="6d7697b49e2e4067b68394c0d22fbba8c98065a2"
● Mod Profile
    Name: SeamlessArena
    Path: f:\dev\SeamlessArena
● Supports
    EldenRing: Supported
● Natives
  ● ersc.dll
      Optional: false
       Enabled: true
          Path: F:/SteamLibrary/steamapps/common/ELDEN RING/Game/SeamlessCoop/ersc.dll
  ● SeamlessArena.dll
      Optional: false
       Enabled: true
          Path: bin/x64/Release/SeamlessArena.dll
● Packages
● Options
    Neutralize Arxan: -
        Start Online: -
```
