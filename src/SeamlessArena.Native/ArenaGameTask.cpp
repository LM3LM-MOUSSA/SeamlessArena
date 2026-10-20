// =============================================================================
// ArenaGameTask.cpp — Native FromSoftware task scheduler callback
// =============================================================================
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3)
// RESEARCH: rechres/08-game-events-hooks.md        (Section 2 row 4)
// =============================================================================

#include "ArenaGameTask.h"
#include "ArenaCoordinator.h"
#include "EngineBridge.h"
#include "Logger.h"

#include <Windows.h>

namespace seamless_arena::native
{
    ArenaGameTask g_ArenaGameTask;

    namespace
    {
        bool TryRegisterEzTask(from::CS::CSEzTask* task, from::CS::CSTaskGroup group) noexcept
        {
            __try
            {
                task->register_task(group);
                return (task->get_task_group() != from::CS::CSTaskGroup::INVALID);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void TryFreeEzTask(from::CS::CSEzTask* task) noexcept
        {
            __try
            {
                task->from::CS::CSEzTask::free_task();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    } // anonymous namespace

    ArenaGameTask::~ArenaGameTask()
    {
        Stop();
    }

    void ArenaGameTask::eztask_execute(from::FD4::FD4TaskData* data)
    {
        m_activeCallbacks.fetch_add(1, std::memory_order_acq_rel);
        struct CallbackExit
        {
            std::atomic<std::uint32_t>& activeCallbacks;

            ~CallbackExit()
            {
                if (activeCallbacks.fetch_sub(1, std::memory_order_release) == 1)
                    activeCallbacks.notify_all();
            }
        } callbackExit{ m_activeCallbacks };

        // libER documents that free_task() may still allow a callback from the
        // current task-group pass. Such a late callback must not touch the
        // coordinator after ME3 finalization has started.
        if (m_stopRequested.load(std::memory_order_acquire))
            return;

        float dt = 0.0166667f;
        if (data != nullptr)
        {
            dt = static_cast<float>(data->get_dt());
            // Sanity bounds on frame time (avoid extreme jumps during loading screens)
            if (dt < 0.0f || dt > 1.0f)
            {
                dt = 0.0166667f;
            }
        }

        // Execute coordinator tick on the game thread
        g_ArenaCoordinator.Tick(dt);
    }

    void ArenaGameTask::free_task()
    {
        m_stopRequested.store(true, std::memory_order_release);
        m_shutdown.store(true, std::memory_order_release);
        m_running.store(false, std::memory_order_release);
        TryFreeEzTask(this);
    }

    bool ArenaGameTask::Start()
    {
        std::lock_guard lifecycleLock(m_lifecycleMutex);
        if (m_shutdown.load(std::memory_order_acquire))
        {
            SA_LOG_WARN("ArenaGameTask", "Task restart rejected after shutdown began.");
            return false;
        }

        if (m_running.load(std::memory_order_acquire))
            return true;

        if (!EngineBridge::IsSystemInitialized())
        {
            SA_LOG_WARN("ArenaGameTask", "Cannot register task: Elden Ring system (DLSY) is not initialized yet.");
            return false;
        }

        m_stopRequested.store(false, std::memory_order_release);
        const bool registered = TryRegisterEzTask(this, from::CS::CSTaskGroup::FrameBegin);
        m_running.store(registered, std::memory_order_release);

        if (registered)
        {
            SA_LOG_INFO("ArenaGameTask",
                "ArenaGameTask successfully registered into FromSoftware task queue (FrameBegin).");
        }
        else
        {
            m_stopRequested.store(true, std::memory_order_release);
            SA_LOG_WARN("ArenaGameTask",
                "Failed to register ArenaGameTask into FromSoftware task queue.");
        }

        return registered;
    }

    void ArenaGameTask::Stop()
    {
        std::lock_guard lifecycleLock(m_lifecycleMutex);
        m_shutdown.store(true, std::memory_order_release);
        m_stopRequested.store(true, std::memory_order_release);

        if (m_running.exchange(false, std::memory_order_acq_rel))
        {
            TryFreeEzTask(this);
        }

        auto activeCallbacks = m_activeCallbacks.load(std::memory_order_acquire);
        while (activeCallbacks != 0)
        {
            m_activeCallbacks.wait(activeCallbacks, std::memory_order_acquire);
            activeCallbacks = m_activeCallbacks.load(std::memory_order_acquire);
        }

        SA_LOG_INFO("ArenaGameTask", "ArenaGameTask unregistered from FromSoftware task queue.");
    }
}
