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
#pragma once

#define MOODYCAMEL_EXCEPTIONS_ENABLED 0
#include "readerwriterqueue.h"
#include "runner.h"
#include <memory>
#include <thread>

namespace unassemblize
{
enum class WorkCommandType
{
    Quit,
    ProcessExe,
    ProcessPdb,
    ProcessAsmOutput,
    ProcessAsmComparison,
};

struct WorkQueueCommand
{
    WorkQueueCommand() : command_id(s_id++) {}
    virtual ~WorkQueueCommand() = default;
    virtual WorkCommandType type() const = 0;

    const uint32_t command_id;
    static uint32_t s_id; // Not atomic, because intended to be created on single thread.
};
using WorkQueueCommandPtr = std::unique_ptr<WorkQueueCommand>;

struct WorkQueueCommandQuit : public WorkQueueCommand
{
    virtual ~WorkQueueCommandQuit() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::Quit; };
};

struct WorkQueueCommandProcessExe : public WorkQueueCommand
{
    virtual ~WorkQueueCommandProcessExe() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::ProcessExe; };

    ExeSaveLoadOptions options;
};

struct WorkQueueCommandProcessPdb : public WorkQueueCommand
{
    virtual ~WorkQueueCommandProcessPdb() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::ProcessPdb; };

    PdbSaveLoadOptions options;
};

struct WorkQueueCommandProcessAsmOutput : public WorkQueueCommand
{
    virtual ~WorkQueueCommandProcessAsmOutput() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::ProcessAsmOutput; };

    AsmOutputOptions options;
};

struct WorkQueueCommandProcessAsmComparison : public WorkQueueCommand
{
    virtual ~WorkQueueCommandProcessAsmComparison() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::ProcessAsmComparison; };

    AsmComparisonOptions options;
};

// ...

struct WorkQueueResult
{
    virtual ~WorkQueueResult() = default;
    WorkCommandType type() const { return command->type(); }

    WorkQueueCommandPtr command;
};
using WorkQueueResultPtr = std::unique_ptr<WorkQueueResult>;

struct WorkQueueResultProcessExe : public WorkQueueResult
{
    virtual ~WorkQueueResultProcessExe() override = default;

    std::unique_ptr<Executable> executable;
};

struct WorkQueueResultProcessPdb : public WorkQueueResult
{
    virtual ~WorkQueueResultProcessPdb() override = default;

    std::unique_ptr<PdbReader> pdb_reader;
};

struct WorkQueueResultProcessAsmOutput : public WorkQueueResult
{
    virtual ~WorkQueueResultProcessAsmOutput() override = default;

    bool success;
};

struct WorkQueueResultProcessAsmComparison : public WorkQueueResult
{
    virtual ~WorkQueueResultProcessAsmComparison() override = default;

    bool success;
};

// ...

class WorkQueue
{
    template<typename T, size_t MAX_BLOCK_SIZE = 512>
    using ReaderWriterQueue = moodycamel::ReaderWriterQueue<T, MAX_BLOCK_SIZE>;

    template<typename T, size_t MAX_BLOCK_SIZE = 512>
    using BlockingReaderWriterQueue = moodycamel::BlockingReaderWriterQueue<T, MAX_BLOCK_SIZE>;

public:
    WorkQueue() : m_commandQueue(31), m_resultQueue(31){};
    ~WorkQueue() = default;

    void start();
    void stop();
    void join();

    bool enqueue(WorkQueueCommandPtr &&command);
    bool try_dequeue(WorkQueueResultPtr &result);

private:
    static void ThreadFunction(WorkQueue *self);
    void ThreadRun();
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessExe &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessPdb &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessAsmOutput &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessAsmComparison &command);

    BlockingReaderWriterQueue<WorkQueueCommandPtr, 32> m_commandQueue;
    ReaderWriterQueue<WorkQueueResultPtr, 32> m_resultQueue;

    std::thread m_thread;
};

} // namespace unassemblize
