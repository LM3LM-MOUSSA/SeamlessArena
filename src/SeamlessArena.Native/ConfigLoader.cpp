// =============================================================================
// ConfigLoader.cpp — TOML config parser implementation
// =============================================================================
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
// =============================================================================

#include "ConfigLoader.h"
#include "Logger.h"

namespace seamless_arena::native
{
    // -------------------------------------------------------------------------
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
    //
    // PSEUDO:
    //   Open configPath for reading
    //   If file does not exist:
    //       SA_LOG_INFO("ConfigLoader", "No config file found at %s, using defaults.", path)
    //       Return default ArenaConfig{}
    //
    //   Parse TOML using chosen library (TODO: integrate toml++)
    //
    //   For each key:
    //     Read value, validate type and range
    //     If invalid: log warning, keep default
    //     If valid: assign to ArenaConfig field
    //
    //   SA_LOG_INFO("ConfigLoader", "Configuration loaded from %s", path)
    //   Return config
    // -------------------------------------------------------------------------
    ArenaConfig LoadConfig(const std::string& configPath)
    {
        ArenaConfig config{};

        // TODO(rechres/09): Implement TOML parsing
        //   For now, return all defaults
        SA_LOG_INFO("ConfigLoader",
            "Config loading not yet implemented. Using defaults. Path: %s",
            configPath.c_str());

        return config;
    }

    // -------------------------------------------------------------------------
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1.1)
    //
    // PSEUDO:
    //   Based on mode, select the appropriate rule fields from config
    //   Populate LobbySettings struct
    // -------------------------------------------------------------------------
    LobbySettings ConfigToLobbyDefaults(const ArenaConfig& config,
                                        seamless_arena::core::MatchMode mode)
    {
        LobbySettings settings{};
        settings.mode  = mode;
        settings.arena = config.defaultColosseum;
        settings.allowSpiritAshes = config.allowSpiritAshes;

        switch (mode)
        {
        case seamless_arena::core::MatchMode::Duel:
            settings.durationSeconds  = config.duelDurationSeconds;
            settings.scoreLimit       = 1;
            settings.maxCrimsonFlasks  = config.duelMaxCrimson;
            settings.maxCeruleanFlasks = config.duelMaxCerulean;
            settings.autoPhysick      = config.duelAutoPhysick;
            break;

        case seamless_arena::core::MatchMode::CombatOrdeal:
            settings.durationSeconds  = config.ordealDurationSeconds;
            settings.scoreLimit       = config.ordealScoreLimit;
            settings.maxCrimsonFlasks  = 3; // TODO(rechres/06): Make configurable
            settings.maxCeruleanFlasks = 3;
            settings.autoPhysick      = true;
            break;

        case seamless_arena::core::MatchMode::UnitedCombat:
            settings.durationSeconds  = config.unitedDurationSeconds;
            settings.scoreLimit       = config.unitedScoreLimit;
            settings.maxCrimsonFlasks  = 3;
            settings.maxCeruleanFlasks = 3;
            settings.autoPhysick      = true;
            break;
        }

        return settings;
    }

} // namespace seamless_arena::native
