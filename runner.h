/**
 * @file
 *
 * @brief Class to instigate all high level functionality for unassemblize
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "executable.h"
#include "function.h"
#include "pdbreader.h"
#include <array>

namespace unassemblize
{
inline constexpr size_t MAX_INPUT_FILES = 2;

struct ExeSaveLoadOptions
{
    std::string input_file;
    std::string config_file;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

struct PdbSaveLoadOptions
{
    std::string input_file;
    std::string config_file;
    bool dump_syms = false;
    bool verbose = false;
};

struct AsmOutputOptions
{
    std::string output_file;
    AsmFormat format;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
};

struct AsmCompareOptions
{
    AsmFormat format;
};

// #TODO: Functionality to discover and organize same functions on 2 sources
// #TODO: Facilitate function asm matching (class AsmMatcher)

/*
 * A function symbol match from multiple sources.
 */
struct FunctionMatch
{
    std::string name;
    std::array<Function, MAX_INPUT_FILES> functions;
};
using FunctionMatches = std::vector<FunctionMatch>;

class Runner
{
public:
    bool process_exe(const ExeSaveLoadOptions &o, size_t file_idx = 0);
    bool process_pdb(const PdbSaveLoadOptions &o, size_t file_idx = 0);
    bool process_asm_output(const AsmOutputOptions &o);
    bool process_asm_compare(const AsmCompareOptions &o);

    bool asm_compare_ready() const;
    const std::string &get_exe_filename(size_t file_idx = 0);
    std::string get_exe_file_name_from_pdb(size_t file_idx = 0);

private:
    void print_sections(Executable &exe);
    void dump_function_to_file(
        const std::string &file_name, Executable &exe, uint64_t start, uint64_t end, AsmFormat format);

private:
    std::array<Executable, MAX_INPUT_FILES> m_executables;
    std::array<PdbReader, MAX_INPUT_FILES> m_pdbReaders;

    FunctionMatches m_matches;
};
} // namespace unassemblize
