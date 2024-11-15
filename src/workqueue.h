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
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace unassemblize
{
enum class WorkCommandType
{
    Quit,
    ProcessExe,
    ProcessPdb,
    ProcessAsmOutput,
    ProcessAsmComparison,
    LoadExe,
    LoadPdb,
    SaveExeConfig,
    SavePdbConfig,
};

struct WorkQueueResult;
using WorkQueueResultPtr = std::unique_ptr<WorkQueueResult>;
using WorkQueueCommandId = uint32_t;
inline constexpr WorkQueueCommandId InvalidWorkQueueCommandId = WorkQueueCommandId(~0);

struct WorkQueueCommand
{
    WorkQueueCommand() : command_id(s_id++) {}
    virtual ~WorkQueueCommand() = default;
    virtual WorkCommandType type() const = 0;

    // Optional callback when the issued command has been completed.
    // This can be used to chain commands with lambda functions.
    // The result will not be pushed to the polling queue.
    std::function<void(WorkQueueResultPtr &result)> callback;

    const WorkQueueCommandId command_id;
    static WorkQueueCommandId s_id; // Not atomic, because intended to be created on single thread.
};
using WorkQueueCommandPtr = std::unique_ptr<WorkQueueCommand>;

struct WorkQueueCommandQuit : public WorkQueueCommand
{
    virtual ~WorkQueueCommandQuit() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::Quit; };
};

struct WorkQueueCommandLoadExe : public WorkQueueCommand
{
    explicit WorkQueueCommandLoadExe(const std::string &input_file) : options(input_file) {};

    virtual ~WorkQueueCommandLoadExe() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::LoadExe; };

    LoadExeOptions options;
};

struct WorkQueueCommandLoadPdb : public WorkQueueCommand
{
    explicit WorkQueueCommandLoadPdb(const std::string &input_file) : options(input_file) {};

    virtual ~WorkQueueCommandLoadPdb() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::LoadPdb; };

    LoadPdbOptions options;
};

struct WorkQueueCommandSaveExeConfig : public WorkQueueCommand
{
    explicit WorkQueueCommandSaveExeConfig(const Executable *executable, const std::string &config_file) :
        options(executable, config_file) {};

    virtual ~WorkQueueCommandSaveExeConfig() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::SaveExeConfig; };

    SaveExeConfigOptions options;
};

struct WorkQueueCommandSavePdbConfig : public WorkQueueCommand
{
    explicit WorkQueueCommandSavePdbConfig(const PdbReader *pdb_reader, const std::string &config_file) :
        options(pdb_reader, config_file) {};

    virtual ~WorkQueueCommandSavePdbConfig() override = default;
    virtual WorkCommandType type() const override { return WorkCommandType::SavePdbConfig; };

    SavePdbConfigOptions options;
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

struct WorkQueueResultLoadExe : public WorkQueueResult
{
    virtual ~WorkQueueResultLoadExe() override = default;

    std::unique_ptr<Executable> executable;
};

struct WorkQueueResultLoadPdb : public WorkQueueResult
{
    virtual ~WorkQueueResultLoadPdb() override = default;

    std::unique_ptr<PdbReader> pdbReader;
};

struct WorkQueueResultSaveExeConfig : public WorkQueueResult
{
    virtual ~WorkQueueResultSaveExeConfig() override = default;

    bool success;
};

struct WorkQueueResultSavePdbConfig : public WorkQueueResult
{
    virtual ~WorkQueueResultSavePdbConfig() override = default;

    bool success;
};

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

private:
    static constexpr std::array<WorkCommandType, 9> ALL_COMMAND_TYPES = {
        WorkCommandType::Quit,
        WorkCommandType::ProcessExe,
        WorkCommandType::ProcessPdb,
        WorkCommandType::ProcessAsmOutput,
        WorkCommandType::ProcessAsmComparison,
        WorkCommandType::LoadExe,
        WorkCommandType::LoadPdb,
        WorkCommandType::SaveExeConfig,
        WorkCommandType::SavePdbConfig};

    struct CommandPool
    {
        BlockingReaderWriterQueue<WorkQueueCommandPtr, 32> queue;
        std::vector<std::thread> threads;
        bool parallel_execution;
        std::atomic<bool> active{true};
    };

    static bool CanRunParallel(WorkCommandType type)
    {
        switch (type)
        {
            case WorkCommandType::LoadExe:
            case WorkCommandType::LoadPdb:
            case WorkCommandType::SaveExeConfig:
            case WorkCommandType::SavePdbConfig:
            case WorkCommandType::ProcessAsmOutput:
                return true;
            default:
                return false;
        }
    }

public:
    WorkQueue() : m_commandQueue(31), m_pollingQueue(31), m_callbackQueue(31) {};
    ~WorkQueue();

    void start();
    void stop(bool wait);
    bool is_busy() const;
    bool is_quitting() const { return m_quit; }

    WorkQueueCommandId get_last_finished_command_id() const { return m_lastFinishedCommandId.load(); }

    bool enqueue(WorkQueueCommandPtr &&command);
    bool try_dequeue(WorkQueueResultPtr &result);
    void update_callbacks();

private:
    static void ThreadFunction(WorkQueue *self);
    void ThreadRun();
    void ProcessPoolCommands(WorkCommandType type);
    WorkQueueResultPtr ProcessPoolCommand(const WorkQueueCommand &command);

    // #TODO Split commands from this class
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandLoadExe &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandLoadPdb &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandSaveExeConfig &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandSavePdbConfig &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessExe &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessPdb &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessAsmOutput &command);
    static WorkQueueResultPtr ProcessCommand(const WorkQueueCommandProcessAsmComparison &command);

    BlockingReaderWriterQueue<WorkQueueCommandPtr, 32> m_commandQueue;
    ReaderWriterQueue<WorkQueueResultPtr, 32> m_pollingQueue;
    ReaderWriterQueue<WorkQueueResultPtr, 32> m_callbackQueue;

    std::thread m_thread;
    std::unordered_map<WorkCommandType, CommandPool> m_commandPools;
    std::mutex m_resultMutex;

    std::atomic<WorkQueueCommandId> m_lastFinishedCommandId{InvalidWorkQueueCommandId};
    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_active{true};
};

} // namespace unassemblize
