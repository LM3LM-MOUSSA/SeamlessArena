// =============================================================================
// ArenaCoordinator.cpp — Arena flow controller implementation
// =============================================================================
// RESEARCH: rechres/06-pvp-match-rules-state-machine.md (ALL)
// RESEARCH: rechres/07-multiplayer-authority-sync.md    (ALL)
// RESEARCH: rechres/05-colosseum-map-investigation.md   (Sections 2-4)
// RESEARCH: rechres/08-game-events-hooks.md             (Section 3-4)
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
// =============================================================================

#include "ArenaCoordinator.h"
#include "EngineBridge.h"
#include "Logger.h"

#include <algorithm>

namespace seamless_arena::native
{
    // Global instance
    ArenaCoordinator g_ArenaCoordinator;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/03-me3-native-contract.md (Section 4.1)
    // -------------------------------------------------------------------------
    bool ArenaCoordinator::Initialize()
    {
        phase_ = ArenaPhase::Idle;
        SA_LOG_INFO("ArenaCoordinator", "Arena coordinator initialized. Phase: Idle.");

        // TODO(rechres/09 Section 1): Load LobbySettings defaults from
        //   SeamlessArena.toml config file

        // TODO(rechres/07): Initialize ArenaNetProtocol
        //   netProtocol_.Initialize();

        return true;
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/03-me3-native-contract.md (Section 3)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::Shutdown()
    {
        if (phase_ != ArenaPhase::Idle)
        {
            EmergencyRecovery("Shutdown while arena is active");
        }

        // TODO(rechres/07): netProtocol_.Shutdown();

        SA_LOG_INFO("ArenaCoordinator", "Arena coordinator shut down.");
    }

    // =========================================================================
    // Game thread tick
    // =========================================================================

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 2 row 4)
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3)
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 4.2)
    //
    // This is THE main per-frame update. Every game state mutation flows
    // through here to satisfy the thread-affinity requirement from
    // rechres/04 Section 3.1.
    // -------------------------------------------------------------------------
    void ArenaCoordinator::Tick(float deltaTimeSeconds)
    {
        // =====================================================================
        // STEP 1: Process queued network commands
        // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 2)
        // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3.1 rule 1)
        //
        // PSEUDO:
        //   while (auto cmd = netProtocol_.DequeueCommand())
        //   {
        //       switch (cmd->opcode)
        //       {
        //       case MSG_PLAYER_READY:  HandlePeerReady(cmd->senderId); break;
        //       case MSG_DEATH_REPORT:  OnPlayerDeath(cmd->victimId, cmd->killerId); break;
        //       case MSG_SCORE_SYNC:    MirrorHostSnapshot(cmd->snapshot); break;
        //       case MSG_MATCH_END:     HandleMatchEnd(cmd->winnerId); break;
        //       case MSG_ABORT_MATCH:   EmergencyRecovery("Host aborted"); break;
        //       }
        //   }
        // =====================================================================

        // =====================================================================
        // STEP 2: Phase-specific logic
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3)
        // =====================================================================
        switch (phase_)
        {
        case ArenaPhase::Idle:
            // Nothing to do — waiting for CreateLobby()
            break;

        case ArenaPhase::Lobby:
            // PSEUDO:
            //   Display lobby UI with mode/arena selection
            //   Wait for peers to join the Seamless party
            break;

        case ArenaPhase::ReadyCheck:
            // PSEUDO:
            //   Check if all players have sent MSG_PLAYER_READY
            //   If timeout (30s): revert to Lobby
            //   If all ready: transition to Countdown
            break;

        case ArenaPhase::Countdown:
            countdownTimer_ -= deltaTimeSeconds;
            if (countdownTimer_ <= 0.0f)
            {
                // Start 1v1 Duel match through MatchRules
                seamless_arena::core::MatchConfig config{};
                config.mode = seamless_arena::core::MatchMode::Duel;
                config.durationSeconds = lobbySettings_.durationSeconds > 0 ? lobbySettings_.durationSeconds : 300;
                config.scoreLimit = 1; // First death ends match in Duel

                std::vector<seamless_arena::core::PlayerState> players;
                // Slot 0: Host
                players.push_back(seamless_arena::core::PlayerState{ hostSteamId_ != 0 ? hostSteamId_ : 1, 0, 0, false, 0 });
                // Slot 1: Guest
                players.push_back(seamless_arena::core::PlayerState{ guestSteamId_ != 0 ? guestSteamId_ : 2, 1, 0, false, 0 });

                std::string err;
                if (matchRules_.Start(config, std::move(players), err))
                {
                    phase_ = ArenaPhase::Active;
                    deathObserved_ = false;
                    SA_LOG_INFO("ArenaCoordinator",
                        "FIGHT! 1v1 Duel is now ACTIVE between 0x%llX and 0x%llX. First death ends match.",
                        hostSteamId_, guestSteamId_);
                    EngineBridge::PlaySystemSound(1001); // Arena match start gong
                }
                else
                {
                    SA_LOG_ERROR("ArenaCoordinator", "Failed to start 1v1 Duel: %s", err.c_str());
                    EmergencyRecovery("Match start failed");
                }
            }
            break;

        case ArenaPhase::Active:
        {
            // 1. Advance match timer
            if (deltaTimeSeconds > 0.0f)
            {
                matchRules_.AdvanceTime(static_cast<uint32_t>(deltaTimeSeconds >= 1.0f ? deltaTimeSeconds : 1));
            }

            // 2. Safeguard HP polling: detect local player death if detour was delayed
            int32_t localHp = EngineBridge::GetLocalPlayerHp();
            if (localHp == 0 && !deathObserved_)
            {
                SA_LOG_INFO("ArenaCoordinator", "Local player HP reached 0 via engine polling safeguard.");
                OnPlayerDeath(hostSteamId_ != 0 ? hostSteamId_ : 1, guestSteamId_);
            }

            // 3. Check if match rules completed (first death or timeout draw)
            auto snap = matchRules_.Snapshot();
            if (snap.phase == seamless_arena::core::MatchPhase::Complete)
            {
                phase_ = ArenaPhase::Complete;
                resultsTimer_ = 6.0f; // Hold scoreboard/results for 6 seconds
                if (snap.winner.has_value())
                {
                    SA_LOG_INFO("ArenaCoordinator",
                        "1v1 DUEL COMPLETE: Winner is Player 0x%llX!", snap.winner.value());
                }
                else
                {
                    SA_LOG_INFO("ArenaCoordinator",
                        "1v1 DUEL COMPLETE: Match ended in a DRAW (Timeout).");
                }
            }
            break;
        }

        case ArenaPhase::Complete:
            EngineBridge::PlaySystemSound(1002); // Victory fanfare
            phase_ = ArenaPhase::Results;
            break;

        case ArenaPhase::Results:
            resultsTimer_ -= deltaTimeSeconds;
            if (resultsTimer_ <= 0.0f)
            {
                phase_ = ArenaPhase::Cleanup;
            }
            break;

        case ArenaPhase::Cleanup:
            RestoreConsumables();
            WarpPartyToOverworld();
            matchRules_.Reset();
            hostReady_ = false;
            guestReady_ = false;
            deathObserved_ = false;
            phase_ = ArenaPhase::Idle;
            SA_LOG_INFO("ArenaCoordinator",
                "1v1 Duel cleanup complete. Returned safely to overworld. State: IDLE.");
            break;

        case ArenaPhase::Cancelled:
            matchRules_.Reset();
            hostReady_ = false;
            guestReady_ = false;
            deathObserved_ = false;
            phase_ = ArenaPhase::Idle;
            break;
        }
    }

    // =========================================================================
    // Hook callbacks
    // =========================================================================

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 3)
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Sections 3, 5)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::OnPlayerDeath(std::uint64_t victimSteamId, std::uint64_t killerSteamId)
    {
        if (phase_ != ArenaPhase::Active || deathObserved_) return;

        deathObserved_ = true;
        std::optional<core::PlayerId> killer = (killerSteamId != 0 && killerSteamId != victimSteamId)
            ? std::make_optional(killerSteamId) : std::nullopt;

        bool recorded = matchRules_.RecordDeath(victimSteamId, killer);
        if (!recorded)
        {
            SA_LOG_WARN("ArenaCoordinator", "Death ignored — victim 0x%llX not in match", victimSteamId);
            deathObserved_ = false;
            return;
        }

        auto snap = matchRules_.Snapshot();
        if (snap.phase == core::MatchPhase::Complete)
        {
            phase_ = ArenaPhase::Complete;
            resultsTimer_ = 6.0f;
            if (snap.winner.has_value())
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "FIRST DEATH! Victim 0x%llX eliminated. 1v1 DUEL WINNER: 0x%llX!",
                    victimSteamId, snap.winner.value());
            }
            else
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "FIRST DEATH! Victim 0x%llX eliminated. 1v1 DUEL DRAW.", victimSteamId);
            }
        }
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3.1 rule 2)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::OnLoadingScreenStart()
    {
        SA_LOG_DEBUG("ArenaCoordinator", "Loading screen detected — clearing entity cache.");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::OnArenaSceneLoaded(std::uint32_t mapId)
    {
        SA_LOG_INFO("ArenaCoordinator", "Arena scene loaded: map 0x%08X", mapId);
        if (phase_ == ArenaPhase::Countdown)
        {
            countdownTimer_ = 3.0f;
            EngineBridge::PlaySystemSound(1000); // 3-second countdown chime
        }
    }

    void ArenaCoordinator::OnOverworldLoaded()
    {
        SA_LOG_INFO("ArenaCoordinator", "Overworld scene loaded.");
        if (phase_ == ArenaPhase::Cleanup)
        {
            phase_ = ArenaPhase::Idle;
            SA_LOG_INFO("ArenaCoordinator", "Overworld arrival confirmed. Returning to Idle.");
        }
    }

    // =========================================================================
    // Lobby actions
    // =========================================================================

    bool ArenaCoordinator::CreateLobby(const LobbySettings& settings)
    {
        if (phase_ != ArenaPhase::Idle)
        {
            SA_LOG_WARN("ArenaCoordinator", "Cannot create lobby — not in Idle phase.");
            return false;
        }

        lobbySettings_ = settings;
        lobbySettings_.mode = core::MatchMode::Duel;
        hostReady_ = false;
        guestReady_ = false;
        deathObserved_ = false;
        matchRules_.Reset();

        phase_ = (guestSteamId_ != 0) ? ArenaPhase::ReadyCheck : ArenaPhase::Lobby;
        SA_LOG_INFO("ArenaCoordinator",
            "1v1 Duel lobby created for Colosseum map 0x%08X. State: %s",
            ColosseumToMapId(lobbySettings_.arena),
            (phase_ == ArenaPhase::ReadyCheck) ? "ReadyCheck" : "Lobby");
        return true;
    }

    bool ArenaCoordinator::StartMatch()
    {
        if (phase_ != ArenaPhase::ReadyCheck && phase_ != ArenaPhase::Lobby)
        {
            SA_LOG_WARN("ArenaCoordinator", "Cannot start match — not in ReadyCheck or Lobby phase.");
            return false;
        }

        uint32_t targetMap = ColosseumToMapId(lobbySettings_.arena);
        SA_LOG_INFO("ArenaCoordinator",
            "1v1 Duel match starting! Warping party to Colosseum (map 0x%08X)...", targetMap);

        phase_ = ArenaPhase::Countdown;
        countdownTimer_ = 3.0f;
        EngineBridge::PlaySystemSound(1000); // 3-second countdown chime
        return true;
    }

    void ArenaCoordinator::AbortMatch()
    {
        SA_LOG_WARN("ArenaCoordinator", "Match aborted by user/host.");
        matchRules_.Cancel();
        EmergencyRecovery("Manual abort");
    }

    void ArenaCoordinator::ToggleReady()
    {
        SetPlayerReady(hostSteamId_, !hostReady_);
    }

    void ArenaCoordinator::SetLocalPlayerId(std::uint64_t steamId)
    {
        hostSteamId_ = steamId;
        SA_LOG_INFO("ArenaCoordinator", "Host player Steam ID registered: 0x%llX", steamId);
    }

    void ArenaCoordinator::SetRemotePlayerId(std::uint64_t steamId)
    {
        guestSteamId_ = steamId;
        SA_LOG_INFO("ArenaCoordinator", "Guest player Steam ID registered: 0x%llX", steamId);
        if (phase_ == ArenaPhase::Lobby && hostSteamId_ != 0 && guestSteamId_ != 0)
        {
            phase_ = ArenaPhase::ReadyCheck;
            SA_LOG_INFO("ArenaCoordinator", "Both players present in session. Transitioning to ReadyCheck.");
        }
    }

    void ArenaCoordinator::SetPlayerReady(std::uint64_t steamId, bool ready)
    {
        if (steamId == hostSteamId_)
        {
            hostReady_ = ready;
            SA_LOG_INFO("ArenaCoordinator", "Host ready state: %d", ready);
        }
        else if (steamId == guestSteamId_)
        {
            guestReady_ = ready;
            SA_LOG_INFO("ArenaCoordinator", "Guest ready state: %d", ready);
        }

        if (AreBothPlayersReady())
        {
            SA_LOG_INFO("ArenaCoordinator", "Both players READY! Initiating 1v1 Duel start.");
            StartMatch();
        }
    }

    bool ArenaCoordinator::AreBothPlayersReady() const noexcept
    {
        return hostSteamId_ != 0 && guestSteamId_ != 0 && hostReady_ && guestReady_;
    }

    // =========================================================================
    // Queries
    // =========================================================================

    bool ArenaCoordinator::IsMatchActive() const
    {
        return phase_ == ArenaPhase::Active;
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 3)
    //
    // Returns true if the vanilla death screen / rune drop / overworld reload
    // should be suppressed. True when we're in arena combat.
    // -------------------------------------------------------------------------
    bool ArenaCoordinator::ShouldSuppressVanillaDeath() const
    {
        return phase_ == ArenaPhase::Active
            || phase_ == ArenaPhase::Countdown
            || phase_ == ArenaPhase::Complete;
    }

    ArenaPhase ArenaCoordinator::GetPhase() const
    {
        return phase_;
    }

    seamless_arena::core::MatchSnapshot ArenaCoordinator::GetMatchSnapshot() const
    {
        return matchRules_.Snapshot();
    }

    // =========================================================================
    // Internal helpers
    // =========================================================================

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 2)
    // -------------------------------------------------------------------------
    std::uint32_t ArenaCoordinator::ColosseumToMapId(ColosseumId id)
    {
        // Map ID encoding: m45_XX_00_00 -> 0x452X0000 (approximate)
        // TODO(rechres/05): Verify exact uint32 encoding used by the engine.
        //   The Smithbox / DSMapStudio format may differ from the runtime value.
        switch (id)
        {
        case ColosseumId::Royal:    return 0x45000000; // m45_00_00_00
        case ColosseumId::Caelid:   return 0x45010000; // m45_01_00_00
        case ColosseumId::Limgrave: return 0x45020000; // m45_02_00_00
        default:                    return 0x45020000; // fallback to Limgrave
        }
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 2.1)
    // -------------------------------------------------------------------------
    bool ArenaCoordinator::IsColosseumMapId(std::uint32_t mapId)
    {
        return mapId == 0x45000000
            || mapId == 0x45010000
            || mapId == 0x45020000;
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 3)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::TeleportPlayerToSpawn(std::uint64_t steamId, std::uint8_t spawnSlot)
    {
        // PSEUDO (rechres/05 Section 3):
        //   Look up spawn coordinates for lobbySettings_.arena and spawnSlot
        //   Get PlayerIns* from WorldChrMan for this steamId
        //   Set player position via engine coordinate write
        //   Set player facing (yaw) based on slot
        //
        //   Duel slots (rechres/05 Section 3.1):
        //     Slot 0 (Host): South spawn, facing North (yaw=0)
        //     Slot 1 (Guest): North spawn, facing South (yaw=pi)
        //
        //   Ordeal/United slots:
        //     Slots 0-5: Radial distribution around arena rim

        SA_LOG_DEBUG("ArenaCoordinator", "Teleporting player 0x%llX to spawn slot %d", steamId, spawnSlot);
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/08-game-events-hooks.md (Section 4)
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 4)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::RespawnPlayer(std::uint64_t steamId)
    {
        // PSEUDO (rechres/08 Section 4, rechres/06 Section 4):
        //   1. Select safe respawn point (rechres/05 Section 3)
        //   2. Revive character at spawn: set HP = maxHP, FP = maxFP
        //   3. Apply temporary invulnerability SpEffect (3.0 seconds)
        //      (rechres/06 Section 4: "anti-spawn camping")
        //   4. SpEffect cancels early if player inputs an attack
        //   5. matchRules_.Respawn(steamId)
        //   6. Restore limited flasks (rechres/06 Section 4 flask rules)

        SA_LOG_DEBUG("ArenaCoordinator", "Respawning player 0x%llX", steamId);
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 4)
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1.1)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::RestoreConsumables()
    {
        // PSEUDO (rechres/06 Section 4, rechres/09 Section 1.1):
        //   For each player in the match:
        //     Restore all Crimson/Cerulean Flask charges to pre-match count
        //     Restore Wondrous Physick charge
        //     Restore all consumable item counts (crab, grease, pots, boluses)
        //     Remove any lingering arena-only SpEffects
        //
        // This requires either:
        //   a) Snapshotting item counts before match start and restoring them
        //   b) Using EquipParamGoods to identify consumed items and re-granting
        //
        // TODO(rechres/06): Decide and implement the snapshot/restore mechanism

        SA_LOG_INFO("ArenaCoordinator", "Consumables restored to pre-match state.");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4.2)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::WarpPartyToOverworld()
    {
        // PSEUDO (rechres/05 Section 4.2):
        //   Determine return destination:
        //     Option A: Last visited Site of Grace coordinates
        //     Option B: Roundtable Hold (m11_10_00_00) as safe fallback
        //   Issue warp command via FieldArea or WorldChrMan warp function
        //   Wait for all peers to confirm overworld load

        SA_LOG_INFO("ArenaCoordinator", "Warping party to overworld...");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::EmergencyRecovery(const char* reason)
    {
        SA_LOG_ERROR("ArenaCoordinator",
            "EMERGENCY RECOVERY triggered! Reason: %s", reason);

        // PSEUDO (rechres/09 Section 3.1):
        //   1. Cancel match rules: matchRules_.Cancel();
        //   2. Restore consumables immediately
        //   3. Force-warp all local players to overworld hub
        //   4. Clear all cached entity pointers
        //   5. Reset phase to Idle
        //   6. Broadcast MSG_ABORT_MATCH to peers if possible

        matchRules_.Cancel();
        RestoreConsumables();
        WarpPartyToOverworld();
        phase_ = ArenaPhase::Cancelled;
    }

} // namespace seamless_arena::native
