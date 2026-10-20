#pragma once
// =============================================================================
// ConfigLoader.h — SeamlessArena.toml configuration parser
// =============================================================================
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
// RESEARCH: rechres/10-assets-licensing-distribution.md  (Section 3)
//
// Reads the TOML configuration file and populates runtime defaults for
// match rules, flask limits, logging levels, and arena preferences.
// =============================================================================

#include "ArenaCoordinator.h"
#include "Logger.h"

#include <string>

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // Configuration data read from SeamlessArena.toml
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1.1)
    // -------------------------------------------------------------------------
    struct ArenaConfig
    {
        // [general]
        LogLevel logLevel = LogLevel::Info;

        // [item]
        std::uint32_t duelGoodsId          = 101; // Duelist's Furled Finger (vanilla)

        // [rules.duel]
        std::uint32_t duelDurationSeconds  = 300;
        std::uint8_t  duelMaxCrimson       = 0;
        std::uint8_t  duelMaxCerulean      = 3;
        bool          duelAutoPhysick      = false;
        bool          duelRestoreItems     = false;

        // [rules.ordeal]
        std::uint32_t ordealDurationSeconds       = 300;
        std::uint16_t ordealScoreLimit             = 10;
        float         ordealRespawnDelay           = 4.0f;
        float         ordealSpawnInvulnSeconds     = 3.0f;

        // [rules.united_combat]
        std::uint32_t unitedDurationSeconds = 300;
        std::uint16_t unitedScoreLimit      = 15;
        bool          unitedFriendlyFire    = false;

        // [arena]
        ColosseumId   defaultColosseum     = ColosseumId::Limgrave;
        bool          allowSpiritAshes     = false;
    };

    // -------------------------------------------------------------------------
    // Parse configuration from file
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
    //
    // PSEUDO:
    //   1. Locate SeamlessArena.toml adjacent to the DLL or game exe
    //   2. If file not found: use defaults, log info
    //   3. If file found: parse TOML sections:
    //        [general]       -> logLevel
    //        [rules.duel]    -> duel* fields
    //        [rules.ordeal]  -> ordeal* fields
    //        [rules.united_combat] -> united* fields
    //        [arena]         -> defaultColosseum, allowSpiritAshes
    //   4. Validate ranges (e.g. durationSeconds > 0, flask counts 0-14)
    //   5. If invalid value: log warning, use default for that field
    //   6. Return populated ArenaConfig
    //
    // TODO(rechres/09): Choose TOML parsing library
    //   Options: toml++ (header-only C++17), toml11, or manual ini-style parser
    // -------------------------------------------------------------------------
    ArenaConfig LoadConfig(const std::string& configPath);

    // -------------------------------------------------------------------------
    // Apply loaded config to the ArenaCoordinator's default lobby settings
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
    // -------------------------------------------------------------------------
    LobbySettings ConfigToLobbyDefaults(const ArenaConfig& config,
                                        seamless_arena::core::MatchMode mode);

} // namespace seamless_arena::native
