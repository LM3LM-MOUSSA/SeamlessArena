#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace seamless_arena::core
{
    using PlayerId = std::uint64_t;

    enum class MatchMode : std::uint8_t
    {
        Duel,
        CombatOrdeal,
        UnitedCombat
    };

    enum class MatchPhase : std::uint8_t
    {
        Lobby,
        Active,
        Complete,
        Cancelled
    };

    struct MatchConfig
    {
        MatchMode mode{ MatchMode::Duel };
        std::uint32_t durationSeconds{ 300 };
        std::uint16_t scoreLimit{};
    };

    struct PlayerState
    {
        PlayerId id{};
        std::uint8_t team{};
        std::uint16_t score{};
        std::uint16_t deaths{};
        bool connected{ true };
        bool alive{ true };
    };

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

    // Game-independent rules for the vanilla Colosseum modes. The game adapter
    // will translate verified Elden Ring events into these calls.
    class MatchRules final
    {
    public:
        bool Start(const MatchConfig& config, std::vector<PlayerState> players, std::string& error);
        bool RecordDeath(PlayerId victim, std::optional<PlayerId> killer = std::nullopt);
        bool Respawn(PlayerId player);
        bool AdvanceTime(std::uint32_t elapsedSeconds);
        void Cancel();

        [[nodiscard]] MatchSnapshot Snapshot() const;

    private:
        [[nodiscard]] PlayerState* FindPlayer(PlayerId id);
        [[nodiscard]] const PlayerState* FindPlayer(PlayerId id) const;
        [[nodiscard]] std::uint32_t TeamScore(std::uint8_t team) const;
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
}
