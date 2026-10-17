#pragma once
// =============================================================================
// Logger.h — Diagnostic logging subsystem
// =============================================================================
// RESEARCH: rechres/09-configuration-logging-recovery.md  (Section 2)
// RESEARCH: rechres/03-me3-native-contract.md             (Section 5)
//
// Thread-safe structured logger that writes timestamped events to
// SeamlessArena.log. Uses a lock-free ring buffer internally so that
// logging from the game thread never hitches rendering.
// =============================================================================

#include <cstdint>
#include <string_view>

namespace seamless_arena::native
{
    enum class LogLevel : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    // -------------------------------------------------------------------------
    // Initialize / Shutdown
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2.1)
    // Called from SeamlessArena_Initialize and SeamlessArena_Finalize
    // -------------------------------------------------------------------------
    bool Logger_Init(std::string_view logFilePath, LogLevel minLevel = LogLevel::Info);
    void Logger_Shutdown();

    // -------------------------------------------------------------------------
    // Logging entry points — safe to call from any thread
    // RESEARCH: rechres/09-configuration-logging-recovery.md (Section 2.2)
    // -------------------------------------------------------------------------
    void Logger_Log(LogLevel level, const char* category, const char* fmt, ...);

    // Convenience macros
    #define SA_LOG_TRACE(cat, ...) ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Trace, cat, __VA_ARGS__)
    #define SA_LOG_DEBUG(cat, ...) ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Debug, cat, __VA_ARGS__)
    #define SA_LOG_INFO(cat, ...)  ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Info,  cat, __VA_ARGS__)
    #define SA_LOG_WARN(cat, ...)  ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Warn,  cat, __VA_ARGS__)
    #define SA_LOG_ERROR(cat, ...) ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Error, cat, __VA_ARGS__)
    #define SA_LOG_FATAL(cat, ...) ::seamless_arena::native::Logger_Log(::seamless_arena::native::LogLevel::Fatal, cat, __VA_ARGS__)

} // namespace seamless_arena::native
