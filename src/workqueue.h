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
#include <functional>
#include <memory>
#include <thread>

namespace unassemblize
{
class WorkQueue;
struct WorkQueueCommand;
struct WorkQueueResult;
struct WorkQueueCommandQuit;

using WorkQueueCommandPtr = std::unique_ptr<WorkQueueCommand>;
using WorkQueueResultPtr = std::unique_ptr<WorkQueueResult>;
using WorkQueueCommandId = uint32_t;
inline constexpr WorkQueueCommandId InvalidWorkQueueCommandId = WorkQueueCommandId(~0);

struct WorkQueueCommand
{
    WorkQueueCommand() : command_id(s_id++) {}
    virtual ~WorkQueueCommand() = default;

    // Mandatory function to perform any work for this command.
    // Returns the result that is used for callback or polling.
    std::function<WorkQueueResultPtr(void)> work;

    // Optional callback when the issued command has been completed.
    // This can be used to chain commands with lambda functions.
    // The result will not be pushed to the polling queue.
    std::function<void(WorkQueueResultPtr &result)> callback;

    // Optional context. Can point to address or store an id.
    // Necessary to identify when polling.
    void *context = nullptr;

    const WorkQueueCommandId command_id;
    static WorkQueueCommandId s_id; // Not atomic, because intended to be created on single thread.
};

struct WorkQueueResult
{
    virtual ~WorkQueueResult() = default;

    WorkQueueCommandPtr command;
};

class WorkQueue
{
    friend class WorkQueueCommandQuit;

    template<typename T, size_t MAX_BLOCK_SIZE = 512>
    using ReaderWriterQueue = moodycamel::ReaderWriterQueue<T, MAX_BLOCK_SIZE>;

    template<typename T, size_t MAX_BLOCK_SIZE = 512>
    using BlockingReaderWriterQueue = moodycamel::BlockingReaderWriterQueue<T, MAX_BLOCK_SIZE>;

public:
    WorkQueue() : m_commandQueue(31), m_pollingQueue(31), m_callbackQueue(31){};
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

    BlockingReaderWriterQueue<WorkQueueCommandPtr, 32> m_commandQueue;
    ReaderWriterQueue<WorkQueueResultPtr, 32> m_pollingQueue;
    ReaderWriterQueue<WorkQueueResultPtr, 32> m_callbackQueue;

    std::thread m_thread;

    std::atomic<WorkQueueCommandId> m_lastFinishedCommandId = InvalidWorkQueueCommandId;

    volatile bool m_quit = false;
};

struct WorkQueueCommandQuit : public WorkQueueCommand
{
    WorkQueueCommandQuit(WorkQueue &owner_queue) : m_ownerQueue(owner_queue)
    {
        WorkQueueCommand::work = [this]() {
            m_ownerQueue.m_quit = true;
            return WorkQueueResultPtr();
        };
    }

    virtual ~WorkQueueCommandQuit() override = default;

    WorkQueue &m_ownerQueue;
};

} // namespace unassemblize
