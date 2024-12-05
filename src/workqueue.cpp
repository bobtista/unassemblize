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
WorkQueueDelayedCommand *get_last_delayed_command(WorkQueueDelayedCommand *delayedCommand)
{
    WorkQueueDelayedCommand *nextCommand = delayedCommand;
    while (nextCommand->has_delayed_command())
        nextCommand = nextCommand->next_delayed_command.get();
    return nextCommand;
}

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

bool WorkQueue::enqueue(WorkQueueDelayedCommand &delayed_command)
{
    if (delayed_command.next_delayed_command == nullptr)
        return false;

    WorkQueueResultPtr result;
    return enqueue(std::move(delayed_command.next_delayed_command), result);
}

bool WorkQueue::enqueue(WorkQueueDelayedCommandPtr &&delayed_command, WorkQueueResultPtr &result)
{
    while (delayed_command != nullptr)
    {
        WorkQueueCommandPtr chained_command = delayed_command->create(result);

        if (chained_command == nullptr)
        {
            // Delayed work can decide to not create a chained command.
            // In this case, the next delayed command is worked on.
            delayed_command = std::move(delayed_command->next_delayed_command);
            continue;
        }

        // Moves next delayed command to the very end of the new chained command.
        WorkQueueDelayedCommand *last_command = get_last_delayed_command(chained_command.get());
        last_command->next_delayed_command = std::move(delayed_command->next_delayed_command);

        return enqueue(std::move(chained_command));
    }
    return false;
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
        assert(result != nullptr);
        assert(result->command != nullptr);
        assert(result->command->has_callback() || result->command->has_delayed_command());

        // Invokes callback if applicable.
        {
            WorkQueueCommandPtr &command = result->command;

            if (command->has_callback())
            {
                // Moves the callback to decouple it from the result.
                WorkQueueCommandCallbackFunction callback = std::move(command->callback);
                callback(result);
            }
        }

        // Evaluates and enqueues next command if applicable.
        if (result != nullptr)
        {
            WorkQueueCommandPtr &command = result->command;

            if (command != nullptr && command->has_delayed_command())
            {
                enqueue(std::move(command->next_delayed_command), result);
            }
        }
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
        assert(command->has_work());

        // #TODO: Spawn jobs for command to unclog the queue.

        WorkQueueResultPtr result = command->work();

        m_lastFinishedCommandId = command->command_id;

        const bool has_callback = command->has_callback();
        const bool has_delayed_command = command->has_delayed_command();

        // Command work functions do not need to return a result,
        // but when using a callback or delayed command then a result is required.
        if (result == nullptr && (has_callback || has_delayed_command))
        {
            result = std::make_unique<WorkQueueResult>();
        }

        if (result != nullptr)
        {
            result->command = std::move(command);

            if (has_callback || has_delayed_command)
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
