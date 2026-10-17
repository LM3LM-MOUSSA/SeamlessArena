// =============================================================================
// me3_entry.cpp — Mod Engine 3 native DLL entry point and lifecycle
// =============================================================================
// RESEARCH: rechres/03-me3-native-contract.md             (ALL sections)
// RESEARCH: rechres/00-compatibility-target.md           (Section 4 — fail-closed gate)
// RESEARCH: rechres/02-seamless-coop-integration.md      (Section 3.1 — load order)
// RESEARCH: rechres/08-game-events-hooks.md              (Section 1 — hook attach)
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2 — diagnostic log)
// RESEARCH: docs/TODO.md                                 (Phase 1 Items 2, 3, 5)
// =============================================================================

#include "ArenaCoordinator.h"
#include "ConfigLoader.h"
#include "HookManager.h"
#include "Logger.h"
#include "VersionGuard.h"

#include <Windows.h>

#define SEAMLESS_ARENA_EXPORT extern "C" __declspec(dllexport)

namespace
{
    // -------------------------------------------------------------------------
    // Explicit runtime bootstrap state
    // RESEARCH: docs/TODO.md (Phase 1 Item 2)
    // -------------------------------------------------------------------------
    enum class BootstrapState
    {
        Disabled, // Unsupported executable PE signature — zero hooks, inert
        SafeIdle, // Seamless absent or detours unverified — safe idle, no MP hooks
        Ready     // Fully validated environment with verified detours
    };

    BootstrapState g_state = BootstrapState::Disabled;

    bool InitializeInternal()
    {
        // ---------------------------------------------------------------------
        // STEP 1: Diagnostic Logging Subsystem
        // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2)
        // RESEARCH: docs/TODO.md (Phase 1 Item 7)
        // ---------------------------------------------------------------------
        if (!seamless_arena::native::Logger_Init("SeamlessArena.log", seamless_arena::native::LogLevel::Debug))
        {
            OutputDebugStringA("[SeamlessArena] Warning: Failed to open SeamlessArena.log; continuing.\n");
        }

        SA_LOG_INFO("Bootstrap", "=================================================");
        SA_LOG_INFO("Bootstrap", "SeamlessArena Native Extension initializing...");
        SA_LOG_INFO("Bootstrap", "Target: Elden Ring + Seamless Co-op (ME3 Profile)");
        SA_LOG_INFO("Bootstrap", "=================================================");

        // ---------------------------------------------------------------------
        // STEP 2: Configuration Ingestion
        // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
        // ---------------------------------------------------------------------
        auto config = seamless_arena::native::LoadConfig("SeamlessArena.toml");
        (void)config;
        SA_LOG_INFO("Bootstrap", "Configuration schema processed.");

        // ---------------------------------------------------------------------
        // STEP 3: Fail-Closed Version & Environment Verification Gate
        // RESEARCH: rechres/00-compatibility-target.md (Sections 3, 4)
        // RESEARCH: docs/TODO.md (Phase 1 Item 2)
        // ---------------------------------------------------------------------
        auto verResult = seamless_arena::native::VerifyEnvironment();
        if (!verResult.eldenRingOk)
        {
            // CRITICAL: Stop before coordinator or hook initialization!
            g_state = BootstrapState::Disabled;
            SA_LOG_FATAL("Bootstrap",
                "State: DISABLED. Executable PE signature does not match target Elden Ring build! "
                "Fail-closed policy active: stopping before coordinator or hooks are attached.");
            return true; // Return true to ME3 to avoid crashing the game process, but remain completely inert
        }

        // ---------------------------------------------------------------------
        // STEP 4: Seamless Co-op Presence Check
        // RESEARCH: rechres/02-seamless-coop-integration.md (Section 3.1)
        // ---------------------------------------------------------------------
        if (!verResult.seamlessCoopOk)
        {
            SA_LOG_WARN("Bootstrap",
                "Seamless Co-op (ersc.dll) not detected in process memory. "
                "Entering Safe Idle mode; multiplayer party features disabled.");

            if (!seamless_arena::native::g_ArenaCoordinator.Initialize())
            {
                SA_LOG_ERROR("Bootstrap", "Failed to initialize ArenaCoordinator in safe idle mode.");
                g_state = BootstrapState::Disabled;
                return false;
            }

            g_state = BootstrapState::SafeIdle;
            SA_LOG_INFO("Bootstrap", "State: SAFE IDLE. Running in offline/idle mode without hooks.");
            return true;
        }

        // ---------------------------------------------------------------------
        // STEP 5: Arena Coordinator Startup
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3)
        // ---------------------------------------------------------------------
        if (!seamless_arena::native::g_ArenaCoordinator.Initialize())
        {
            SA_LOG_ERROR("Bootstrap", "Failed to initialize ArenaCoordinator subsystem.");
            g_state = BootstrapState::Disabled;
            return false;
        }

