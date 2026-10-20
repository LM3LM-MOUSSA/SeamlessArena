// =============================================================================
// HookManager.cpp — MinHook detour installation and engine event hooks
// =============================================================================
// RESEARCH: rechres/08-game-events-hooks.md        (ALL sections)
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Sections 2, 3, 5)
// =============================================================================

#include "HookManager.h"
#include "ArenaCoordinator.h"
#include "ArenaGameTask.h"
#include "EngineBridge.h"
#include "Logger.h"

#include <MinHook.h>
#include <Windows.h>
#include <cstring>

namespace seamless_arena::native
{
    namespace
    {
        bool g_minHookInitialized = false;

        bool g_deathHookInstalled = false;
        bool g_damageHookInstalled = false;
        bool g_mapHookInstalled = false;
        bool g_itemHookInstalled = false;
        bool g_gameTickInstalled = false;

        // -- Trampoline prototypes --------------------------------------------
        typedef void(__fastcall* tChrDeadEvent)(void* chrIns, void* killerChrIns);
        tChrDeadEvent oChrDeadEvent = nullptr;

        typedef void(__fastcall* tFieldAreaLoadMap)(void* fieldArea, uint32_t mapId);
        tFieldAreaLoadMap oFieldAreaLoadMap = nullptr;

        typedef void(__fastcall* tApplyDamage)(void* victim, void* attacker, void* dmgData);
        tApplyDamage oApplyDamage = nullptr;

        typedef bool(__fastcall* tPlayerUseGoods)(void* chrIns, uint32_t goodsId, void* unkData);
        tPlayerUseGoods oPlayerUseGoods = nullptr;

        // -- Detour Implementations -------------------------------------------

        bool __fastcall HookedPlayerUseGoods(void* chrIns, uint32_t goodsId, void* unkData)
        {
            const uint32_t duelItemId = g_ArenaCoordinator.GetDuelGoodsId();
            if (goodsId == duelItemId)
            {
                uint64_t playerSteamId = reinterpret_cast<uintptr_t>(chrIns);
                SA_LOG_INFO("ItemHook",
                    "Custom Duel Item used (Goods ID: %u) by entity 0x%llX.",
                    goodsId, playerSteamId);

                g_ArenaCoordinator.OnDuelItemUsed(playerSteamId, goodsId);
                return true;
            }

            if (oPlayerUseGoods)
            {
                return oPlayerUseGoods(chrIns, goodsId, unkData);
            }
            return false;
        }

        void __fastcall HookedChrDeadEvent(void* chrIns, void* killerChrIns)
        {
            if (g_ArenaCoordinator.IsMatchActive())
            {
                // In Elden Ring ChrIns: Steam ID / entity identifier can be resolved
                // or mapped to the active participant list.
                uint64_t victimId = reinterpret_cast<uintptr_t>(chrIns);
                uint64_t killerId = killerChrIns ? reinterpret_cast<uintptr_t>(killerChrIns) : 0;

                SA_LOG_INFO("DeathHook",
                    "ChrDeadEvent intercepted during active match: victim=0x%llX, killer=0x%llX",
                    victimId, killerId);

                g_ArenaCoordinator.OnPlayerDeath(victimId, killerId);

                if (g_ArenaCoordinator.ShouldSuppressVanillaDeath())
                {
                    // In Duel: suppress vanilla overworld reload screen and rune penalty
                    return;
                }
            }

            if (oChrDeadEvent)
            {
                oChrDeadEvent(chrIns, killerChrIns);
            }
        }

        void* g_cachedFieldArea = nullptr;

        void __fastcall HookedFieldAreaLoadMap(void* fieldArea, uint32_t mapId)
        {
            g_cachedFieldArea = fieldArea;
            SA_LOG_INFO("MapHook", "FieldArea::LoadMap intercepted for map ID 0x%08X", mapId);

            // Clear cached character pointers during map reload (rechres/04 Section 3.1)
            g_ArenaCoordinator.OnLoadingScreenStart();

            if (oFieldAreaLoadMap)
            {
                oFieldAreaLoadMap(fieldArea, mapId);
            }

            // Check if arriving at a vanilla Colosseum (m45_00_00_00, m45_01_00_00, m45_02_00_00)
            uint32_t mapMajor = (mapId >> 24) & 0xFF;
            if (mapMajor == 0x45 || mapId == 45000000 || mapId == 45010000 || mapId == 45020000)
            {
                g_ArenaCoordinator.OnArenaSceneLoaded(mapId);
            }
            else
            {
                g_ArenaCoordinator.OnOverworldLoaded();
            }
        }

        void __fastcall HookedApplyDamage(void* victim, void* attacker, void* dmgData)
        {
            if (g_ArenaCoordinator.IsMatchActive() && victim != nullptr && attacker != nullptr)
            {
                // Damage attribution tracking for kill credit
            }

            if (oApplyDamage)
            {
                oApplyDamage(victim, attacker, dmgData);
            }
        }

