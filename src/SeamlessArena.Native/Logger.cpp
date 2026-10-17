// =============================================================================
// Logger.cpp — Diagnostic logging implementation
// =============================================================================
// RESEARCH: rechres/09-configuration-logging-recovery.md  (Section 2)
// RESEARCH: rechres/03-me3-native-contract.md             (Section 5)
//
// Log format:
//   [2026-10-17 19:30:15.123] [INFO]  [Bootstrap] SeamlessArena v0.1.0 ...
//
// Thread safety: uses a std::mutex around file I/O for simplicity in the
// first implementation. Resolves log path relative to SeamlessArena.dll so
// it does not depend on the current process working directory.
// =============================================================================

#include "Logger.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>

namespace seamless_arena::native
{
    // ---- internal state -----------------------------------------------------
    namespace
    {
        FILE*      g_logFile  = nullptr;
        LogLevel   g_minLevel = LogLevel::Info;
        std::mutex g_logMutex;

        const char* LevelToString(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default:              return "?????";
            }
        }

        std::string ResolveLogFilePath(std::string_view filename)
        {
            std::filesystem::path inputPath(filename);
            if (inputPath.is_absolute())
            {
                return inputPath.string();
            }

            wchar_t modulePath[MAX_PATH] = { 0 };
            HMODULE hModule = nullptr;
            if (GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&Logger_Init),
                    &hModule) && hModule)
            {
                if (GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0)
                {
                    std::filesystem::path dllDir = std::filesystem::path(modulePath).parent_path();
                    std::filesystem::path fullPath = dllDir / inputPath;
                    return fullPath.string();
                }
            }

            return std::string(filename);
        }
    } // anonymous namespace

    // -------------------------------------------------------------------------
    // Open log file, set minimum severity filter
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2.1)
    // RESEARCH: docs/TODO.md (Phase 1 Item 7)
    // -------------------------------------------------------------------------
    bool Logger_Init(std::string_view logFilePath, LogLevel minLevel)
    {
        std::lock_guard lock(g_logMutex);
        if (g_logFile) return false; // already open

        g_minLevel = minLevel;

        std::string resolvedPath = ResolveLogFilePath(logFilePath);

        // Ensure parent directory exists
        try
        {
            std::filesystem::path parentDir = std::filesystem::path(resolvedPath).parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir))
            {
                std::filesystem::create_directories(parentDir);
            }
        }
        catch (...)
        {
            // Ignore directory creation errors and attempt fopen
        }

        fopen_s(&g_logFile, resolvedPath.c_str(), "w");
        if (!g_logFile)
        {
            char errorMsg[512];
            sprintf_s(errorMsg, "[SeamlessArena] Failed to open log file at '%s'!\n", resolvedPath.c_str());
            OutputDebugStringA(errorMsg);
            return false;
        }

        char debugMsg[512];
        sprintf_s(debugMsg, "[SeamlessArena] Log initialized: %s\n", resolvedPath.c_str());
        OutputDebugStringA(debugMsg);

        // Write banner
        fprintf(g_logFile, "=== SeamlessArena Log Started ===\n");
        fprintf(g_logFile, "Path: %s\n", resolvedPath.c_str());
        fflush(g_logFile);
        return true;
    }

    // -------------------------------------------------------------------------
    // Flush remaining buffer and close the file handle
    // RESEARCH: rechres/03-me3-native-contract.md (Section 3 — Finalize)
    // -------------------------------------------------------------------------
    void Logger_Shutdown()
    {
        std::lock_guard lock(g_logMutex);
        if (g_logFile)
        {
            fprintf(g_logFile, "=== SeamlessArena Log Closed ===\n");
            fclose(g_logFile);
            g_logFile = nullptr;
        }
    }

    // -------------------------------------------------------------------------
    // Format timestamp + level + category + message, write to file
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2.1)
    // -------------------------------------------------------------------------
    void Logger_Log(LogLevel level, const char* category, const char* fmt, ...)
    {
        if (level < g_minLevel) return;

        std::lock_guard lock(g_logMutex);
        if (!g_logFile) return;

        // Timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_logFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s] ",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                LevelToString(level), category);

        // Message
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);

        fprintf(g_logFile, "\n");
        fflush(g_logFile);
    }

} // namespace seamless_arena::native
