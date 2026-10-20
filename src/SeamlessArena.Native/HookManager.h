#pragma once
// =============================================================================
// HookManager.h — MinHook-based detour installation and lifecycle
// =============================================================================
// RESEARCH: rechres/08-game-events-hooks.md     (ALL sections)
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Sections 2, 3)
//
// Centralizes all game memory detours. Each hook:
//   - Validates target instruction bytes before patching (fail-closed)
//   - Stores the original trampoline for clean unhooking
//   - Is removable via HookManager_Shutdown()
//
// Dependencies: MinHook (https://github.com/TsudaKageworthy/minhook)
// =============================================================================

#include <cstdint>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Initialize / Shutdown the hook subsystem
    // RESEARCH: rechres/08-game-events-hooks.md (Section 1 — design rules)
    // RESEARCH: rechres/03-me3-native-contract.md (Section 3 — Finalize)
    //
    // Init:   Call MH_Initialize(). Must succeed before any hook can be created.
    // Shutdown: Disable and remove ALL hooks, call MH_Uninitialize().
    //           Called from SeamlessArena_Finalize().
    // -------------------------------------------------------------------------
    bool HookManager_Init();
    void HookManager_Shutdown();

    // -------------------------------------------------------------------------
    // Install individual game hooks
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 — hook targets)
    //
    // Each function below scans for its target signature, verifies instruction
    // bytes, and creates a MinHook detour. Returns false if the signature is
    // not found or instruction bytes don't match (fail-closed).
    //
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 5)
    //   "If a single byte differs, abandon hook installation and log the failure."
    // -------------------------------------------------------------------------

    // -- Player death observation ---------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 1, Section 3)
    //
    // PSEUDO:
    //   1. Pattern scan for ChrDeadEvent signature in eldenring.exe
    //   2. Verify first N instruction bytes match expected pattern
    //   3. MH_CreateHook(target, &HookedChrDeadEvent, &oChrDeadEvent)
    //   4. MH_EnableHook(target)
    //   5. Return true on success
    //
    // The hooked function:
    //   - Checks if a SeamlessArena match is Active
    //   - Extracts victim/killer Steam IDs from ChrIns pointers
    //   - Calls g_ArenaCoordinator.OnPlayerDeath(victim, killer)
    //   - In Duel mode: may suppress vanilla death screen (rechres/08 Section 3)
    //   - Always calls original trampoline for non-arena deaths
    // -------------------------------------------------------------------------
    bool Hook_PlayerDeath();

    // -- Damage attribution ---------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 2)
    //
    // PSEUDO:
    //   1. Pattern scan for CalcDamage / ApplyDamage
    //   2. On damage application, if match is Active:
    //        record last-attacker for each player entity
    //   3. This feeds accurate kill credits to RecordDeath()
    //
    // TODO(rechres/08): Determine exact function name and signature via
    //   libER symbol tables or IDA/Ghidra analysis of target build.
    // -------------------------------------------------------------------------
    bool Hook_DamageAttribution();

    // -- Map scene loading notification ---------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 3)
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4.1)
    //
    // PSEUDO:
    //   1. Hook FieldArea::LoadMap or equivalent map transition callback
    //   2. On scene load complete, notify ArenaCoordinator:
    //        - If loading an arena map (m45_*): start countdown
    //        - If loading overworld: cleanup and return to Idle
    //   3. On loading screen START: clear cached PlayerIns* pointers
    //      (rechres/04 Section 3.1 rule 2)
    // -------------------------------------------------------------------------
    bool Hook_MapTransition();

    // -- Main game loop tick --------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 4)
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3 — thread model)
    //
    // PSEUDO:
    //   1. Hook the main TaskScheduler update or equivalent per-frame callback
    //   2. Each tick on the game thread:
    //        g_ArenaCoordinator.Tick(deltaTime)
    //        - Process queued network messages (rechres/07)
    //        - Advance match timer
    //        - Check for pending respawns
    //        - Broadcast score sync packets
    //
    // -- Custom in-game Duel Item use observation ----------------------------
    // RESEARCH: rechres/12-custom-item-to-native-hook.md (ALL sections)
    //
    // Intercepts player item usage for the configured custom Arena Duel item
    // (Goods ID from SeamlessArena.toml, default 220000). Triggers local Ready
    // and initiates 1v1 Duel lobby flow.
    // -------------------------------------------------------------------------
    bool Hook_DuelItemUse();

    bool Hook_GameTick();

    // Safely request map load via cached FieldArea if available
    bool HookManager_RequestMapLoad(uint32_t mapId);
    void* HookManager_GetCachedFieldArea() noexcept;

} // namespace seamless_arena::native
