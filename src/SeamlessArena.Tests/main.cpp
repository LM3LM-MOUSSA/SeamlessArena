// =============================================================================
// SeamlessArena.Tests — Comprehensive unit test runner for SeamlessArena.Core
// =============================================================================
// RESEARCH: rechres/11-build-and-validation-gates.md (Section 2 — automated tests)
// RESEARCH: rechres/06-pvp-match-rules-state-machine.md (Sections 2, 3, 5)
// RESEARCH: docs/TODO.md (Phase 2 Item 3 — rule-edge cases)
//
// Standalone console test runner that validates match rules, edge cases, and
// state transitions without any dependency on Elden Ring, DirectX, or hooks.
// =============================================================================

#include <SeamlessArena/Core/MatchRules.h>

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace seamless_arena::core;

    int g_passedTests = 0;
    int g_failedTests = 0;

    #define TEST_ASSERT(cond, msg) \
        do { \
            if (!(cond)) { \
                std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
                ++g_failedTests; \
                return; \
            } \
        } while (0)

    #define RUN_TEST(fn) \
        do { \
            std::cout << "[RUN ] " #fn "...\n"; \
            int before = g_failedTests; \
            fn(); \
            if (g_failedTests == before) { \
                std::cout << "  [PASS] " #fn "\n"; \
                ++g_passedTests; \
            } \
        } while (0)

    // -------------------------------------------------------------------------
    // TEST 1: Duel Mode Win Condition
    // -------------------------------------------------------------------------
    void Test_Duel_WinCondition()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Duel should start with 2 connected players");
        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Active, "Phase should be Active");

        TEST_ASSERT(rules.RecordDeath(1002, 1001), "Death of player 1002 should succeed");

        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Duel should complete on first death");
        TEST_ASSERT(snap.winningPlayer.has_value() && *snap.winningPlayer == 1001, "Player 1001 should be winner");
        TEST_ASSERT(!snap.isDraw, "Duel should not be a draw");
    }

    // -------------------------------------------------------------------------
    // TEST 2: Duel Requires Exactly 2 Players
    // -------------------------------------------------------------------------
    void Test_Duel_InvalidPlayerCount()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(!rules.Start(config, players, err), "Duel should reject 1 player");
        TEST_ASSERT(!err.empty(), "Error message should be set");

        std::vector<PlayerState> threePlayers{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true },
            { 1003, 0, 0, 0, true, true }
        };
        TEST_ASSERT(!rules.Start(config, threePlayers, err), "Duel should reject 3 players");
    }

    // -------------------------------------------------------------------------
    // TEST 3: Duel Timeout Results in Draw
    // -------------------------------------------------------------------------
    void Test_Duel_TimeoutDraw()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Duel started");

        TEST_ASSERT(rules.AdvanceTime(300), "Advance full 300s");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Match complete on timeout");
        TEST_ASSERT(snap.isDraw, "Duel timeout without deaths must be a draw");
        TEST_ASSERT(!snap.winningPlayer.has_value(), "No winner on draw");
    }

    // -------------------------------------------------------------------------
    // TEST 4: Duel Disconnect Forfeit
    // -------------------------------------------------------------------------
    void Test_Duel_DisconnectForfeit()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Duel started");

        TEST_ASSERT(rules.HandleDisconnect(1002), "Disconnect player 1002");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Match complete on forfeit");
        TEST_ASSERT(snap.winningPlayer.has_value() && *snap.winningPlayer == 1001, "Remaining player wins");
    }

    // -------------------------------------------------------------------------
    // TEST 5: Duel Suicide / Environmental Death
    // -------------------------------------------------------------------------
    void Test_Duel_SuicideOrEnvironment()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Duel started");

        // Player 1002 dies from environment (killer = nullopt)
        TEST_ASSERT(rules.RecordDeath(1002, std::nullopt), "Environmental death recorded");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Match complete on environmental death");
        TEST_ASSERT(snap.winningPlayer.has_value() && *snap.winningPlayer == 1001, "Surviving player wins");
    }

    // -------------------------------------------------------------------------
    // TEST 6: Death While Dead Rejected
    // -------------------------------------------------------------------------
    void Test_DeathWhileDead_Rejected()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::CombatOrdeal, 300, 10 };
        std::vector<PlayerState> players{
            { 2001, 0, 0, 0, true, true },
            { 2002, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Ordeal started");

        TEST_ASSERT(rules.RecordDeath(2002, 2001), "First death succeeds");
        TEST_ASSERT(!rules.RecordDeath(2002, 2001), "Duplicate death while dead must be rejected");
    }

    // -------------------------------------------------------------------------
    // TEST 7: Stale Deaths After Complete Rejected
    // -------------------------------------------------------------------------
    void Test_StaleDeathAfterComplete_Rejected()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Duel started");
        TEST_ASSERT(rules.RecordDeath(1002, 1001), "Duel finished");
        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Complete, "Phase complete");

        TEST_ASSERT(!rules.RecordDeath(1001, 1002), "Stale death after completion rejected");
    }

    // -------------------------------------------------------------------------
    // TEST 8: Duplicate Player IDs Rejected
    // -------------------------------------------------------------------------
    void Test_DuplicatePlayerIDs_Rejected()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1001, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(!rules.Start(config, players, err), "Duplicate IDs must be rejected");
    }

    // -------------------------------------------------------------------------
    // TEST 9: Disconnected Player At Start Rejected
    // -------------------------------------------------------------------------
    void Test_DisconnectedPlayerAtStart_Rejected()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 1001, 0, 0, 0, true, true },
            { 1002, 1, 0, 0, false, true } // disconnected
        };

        std::string err;
        TEST_ASSERT(!rules.Start(config, players, err), "Disconnected player must be rejected");
    }

    // -------------------------------------------------------------------------
    // TEST 10: United Combat Unbalanced Teams Rejected
    // -------------------------------------------------------------------------
    void Test_UnitedCombat_UnbalancedTeams_Rejected()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::UnitedCombat, 300, 10 };
        std::vector<PlayerState> players{
            { 3001, 0, 0, 0, true, true },
            { 3002, 0, 0, 0, true, true },
            { 3003, 0, 0, 0, true, true },
            { 3004, 1, 0, 0, true, true } // 3 vs 1 -> difference is 2 (must be <= 1)
        };

        std::string err;
        TEST_ASSERT(!rules.Start(config, players, err), "Unbalanced teams (3v1) must be rejected");
    }

    // -------------------------------------------------------------------------
    // TEST 11: United Combat Team Disconnect
    // -------------------------------------------------------------------------
    void Test_UnitedCombat_TeamDisconnect()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::UnitedCombat, 300, 10 };
        std::vector<PlayerState> players{
            { 3001, 0, 0, 0, true, true },
            { 3002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "United Combat started");

        TEST_ASSERT(rules.HandleDisconnect(3002), "Team 1 player disconnects");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Complete when team disconnects");
        TEST_ASSERT(snap.winningTeam.has_value() && *snap.winningTeam == 0, "Team 0 wins by abandonment");
    }

    // -------------------------------------------------------------------------
    // TEST 12: Friendly Fire Gives No Score
    // -------------------------------------------------------------------------
    void Test_FriendlyFire_NoScore()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::UnitedCombat, 300, 10 };
        std::vector<PlayerState> players{
            { 3001, 0, 0, 0, true, true },
            { 3002, 0, 0, 0, true, true },
            { 3003, 1, 0, 0, true, true },
            { 3004, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Started");

        // Player 3001 kills teammate 3002
        TEST_ASSERT(rules.RecordDeath(3002, 3001), "Teammate death recorded");
        auto snap = rules.Snapshot();
        for (const auto& p : snap.players)
        {
            TEST_ASSERT(p.score == 0, "Friendly fire must not award score");
        }
    }

    // -------------------------------------------------------------------------
    // TEST 13: Combat Ordeal Score Cap & Respawn
    // -------------------------------------------------------------------------
    void Test_CombatOrdeal_ScoreCapAndRespawn()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::CombatOrdeal, 300, 3 };
        std::vector<PlayerState> players{
            { 2001, 0, 0, 0, true, true },
            { 2002, 0, 0, 0, true, true },
            { 2003, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Ordeal should start with 3 players");

        TEST_ASSERT(rules.RecordDeath(2002, 2001), "Kill 1 recorded");
        TEST_ASSERT(rules.Respawn(2002), "Respawn 2002");
        TEST_ASSERT(rules.RecordDeath(2002, 2001), "Kill 2 recorded");
        TEST_ASSERT(rules.Respawn(2002), "Respawn 2002");

        auto snapMid = rules.Snapshot();
        TEST_ASSERT(snapMid.phase == MatchPhase::Active, "Match still active at 2 kills");

        TEST_ASSERT(rules.RecordDeath(2003, 2001), "Kill 3 recorded");

        auto snapEnd = rules.Snapshot();
        TEST_ASSERT(snapEnd.phase == MatchPhase::Complete, "Match complete after hitting score limit");
        TEST_ASSERT(snapEnd.winningPlayer.has_value() && *snapEnd.winningPlayer == 2001, "Player 2001 won");
    }

    // -------------------------------------------------------------------------
    // TEST 14: Score Limit Zero (Purely Timed)
    // -------------------------------------------------------------------------
    void Test_ScoreLimitZero_TimedOnly()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::CombatOrdeal, 300, 0 /* 0 = purely timed */ };
        std::vector<PlayerState> players{
            { 2001, 0, 0, 0, true, true },
            { 2002, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Ordeal started with scoreLimit = 0");

        // High kill count does not end match early
        for (int i = 0; i < 50; ++i)
        {
            TEST_ASSERT(rules.RecordDeath(2002, 2001), "Kill recorded");
            TEST_ASSERT(rules.Respawn(2002), "Respawn recorded");
        }

        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Active, "Match remains active despite 50 kills");
        TEST_ASSERT(rules.AdvanceTime(300), "Timeout");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Complete on timer expiry");
        TEST_ASSERT(snap.winningPlayer.has_value() && *snap.winningPlayer == 2001, "Player 2001 won by high score");
    }

    // -------------------------------------------------------------------------
    // TEST 15: Timer Advance And Draw
    // -------------------------------------------------------------------------
    void Test_TimerAdvance_And_Draw()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::CombatOrdeal, 60, 10 };
        std::vector<PlayerState> players{
            { 4001, 0, 0, 0, true, true },
            { 4002, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Match started");

        TEST_ASSERT(rules.AdvanceTime(30), "Advance 30s");
        TEST_ASSERT(rules.Snapshot().secondsRemaining == 30, "30s remaining");
        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Active, "Still active");

        TEST_ASSERT(rules.AdvanceTime(30), "Advance to end");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Completed by timeout");
        TEST_ASSERT(snap.isDraw, "Tied at 0-0 should be a draw");
    }

    // -------------------------------------------------------------------------
    // TEST 16: Timer Overrun Clamps To Zero
    // -------------------------------------------------------------------------
    void Test_TimerOverrun_ClampedToZero()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::CombatOrdeal, 60, 10 };
        std::vector<PlayerState> players{
            { 4001, 0, 0, 0, true, true },
            { 4002, 0, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Match started");

        TEST_ASSERT(rules.AdvanceTime(120), "Advance 120s on 60s timer");
        auto snap = rules.Snapshot();
        TEST_ASSERT(snap.secondsRemaining == 0, "Timer clamped to 0");
        TEST_ASSERT(snap.phase == MatchPhase::Complete, "Phase Complete");
    }

    // -------------------------------------------------------------------------
    // TEST 17: Match Cancellation
    // -------------------------------------------------------------------------
    void Test_MatchCancellation()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 5001, 0, 0, 0, true, true },
            { 5002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Started");
        rules.Cancel();

        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Cancelled, "Phase should be Cancelled");
        TEST_ASSERT(!rules.RecordDeath(5002, 5001), "Deaths should be rejected when cancelled");
    }

    // -------------------------------------------------------------------------
    // TEST 18: Reset And Restart Clean Lifecycle
    // -------------------------------------------------------------------------
    void Test_ResetAndRestart()
    {
        MatchRules rules;
        MatchConfig config{ MatchMode::Duel, 300, 1 };
        std::vector<PlayerState> players{
            { 6001, 0, 0, 0, true, true },
            { 6002, 1, 0, 0, true, true }
        };

        std::string err;
        TEST_ASSERT(rules.Start(config, players, err), "Match 1 started");
        TEST_ASSERT(rules.RecordDeath(6002, 6001), "Match 1 finished");
        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Complete, "Match 1 Complete");

        // Reset
        rules.Reset();
        auto snapReset = rules.Snapshot();
        TEST_ASSERT(snapReset.phase == MatchPhase::Lobby, "Phase reset to Lobby");
        TEST_ASSERT(snapReset.players.empty(), "Players cleared");
        TEST_ASSERT(!snapReset.winningPlayer.has_value(), "Winner cleared");
        TEST_ASSERT(!snapReset.isDraw, "Draw flag cleared");

        // Start Match 2
        std::vector<PlayerState> newPlayers{
            { 7001, 0, 0, 0, true, true },
            { 7002, 1, 0, 0, true, true }
        };
        TEST_ASSERT(rules.Start(config, newPlayers, err), "Match 2 started cleanly");
        TEST_ASSERT(rules.Snapshot().phase == MatchPhase::Active, "Match 2 is Active");
    }
} // anonymous namespace

