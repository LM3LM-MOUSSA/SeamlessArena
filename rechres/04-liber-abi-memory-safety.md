# 04 — libER ABI, Memory Safety & Engine Interop

## 1. Overview of libER

**libER** (authored by Dasaav) is the standard open-source C++ library providing reverse-engineered type definitions, singletons, virtual method tables, and memory-safe abstractions for Elden Ring's internal FromSoftware engine (Dantelion / FD4).

Using libER allows SeamlessArena to interact with the game engine using strongly-typed C++ structs rather than relying on brittle raw byte offsets.

---

## 2. Engine Subsystems and Key Singletons

| Singleton / Subsystem | Purpose in SeamlessArena | Safety Considerations |
|---|---|---|
| `CS::WorldChrMan` | Manages player and NPC character instances (`CS::PlayerIns`, `CS::ChrIns`). | Pointers invalidate upon map unloading or fast travel. Never cache raw `PlayerIns*` across frames. |
| `CS::CSMenuMan` | Handles UI rendering, HUD elements, banners ("VICTORY", "DEFEAT"). | Must only be queried and mutated from the **Main Render/Update Thread**. |
| `CS::FieldArea` | Manages active map sectors, block IDs, and scene transitions. | Reads must be guarded when transitions are in progress (`FieldArea::IsLoading()`). |
| `CS::EventFlagMan` | Queries and sets game event flags. | Read-only usage in arena to check unlock status. Avoid dirtying vanilla boss/quest flags. |
| `CS::SoloParamRepository` | Accesses game parameters (SpEffect, EquipParam, Bullet). | Static read-only table in memory. Safe across threads for reads. |

---

## 3. Thread Affinity & Concurrency Model

Elden Ring uses a multi-threaded task scheduler (Havok worker threads, rendering thread, physics thread, and main game logic thread).

```text
+-------------------------------------------------------------+
|                     Main Game Thread                        |
|  - WorldChrMan updates                                      |
|  - Health checks & death events                             |
|  - SeamlessArena Match Coordinator Tick                     |
|  - Scene warp dispatches                                    |
+-------------------------------------------------------------+
                              ^
                              | (Thread-safe Message Queue)
                              v
+-------------------------------------------------------------+
|                   Network / Worker Thread                   |
|  - Steam socket packet reception                            |
|  - Peer-to-peer arena message deserialization               |
|  - Asynchronous telemetry & logging                         |
+-------------------------------------------------------------+
```

### 3.1. Rules for Concurrency
1. **Never mutate game state from worker threads:** If an arena packet (e.g., `START_MATCH`) arrives over the network on a background thread, queue an action to execute on the next frame inside the game thread hook.
2. **Handle Invalidation on Loading Screens:** When a loading screen starts:
   - Clear all local `PlayerIns*` pointers.
   - Pause match countdown/scoring.
   - Re-acquire player entities once the new map block is confirmed fully loaded.

---

## 4. libER Toolchain and Build Configuration

To ensure binary compatibility between libER and SeamlessArena:
- **Platform:** `x64`
- **C++ Standard:** C++20 (`/std:c++20`)
- **Toolset:** Visual Studio 2026 (MSVC v145 or v143 toolset)
- **Runtime Library:** `/MT` (Release) or `/MTd` (Debug) matching across all projects.
- **RTTI & Exceptions:** C++ Exceptions enabled (`/EHsc`), RTTI enabled (`/GR`).

---

## 5. Memory Safety & Fail-Closed Symbol Guard

If Elden Ring updates, memory patterns or symbol addresses in libER might resolve to incorrect addresses. SeamlessArena prevents access violations with an address validation guard:

```cpp
bool ValidateEnginePointers() {
    auto worldChrMan = CS::WorldChrMan::instance();
    if (!worldChrMan) {
        Logger::Warn("WorldChrMan singleton not yet initialized.");
        return false;
    }

    // Verify pointer falls within game heap/module memory space
    if (IsBadReadPtr(worldChrMan, sizeof(void*))) {
        Logger::Error("WorldChrMan points to invalid memory! Aborting hooks.");
        return false;
    }

    return true;
}
```
