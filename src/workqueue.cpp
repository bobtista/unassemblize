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
    m_thread = std::thread(ThreadFunction, this);
}

void WorkQueue::stop(bool wait)
{
    if (m_thread.joinable())
    {
        m_commandQueue.enqueue(std::make_unique<WorkQueueCommandQuit>());
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
        WorkQueueResultPtr result;

        // #TODO: Spawn jobs for command to unclog the queue.
        switch (command->type())
        {
            case WorkCommandType::Quit:
                m_quit = true;
                break;
            case WorkCommandType::LoadExe:
                result = ProcessCommand(static_cast<WorkQueueCommandLoadExe &>(*command));
                break;
            case WorkCommandType::LoadPdb:
                result = ProcessCommand(static_cast<WorkQueueCommandLoadPdb &>(*command));
                break;
            case WorkCommandType::SaveExeConfig:
                result = ProcessCommand(static_cast<WorkQueueCommandSaveExeConfig &>(*command));
                break;
            case WorkCommandType::SavePdbConfig:
                result = ProcessCommand(static_cast<WorkQueueCommandSavePdbConfig &>(*command));
                break;
            case WorkCommandType::ProcessExe:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessExe &>(*command));
                break;
            case WorkCommandType::ProcessPdb:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessPdb &>(*command));
                break;
            case WorkCommandType::ProcessAsmOutput:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessAsmOutput &>(*command));
                break;
            case WorkCommandType::ProcessAsmComparison:
                result = ProcessCommand(static_cast<WorkQueueCommandProcessAsmComparison &>(*command));
                break;
            default:
                assert(false);
                break;
        }

        m_lastFinishedCommandId = command->command_id;

        // Not all commands need to return a result.
        if (result != nullptr)
        {
            result->command = std::move(command);

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

} // namespace unassemblize
