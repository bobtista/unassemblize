/**
 * @file
 *
 * @brief Asynchronous commands to instigate all high level functionality
 *        for unassemblize
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "workqueue.h"

namespace unassemblize
{
struct AsyncLoadExeCommand : public WorkQueueCommand
{
    explicit AsyncLoadExeCommand(LoadExeOptions &&o);
    virtual ~AsyncLoadExeCommand() override = default;

    LoadExeOptions options;
};

struct AsyncLoadPdbCommand : public WorkQueueCommand
{
    explicit AsyncLoadPdbCommand(LoadPdbOptions &&o);
    virtual ~AsyncLoadPdbCommand() override = default;

    LoadPdbOptions options;
};

struct AsyncSaveExeConfigCommand : public WorkQueueCommand
{
    explicit AsyncSaveExeConfigCommand(SaveExeConfigOptions &&o);
    virtual ~AsyncSaveExeConfigCommand() override = default;

    SaveExeConfigOptions options;
};

struct AsyncSavePdbConfigCommand : public WorkQueueCommand
{
    explicit AsyncSavePdbConfigCommand(SavePdbConfigOptions &&o);
    virtual ~AsyncSavePdbConfigCommand() override = default;

    SavePdbConfigOptions options;
};

struct AsyncProcessAsmOutputCommand : public WorkQueueCommand
{
    explicit AsyncProcessAsmOutputCommand(AsmOutputOptions &&o);
    virtual ~AsyncProcessAsmOutputCommand() override = default;

    AsmOutputOptions options;
};

struct AsyncProcessAsmComparisonCommand : public WorkQueueCommand
{
    explicit AsyncProcessAsmComparisonCommand(AsmComparisonOptions &&o);
    virtual ~AsyncProcessAsmComparisonCommand() override = default;

    AsmComparisonOptions options;
};

// ...

struct AsyncLoadExeResult : public WorkQueueResult
{
    virtual ~AsyncLoadExeResult() override = default;

    std::unique_ptr<Executable> executable;
};

struct AsyncLoadPdbResult : public WorkQueueResult
{
    virtual ~AsyncLoadPdbResult() override = default;

    std::unique_ptr<PdbReader> pdbReader;
};

struct AsyncSaveExeConfigResult : public WorkQueueResult
{
    virtual ~AsyncSaveExeConfigResult() override = default;

    bool success;
};

struct AsyncSavePdbConfigResult : public WorkQueueResult
{
    virtual ~AsyncSavePdbConfigResult() override = default;

    bool success;
};

struct AsyncProcessAsmOutputResult : public WorkQueueResult
{
    virtual ~AsyncProcessAsmOutputResult() override = default;

    bool success;
};

struct AsyncProcessAsmComparisonResult : public WorkQueueResult
{
    virtual ~AsyncProcessAsmComparisonResult() override = default;

    bool success;
};

// ...

} // namespace unassemblize
