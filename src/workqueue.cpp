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
uint32_t WorkQueueCommand::s_id = 0;

void WorkQueue::start()
{
    assert(!m_thread.joinable());
    m_thread = std::thread(ThreadFunction, this);
}

void WorkQueue::stop()
{
    m_commandQueue.enqueue(std::make_unique<WorkQueueCommandQuit>());
}

void WorkQueue::join()
{
    m_thread.join();
}

bool WorkQueue::enqueue(WorkQueueCommandPtr &&command)
{
    return m_commandQueue.enqueue(std::move(command));
}
bool WorkQueue::try_dequeue(WorkQueueResultPtr &result)
{
    return m_resultQueue.try_dequeue(result);
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
        m_commandQueue.wait_dequeue(command);

        assert(command != nullptr);
        WorkQueueResultPtr result;

        // #TODO: Spawn jobs for command to unclog the queue.
        switch (command->type())
        {
            case WorkCommandType::Quit:
                // Wait for jobs here when applicable.
                goto quit;
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

        result->command = std::move(command);
        m_resultQueue.enqueue(std::move(result));
    }
quit:
    return;
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
