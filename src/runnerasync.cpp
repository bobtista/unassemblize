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
#include "runnerasync.h"
#include "runner.h"

namespace unassemblize
{
AsyncLoadExeCommand::AsyncLoadExeCommand(LoadExeOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncLoadExeResult>();
        result->executable = Runner::load_exe(options);

        assert(result->executable == nullptr || result->executable->is_loaded());
        return result;
    };
};

AsyncLoadPdbCommand::AsyncLoadPdbCommand(LoadPdbOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncLoadPdbResult>();
        result->pdbReader = Runner::load_pdb(options);

        assert(result->pdbReader == nullptr || result->pdbReader->is_loaded());
        return result;
    };
};

AsyncSaveExeConfigCommand::AsyncSaveExeConfigCommand(SaveExeConfigOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncSaveExeConfigResult>();
        result->success = Runner::save_exe_config(options);
        return result;
    };
};

AsyncSavePdbConfigCommand::AsyncSavePdbConfigCommand(SavePdbConfigOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncSavePdbConfigResult>();
        result->success = Runner::save_pdb_config(options);
        return result;
    };
};

AsyncProcessAsmOutputCommand::AsyncProcessAsmOutputCommand(AsmOutputOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncProcessAsmOutputResult>();
        result->success = Runner::process_asm_output(options);
        return result;
    };
};

AsyncProcessAsmComparisonCommand::AsyncProcessAsmComparisonCommand(AsmComparisonOptions &&o) : options(std::move(o))
{
    WorkQueueCommand::work = [this]() {
        auto result = std::make_unique<AsyncProcessAsmComparisonResult>();
        result->success = Runner::process_asm_comparison(options);
        return result;
    };
};

} // namespace unassemblize
