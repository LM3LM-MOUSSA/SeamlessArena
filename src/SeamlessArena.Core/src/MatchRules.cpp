// =============================================================================
// MatchRules.cpp — Deterministic Colosseum match rules implementation
// =============================================================================
// RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Sections 2, 3, 5)
// RESEARCH: rechres/01-better-multiplayer-audit.md       (Section 3 — pure rules)
// RESEARCH: rechres/07-multiplayer-authority-sync.md     (Section 1 — authoritative host)
// =============================================================================

#include "SeamlessArena/Core/MatchRules.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_set>
#include <utility>

namespace seamless_arena::core
{
    namespace
    {
        constexpr std::size_t kMinimumPlayers = 2;
        constexpr std::size_t kMaximumPlayers = 6;

        bool IsKnownMode(MatchMode mode)
        {
            switch (mode)
            {
            case MatchMode::Duel:
            case MatchMode::CombatOrdeal:
            case MatchMode::UnitedCombat:
                return true;
            default:
                return false;
            }
        }
    }

    bool MatchRules::Start(const MatchConfig& config, std::vector<PlayerState> players, std::string& error)
    {
        error.clear();
        if (phase_ != MatchPhase::Lobby)
        {
            error = "A match can only start from the lobby phase.";
            return false;
        }
        if (!IsKnownMode(config.mode))
        {
            error = "The selected match mode is not supported.";
            return false;
        }
        if (players.size() < kMinimumPlayers || players.size() > kMaximumPlayers)
        {
            error = "A match requires between two and six players.";
            return false;
        }
        if (config.mode == MatchMode::Duel && players.size() != 2)
        {
            error = "Duel mode requires exactly two players.";
            return false;
        }
        if (config.mode != MatchMode::Duel && config.durationSeconds == 0)
        {
            error = "Timed modes require a non-zero duration.";
            return false;
        }

        std::unordered_set<PlayerId> ids;
        std::size_t teamCounts[2]{};
        for (const PlayerState& player : players)
        {
            if (!player.connected)
            {
                error = "Every player must be connected when the match starts.";
                return false;
            }
            if (!ids.insert(player.id).second)
            {
                error = "Player identifiers must be unique.";
                return false;
            }
            if (config.mode == MatchMode::UnitedCombat)
            {
                if (player.team > 1)
                {
                    error = "United Combat supports team identifiers 0 and 1.";
                    return false;
                }
                ++teamCounts[player.team];
            }
        }

        if (config.mode == MatchMode::UnitedCombat)
        {
            if (teamCounts[0] == 0 || teamCounts[1] == 0)
            {
                error = "United Combat requires both teams to have players.";
                return false;
            }
            const std::size_t teamDifference = teamCounts[0] > teamCounts[1]
                ? teamCounts[0] - teamCounts[1]
                : teamCounts[1] - teamCounts[0];
            if (teamDifference > 1)
            {
                error = "United Combat teams must be balanced within one player.";
                return false;
            }
        }

        for (PlayerState& player : players)
        {
            player.score = 0;
            player.deaths = 0;
            player.alive = true;
        }

        config_ = config;
        secondsRemaining_ = config.durationSeconds;
        players_ = std::move(players);
        winningPlayer_.reset();
        winningTeam_.reset();
        isDraw_ = false;
        phase_ = MatchPhase::Active;
        return true;
    }

    bool MatchRules::RecordDeath(PlayerId victimId, std::optional<PlayerId> killerId)
    {
        if (phase_ != MatchPhase::Active)
        {
            return false;
        }

        PlayerState* victim = FindPlayer(victimId);
        if (victim == nullptr || !victim->connected || !victim->alive)
        {
            return false;
        }

        victim->alive = false;
        if (victim->deaths != std::numeric_limits<std::uint16_t>::max())
        {
            ++victim->deaths;
        }

        PlayerState* killer = killerId.has_value() ? FindPlayer(*killerId) : nullptr;
        const bool validEnemyKill = killer != nullptr
            && killer->connected
            && killer->id != victim->id
            && (config_.mode != MatchMode::UnitedCombat || killer->team != victim->team);

        if (validEnemyKill && killer->score != std::numeric_limits<std::uint16_t>::max())
        {
            ++killer->score;
        }

        if (config_.mode == MatchMode::Duel)
        {
            FinishDuel(victimId);
            return true;
        }

        if (validEnemyKill && config_.scoreLimit > 0)
        {
            if (config_.mode == MatchMode::CombatOrdeal && killer->score >= config_.scoreLimit)
            {
                winningPlayer_ = killer->id;
                phase_ = MatchPhase::Complete;
            }
            else if (config_.mode == MatchMode::UnitedCombat && TeamScore(killer->team) >= config_.scoreLimit)
            {
                winningTeam_ = killer->team;
                phase_ = MatchPhase::Complete;
            }
        }

        return true;
    }

    bool MatchRules::Respawn(PlayerId playerId)
    {
        if (phase_ != MatchPhase::Active || config_.mode == MatchMode::Duel)
        {
            return false;
        }

        PlayerState* player = FindPlayer(playerId);
        if (player == nullptr || !player->connected || player->alive)
        {
            return false;
        }

        player->alive = true;
        return true;
    }