int main()
{
    std::cout << "=================================================\n";
    std::cout << "SeamlessArena.Core Automated Test Suite (CI Gate)\n";
    std::cout << "=================================================\n";

    RUN_TEST(Test_Duel_WinCondition);
    RUN_TEST(Test_Duel_InvalidPlayerCount);
    RUN_TEST(Test_Duel_TimeoutDraw);
    RUN_TEST(Test_Duel_DisconnectForfeit);
    RUN_TEST(Test_Duel_SuicideOrEnvironment);
    RUN_TEST(Test_DeathWhileDead_Rejected);
    RUN_TEST(Test_StaleDeathAfterComplete_Rejected);
    RUN_TEST(Test_DuplicatePlayerIDs_Rejected);
    RUN_TEST(Test_DisconnectedPlayerAtStart_Rejected);
    RUN_TEST(Test_UnitedCombat_UnbalancedTeams_Rejected);
    RUN_TEST(Test_UnitedCombat_TeamDisconnect);
    RUN_TEST(Test_FriendlyFire_NoScore);
    RUN_TEST(Test_CombatOrdeal_ScoreCapAndRespawn);
    RUN_TEST(Test_ScoreLimitZero_TimedOnly);
    RUN_TEST(Test_TimerAdvance_And_Draw);
    RUN_TEST(Test_TimerOverrun_ClampedToZero);
    RUN_TEST(Test_MatchCancellation);
    RUN_TEST(Test_ResetAndRestart);

    std::cout << "=================================================\n";
    std::cout << "Test Summary: " << g_passedTests << " Passed, "
              << g_failedTests << " Failed\n";
    std::cout << "=================================================\n";

    return g_failedTests == 0 ? 0 : 1;
}
