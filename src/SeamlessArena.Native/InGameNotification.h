#pragma once
// =============================================================================
// InGameNotification.h — On-screen in-game notification banner subsystem
// =============================================================================
// Provides Elden Ring styled, non-intrusive on-screen visual banners and 
// audio feedback so players always know what SeamlessArena is doing in-game.
// =============================================================================

#include <string>

namespace seamless_arena::native
{
    enum class NotificationType
    {
        Info,
        Success,
        Warning,
        Error
    };

    class InGameNotification
    {
    public:
        // Initialize the notification subsystem and worker thread.
        static bool Initialize() noexcept;

        // Cleanly shut down the notification window and thread.
        static void Shutdown() noexcept;

        // Display an on-screen banner message in-game for durationSeconds.
        static void Show(const std::string& title,
                         const std::string& message,
                         NotificationType type = NotificationType::Info,
                         float durationSeconds = 4.0f) noexcept;
    };
}
