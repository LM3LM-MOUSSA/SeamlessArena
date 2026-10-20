// =============================================================================
// ConfigLoader.cpp — TOML config parser implementation
// =============================================================================
// RESEARCH: rechres/09-configuration-logging-recovery.md (Section 1)
// =============================================================================

#include "ConfigLoader.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace seamless_arena::native
{
    namespace
    {
        // Trim leading and trailing whitespace
        std::string Trim(const std::string& str)
        {
            const auto start = str.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return "";
            const auto end = str.find_last_not_of(" \t\r\n");
            return str.substr(start, end - start + 1);
        }

        // Convert string to lowercase
        std::string ToLower(std::string str)
        {
            std::transform(str.begin(), str.end(), str.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return str;
        }

        // Parse boolean ("true", "false", "1", "0")
        bool ParseBool(const std::string& val, bool defaultVal)
        {
            std::string s = ToLower(Trim(val));
            if (s == "true" || s == "1" || s == "yes")
                return true;
            if (s == "false" || s == "0" || s == "no")
                return false;
            return defaultVal;
        }

        // Parse unsigned integer
        uint32_t ParseUint32(const std::string& val, uint32_t defaultVal)
        {
            try
            {
                std::string s = Trim(val);
                if (s.empty()) return defaultVal;
                unsigned long parsed = std::stoul(s);
                return static_cast<uint32_t>(parsed);
            }
            catch (...)
            {
                return defaultVal;
            }
        }

        // Parse float
        float ParseFloat(const std::string& val, float defaultVal)
        {
            try
            {
                std::string s = Trim(val);
                if (s.empty()) return defaultVal;
                return std::stof(s);
            }
            catch (...)
            {
                return defaultVal;
            }
        }

        // Parse quoted string: "foo" -> foo
        std::string ParseQuotedString(const std::string& val)
        {
            std::string s = Trim(val);
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            {
                return s.substr(1, s.size() - 2);
            }
            return s;
        }
    } // anonymous namespace

    // -------------------------------------------------------------------------
    // Parse configuration from file
    // -------------------------------------------------------------------------
    ArenaConfig LoadConfig(const std::string& configPath)
    {
        ArenaConfig config{};

        std::ifstream file(configPath);
        if (!file.is_open())
        {
            SA_LOG_INFO("ConfigLoader",
                "No config file found at %s. Using default Alpha configuration.",
                configPath.c_str());
            return config;
        }

        std::string currentSection;
        std::string line;
        uint32_t lineNumber = 0;

        while (std::getline(file, line))
        {
            lineNumber++;
            line = Trim(line);

            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;

            // Section header: [section_name]
            if (line.front() == '[' && line.back() == ']')
            {
                currentSection = ToLower(Trim(line.substr(1, line.size() - 2)));
                continue;
            }

            // Key-value pair: key = value
            const auto eqPos = line.find('=');
            if (eqPos == std::string::npos)
                continue;

            std::string key = ToLower(Trim(line.substr(0, eqPos)));
            std::string val = Trim(line.substr(eqPos + 1));

            // Strip trailing inline comment if present
            const auto commentPos = val.find_first_of("#;");
            if (commentPos != std::string::npos)
            {
                val = Trim(val.substr(0, commentPos));
            }

            // Route key-value by section
            if (currentSection == "general")
            {
                if (key == "log_level")
                {
                    std::string levelStr = ToLower(ParseQuotedString(val));
                    if (levelStr == "trace") config.logLevel = LogLevel::Trace;
                    else if (levelStr == "debug") config.logLevel = LogLevel::Debug;
                    else if (levelStr == "info")  config.logLevel = LogLevel::Info;
                    else if (levelStr == "warn")  config.logLevel = LogLevel::Warn;
                    else if (levelStr == "error") config.logLevel = LogLevel::Error;
                    else if (levelStr == "fatal") config.logLevel = LogLevel::Fatal;
                }
            }
            else if (currentSection == "item")
            {
                if (key == "duel_goods_id")
                {
                    config.duelGoodsId = ParseUint32(val, config.duelGoodsId);
                }
            }
            else if (currentSection == "rules.duel")
            {
                if (key == "duration_seconds")
                    config.duelDurationSeconds = ParseUint32(val, config.duelDurationSeconds);
                else if (key == "max_crimson_flasks")
                    config.duelMaxCrimson = static_cast<uint8_t>(ParseUint32(val, config.duelMaxCrimson));
                else if (key == "max_cerulean_flasks")
                    config.duelMaxCerulean = static_cast<uint8_t>(ParseUint32(val, config.duelMaxCerulean));
                else if (key == "auto_physick")
                    config.duelAutoPhysick = ParseBool(val, config.duelAutoPhysick);
                else if (key == "restore_consumables")
                    config.duelRestoreItems = ParseBool(val, config.duelRestoreItems);
            }
            else if (currentSection == "rules.ordeal")
            {
                if (key == "duration_seconds")
                    config.ordealDurationSeconds = ParseUint32(val, config.ordealDurationSeconds);
                else if (key == "score_limit")
                    config.ordealScoreLimit = static_cast<uint16_t>(ParseUint32(val, config.ordealScoreLimit));
                else if (key == "respawn_delay_seconds")
                    config.ordealRespawnDelay = ParseFloat(val, config.ordealRespawnDelay);
                else if (key == "spawn_invulnerability_seconds")
                    config.ordealSpawnInvulnSeconds = ParseFloat(val, config.ordealSpawnInvulnSeconds);
            }
            else if (currentSection == "rules.united_combat")
            {
                if (key == "duration_seconds")
                    config.unitedDurationSeconds = ParseUint32(val, config.unitedDurationSeconds);
                else if (key == "score_limit")
                    config.unitedScoreLimit = static_cast<uint16_t>(ParseUint32(val, config.unitedScoreLimit));
                else if (key == "friendly_fire")
                    config.unitedFriendlyFire = ParseBool(val, config.unitedFriendlyFire);
            }
            else if (currentSection == "arena")
            {
                if (key == "default_colosseum")
                {
                    std::string colosseumStr = ToLower(ParseQuotedString(val));
                    if (colosseumStr == "royal")
                        config.defaultColosseum = ColosseumId::Royal;
                    else if (colosseumStr == "caelid")
                        config.defaultColosseum = ColosseumId::Caelid;
                    else if (colosseumStr == "limgrave")
                        config.defaultColosseum = ColosseumId::Limgrave;
                }
                else if (key == "allow_spirit_ashes")
                {
                    config.allowSpiritAshes = ParseBool(val, config.allowSpiritAshes);
                }
            }
        }

        SA_LOG_INFO("ConfigLoader",
            "Configuration successfully loaded from %s (Duel Item ID: %u, Arena: %u, MaxCrimson: %u, MaxCerulean: %u)",
            configPath.c_str(), config.duelGoodsId, static_cast<uint8_t>(config.defaultColosseum),
            config.duelMaxCrimson, config.duelMaxCerulean);

        return config;
    }

    // -------------------------------------------------------------------------
    // Apply loaded config to the ArenaCoordinator's default lobby settings
    // -------------------------------------------------------------------------
    LobbySettings ConfigToLobbyDefaults(const ArenaConfig& config,
                                        seamless_arena::core::MatchMode mode)
    {
        LobbySettings settings{};
        settings.mode  = mode;
        // In Alpha release, lock to Limgrave Colosseum
        settings.arena = (mode == seamless_arena::core::MatchMode::Duel)
            ? ColosseumId::Limgrave
            : config.defaultColosseum;
        settings.allowSpiritAshes = config.allowSpiritAshes;

        switch (mode)
        {
        case seamless_arena::core::MatchMode::Duel:
            settings.durationSeconds   = config.duelDurationSeconds > 0 ? config.duelDurationSeconds : 300;
            settings.scoreLimit        = 1; // 1v1 Duel ends on first death
            settings.maxCrimsonFlasks  = config.duelMaxCrimson;  // 0 for duel integrity
            settings.maxCeruleanFlasks = config.duelMaxCerulean; // 3
            settings.autoPhysick       = config.duelAutoPhysick;
            break;

        case seamless_arena::core::MatchMode::CombatOrdeal:
            settings.durationSeconds   = config.ordealDurationSeconds;
            settings.scoreLimit        = config.ordealScoreLimit;
            settings.maxCrimsonFlasks  = 3;
            settings.maxCeruleanFlasks = 3;
            settings.autoPhysick       = true;
            break;

        case seamless_arena::core::MatchMode::UnitedCombat:
            settings.durationSeconds   = config.unitedDurationSeconds;
            settings.scoreLimit        = config.unitedScoreLimit;
            settings.maxCrimsonFlasks  = 3;
            settings.maxCeruleanFlasks = 3;
            settings.autoPhysick       = true;
            break;
        }

        return settings;
    }

} // namespace seamless_arena::native

