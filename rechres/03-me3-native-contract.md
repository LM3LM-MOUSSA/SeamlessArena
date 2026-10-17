# 03 — Mod Engine 3 (`me3`) Native DLL Contract

## 1. Overview

**Mod Engine 3 (`me3`)** provides a modern, declarative profile-based runtime loader for FromSoftware games. It intercepts Windows loader calls and orchestrates the loading of native DLLs, file replacements, and memory patches.

---

## 2. Profile Configuration Schema

Mod Engine 3 profiles are written in TOML (`*.me3`). The root table defines game information, file system mounts, and the `[[natives]]` list.

### 2.1. Complete Production Profile Example (`SeamlessArena.me3`)

```toml
[profile]
name = "SeamlessArena"
game = "eldenring"

# 1. Early-loaded Seamless Co-op native extension
[[natives]]
path = "SeamlessCoop/ersc.dll"
load_early = true

# 2. SeamlessArena native extension
[[natives]]
path = "bin/Release/SeamlessArena.Native.dll"
load_early = false
initializer = "SeamlessArena_Initialize"
finalizer = "SeamlessArena_Finalize"
```

### 2.2. Schema Field Specifications

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | String | Yes | Path to DLL relative to the directory containing the `.me3` file. |
| `load_early` | Boolean | No (default `false`) | If `true`, loaded before main game initialization via `modengine_ext_init`. Required for `ersc.dll`. |
| `initializer` | String | No | Name of exported C-ABI function to invoke immediately after `LoadLibrary`. |
| `finalizer` | String | No | Name of exported C-ABI function to invoke prior to process termination. |
| `enabled` | Boolean | No (default `true`) | Allows quick toggling of specific natives without deletion. |

---

## 3. ABI and Exported Function Signatures

ME3 invokes initializers using standard `__cdecl` C-linkage (`extern "C"`).

```cpp
// In src/SeamlessArena.Native/me3_entry.cpp
#include <windows.h>

#define SEAMLESS_ARENA_EXPORT extern "C" __declspec(dllexport)

SEAMLESS_ARENA_EXPORT bool __cdecl SeamlessArena_Initialize() {
    // 1. Initialize logging subsystem
    // 2. Perform fail-closed game version check
    // 3. Detect Seamless Co-op presence
    // 4. Register engine hooks when game reaches steady state
    return true;
}

SEAMLESS_ARENA_EXPORT void __cdecl SeamlessArena_Finalize() {
    // 1. Unhook all detours (MinHook unhook)
    // 2. Restore modified engine tables
    // 3. Flush diagnostic log buffer
}
```

---

## 4. Execution Timing & Dependencies

### 4.1. Lifecycle Timing
1. **Process Spawn:** Steam launches `eldenring.exe` (suspended by ME3).
2. **Early Native Ingestion:** ME3 loads natives flagged with `load_early = true` (`ersc.dll`). `modengine_ext_init` is invoked.
3. **Standard Native Ingestion:** ME3 loads `SeamlessArena.Native.dll`.
4. **Initializer Invocation:** ME3 resolves `SeamlessArena_Initialize` via `GetProcAddress` and executes it.
5. **Game Engine Bootstrap:** FromSoftware engine initializes Direct3D, Havok, WorldChrMan, and Steam networking.
6. **Hook Attachment:** SeamlessArena waits until the engine singletons are populated before attaching runtime detours.

### 4.2. MSVC Runtime Library Strategy (`/MD` vs `/MT`)
- **Recommendation:** Use `/MT` (Multi-threaded Static CRT) or ensure Visual C++ Redistributable (2026/latest) is installed.
- **Why `/MT` is preferred for end-user distribution:** It eliminates `VCRUNTIME140.dll` or `MSVCP140.dll` missing-dependency crashes on client machines.
- **Why `/MD` is useful for development:** Faster linking and shared CRT heap if interacting directly with dynamically linked CRT modules.

---

## 5. Diagnostics, Logs, and Crash Reporting

- **ME3 Log Location:** `%APPDATA%\ModEngine\logs\` or the launch folder's `modengine.log`.
- **SeamlessArena Log Location:** `SeamlessArena.log` located adjacent to the game executable or in `logs/SeamlessArena.log`.
- **Unhandled Exceptions:** Initializer wraps entry points in `__try ... __except` structured exception handling (SEH) to log stack traces without silently terminating the game process.
