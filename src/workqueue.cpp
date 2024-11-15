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
WorkQueueCommandId WorkQueueCommand::s_id = 0;

WorkQueue::~WorkQueue()
{
    stop(true);
}

void WorkQueue::start()
{
    assert(!m_thread.joinable());
    m_active = true;

    // Initialize command pools
    for (const auto &type : ALL_COMMAND_TYPES)
    {
        auto &pool = m_commandPools[type];
        pool.parallel_execution = CanRunParallel(type);
        pool.active = true;

        if (pool.parallel_execution)
        {
            // Create worker threads for parallel commands
            const size_t thread_count = std::min<size_t>(2, static_cast<size_t>(std::thread::hardware_concurrency() - 1));

            for (size_t i = 0; i < thread_count; i++)
            {
                pool.threads.emplace_back([this, type]() { ProcessPoolCommands(type); });
            }
        }
    }

    // Main thread handles sequential commands
    m_thread = std::thread(ThreadFunction, this);
}

void WorkQueue::stop(bool wait)
{
    m_active = false;

    // Stop all command pools
    for (auto &[type, pool] : m_commandPools)
    {
        pool.active = false;

        if (wait)
        {
            for (auto &thread : pool.threads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }
        else
        {
            for (auto &thread : pool.threads)
            {
                if (thread.joinable())
                {
                    thread.detach();
                }
            }
        }
    }

    // Stop main thread
    if (m_thread.joinable())
    {
        m_commandQueue.enqueue(std::make_unique<WorkQueueCommandQuit>());
        if (wait)
        {
            m_thread.join();
        }
        else
        {
            m_thread.detach();
        }
    }
}

bool WorkQueue::is_busy() const
{
    return m_thread.joinable();
}

bool WorkQueue::enqueue(WorkQueueCommandPtr &&command)
{
    if (!m_active)
    {
        return false;
    }

    auto type = command->type();
    auto it = m_commandPools.find(type);

    if (it != m_commandPools.end() && it->second.parallel_execution)
    {
        // Parallel command goes to its dedicated pool
        return it->second.queue.enqueue(std::move(command));
    }
    else
    {
        // Sequential commands go to main queue
        return m_commandQueue.enqueue(std::move(command));
    }
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
    while (m_active)
    {
        WorkQueueCommandPtr command;

        if (m_quit)
        {
            if (!m_commandQueue.try_dequeue(command))
            {
                break;
            }
        }
        else
        {
            m_commandQueue.wait_dequeue(command);
        }

        if (!command)
            continue;

        WorkQueueResultPtr result;

        switch (command->type())
        {
            case WorkCommandType::Quit:
                m_quit = true;
                break;
            case WorkCommandType::ProcessExe:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessExe &>(*command));
                break;
            case WorkCommandType::ProcessPdb:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessPdb &>(*command));
                break;
            case WorkCommandType::ProcessAsmComparison:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessAsmComparison &>(*command));
                break;
            default:
                // Should not get here - parallel commands should be in pools
                assert(false);
                break;
        }

        m_lastFinishedCommandId = command->command_id;

        if (result)
        {
            result->command = std::move(command);

            std::lock_guard<std::mutex> lock(m_resultMutex);
            if (result->command->callback)
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

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandLoadExe &command)
{
    auto result = std::make_unique<WorkQueueResultLoadExe>();
    result->executable = Runner::load_exe(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandLoadPdb &command)
{
    auto result = std::make_unique<WorkQueueResultLoadPdb>();
    result->pdbReader = Runner::load_pdb(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandSaveExeConfig &command)
{
    auto result = std::make_unique<WorkQueueResultSaveExeConfig>();
    result->success = Runner::save_exe_config(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandSavePdbConfig &command)
{
    auto result = std::make_unique<WorkQueueResultSavePdbConfig>();
    result->success = Runner::save_pdb_config(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandProcessExe &command)
{
    auto result = std::make_unique<WorkQueueResultProcessExe>();
    result->executable = Runner::process_exe(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandProcessPdb &command)
{
    auto result = std::make_unique<WorkQueueResultProcessPdb>();
    result->pdb_reader = Runner::process_pdb(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandProcessAsmOutput &command)
{
    auto result = std::make_unique<WorkQueueResultProcessAsmOutput>();
    result->success = Runner::process_asm_output(command.options);
    return result;
}

WorkQueueResultPtr WorkQueue::ProcessCommand(const WorkQueueCommandProcessAsmComparison &command)
{
    auto result = std::make_unique<WorkQueueResultProcessAsmComparison>();
    result->success = Runner::process_asm_comparison(command.options);
    return result;
}

void WorkQueue::ProcessPoolCommands(WorkCommandType type)
{
    auto &pool = m_commandPools[type];

    while (pool.active)
    {
        WorkQueueCommandPtr command;
        if (pool.queue.wait_dequeue_timed(command, std::chrono::milliseconds(100)))
        {
            if (command)
            {
                WorkQueueResultPtr result = ProcessPoolCommand(*command);

                if (result)
                {
                    result->command = std::move(command);

                    std::lock_guard<std::mutex> lock(m_resultMutex);
                    if (result->command->callback)
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
    }
}

WorkQueueResultPtr WorkQueue::ProcessPoolCommand(const WorkQueueCommand &command)
{
    switch (command.type())
    {
        case WorkCommandType::LoadExe:
            return ProcessCommand(static_cast<const WorkQueueCommandLoadExe &>(command));
        case WorkCommandType::LoadPdb:
            return ProcessCommand(static_cast<const WorkQueueCommandLoadPdb &>(command));
        case WorkCommandType::SaveExeConfig:
            return ProcessCommand(static_cast<const WorkQueueCommandSaveExeConfig &>(command));
        case WorkCommandType::SavePdbConfig:
            return ProcessCommand(static_cast<const WorkQueueCommandSavePdbConfig &>(command));
        case WorkCommandType::ProcessAsmOutput:
            return ProcessCommand(static_cast<const WorkQueueCommandProcessAsmOutput &>(command));
        default:
            assert(false && "Command type not supported in pool");
            return nullptr;
    }
}

} // namespace unassemblize
