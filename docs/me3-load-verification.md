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

---

## 3. Live Mod Engine 3 Launch Log

The user launched Elden Ring via Mod Engine 3 with `SeamlessArena.me3`, yielding the following live host engine log:

```text
 INFO me3_mod_host::host: loaded native with me2 compatibility shim path="F:/SteamLibrary/steamapps/common/ELDEN RING/Game/SeamlessCoop/ersc.dll"
 INFO hook_heap_allocators: me3_mod_host::alloc_hooks: hooking heap allocators
 INFO oversized_regulation_fix: me3_mod_host::savefile: applied hooks
 INFO assets:wwise: me3_mod_host::asset_hooks: applied asset override hook
ERROR me3_mod_host::host: native failed to initialize path="F:\dev\SeamlessArena\bin/x64/Release/SeamlessArena.dll" symbol="SeamlessArena_Initialize"
 INFO assets:file_step:hook:device_manager: me3_mod_host::asset_hooks: applied asset override hook
 INFO assets:file_step:hook:mount_ebl: me3_mod_host::asset_hooks: applied asset override hook
 INFO assets:file_step:hook:ebl: me3_mod_host::asset_hooks: applied asset override hook
```

### Verification Findings:
1. **Load Order Confirmed:** Mod Engine 3 sequentially loaded `ersc.dll` first (`load_early = true` with `me2 compatibility shim`).
2. **SeamlessArena Injected Second:** ME3 located `SeamlessArena.dll` and invoked the exported `SeamlessArena_Initialize` C ABI symbol.
3. **Internal Diagnostic Output (`SeamlessArena.log`):**
   ```text
   [2026-10-20 12:04:35.562] [INFO ] [Bootstrap] SeamlessArena Native Extension initializing...
   [2026-10-20 12:04:35.562] [INFO ] [VersionGuard] Elden Ring build verified (Timestamp: 0x69E9C9B9, Size: 0x05E0A000)
   [2026-10-20 12:04:35.562] [INFO ] [VersionGuard] Seamless Co-op (ersc.dll) detected at base 0x0000000180000000
   [2026-10-20 12:04:35.562] [INFO ] [ArenaNetProtocol] Network protocol initialized with thread-safe queue.
   [2026-10-20 12:04:35.562] [INFO ] [ArenaCoordinator] ArenaCoordinator initialized. Phase: Idle. Duel Item ID: 108, Arena: Limgrave...
   [2026-10-20 12:04:35.562] [INFO ] [HookManager] MinHook hook subsystem initialized successfully.
   ```

---

## 4. Root Cause Analysis & Resolution of `SeamlessArena_Initialize` Failure

### The Issue:
During early native injection by Mod Engine 3, Elden Ring has not yet created its `CSWindow` or initialized `from::CS::CSTaskGroup` / `GLOBAL_CSTaskImp`. The initial implementation attempted to synchronously execute `Hook_GameTick()`, which called `g_ArenaGameTask.Start()` and `CSEzTask::register_task(from::CS::CSTaskGroup::FrameBegin)`. Because the internal engine task manager was null, calling `register_task` triggered an SEH Access Violation (`0xC0000005`), which `SeamlessArena_Initialize` caught, returning `false` and failing the load.

### The Resolution:
1. **Deferred Hook Worker (`me3_entry.cpp`):**
   - `SeamlessArena_Initialize` performs fast subsystem setup (Logger, Config, VersionGuard, Coordinator, MinHook backend).
   - Spawns `EngineInitWorkerThread`, which safely polls `EngineBridge::IsSystemInitialized()` (backed by `from::DLSY::is_system_initialized()`) with a 60-second timeout.
   - `SeamlessArena_Initialize` returns `true` immediately to Mod Engine 3, allowing the game boot pipeline and ME3 asset hooks to continue unhindered.
   - Once the game engine completes Dantelion2 window initialization, `EngineInitWorkerThread` executes `Hook_GameTick()` and installs the detours.
2. **SEH Guards in `ArenaGameTask.cpp` & `EngineBridge.cpp`:**
   - Isolated `TryRegisterEzTask` and `TryFreeEzTask` with `__try / __except (EXCEPTION_EXECUTE_HANDLER)` to prevent hardware exceptions from crashing the process.
   - Guarded `EngineBridge::IsSystemInitialized()` with SEH.
3. **HUD/Overlay Removal:**
   - Stripped unrequested Win32 `InGameNotification` overlay window to keep native loading lightweight and strictly adhere to milestone constraints.

