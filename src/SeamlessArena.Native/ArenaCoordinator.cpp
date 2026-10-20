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
#include "ConfigLoader.h"
#include "EngineBridge.h"
#include "HookManager.h"
#include "InGameNotification.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>

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

        // Load configuration and apply Alpha defaults
        auto config = LoadConfig("SeamlessArena.toml");
        duelGoodsId_ = config.duelGoodsId;
        lobbySettings_ = ConfigToLobbyDefaults(config, core::MatchMode::Duel);

        // Alpha constraints: Limgrave Colosseum only, 0 Crimson flasks, 1v1 Duel
        lobbySettings_.mode = core::MatchMode::Duel;
        lobbySettings_.arena = ColosseumId::Limgrave;
        lobbySettings_.maxCrimsonFlasks = 0;
        lobbySettings_.maxCeruleanFlasks = config.duelMaxCerulean;

        netProtocol_.Initialize();

        SA_LOG_INFO("ArenaCoordinator",
            "ArenaCoordinator initialized. Phase: Idle. Duel Item ID: %u, Arena: Limgrave, MaxCrimson: 0, MaxCerulean: %u",
            duelGoodsId_, lobbySettings_.maxCeruleanFlasks);

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

        netProtocol_.Shutdown();

        SA_LOG_INFO("ArenaCoordinator", "Arena coordinator shut down.");
    }

    void ArenaCoordinator::ApplyLobbySettings(const LobbySettings& settings)
    {
        lobbySettings_ = settings;
        // Enforce Alpha boundaries
        lobbySettings_.mode = core::MatchMode::Duel;
        lobbySettings_.arena = ColosseumId::Limgrave;
        lobbySettings_.maxCrimsonFlasks = 0;
        lobbySettings_.scoreLimit = 1;
    }

    void ArenaCoordinator::OnDuelItemUsed(std::uint64_t steamId, std::uint32_t goodsId)
    {
        // Accept configured duelGoodsId_, 101 (Duelist's Furled Finger), 108 (Taunter's Tongue),
        // 100 (Tarnished's Furled Finger), or 8380005 (Judicator's Rulebook)
        const bool isMatchItem = (goodsId == duelGoodsId_ || goodsId == 101 || goodsId == 108 || goodsId == 100 || goodsId == 8380005);
        if (!isMatchItem)
            return;

        // Fallback for local player identity if not yet resolved
        if (steamId == 0)
        {
            steamId = hostSteamId_ != 0 ? hostSteamId_ : 0x10001;
        }

        if (hostSteamId_ == 0)
        {
            SetLocalPlayerId(steamId);
        }
        else if (guestSteamId_ == 0 && steamId != hostSteamId_)
        {
            SetRemotePlayerId(steamId);
        }

        if (phase_ == ArenaPhase::Idle)
        {
            CreateLobby(lobbySettings_);
            SetPlayerReady(steamId, true);
            netProtocol_.BroadcastPlayerReady(steamId, true);

            // Immediate transition: Player 1 teleports to Colosseum first and waits there
            phase_ = ArenaPhase::AwaitingArena;
            TeleportToColosseum();

            InGameNotification::Show("SEAMLESS ARENA", "Entering Limgrave Colosseum (1v1 Duel)...", NotificationType::Info, 4.0f);
            EngineBridge::PlaySystemSound(1000);

            SA_LOG_INFO("ArenaCoordinator",
                "Custom Duel Item (Goods ID: %u) used by Player 0x%llX. Teleporting Host to Limgrave Colosseum first to wait in arena.",
                goodsId, steamId);
        }
        else if (phase_ == ArenaPhase::WaitingInArena)
        {
            if (steamId == hostSteamId_)
            {
                // Host used the item again while waiting alone in the arena:
                // Activate Solo Practice Mode so the player can test combat mechanics immediately without a friend!
                if (guestSteamId_ == 0)
                {
                    SA_LOG_INFO("ArenaCoordinator",
                        "Solo Practice Mode activated by Host 0x%llX! Registering virtual sparring partner for testing.",
                        steamId);
                    InGameNotification::Show("SOLO PRACTICE MODE", "Virtual sparring partner ready!\nStarting in 3 seconds...", NotificationType::Success, 4.0f);
                    EngineBridge::PlaySystemSound(1001);
                    SetRemotePlayerId(0x20002); // Virtual sparring partner
                    SetPlayerReady(0x20002, true);
                }
                else
                {
                    InGameNotification::Show("LIMGRAVE COLOSSEUM", "Waiting for Opponent in arena.\nPress F5 to start Solo Practice Mode!", NotificationType::Info, 4.0f);
                    SA_LOG_INFO("ArenaCoordinator",
                        "Host 0x%llX is waiting in Limgrave Colosseum for Opponent (0x%llX) to ready up.",
                        hostSteamId_, guestSteamId_);
                }
            }
            else
            {
                // Guest used the duel item while Host is waiting in arena!
                SetPlayerReady(steamId, true);
                netProtocol_.BroadcastPlayerReady(steamId, true);
            }
        }
        else if (phase_ == ArenaPhase::Lobby || phase_ == ArenaPhase::ReadyCheck)
        {
            SetPlayerReady(steamId, true);
            netProtocol_.BroadcastPlayerReady(steamId, true);
            InGameNotification::Show("SEAMLESS ARENA", "Duelist's Furled Finger confirmed!\nPlayer is READY.", NotificationType::Success, 3.0f);
            EngineBridge::PlaySystemSound(1000);
            SA_LOG_INFO("ArenaCoordinator",
                "Custom Duel Item (Goods ID: %u) used by Player 0x%llX. Player is now READY.",
                goodsId, steamId);
        }
        else
        {
            SA_LOG_INFO("ArenaCoordinator",
                "Custom Duel Item used during phase %d; ignored.", static_cast<int>(phase_));
        }
    }

    // =========================================================================
    // Game thread tick
    // =========================================================================

    void ArenaCoordinator::Tick(float deltaTimeSeconds)
    {
        // Reject invalid frame deltas and cap long stalls
        if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds < 0.0f)
            return;
        deltaTimeSeconds = std::min(deltaTimeSeconds, 0.25f);

        // =====================================================================
        // STEP 1: Process queued network commands
        // =====================================================================
        while (auto cmd = netProtocol_.DequeueCommand())
        {
            switch (cmd->opcode)
            {
            case Opcode::MSG_PLAYER_READY:
                if (cmd->senderSteamId != 0)
                {
                    if (cmd->senderSteamId != hostSteamId_ && guestSteamId_ == 0)
                    {
                        SetRemotePlayerId(cmd->senderSteamId);
                    }
                    SetPlayerReady(cmd->senderSteamId, cmd->readyFlag != 0);
                }
                break;

            case Opcode::MSG_DEATH_REPORT:
                OnPlayerDeath(cmd->victimId, cmd->killerId);
                break;

            case Opcode::MSG_MATCH_END:
                if (phase_ == ArenaPhase::Active)
                {
                    phase_ = ArenaPhase::Complete;
                    resultsTimer_ = 6.0f;
                    SA_LOG_INFO("ArenaCoordinator", "Match ended via peer MSG_MATCH_END. Winner: 0x%llX", cmd->winnerId);
                }
                break;

            case Opcode::MSG_ABORT_MATCH:
                EmergencyRecovery("Peer requested match abort");
                break;

            case Opcode::MSG_LOBBY_STATE:
                if (phase_ == ArenaPhase::Idle || phase_ == ArenaPhase::Lobby)
                {
                    lobbySettings_.arena = ColosseumId::Limgrave;
                    lobbySettings_.durationSeconds = cmd->durationSeconds;
                    lobbySettings_.scoreLimit = 1;
                }
                break;

            default:
                break;
            }
        }

        // =====================================================================
        // STEP 1.5: Input and in-game item trigger observation
        // =====================================================================
        // 1. Hotkey trigger (F5): Press F5 to trigger Duelist's Furled Finger flow
        static bool s_f5WasDown = false;
        const bool f5IsDown = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5IsDown && !s_f5WasDown)
        {
            s_f5WasDown = true;
            SA_LOG_INFO("ArenaCoordinator", "Hotkey F5 pressed! Triggering Duelist's Furled Finger action.");
            OnDuelItemUsed(hostSteamId_, 101);
        }
        else if (!f5IsDown)
        {
            s_f5WasDown = false;
        }

        // 2. Direct Teleport Hotkey (F6): Instantly warp directly to Limgrave Colosseum
        static bool s_f6WasDown = false;
        const bool f6IsDown = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        if (f6IsDown && !s_f6WasDown)
        {
            s_f6WasDown = true;
            SA_LOG_INFO("ArenaCoordinator", "Hotkey F6 pressed! Direct warp to Limgrave Colosseum.");
            TeleportToColosseum();
        }
        else if (!f6IsDown)
        {
            s_f6WasDown = false;
        }

        // 3. In-game character animation observation
        // When the player uses the Duelist's Furled Finger in-game, the character plays
        // the finger summon sign animation (anim IDs 60060..60075, 60240, 60210..60310)
        static int32_t s_lastObservedAnim = -1;
        int32_t currentAnim = EngineBridge::GetLocalPlayerCurrentAnimation();
        if (currentAnim != s_lastObservedAnim)
        {
            s_lastObservedAnim = currentAnim;
            if ((currentAnim >= 60060 && currentAnim <= 60075) || currentAnim == 60240 || (currentAnim >= 60210 && currentAnim <= 60310))
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "In-game Duelist's Furled Finger animation detected (Anim ID: %d). Triggering duel initiation!", currentAnim);
                OnDuelItemUsed(hostSteamId_, 101);
            }
        }

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

        case ArenaPhase::AwaitingArena:
            // Waiting for Colosseum scene load event.
            break;

        case ArenaPhase::WaitingInArena:
            // Player 1 is inside Colosseum holding position, waiting for opponent to join/ready.
            break;

        case ArenaPhase::Countdown:
            countdownTimer_ -= deltaTimeSeconds;
            if (countdownTimer_ <= 0.0f)
            {
                if (hostSteamId_ == 0 || guestSteamId_ == 0 || hostSteamId_ == guestSteamId_)
                {
                    SA_LOG_ERROR("ArenaCoordinator",
                        "Cannot start Duel: two distinct participant IDs are required.");
                    EmergencyRecovery("Invalid Duel participants");
                    break;
                }

                // Start 1v1 Duel match through MatchRules
                seamless_arena::core::MatchConfig config{};
                config.mode = seamless_arena::core::MatchMode::Duel;
                config.durationSeconds = lobbySettings_.durationSeconds > 0 ? lobbySettings_.durationSeconds : 300;
                config.scoreLimit = 1; // First death ends match in Duel

                std::vector<seamless_arena::core::PlayerState> players;
                // Slot 0: Host
                players.push_back(seamless_arena::core::PlayerState{ hostSteamId_, 0, 0, 0, true, true });
                // Slot 1: Guest
                players.push_back(seamless_arena::core::PlayerState{ guestSteamId_, 1, 0, 0, true, true });

                std::string err;
                if (matchRules_.Start(config, std::move(players), err))
                {
                    phase_ = ArenaPhase::Active;
                    deathObserved_ = false;
                    lastHitByVictim_.clear();
                    matchElapsedSeconds_ = 0.0f;
                    matchTimeAccumulator_ = 0.0f;
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
            // Advance whole seconds only after accumulating real frame time.
            // The previous implementation rounded every frame up to one second,
            // making a 60 FPS match timer run roughly 60 times too fast.
            matchElapsedSeconds_ += deltaTimeSeconds;
            matchTimeAccumulator_ += deltaTimeSeconds;
            while (matchTimeAccumulator_ >= 1.0f)
            {
                matchRules_.AdvanceTime(1);
                matchTimeAccumulator_ -= 1.0f;
            }

            // 2. Safeguard HP polling: detect local player death if detour was delayed
            int32_t localHp = EngineBridge::GetLocalPlayerHp();
            if (localHp == 0 && !deathObserved_)
            {
                SA_LOG_INFO("ArenaCoordinator", "Local player HP reached 0 via engine polling safeguard.");
                // HP polling proves the victim only. Never award a kill to the
                // guest by assumption; OnPlayerDeath can use recent verified
                // participant damage if the hook supplied it.
                OnPlayerDeath(hostSteamId_, 0);
            }

            // 3. Check if match rules completed (first death or timeout draw)
            auto snap = matchRules_.Snapshot();
            if (snap.phase == seamless_arena::core::MatchPhase::Complete)
            {
                phase_ = ArenaPhase::Complete;
                resultsTimer_ = 6.0f; // Hold scoreboard/results for 6 seconds
                if (snap.winningPlayer.has_value())
                {
                    SA_LOG_INFO("ArenaCoordinator",
                        "1v1 DUEL COMPLETE: Winner is Player 0x%llX!", snap.winningPlayer.value());
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
            if (!cleanupRequested_)
            {
                cleanupRequested_ = true;
                RestoreConsumables();
                WarpPartyToOverworld();
                SA_LOG_WARN("ArenaCoordinator",
                    "Cleanup is waiting for a verified overworld-load event; no warp is implemented yet.");
            }
            break;

        case ArenaPhase::Cancelled:
            matchRules_.Reset();
            hostReady_ = false;
            guestReady_ = false;
            deathObserved_ = false;
            cleanupRequested_ = false;
            lastHitByVictim_.clear();
            matchElapsedSeconds_ = 0.0f;
            matchTimeAccumulator_ = 0.0f;
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

        const auto snapshot = matchRules_.Snapshot();
        const auto isConnectedParticipant = [&snapshot](std::uint64_t id)
        {
            return id != 0 && std::any_of(snapshot.players.begin(), snapshot.players.end(),
                [id](const seamless_arena::core::PlayerState& player)
                {
                    return player.id == id && player.connected;
                });
        };

        if (!isConnectedParticipant(victimSteamId))
        {
            SA_LOG_WARN("ArenaCoordinator", "Ignoring death for non-participant ID 0x%llX", victimSteamId);
            return;
        }

        // Prefer a killer explicitly resolved by a verified death callback. If
        // unavailable, credit only a recent hit from another connected match
        // participant. The 10-second window is a project rule, not a game offset.
        constexpr float kKillCreditWindowSeconds = 10.0f;
        if (!isConnectedParticipant(killerSteamId) || killerSteamId == victimSteamId)
        {
            killerSteamId = 0;
            const auto hit = lastHitByVictim_.find(victimSteamId);
            if (hit != lastHitByVictim_.end()
                && matchElapsedSeconds_ - hit->second.matchTimeSeconds <= kKillCreditWindowSeconds
                && hit->second.attackerSteamId != victimSteamId
                && isConnectedParticipant(hit->second.attackerSteamId))
            {
                killerSteamId = hit->second.attackerSteamId;
            }
        }
        lastHitByVictim_.erase(victimSteamId);

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

        // Broadcast death report to peers
        netProtocol_.BroadcastDeathReport(victimSteamId, killerSteamId);

        auto snap = matchRules_.Snapshot();
        if (snap.phase == core::MatchPhase::Complete)
        {
            phase_ = ArenaPhase::Complete;
            resultsTimer_ = 6.0f;
            uint64_t winnerId = snap.winningPlayer.value_or(0);
            netProtocol_.BroadcastMatchEnd(winnerId, !snap.winningPlayer.has_value());

            if (snap.winningPlayer.has_value())
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "FIRST DEATH! Victim 0x%llX eliminated. 1v1 DUEL WINNER: 0x%llX!",
                    victimSteamId, snap.winningPlayer.value());
            }
            else
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "FIRST DEATH! Victim 0x%llX eliminated. 1v1 DUEL DRAW.", victimSteamId);
            }
        }
    }

    void ArenaCoordinator::OnDamageApplied(std::uint64_t attackerSteamId,
                                           std::uint64_t victimSteamId,
                                           float damage)
    {
        if (phase_ != ArenaPhase::Active || !std::isfinite(damage) || damage <= 0.0f
            || attackerSteamId == 0 || victimSteamId == 0
            || attackerSteamId == victimSteamId)
        {
            return;
        }

        const auto snapshot = matchRules_.Snapshot();
        const auto isConnectedParticipant = [&snapshot](std::uint64_t id)
        {
            return std::any_of(snapshot.players.begin(), snapshot.players.end(),
                [id](const seamless_arena::core::PlayerState& player)
                {
                    return player.id == id && player.connected;
                });
        };

        // This filters PvE and unrelated entities. A future damage hook must
        // resolve stable participant IDs before calling this function.
        if (!isConnectedParticipant(attackerSteamId) || !isConnectedParticipant(victimSteamId))
            return;

        lastHitByVictim_[victimSteamId] = LastHitRecord{ attackerSteamId, matchElapsedSeconds_ };
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3.1 rule 2)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::OnLoadingScreenStart()
    {
        lastHitByVictim_.clear();
        SA_LOG_DEBUG("ArenaCoordinator", "Loading screen detected — clearing entity cache.");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 4)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::OnArenaSceneLoaded(std::uint32_t mapId)
    {
        SA_LOG_INFO("ArenaCoordinator", "Arena scene loaded: map 0x%08X", mapId);

        // Alpha release strictly targets Limgrave Colosseum (m45_02_00_00: 45020000 or 0x45020000)
        const bool isLimgrave = (mapId == 45020000 || mapId == 0x45020000 || ((mapId >> 16) & 0xFFFF) == 0x4502);
        if (isLimgrave)
        {
            SA_LOG_INFO("ArenaCoordinator", "Limgrave Colosseum scene confirmed for 1v1 Duel.");
        }
        else
        {
            SA_LOG_WARN("ArenaCoordinator",
                "Map 0x%08X loaded; note that the Alpha milestone targets Limgrave Colosseum (m45_02).", mapId);
        }

        if (phase_ == ArenaPhase::AwaitingArena || phase_ == ArenaPhase::Lobby || phase_ == ArenaPhase::ReadyCheck)
        {
            if (AreBothPlayersReady())
            {
                phase_ = ArenaPhase::Countdown;
                countdownTimer_ = 3.0f;
                TeleportPlayerToSpawn(hostSteamId_, 0);
                TeleportPlayerToSpawn(guestSteamId_, 1);
                EngineBridge::PlaySystemSound(1000); // 3-second countdown chime
                InGameNotification::Show("1v1 DUEL READY", "Both players in Limgrave Colosseum!\nMatch starting in 3 seconds...", NotificationType::Success, 3.0f);
                SA_LOG_INFO("ArenaCoordinator",
                    "Both players present and ready in Limgrave Colosseum. Starting 3-second countdown!");
            }
            else
            {
                // Solo player arrived first: hold position in arena and wait for opponent
                phase_ = ArenaPhase::WaitingInArena;
                TeleportPlayerToSpawn(hostSteamId_, 0);
                EngineBridge::PlaySystemSound(1000);
                InGameNotification::Show("LIMGRAVE COLOSSEUM", "Arrived at arena! Waiting for Opponent.\nPress F5 or use Duel Finger for Solo Practice.", NotificationType::Info, 5.0f);
                SA_LOG_INFO("ArenaCoordinator",
                    "Player 1 (Host 0x%llX) arrived at Limgrave Colosseum! Holding position at South Spawn. Waiting in arena for Player 2 to join...",
                    hostSteamId_);
            }
        }
    }

    void ArenaCoordinator::OnOverworldLoaded()
    {
        SA_LOG_INFO("ArenaCoordinator", "Overworld scene loaded.");
        if (phase_ == ArenaPhase::AwaitingArena || phase_ == ArenaPhase::WaitingInArena || phase_ == ArenaPhase::Countdown)
        {
            matchRules_.Reset();
            hostReady_ = false;
            guestReady_ = false;
            lastHitByVictim_.clear();
            phase_ = ArenaPhase::Idle;
            SA_LOG_INFO("ArenaCoordinator", "Arena session exited; returned to Idle.");
            return;
        }
        if (phase_ == ArenaPhase::Cleanup)
        {
            matchRules_.Reset();
            hostReady_ = false;
            guestReady_ = false;
            deathObserved_ = false;
            cleanupRequested_ = false;
            lastHitByVictim_.clear();
            matchElapsedSeconds_ = 0.0f;
            matchTimeAccumulator_ = 0.0f;
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

        if (hostSteamId_ == 0 || settings.durationSeconds == 0
            || settings.arena > ColosseumId::Limgrave)
        {
            SA_LOG_WARN("ArenaCoordinator",
                "Cannot create lobby: host identity, positive duration, and valid arena are required.");
            return false;
        }

        lobbySettings_ = settings;
        lobbySettings_.mode = core::MatchMode::Duel;
        hostReady_ = false;
        guestReady_ = false;
        deathObserved_ = false;
        cleanupRequested_ = false;
        lastHitByVictim_.clear();
        matchElapsedSeconds_ = 0.0f;
        matchTimeAccumulator_ = 0.0f;
        matchRules_.Reset();

        phase_ = (guestSteamId_ != 0) ? ArenaPhase::ReadyCheck : ArenaPhase::Lobby;
        SA_LOG_INFO("ArenaCoordinator",
            "1v1 Duel lobby created for %s Colosseum. State: %s",
            ColosseumName(lobbySettings_.arena),
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

        if (!AreBothPlayersReady())
        {
            SA_LOG_WARN("ArenaCoordinator", "Cannot start Duel until two distinct players are ready.");
            return false;
        }

        SA_LOG_INFO("ArenaCoordinator",
            "Both players are ready. Awaiting a verified Colosseum scene-load event; no warp API is connected.");

        phase_ = ArenaPhase::AwaitingArena;
        countdownTimer_ = 0.0f;
        return true;
    }

    void ArenaCoordinator::AbortMatch()
    {
        SA_LOG_WARN("ArenaCoordinator", "Match aborted by user/host.");
        netProtocol_.BroadcastAbortMatch(hostSteamId_);
        matchRules_.Cancel();
        EmergencyRecovery("Manual abort");
    }

    void ArenaCoordinator::ToggleReady()
    {
        SetPlayerReady(hostSteamId_, !hostReady_);
    }

    void ArenaCoordinator::SetLocalPlayerId(std::uint64_t steamId)
    {
        if (steamId == 0 || steamId == guestSteamId_)
        {
            SA_LOG_WARN("ArenaCoordinator", "Ignoring invalid or duplicate local player ID.");
            return;
        }
        hostSteamId_ = steamId;
        SA_LOG_INFO("ArenaCoordinator", "Host player Steam ID registered: 0x%llX", steamId);
    }

    void ArenaCoordinator::SetRemotePlayerId(std::uint64_t steamId)
    {
        if (steamId == 0 || steamId == hostSteamId_)
        {
            SA_LOG_WARN("ArenaCoordinator", "Ignoring invalid or duplicate remote player ID.");
            return;
        }
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
        if (phase_ != ArenaPhase::ReadyCheck && phase_ != ArenaPhase::Lobby && phase_ != ArenaPhase::WaitingInArena)
        {
            SA_LOG_WARN("ArenaCoordinator", "Ignoring ready change outside a lobby or waiting arena.");
            return;
        }

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
            if (phase_ == ArenaPhase::WaitingInArena)
            {
                // Already inside Colosseum! Transition directly to Countdown
                phase_ = ArenaPhase::Countdown;
                countdownTimer_ = 3.0f;
                TeleportPlayerToSpawn(hostSteamId_, 0);
                TeleportPlayerToSpawn(guestSteamId_, 1);
                EngineBridge::PlaySystemSound(1000);
                SA_LOG_INFO("ArenaCoordinator",
                    "Opponent readied while Host is waiting in arena. Starting 3-second countdown!");
            }
            else
            {
                StartMatch();
            }
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
            || phase_ == ArenaPhase::Complete
            || phase_ == ArenaPhase::WaitingInArena;
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
    const char* ArenaCoordinator::ColosseumName(ColosseumId id) noexcept
    {
        switch (id)
        {
        case ColosseumId::Royal:    return "Royal";
        case ColosseumId::Caelid:   return "Caelid";
        case ColosseumId::Limgrave: return "Limgrave";
        default:                    return "Unknown";
        }
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 3)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::TeleportPlayerToSpawn(std::uint64_t steamId, std::uint8_t spawnSlot)
    {
        // RESEARCH: rechres/05-colosseum-map-investigation.md (Section 3)
        // Limgrave Colosseum (m45_02_00_00) local spawn coordinates:
        //   Slot 0 (Host / Team 1): South spawn (Z = -18.0f), facing North
        //   Slot 1 (Guest / Team 2): North spawn (Z = 18.0f), facing South
        float spawnX = 0.0f;
        float spawnY = 0.0f;
        float spawnZ = (spawnSlot == 0) ? -18.0f : 18.0f;

        SA_LOG_INFO("ArenaCoordinator",
            "Positioning player 0x%llX at spawn slot %d (X: %.1f, Y: %.1f, Z: %.1f)",
            steamId, spawnSlot, spawnX, spawnY, spawnZ);

        if (steamId == hostSteamId_ || steamId == 0)
        {
            bool moved = EngineBridge::SetLocalPlayerPosition(spawnX, spawnY, spawnZ);
            if (moved)
            {
                SA_LOG_INFO("ArenaCoordinator",
                    "Local player repositioned to spawn slot %d (X: %.1f, Y: %.1f, Z: %.1f).",
                    spawnSlot, spawnX, spawnY, spawnZ);
            }
            else
            {
                SA_LOG_WARN("ArenaCoordinator",
                    "Local player coordinate write skipped (local player memory not yet readable).");
            }
        }
    }

    void ArenaCoordinator::TeleportToColosseum()
    {
        SA_LOG_INFO("ArenaCoordinator",
            "Initiating travel/warp for Host to Limgrave Colosseum (m45_02_00_00: 45020000)...");

        float curX = 0.0f, curY = 0.0f, curZ = 0.0f;
        if (EngineBridge::GetLocalPlayerPosition(curX, curY, curZ))
        {
            SA_LOG_INFO("ArenaCoordinator", "Current player position before warp: X=%.2f, Y=%.2f, Z=%.2f", curX, curY, curZ);
        }

        // 1. Try engine map load via HookManager_RequestMapLoad
        bool mapRequested = HookManager_RequestMapLoad(45020000);
        if (mapRequested)
        {
            SA_LOG_INFO("ArenaCoordinator",
                "FieldArea::LoadMap requested for Limgrave Colosseum (45020000).");
            InGameNotification::Show("SEAMLESS ARENA", "Entering Limgrave Colosseum...", NotificationType::Info, 4.0f);
        }
        else
        {
            // Limgrave Colosseum arena floor coordinates (Stormhill, northern Limgrave cliffside)
            constexpr float kColosseumArenaX = -775.0f;
            constexpr float kColosseumArenaY = 1380.0f;
            constexpr float kColosseumArenaZ = 125.0f;

            SA_LOG_INFO("ArenaCoordinator",
                "Repositioning local player to Limgrave Colosseum arena coordinates (%.2f, %.2f, %.2f)...",
                kColosseumArenaX, kColosseumArenaY, kColosseumArenaZ);

            bool moved = EngineBridge::SetLocalPlayerPosition(kColosseumArenaX, kColosseumArenaY, kColosseumArenaZ);
            if (moved)
            {
                InGameNotification::Show("LIMGRAVE COLOSSEUM", "Arrived at Limgrave Colosseum!\nPress F5 for Solo Practice Mode.", NotificationType::Success, 6.0f);
                phase_ = ArenaPhase::WaitingInArena;
                SA_LOG_INFO("ArenaCoordinator", "Local player teleported to Limgrave Colosseum. Phase set to WaitingInArena.");
            }
            else
            {
                InGameNotification::Show("SEAMLESS ARENA", "Entering arena... Waiting for character spawn.", NotificationType::Info, 4.0f);
                SA_LOG_WARN("ArenaCoordinator", "SetLocalPlayerPosition failed; player character memory might still be loading.");
            }
        }
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

        SA_LOG_WARN("ArenaCoordinator", "Respawn not implemented; player 0x%llX was not revived.", steamId);
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

        SA_LOG_WARN("ArenaCoordinator", "Inventory snapshot/restore is not implemented; no items were changed.");
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

        SA_LOG_WARN("ArenaCoordinator", "Overworld warp is not implemented; no player was moved.");
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
    // -------------------------------------------------------------------------
    void ArenaCoordinator::EmergencyRecovery(const char* reason)
    {
        SA_LOG_ERROR("ArenaCoordinator",
            "EMERGENCY RECOVERY triggered! Reason: %s", reason);
        SA_LOG_ERROR("ArenaCoordinator",
            "Automatic recovery warp and inventory restoration are unavailable in this build.");

        InGameNotification::Show("ARENA NOTICE", "Match aborted or reset. Check SeamlessArena.log.", NotificationType::Error, 4.0f);
        EngineBridge::PlaySystemSound(1003);

        matchRules_.Cancel();
        RestoreConsumables();
        WarpPartyToOverworld();
        phase_ = ArenaPhase::Cancelled;
    }

} // namespace seamless_arena::native
