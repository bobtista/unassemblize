/**
 * @file
 *
 * @brief Class to instigate all high level functionality for unassemblize
 *        from another thread
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "workqueue.h"

namespace unassemblize
{
WorkQueueCommandId WorkQueueCommand::s_id = InvalidWorkQueueCommandId + 1; // 1

WorkQueue::~WorkQueue()
{
    stop(true);
}

void WorkQueue::start()
{
    assert(!m_thread.joinable());
    m_thread = std::thread(ThreadFunction, this);
}

void WorkQueue::stop(bool wait)
{
    if (m_thread.joinable())
    {
        m_commandQueue.enqueue(std::make_unique<WorkQueueCommandQuit>(*this));
        if (wait)
            m_thread.join();
        else
            m_thread.detach();
    }
}

bool WorkQueue::is_busy() const
{
    return m_thread.joinable();
}

bool WorkQueue::enqueue(WorkQueueCommandPtr &&command)
{
    return m_commandQueue.enqueue(std::move(command));
}

bool WorkQueue::try_dequeue(WorkQueueResultPtr &result)
{
    return m_pollingQueue.try_dequeue(result);
}

void WorkQueue::update_callbacks()
{
    WorkQueueResultPtr result;

    while (m_callbackQueue.try_dequeue(result))
    {
        assert(bool(result->command->callback));

        // Moves the callback to decouple it from the result.
        auto callback = std::move(result->command->callback);
        callback(result);
    }
}

void WorkQueue::ThreadFunction(WorkQueue *self)
{
    self->ThreadRun();
}

void WorkQueue::ThreadRun()
{
    while (true)
    {
        WorkQueueCommandPtr command;

        if (m_quit)
        {
            if (!m_commandQueue.try_dequeue(command))
                // Queue is finally empty. Quit.
                // Wait for jobs here when applicable.
                break;
        }
        else
        {
            m_commandQueue.wait_dequeue(command);
        }

        assert(command != nullptr);
        assert(bool(command->work));

        // #TODO: Spawn jobs for command to unclog the queue.

        WorkQueueResultPtr result = command->work();

        m_lastFinishedCommandId = command->command_id;

        // Commands do not have to return a result.
        if (result != nullptr)
        {
            result->command = std::move(command);

            if (bool(result->command->callback))
            {
                m_callbackQueue.enqueue(std::move(result));
            }
            else
            {
                m_pollingQueue.enqueue(std::move(result));
            }
        }
    }
}

} // namespace unassemblize
