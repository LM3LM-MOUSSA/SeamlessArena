#pragma once
// =============================================================================
// ArenaCoordinator.h — Central arena flow controller (game-side orchestrator)
// =============================================================================
// RESEARCH: rechres/06-pvp-match-rules-state-machine.md (ALL sections)
// RESEARCH: rechres/07-multiplayer-authority-sync.md    (ALL sections)
// RESEARCH: rechres/05-colosseum-map-investigation.md   (Sections 3, 4)
// RESEARCH: rechres/08-game-events-hooks.md             (Section 3)
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
//
// The ArenaCoordinator is the bridge between game hooks (HookManager) and the
// pure rules engine (MatchRules in SeamlessArena.Core). It owns:
//   - The MatchRules instance
//   - The ArenaNetProtocol instance (network messages)
//   - The lobby/countdown/results state extensions beyond MatchRules
//   - Emergency recovery logic
//
// All public methods are called from the GAME THREAD only (via Hook_GameTick).
// Network callbacks queue actions into a thread-safe command queue.
// =============================================================================

#include <SeamlessArena/Core/MatchRules.h>
#include <cstdint>
#include <optional>
#include <string>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Colosseum identifiers
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 2)
    // -------------------------------------------------------------------------
    enum class ColosseumId : std::uint8_t
    {
        Royal    = 0,  // m45_00_00_00
        Caelid   = 1,  // m45_01_00_00
        Limgrave = 2,  // m45_02_00_00
    };

    // -------------------------------------------------------------------------
    // Extended arena phase (wraps MatchPhase with pre/post states)
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3)
    // -------------------------------------------------------------------------
    enum class ArenaPhase : std::uint8_t
    {
        Idle,           // No arena activity; normal PvE Seamless session
        Lobby,          // Host selecting mode/arena; peers joining
        ReadyCheck,     // All players must confirm readiness
        Countdown,      // Players frozen at spawn; 3-2-1-FIGHT banner
        Active,         // Combat in progress (delegates to MatchRules)
        Complete,       // Win condition met; scores being displayed
        Results,        // Scoreboard shown for 5-10 seconds
        Cleanup,        // Warping back to overworld, restoring consumables
        Cancelled,      // Aborted due to disconnect/error
    };

    // -------------------------------------------------------------------------
    // Lobby configuration set by the host before match start
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 2)
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1.1)
    // -------------------------------------------------------------------------
    struct LobbySettings
    {
        seamless_arena::core::MatchMode mode = seamless_arena::core::MatchMode::Duel;
        ColosseumId arena = ColosseumId::Limgrave;
        std::uint32_t durationSeconds = 300;
        std::uint16_t scoreLimit = 0;
        std::uint8_t  maxCrimsonFlasks = 0;  // Duel: 0; Ordeal/United: 1-3
        std::uint8_t  maxCeruleanFlasks = 3;
        bool autoPhysick = true;
        bool allowSpiritAshes = false;
    };

    // =========================================================================
    // ArenaCoordinator — singleton orchestrator
    // =========================================================================
    class ArenaCoordinator
    {
    public:
        // -- Lifecycle --------------------------------------------------------

        // RESEARCH: rechres/03-me3-native-contract.md (Section 4.1)
        // Called once from SeamlessArena_Initialize after version guard passes
        bool Initialize();

        // RESEARCH: rechres/03-me3-native-contract.md (Section 3 — Finalize)
        // Called from SeamlessArena_Finalize
        void Shutdown();

        // -- Game thread tick --------------------------------------------------
        // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 4)
        // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 4.2 — heartbeat)
        //
        // PSEUDO:
        //   1. Dequeue all pending network commands
        //   2. Switch on current ArenaPhase:
        //        Idle:       check for lobby creation request
        //        Lobby:      wait for all players
        //        ReadyCheck: check all readied; timeout -> back to Lobby
        //        Countdown:  decrement countdown timer; at 0 -> Active
        //        Active:     MatchRules.AdvanceTime(); broadcast score sync
        //        Complete:   start results timer
        //        Results:    when timer done -> transition to Cleanup
        //        Cleanup:    warp party to overworld; restore consumables
        //   3. Send periodic MSG_SCORE_SYNC if host (rechres/07 Section 4.2)
        void Tick(float deltaTimeSeconds);

        // -- Hook callbacks (called from HookManager detours) -----------------

        // RESEARCH: rechres/08-game-events-hooks.md (Section 3)
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3.1 states 5-6)
        //
        // PSEUDO:
        //   If phase != Active: ignore
        //   Record death in MatchRules.RecordDeath(victim, killer)
        //   If Duel and match complete: set phase = Complete, show winner banner
        //   If Ordeal/United: queue respawn for victim after delay
        void OnPlayerDeath(std::uint64_t victimSteamId, std::uint64_t killerSteamId);

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4.1)
        // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3.1 rule 2)
        //
        // PSEUDO:
        //   Clear all cached PlayerIns* pointers
        //   Pause match timer
        void OnLoadingScreenStart();

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4)
        //
        // PSEUDO:
        //   Re-acquire PlayerIns* pointers from WorldChrMan
        //   If phase == Countdown: begin 3-second countdown
        //   If phase == Cleanup returning to overworld: finalize cleanup
        void OnArenaSceneLoaded(std::uint32_t mapId);
        void OnOverworldLoaded();

        // -- Lobby actions (host only) ----------------------------------------
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3.1 states 1-2)
        // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 1)

        // PSEUDO:
        //   Validate host authority
        //   Set lobby settings
        //   Broadcast MSG_LOBBY_STATE to all peers
        bool CreateLobby(const LobbySettings& settings);

        // PSEUDO:
        //   Validate all players joined and readied
        //   Initiate warp to Colosseum map (rechres/05 Section 4.1)
        //   Transition to Countdown phase
        bool StartMatch();

        // PSEUDO:
        //   Set phase = Cancelled
        //   Broadcast MSG_ABORT_MATCH
        //   Trigger emergency recovery warp (rechres/09 Section 3)
        void AbortMatch();

        // -- Player actions (any peer) ----------------------------------------
        // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 2.1 opcode 0x02)
        //
        // PSEUDO:
        //   Send MSG_PLAYER_READY to host
        void ToggleReady();

        // -- 1v1 Duel Participant & Ready State ------------------------------
        void SetLocalPlayerId(std::uint64_t steamId);
        void SetRemotePlayerId(std::uint64_t steamId);
        void SetPlayerReady(std::uint64_t steamId, bool ready);
        [[nodiscard]] bool AreBothPlayersReady() const noexcept;

        // -- Queries ----------------------------------------------------------

        [[nodiscard]] bool IsMatchActive() const;
        [[nodiscard]] bool ShouldSuppressVanillaDeath() const;
        [[nodiscard]] ArenaPhase GetPhase() const;
        [[nodiscard]] seamless_arena::core::MatchSnapshot GetMatchSnapshot() const;

    private:
        // -- Internal helpers -------------------------------------------------

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 2)
        //
        // PSEUDO: Convert ColosseumId to map ID uint32
        //   Royal    -> 0x45000000 (m45_00_00_00)
        //   Caelid   -> 0x45010000 (m45_01_00_00)
        //   Limgrave -> 0x45020000 (m45_02_00_00)
        static std::uint32_t ColosseumToMapId(ColosseumId id);

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 2)
        static bool IsColosseumMapId(std::uint32_t mapId);

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 3)
        // RESEARCH: rechres/08-game-events-hooks.md (Section 4)
        //
        // PSEUDO:
        //   Teleport player to spawn coordinates for their slot/team
        //   Set facing direction based on arena layout
        void TeleportPlayerToSpawn(std::uint64_t steamId, std::uint8_t spawnSlot);

        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 4)
        //
        // PSEUDO:
        //   Apply SpEffect invulnerability for spawn_invulnerability_seconds
        //   Revive character in-place at spawn point
        //   Restore HP/FP to max
        void RespawnPlayer(std::uint64_t steamId);

        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 4)
        // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1.1 — restore_consumables)
        //
        // PSEUDO:
        //   Revert all consumable counts to pre-match snapshot
        //   Restore Crimson/Cerulean flask charges
        //   Remove arena-specific SpEffects
        void RestoreConsumables();

        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4.2)
        //
        // PSEUDO:
        //   Warp party back to last Site of Grace or Roundtable Hold
        void WarpPartyToOverworld();

        // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
        //
        // PSEUDO:
        //   Force-warp all players out of arena
        //   Reset MatchRules to Idle
        //   Log emergency event
        void EmergencyRecovery(const char* reason);

        // -- 1v1 Duel State --------------------------------------------------
        std::uint64_t hostSteamId_{ 0 };
        std::uint64_t guestSteamId_{ 0 };
        bool hostReady_{ false };
        bool guestReady_{ false };
        bool deathObserved_{ false };

        // -- State ------------------------------------------------------------
        ArenaPhase phase_ = ArenaPhase::Idle;
        LobbySettings lobbySettings_{};
        seamless_arena::core::MatchRules matchRules_{};

        float countdownTimer_  = 0.0f;
        float resultsTimer_    = 0.0f;
        float syncBroadcastAccumulator_ = 0.0f;

        // TODO(rechres/07): Network protocol instance
        // ArenaNetProtocol netProtocol_;

        // TODO(rechres/04 Section 3.1 rule 2): Cached entity pointers
        //   Clear these on every loading screen
        // void* cachedPlayerEntities_[6] = {};
    };

    // -------------------------------------------------------------------------
    // Global singleton instance
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 2)
    //   Lifetime: created in SeamlessArena_Initialize, destroyed in Finalize
    // -------------------------------------------------------------------------
    extern ArenaCoordinator g_ArenaCoordinator;

} // namespace seamless_arena::native
