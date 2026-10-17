// =============================================================================
// ArenaGameTask.cpp — Native FromSoftware task scheduler callback
// =============================================================================
// RESEARCH: rechres/04-liber-abi-memory-safety.md (Section 3)
// RESEARCH: rechres/08-game-events-hooks.md        (Section 2 row 4)
// =============================================================================

#include "ArenaGameTask.h"
#include "ArenaCoordinator.h"
#include "Logger.h"

namespace seamless_arena::native
{
    ArenaGameTask g_ArenaGameTask;

    ArenaGameTask::~ArenaGameTask()
    {
        Stop();
    }

    void ArenaGameTask::eztask_execute(from::FD4::FD4TaskData* data)
    {
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
        m_running = false;
        from::CS::CSEzTask::free_task();
    }

    bool ArenaGameTask::Start()
    {
        if (m_running)
            return true;

        try
        {
            // Register into FrameBegin task group (the first task group in the engine frame)
            this->register_task(from::CS::CSTaskGroup::FrameBegin);
            m_running = (this->get_task_group() != from::CS::CSTaskGroup::INVALID);

            if (m_running)
            {
                SA_LOG_INFO("ArenaGameTask",
                    "ArenaGameTask successfully registered into FromSoftware task queue (FrameBegin).");
            }
            else
            {
                SA_LOG_WARN("ArenaGameTask",
                    "Failed to register ArenaGameTask into FromSoftware task queue.");
            }
        }
        catch (...)
        {
            SA_LOG_ERROR("ArenaGameTask", "Exception encountered while registering ArenaGameTask.");
            m_running = false;
        }

        return m_running;
    }

    void ArenaGameTask::Stop()
    {
        if (!m_running)
            return;

        try
        {
            this->free_task();
        }
        catch (...)
        {
            SA_LOG_WARN("ArenaGameTask", "Exception while freeing ArenaGameTask.");
        }

        m_running = false;
        SA_LOG_INFO("ArenaGameTask", "ArenaGameTask unregistered from FromSoftware task queue.");
    }
}