        // ---------------------------------------------------------------------
        // STEP 6: Hook Subsystem Installation (Transactional)
        // RESEARCH: rechres/08-game-events-hooks.md (Sections 1, 2)
        // RESEARCH: docs/TODO.md (Phase 1 Item 3, 4)
        // ---------------------------------------------------------------------
        if (!seamless_arena::native::HookManager_Init())
        {
            SA_LOG_ERROR("Bootstrap", "HookManager backend initialization failed.");
            seamless_arena::native::g_ArenaCoordinator.Shutdown();
            g_state = BootstrapState::Disabled;
            return false;
        }

        // Attempt detour registration for each monitored event
        bool deathHookOk  = seamless_arena::native::Hook_PlayerDeath();
        bool damageHookOk = seamless_arena::native::Hook_DamageAttribution();
        bool mapHookOk    = seamless_arena::native::Hook_MapTransition();
        bool tickHookOk   = seamless_arena::native::Hook_GameTick();

        if (!deathHookOk || !damageHookOk || !mapHookOk || !tickHookOk)
        {
            // Hooks are currently stubs awaiting verified IDA/Ghidra byte signatures (rechres/08).
            // Do NOT leave partial or unverified hooks enabled!
            SA_LOG_WARN("Bootstrap",
                "One or more gameplay hooks are stubs or unverified on this build. "
                "Unrolling detours to avoid corrupting memory. Operating in Safe Idle mode.");

            seamless_arena::native::HookManager_Shutdown();
            g_state = BootstrapState::SafeIdle;
            SA_LOG_INFO("Bootstrap", "State: SAFE IDLE. Coordinator active, zero hooks attached.");
            return true;
        }

        g_state = BootstrapState::Ready;
        SA_LOG_INFO("Bootstrap", "State: READY. SeamlessArena initialization successful.");
        return true;
    }

    void FinalizeInternal()
    {
        SA_LOG_INFO("Shutdown", "SeamlessArena finalizer invoked by Mod Engine 3.");

        // Deterministic reverse-order shutdown
        // RESEARCH: docs/TODO.md (Phase 1 Item 5)
        if (g_state == BootstrapState::Ready)
        {
            seamless_arena::native::HookManager_Shutdown();
            seamless_arena::native::g_ArenaCoordinator.Shutdown();
        }
        else if (g_state == BootstrapState::SafeIdle)
        {
            seamless_arena::native::g_ArenaCoordinator.Shutdown();
        }

        g_state = BootstrapState::Disabled;
        SA_LOG_INFO("Shutdown", "SeamlessArena cleanup complete. Closing diagnostic logger.");
        seamless_arena::native::Logger_Shutdown();
    }
} // anonymous namespace

// =============================================================================
// Exported C ABI Functions for Mod Engine 3 (me3)
// RESEARCH: rechres/03-me3-native-contract.md (Section 3)
// =============================================================================

SEAMLESS_ARENA_EXPORT bool __cdecl SeamlessArena_Initialize() noexcept
{
    __try
    {
        return InitializeInternal();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        OutputDebugStringA("[SeamlessArena] Critical: Structured exception during SeamlessArena_Initialize!\n");
        return false;
    }
}

SEAMLESS_ARENA_EXPORT void __cdecl SeamlessArena_Finalize() noexcept
{
    __try
    {
        FinalizeInternal();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        OutputDebugStringA("[SeamlessArena] Critical: Structured exception during SeamlessArena_Finalize!\n");
    }
}
