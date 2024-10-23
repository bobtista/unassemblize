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
    AsmFormat format = AsmFormat::IGAS;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
};

enum class MatchBundleType
{
    Compiland, // Functions will be bundled by the compilands they belong to.
    SourceFile, // Functions will be bundled by the source files they belong to (.h .cpp).
    None, // Functions will be bundled into one.
};

MatchBundleType to_match_bundle_type(const char *str);

struct AsmCompareOptions
{
    AsmFormat format = AsmFormat::IGAS;
    size_t bundle_file_idx = 0; // The executable file that will be used to group symbols with.
    MatchBundleType bundle_type = MatchBundleType::None; // The method to group symbols with.
};

// #TODO: Facilitate function asm matching (class AsmMatcher)

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

class Runner
{
public:
    bool process_exe(const ExeSaveLoadOptions &o, size_t file_idx = 0);
    bool process_pdb(const PdbSaveLoadOptions &o, size_t file_idx = 0);
    bool process_asm_output(const AsmOutputOptions &o);
    bool process_asm_compare(const AsmCompareOptions &o);

    bool asm_compare_ready() const;
    const std::string &get_exe_filename(size_t file_idx = 0) const;
    std::string get_exe_file_name_from_pdb(size_t file_idx = 0) const;

private:
    static void print_sections(Executable &exe);
    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    static void dissassemble_function(FILE *fp, const Executable &exe, uint64_t start, uint64_t end, AsmFormat format);
    static void dissassemble_gas_func(FILE *fp, const Executable &exe, uint64_t start, uint64_t end, AsmFormat format);

private:
    std::array<Executable, MAX_INPUT_FILES> m_executables;
    std::array<PdbReader, MAX_INPUT_FILES> m_pdbReaders;

    FunctionMatches m_matches;
    FunctionMatchBundles m_matchBundles;
};
} // namespace unassemblize
