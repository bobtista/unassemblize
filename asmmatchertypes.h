/**
 * @file
 *
 * @brief Types to store relevant data for asm matching
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "function.h"
#include <array>
#include <string>
#include <variant>
#include <vector>

namespace unassemblize
{
/*
 * Pairs a function from 2 executables.
 */
struct FunctionMatch
{
    std::string name;
    std::array<Function, 2> functions;
};
using FunctionMatches = std::vector<FunctionMatch>;

/*
 * Groups function matches of the same compiland or source file together.
 */
struct FunctionMatchBundle
{
    std::string name; // Compiland or source file name.
    std::vector<IndexT> matches;
};
using FunctionMatchBundles = std::vector<FunctionMatchBundle>;

struct FunctionMatchCollection
{
    FunctionMatches matches;
    FunctionMatchBundles bundles;
};

struct AsmTextMismatchInfo
{
    bool is_match() const { return mismatch_bits == 0; }
    bool is_mismatch() const { return !is_match(); }

    uint16_t mismatch_bits = 0; // Bits representing positions where instructions are mismatching.
};

struct AsmLabel
{
    std::array<const AsmInstructionLabel *, 2> label_pair;
};

struct AsmMatch
{
    std::array<const AsmInstruction *, 2> instruction_pair;
};

struct AsmMismatch
{
    std::array<const AsmInstruction *, 2> instruction_pair;
    AsmTextMismatchInfo text_info;
};

using AsmComparisonRecord = std::variant<AsmLabel, AsmMatch, AsmMismatch>;
using AsmComparisonRecords = std::vector<AsmComparisonRecord>;

struct AsmComparisonResult
{
    uint32_t get_instruction_count() const { return match_count + mismatch_count; }

    AsmComparisonRecords records;
    uint32_t label_count = 0;
    uint32_t match_count = 0;
    uint32_t mismatch_count = 0;
};
using AsmComparisonResults = std::vector<AsmComparisonResult>;

struct AsmComparisonResultBundle
{
    std::string name; // Compiland or source file name.
    AsmComparisonResults results;
};
using AsmComparisonResultBundles = std::vector<AsmComparisonResultBundle>;

using StringPair = std::array<std::string, 2>;

} // namespace unassemblize
