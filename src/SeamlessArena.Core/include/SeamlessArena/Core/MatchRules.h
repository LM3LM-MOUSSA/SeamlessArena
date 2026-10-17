#pragma once
// =============================================================================
// MatchRules.h — Pure, deterministic arena match state machine and rules
// =============================================================================
// RESEARCH: rechres/06-pvp-match-rules-state-machine.md (ALL sections)
// RESEARCH: rechres/01-better-multiplayer-audit.md       (Section 3 — decoupled rules)
// RESEARCH: rechres/07-multiplayer-authority-sync.md     (Section 1 — authoritative host)
//
// Pure C++20 game-independent rules engine for vanilla Colosseum game modes:
//   - Duel (1v1, first death ends match, or timeout draw)
//   - Combat Ordeal (FFA, 2-6 players, score cap or highest score on timeout)
//   - United Combat (2-team deathmatch, balanced teams, score cap / timeout)
//
// Zero dependencies on Elden Ring, DirectX, libER, or Windows hooks.
// Can be compiled and tested headlessly on any platform (rechres/11 Section 2).
// =============================================================================

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace seamless_arena::core
{
    using PlayerId = std::uint64_t;

    // -------------------------------------------------------------------------
    // Supported match modes
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 2)
    // -------------------------------------------------------------------------
    enum class MatchMode : std::uint8_t
    {
        Duel,
        CombatOrdeal,
        UnitedCombat
    };

    // -------------------------------------------------------------------------
    // Core match phase
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3.1)
    // -------------------------------------------------------------------------
    enum class MatchPhase : std::uint8_t
    {
        Lobby,
        Active,
        Complete,
        Cancelled
    };

    // -------------------------------------------------------------------------
    // Match configuration supplied at match start
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 2)
    // -------------------------------------------------------------------------
    struct MatchConfig
    {
        MatchMode mode{ MatchMode::Duel };
        std::uint32_t durationSeconds{ 300 }; // 0 = untimed (Duel only)
        std::uint16_t scoreLimit{};           // 0 = no score cap (timed only)
    };

    // -------------------------------------------------------------------------
    // Individual player combatant state
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3)
    // -------------------------------------------------------------------------
    struct PlayerState
    {
        PlayerId id{};
        std::uint8_t team{};
        std::uint16_t score{};
        std::uint16_t deaths{};
        bool connected{ true };
        bool alive{ true };
    };

    // -------------------------------------------------------------------------
    // Immutable snapshot broadcast to peers
    // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 3.2 — MSG_SCORE_SYNC)
    // -------------------------------------------------------------------------
    struct MatchSnapshot
    {
        MatchMode mode{ MatchMode::Duel };
        MatchPhase phase{ MatchPhase::Lobby };
        std::uint32_t secondsRemaining{};
        std::vector<PlayerState> players;
        std::optional<PlayerId> winningPlayer;
        std::optional<std::uint8_t> winningTeam;
        bool isDraw{};
    };

    // -------------------------------------------------------------------------
    // MatchRules class declaration
    // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Sections 3, 5)
    // -------------------------------------------------------------------------
    class MatchRules final
    {
    public:
        // Transition from Lobby -> Active. Validates team balancing and player counts.
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3.1)
        bool Start(const MatchConfig& config, std::vector<PlayerState> players, std::string& error);

        // Process a player death event, attributing score to valid enemy killers.
        // Rejects duplicate deaths if victim is already dead.
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 5)
        // RESEARCH: rechres/08-game-events-hooks.md             (Section 3)
        bool RecordDeath(PlayerId victim, std::optional<PlayerId> killer = std::nullopt);

        // Process player respawn in multi-life modes (Combat Ordeal / United Combat).
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 4)
        bool Respawn(PlayerId player);

        // Advance match timer on each second tick. Handles Duel draws and timeout finishes.
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 3.1 state 5)
        bool AdvanceTime(std::uint32_t elapsedSeconds);

        // Process a player disconnection mid-match (forfeiture in Duel, team checks in United).
        // RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Section 5)
        bool HandleDisconnect(PlayerId player);

        // Abort the match due to error or host departure.
        // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 3)
        void Cancel();

        // Reset state machine back to clean Lobby state for repeated match reuse.
        // RESEARCH: docs/TODO.md (Phase 2 Item 3)
        void Reset();

        // Get an immutable point-in-time snapshot.
        // RESEARCH: rechres/07-multiplayer-authority-sync.md (Section 3.2)
        [[nodiscard]] MatchSnapshot Snapshot() const;

    private:
        [[nodiscard]] PlayerState* FindPlayer(PlayerId id);
        [[nodiscard]] const PlayerState* FindPlayer(PlayerId id) const;
        [[nodiscard]] std::uint32_t TeamScore(std::uint8_t team) const;
        [[nodiscard]] std::size_t ConnectedTeamCount(std::uint8_t team) const;
        [[nodiscard]] std::size_t ConnectedPlayerCount() const;
        [[nodiscard]] bool IsTimedMode() const;
        void FinishByCurrentScore();
        void FinishDuel(PlayerId victim);

        MatchConfig config_{};
        MatchPhase phase_{ MatchPhase::Lobby };
        std::uint32_t secondsRemaining_{};
        std::vector<PlayerState> players_;
        std::optional<PlayerId> winningPlayer_;
        std::optional<std::uint8_t> winningTeam_;
        bool isDraw_{};
    };
} // namespace seamless_arena::core
