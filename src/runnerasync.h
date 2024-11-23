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
    AsyncLoadExeCommand(LoadExeOptions &&o);
    virtual ~AsyncLoadExeCommand() override = default;

    LoadExeOptions options;
};

struct AsyncLoadPdbCommand : public WorkQueueCommand
{
    AsyncLoadPdbCommand(LoadPdbOptions &&o);
    virtual ~AsyncLoadPdbCommand() override = default;

    LoadPdbOptions options;
};

struct AsyncSaveExeConfigCommand : public WorkQueueCommand
{
    AsyncSaveExeConfigCommand(SaveExeConfigOptions &&o);
    virtual ~AsyncSaveExeConfigCommand() override = default;

    SaveExeConfigOptions options;
};

struct AsyncSavePdbConfigCommand : public WorkQueueCommand
{
    AsyncSavePdbConfigCommand(SavePdbConfigOptions &&o);
    virtual ~AsyncSavePdbConfigCommand() override = default;

    SavePdbConfigOptions options;
};

struct AsyncBuildFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildFunctionsCommand(BuildFunctionsOptions &&o);
    virtual ~AsyncBuildFunctionsCommand() override = default;

    BuildFunctionsOptions options;
};

struct AsyncBuildMatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildMatchedFunctionsCommand(BuildMatchedFunctionsOptions &&o);
    virtual ~AsyncBuildMatchedFunctionsCommand() override = default;

    BuildMatchedFunctionsOptions options;
};

struct AsyncBuildUnmatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildUnmatchedFunctionsCommand(BuildUnmatchedFunctionsOptions &&o);
    virtual ~AsyncBuildUnmatchedFunctionsCommand() override = default;

    BuildUnmatchedFunctionsOptions options;
};

struct AsyncBuildBundlesFromCompilandsCommand : public WorkQueueCommand
{
    AsyncBuildBundlesFromCompilandsCommand(BuildBundlesFromCompilandsOptions &&o);
    virtual ~AsyncBuildBundlesFromCompilandsCommand() override = default;

    BuildBundlesFromCompilandsOptions options;
};

struct AsyncBuildBundlesFromSourceFilesCommand : public WorkQueueCommand
{
    AsyncBuildBundlesFromSourceFilesCommand(BuildBundlesFromSourceFilesOptions &&o);
    virtual ~AsyncBuildBundlesFromSourceFilesCommand() override = default;

    BuildBundlesFromSourceFilesOptions options;
};

struct AsyncBuildSingleBundleCommand : public WorkQueueCommand
{
    AsyncBuildSingleBundleCommand(BuildSingleBundleOptions &&o);
    virtual ~AsyncBuildSingleBundleCommand() override = default;

    BuildSingleBundleOptions options;
};

struct AsyncDisassembleMatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncDisassembleMatchedFunctionsCommand(DisassembleMatchedFunctionsOptions &&o);
    virtual ~AsyncDisassembleMatchedFunctionsCommand() override = default;

    DisassembleMatchedFunctionsOptions options;
};

struct AsyncDisassembleBundledFunctionsCommand : public WorkQueueCommand
{
    AsyncDisassembleBundledFunctionsCommand(DisassembleBundledFunctionsOptions &&o);
    virtual ~AsyncDisassembleBundledFunctionsCommand() override = default;

    DisassembleBundledFunctionsOptions options;
};

struct AsyncDisassembleFunctionsCommand : public WorkQueueCommand
{
    AsyncDisassembleFunctionsCommand(DisassembleFunctionsOptions &&o);
    virtual ~AsyncDisassembleFunctionsCommand() override = default;

    DisassembleFunctionsOptions options;
};

struct AsyncBuildSourceLinesForMatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildSourceLinesForMatchedFunctionsCommand(BuildSourceLinesForMatchedFunctionsOptions &&o);
    virtual ~AsyncBuildSourceLinesForMatchedFunctionsCommand() override = default;

    BuildSourceLinesForMatchedFunctionsOptions options;
};

struct AsyncBuildSourceLinesForBundledFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildSourceLinesForBundledFunctionsCommand(BuildSourceLinesForBundledFunctionsOptions &&o);
    virtual ~AsyncBuildSourceLinesForBundledFunctionsCommand() override = default;

    BuildSourceLinesForBundledFunctionsOptions options;
};

struct AsyncBuildSourceLinesForFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildSourceLinesForFunctionsCommand(BuildSourceLinesForFunctionsOptions &&o);
    virtual ~AsyncBuildSourceLinesForFunctionsCommand() override = default;

    BuildSourceLinesForFunctionsOptions options;
};

struct AsyncLoadSourceFilesForMatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncLoadSourceFilesForMatchedFunctionsCommand(LoadSourceFilesForMatchedFunctionsOptions &&o);
    virtual ~AsyncLoadSourceFilesForMatchedFunctionsCommand() override = default;

    LoadSourceFilesForMatchedFunctionsOptions options;
};

struct AsyncLoadSourceFilesForBundledFunctionsCommand : public WorkQueueCommand
{
    AsyncLoadSourceFilesForBundledFunctionsCommand(LoadSourceFilesForBundledFunctionsOptions &&o);
    virtual ~AsyncLoadSourceFilesForBundledFunctionsCommand() override = default;

    LoadSourceFilesForBundledFunctionsOptions options;
};

struct AsyncLoadSourceFilesForFunctionsCommand : public WorkQueueCommand
{
    AsyncLoadSourceFilesForFunctionsCommand(LoadSourceFilesForFunctionsOptions &&o);
    virtual ~AsyncLoadSourceFilesForFunctionsCommand() override = default;

    LoadSourceFilesForFunctionsOptions options;
};

struct AsyncBuildComparisonRecordsForMatchedFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildComparisonRecordsForMatchedFunctionsCommand(BuildComparisonRecordsForMatchedFunctionsOptions &&o);
    virtual ~AsyncBuildComparisonRecordsForMatchedFunctionsCommand() override = default;

    BuildComparisonRecordsForMatchedFunctionsOptions options;
};

struct AsyncBuildComparisonRecordsForBundledFunctionsCommand : public WorkQueueCommand
{
    AsyncBuildComparisonRecordsForBundledFunctionsCommand(BuildComparisonRecordsForBundledFunctionsOptions &&o);
    virtual ~AsyncBuildComparisonRecordsForBundledFunctionsCommand() override = default;

    BuildComparisonRecordsForBundledFunctionsOptions options;
};

struct AsyncProcessAsmOutputCommand : public WorkQueueCommand
{
    AsyncProcessAsmOutputCommand(AsmOutputOptions &&o);
    virtual ~AsyncProcessAsmOutputCommand() override = default;

    AsmOutputOptions options;
};

struct AsyncProcessAsmComparisonCommand : public WorkQueueCommand
{
    AsyncProcessAsmComparisonCommand(AsmComparisonOptions &&o);
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

struct AsyncBuildFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildFunctionsResult() override = default;

    NamedFunctions named_functions;
};

struct AsyncBuildMatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildMatchedFunctionsResult() override = default;

    MatchedFunctions matched_functions;
};

struct AsyncBuildUnmatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildUnmatchedFunctionsResult() override = default;

    std::vector<IndexT> unmatched_function_indices;
};

struct AsyncBuildBundlesFromCompilandsResult : public WorkQueueResult
{
    virtual ~AsyncBuildBundlesFromCompilandsResult() override = default;

    NamedFunctionBundles bundles;
};

struct AsyncBuildBundlesFromSourceFilesResult : public WorkQueueResult
{
    virtual ~AsyncBuildBundlesFromSourceFilesResult() override = default;

    NamedFunctionBundles bundles;
};

struct AsyncBuildSingleBundleResult : public WorkQueueResult
{
    virtual ~AsyncBuildSingleBundleResult() override = default;

    NamedFunctionBundle bundle;
};

struct AsyncDisassembleMatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncDisassembleMatchedFunctionsResult() override = default;
};

struct AsyncDisassembleBundledFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncDisassembleBundledFunctionsResult() override = default;
};

struct AsyncDisassembleFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncDisassembleFunctionsResult() override = default;
};

struct AsyncBuildSourceLinesForMatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildSourceLinesForMatchedFunctionsResult() override = default;
};

struct AsyncBuildSourceLinesForBundledFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildSourceLinesForBundledFunctionsResult() override = default;
};

struct AsyncBuildSourceLinesForFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildSourceLinesForFunctionsResult() override = default;
};

struct AsyncLoadSourceFilesForMatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncLoadSourceFilesForMatchedFunctionsResult() override = default;

    bool success;
};

struct AsyncLoadSourceFilesForBundledFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncLoadSourceFilesForBundledFunctionsResult() override = default;

    bool success;
};

struct AsyncLoadSourceFilesForFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncLoadSourceFilesForFunctionsResult() override = default;

    bool success;
};

struct AsyncBuildComparisonRecordsForMatchedFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildComparisonRecordsForMatchedFunctionsResult() override = default;
};

struct AsyncBuildComparisonRecordsForBundledFunctionsResult : public WorkQueueResult
{
    virtual ~AsyncBuildComparisonRecordsForBundledFunctionsResult() override = default;
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
