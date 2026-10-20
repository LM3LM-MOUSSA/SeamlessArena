#pragma once
// =============================================================================
// ArenaGameTask.h — Native FromSoftware task scheduler callback
// =============================================================================
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3 — Thread Affinity)
// RESEARCH: rechres/08-game-events-hooks.md        (Section 2 row 4 — World Tick)
//
// Leverages libER's CSEzTask interface to schedule ArenaCoordinator::Tick
// natively inside Elden Ring's internal CSTaskManager on the game thread.
// Eliminates the need for fragile/invasive detours on the engine main loop.
// =============================================================================

#include <coresystem/task.hpp>
#include <fd4/detail/fd4_task.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>

namespace seamless_arena::native
{
    class ArenaGameTask : public from::CS::CSEzTask
    {
    public:
        ArenaGameTask() = default;
        ~ArenaGameTask() override;

        // Called natively by FromSoftware's engine on the game thread every frame.
        void eztask_execute(from::FD4::FD4TaskData* data) override;

        // Cleans up the task proxy upon destruction or explicit stop.
        void free_task() override;

        // Register the task into CSTaskGroup::FrameBegin.
        bool Start();

        // Remove the task from the engine queue.
        void Stop();

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_running.load(std::memory_order_acquire);
        }

    private:
        // Serializes ME3 lifecycle calls; the engine callback never takes this lock.
        std::mutex m_lifecycleMutex;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<bool> m_shutdown{ false };
        std::atomic<std::uint32_t> m_activeCallbacks{ 0 };
    };

    // Global task instance
    extern ArenaGameTask g_ArenaGameTask;
}