        // Helper to verify target prologue bytes before patching (fail-closed)
        bool VerifyPrologue(const void* target, const uint8_t* expectedBytes, size_t count)
        {
            if (!target || !expectedBytes || count == 0)
                return false;

            if (!EngineBridge::IsReadable(target, count))
            {
                SA_LOG_ERROR("HookManager", "Target memory at %p is not readable!", target);
                return false;
            }

            if (std::memcmp(target, expectedBytes, count) != 0)
            {
                SA_LOG_WARN("HookManager",
                    "Prologue mismatch at %p! Expected %zu bytes do not match memory.", target, count);
                return false;
            }

            return true;
        }

    } // anonymous namespace

    // =========================================================================
    // Public API
    // =========================================================================

    bool HookManager_Init()
    {
        if (g_minHookInitialized)
            return true;

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            SA_LOG_ERROR("HookManager", "MinHook initialization failed with error: %d", status);
            return false;
        }

        g_minHookInitialized = true;
        SA_LOG_INFO("HookManager", "MinHook hook subsystem initialized successfully.");
        return true;
    }

    void HookManager_Shutdown()
    {
        // 1. Remove native task from FromSoftware scheduler
        if (g_gameTickInstalled)
        {
            g_ArenaGameTask.Stop();
            g_gameTickInstalled = false;
        }

        // 2. Disable and unhook all MinHook detours
        if (g_minHookInitialized)
        {
            MH_STATUS disableStatus = MH_DisableHook(MH_ALL_HOOKS);
            if (disableStatus != MH_OK)
            {
                SA_LOG_WARN("HookManager", "MH_DisableHook returned status: %d", disableStatus);
            }

            MH_STATUS uninitStatus = MH_Uninitialize();
            if (uninitStatus != MH_OK)
            {
                SA_LOG_WARN("HookManager", "MH_Uninitialize returned status: %d", uninitStatus);
            }

            g_minHookInitialized = false;
        }

        g_deathHookInstalled = false;
        g_damageHookInstalled = false;
        g_mapHookInstalled = false;
        g_itemHookInstalled = false;

        oChrDeadEvent = nullptr;
        oFieldAreaLoadMap = nullptr;
        oApplyDamage = nullptr;
        oPlayerUseGoods = nullptr;
        g_cachedFieldArea = nullptr;

        SA_LOG_INFO("HookManager", "All hooks cleanly uninstalled and MinHook uninitialized.");
    }

    bool Hook_PlayerDeath()
    {
        if (!g_minHookInitialized)
        {
            SA_LOG_ERROR("HookManager", "Hook_PlayerDeath: MinHook not initialized!");
            return false;
        }

        // Fail-closed safety: In the absence of an exact pinned symbol offset for ChrDeadEvent,
        // we log and return false so me3_entry rolls back to Safe Idle instead of guessing memory.
        SA_LOG_WARN("HookManager",
            "Hook_PlayerDeath: Byte signature verification pending IDA/Ghidra confirmation (rechres/08). Fail-closed.");
        return false;
    }

    bool Hook_DamageAttribution()
    {
        if (!g_minHookInitialized)
        {
            SA_LOG_ERROR("HookManager", "Hook_DamageAttribution: MinHook not initialized!");
            return false;
        }

        SA_LOG_WARN("HookManager",
            "Hook_DamageAttribution: Byte signature verification pending IDA/Ghidra confirmation (rechres/08). Fail-closed.");
        return false;
    }

    bool Hook_MapTransition()
    {
        if (!g_minHookInitialized)
        {
            SA_LOG_ERROR("HookManager", "Hook_MapTransition: MinHook not initialized!");
            return false;
        }

        SA_LOG_WARN("HookManager",
            "Hook_MapTransition: Byte signature verification pending IDA/Ghidra confirmation (rechres/08). Fail-closed.");
        return false;
    }

    bool Hook_DuelItemUse()
    {
        if (!g_minHookInitialized)
        {
            SA_LOG_ERROR("HookManager", "Hook_DuelItemUse: MinHook not initialized!");
            return false;
        }

        SA_LOG_WARN("HookManager",
            "Hook_DuelItemUse: Byte signature verification pending IDA/Ghidra confirmation (rechres/12). Fail-closed.");
        return false;
    }

    bool Hook_GameTick()
    {
        // Use libER native FromSoftware task scheduler callback
        if (!g_ArenaGameTask.Start())
        {
            SA_LOG_WARN("HookManager",
                "Hook_GameTick: Engine task registration deferred (system not yet initialized).");
            return false;
        }

        g_gameTickInstalled = true;
        SA_LOG_INFO("HookManager", "Hook_GameTick: Successfully attached via FromSoftware CSEzTask.");
        return true;
    }

    bool HookManager_RequestMapLoad(uint32_t mapId)
    {
        if (g_cachedFieldArea && oFieldAreaLoadMap)
        {
            SA_LOG_INFO("HookManager", "Requesting FieldArea::LoadMap for map ID 0x%08X", mapId);
            oFieldAreaLoadMap(g_cachedFieldArea, mapId);
            return true;
        }
        return false;
    }

    void* HookManager_GetCachedFieldArea() noexcept
    {
        return g_cachedFieldArea;
    }

} // namespace seamless_arena::native