    bool MatchRules::AdvanceTime(std::uint32_t elapsedSeconds)
    {
        if (phase_ != MatchPhase::Active || !IsTimedMode() || elapsedSeconds == 0)
        {
            return false;
        }

        if (elapsedSeconds >= secondsRemaining_)
        {
            secondsRemaining_ = 0;
            if (config_.mode == MatchMode::Duel)
            {
                // In Duel: timeout with no deaths results in a draw
                isDraw_ = true;
                winningPlayer_.reset();
                phase_ = MatchPhase::Complete;
            }
            else
            {
                FinishByCurrentScore();
            }
        }
        else
        {
            secondsRemaining_ -= elapsedSeconds;
        }

        return true;
    }

    bool MatchRules::HandleDisconnect(PlayerId playerId)
    {
        if (phase_ != MatchPhase::Active)
        {
            return false;
        }

        PlayerState* player = FindPlayer(playerId);
        if (player == nullptr || !player->connected)
        {
            return false;
        }

        player->connected = false;
        player->alive = false;

        if (config_.mode == MatchMode::Duel)
        {
            // Forfeit in Duel: the surviving connected player wins
            for (const PlayerState& p : players_)
            {
                if (p.id != playerId && p.connected)
                {
                    winningPlayer_ = p.id;
                    phase_ = MatchPhase::Complete;
                    return true;
                }
            }
            isDraw_ = true;
            phase_ = MatchPhase::Complete;
            return true;
        }

        if (config_.mode == MatchMode::UnitedCombat)
        {
            const std::size_t t0 = ConnectedTeamCount(0);
            const std::size_t t1 = ConnectedTeamCount(1);
            if (t0 == 0 && t1 == 0)
            {
                phase_ = MatchPhase::Cancelled;
            }
            else if (t0 == 0)
            {
                winningTeam_ = 1;
                phase_ = MatchPhase::Complete;
            }
            else if (t1 == 0)
            {
                winningTeam_ = 0;
                phase_ = MatchPhase::Complete;
            }
            return true;
        }

        if (config_.mode == MatchMode::CombatOrdeal)
        {
            if (ConnectedPlayerCount() < 2)
            {
                FinishByCurrentScore();
            }
            return true;
        }

        return true;
    }

    void MatchRules::Cancel()
    {
        if (phase_ == MatchPhase::Lobby || phase_ == MatchPhase::Active)
        {
            phase_ = MatchPhase::Cancelled;
        }
    }

    void MatchRules::Reset()
    {
        config_ = {};
        phase_ = MatchPhase::Lobby;
        secondsRemaining_ = 0;
        players_.clear();
        winningPlayer_.reset();
        winningTeam_.reset();
        isDraw_ = false;
    }

    MatchSnapshot MatchRules::Snapshot() const
    {
        return MatchSnapshot{
            config_.mode,
            phase_,
            secondsRemaining_,
            players_,
            winningPlayer_,
            winningTeam_,
            isDraw_
        };
    }

    PlayerState* MatchRules::FindPlayer(PlayerId id)
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const PlayerState& player)
        {
            return player.id == id;
        });
        return found == players_.end() ? nullptr : &*found;
    }

    const PlayerState* MatchRules::FindPlayer(PlayerId id) const
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const PlayerState& player)
        {
            return player.id == id;
        });
        return found == players_.end() ? nullptr : &*found;
    }

    std::uint32_t MatchRules::TeamScore(std::uint8_t team) const
    {
        std::uint32_t score = 0;
        for (const PlayerState& player : players_)
        {
            if (player.team == team)
            {
                score += player.score;
            }
        }
        return score;
    }

    std::size_t MatchRules::ConnectedTeamCount(std::uint8_t team) const
    {
        std::size_t count = 0;
        for (const PlayerState& player : players_)
        {
            if (player.team == team && player.connected)
            {
                ++count;
            }
        }
        return count;
    }

    std::size_t MatchRules::ConnectedPlayerCount() const
    {
        std::size_t count = 0;
        for (const PlayerState& player : players_)
        {
            if (player.connected)
            {
                ++count;
            }
        }
        return count;
    }

    bool MatchRules::IsTimedMode() const
    {
        return config_.durationSeconds > 0;
    }

    void MatchRules::FinishByCurrentScore()
    {
        if (config_.mode == MatchMode::UnitedCombat)
        {
            const std::uint32_t teamZeroScore = TeamScore(0);
            const std::uint32_t teamOneScore = TeamScore(1);
            if (teamZeroScore == teamOneScore)
            {
                isDraw_ = true;
            }
            else
            {
                winningTeam_ = teamZeroScore > teamOneScore ? std::uint8_t{0} : std::uint8_t{1};
            }
        }
        else
        {
            std::uint16_t highScore = 0;
            bool hasLeader = false;
            bool tied = false;
            for (const PlayerState& player : players_)
            {
                if (!hasLeader || player.score > highScore)
                {
                    highScore = player.score;
                    winningPlayer_ = player.id;
                    hasLeader = true;
                    tied = false;
                }
                else if (player.score == highScore)
                {
                    tied = true;
                }
            }
            if (!hasLeader || tied)
            {
                winningPlayer_.reset();
                isDraw_ = true;
            }
        }

        phase_ = MatchPhase::Complete;
    }

    void MatchRules::FinishDuel(PlayerId victimId)
    {
        for (const PlayerState& player : players_)
        {
            if (player.id != victimId && player.connected && player.alive)
            {
                winningPlayer_ = player.id;
                phase_ = MatchPhase::Complete;
                return;
            }
        }

        isDraw_ = true;
        phase_ = MatchPhase::Complete;
    }
} // namespace seamless_arena::core
