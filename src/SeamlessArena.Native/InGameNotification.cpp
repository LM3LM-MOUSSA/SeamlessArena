// =============================================================================
// InGameNotification.cpp — Fail-safe, non-intrusive notification banner system
// =============================================================================
#include "InGameNotification.h"
#include "EngineBridge.h"
#include "Logger.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace seamless_arena::native
{
    namespace
    {
        struct NotificationRequest
        {
            std::string title;
            std::string message;
            NotificationType type{ NotificationType::Info };
            float durationSeconds{ 4.0f };
        };

        std::atomic<bool> g_initialized{ false };
        std::atomic<bool> g_stopRequested{ false };
        std::thread g_workerThread;
        std::mutex g_notifyMutex;
        std::condition_variable g_notifyCv;
        NotificationRequest g_activeRequest;
        bool g_hasNewRequest{ false };

        void NotificationWorkerProc()
        {
            while (!g_stopRequested.load(std::memory_order_acquire))
            {
                NotificationRequest req;
                {
                    std::unique_lock lock(g_notifyMutex);
                    g_notifyCv.wait(lock, []() {
                        return g_hasNewRequest || g_stopRequested.load(std::memory_order_acquire);
                    });

                    if (g_stopRequested.load(std::memory_order_acquire))
                        break;

                    if (!g_hasNewRequest)
                        continue;

                    req = g_activeRequest;
                    g_hasNewRequest = false;
                }

                HWND gameHwnd = static_cast<HWND>(EngineBridge::GetGameWindowHandle());
                if (gameHwnd && IsWindow(gameHwnd))
                {
                    std::string fullTitle = "ELDEN RING - [" + req.title + "] " + req.message;
                    SetWindowTextA(gameHwnd, fullTitle.c_str());

                    // Wait for duration or next request or shutdown
                    auto startTime = std::chrono::steady_clock::now();
                    const auto duration = std::chrono::milliseconds(static_cast<int>(req.durationSeconds * 1000.0f));

                    while (std::chrono::steady_clock::now() - startTime < duration &&
                           !g_stopRequested.load(std::memory_order_acquire))
                    {
                        {
                            std::lock_guard lock(g_notifyMutex);
                            if (g_hasNewRequest)
                                break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }

                    // Reset title to normal game caption if no new message came in
                    {
                        std::lock_guard lock(g_notifyMutex);
                        if (!g_hasNewRequest && !g_stopRequested.load(std::memory_order_acquire))
                        {
                            SetWindowTextA(gameHwnd, "ELDEN RING™");
                        }
                    }
                }
            }
        }
    } // anonymous namespace

    bool InGameNotification::Initialize() noexcept
    {
        if (g_initialized.exchange(true))
            return true;

        g_stopRequested.store(false, std::memory_order_release);
        g_hasNewRequest = false;

        try
        {
            g_workerThread = std::thread(NotificationWorkerProc);
            SA_LOG_INFO("InGameNotification", "Notification subsystem initialized successfully.");
            return true;
        }
        catch (...)
        {
            SA_LOG_ERROR("InGameNotification", "Failed to start notification worker thread.");
            g_initialized.store(false, std::memory_order_release);
            return false;
        }
    }

    void InGameNotification::Shutdown() noexcept
    {
        if (!g_initialized.exchange(false))
            return;

        g_stopRequested.store(true, std::memory_order_release);
        g_notifyCv.notify_all();

        if (g_workerThread.joinable())
        {
            g_workerThread.join();
        }

        // Restore window title if game window still exists
        HWND gameHwnd = static_cast<HWND>(EngineBridge::GetGameWindowHandle());
        if (gameHwnd && IsWindow(gameHwnd))
        {
            SetWindowTextA(gameHwnd, "ELDEN RING™");
        }

        SA_LOG_INFO("InGameNotification", "Notification subsystem shut down.");
    }

    void InGameNotification::Show(const std::string& title,
                                 const std::string& message,
                                 NotificationType type,
                                 float durationSeconds) noexcept
    {
        // 1. Diagnostic log
        SA_LOG_INFO("Notification", "[%s] %s", title.c_str(), message.c_str());

        // 2. Audible chime (safe Windows sound)
        EngineBridge::PlaySystemSound(1000);

        // 3. Immediate window title update if window is available
        HWND gameHwnd = static_cast<HWND>(EngineBridge::GetGameWindowHandle());
        if (gameHwnd && IsWindow(gameHwnd))
        {
            std::string winTitle = "ELDEN RING - [" + title + "] " + message;
            SetWindowTextA(gameHwnd, winTitle.c_str());
        }

        // 4. Queue to worker for timed reset
        if (!g_initialized.load(std::memory_order_acquire))
            return;

        {
            std::lock_guard lock(g_notifyMutex);
            g_activeRequest.title = title;
            g_activeRequest.message = message;
            g_activeRequest.type = type;
            g_activeRequest.durationSeconds = durationSeconds;
            g_hasNewRequest = true;
        }

        g_notifyCv.notify_one();
    }
}
