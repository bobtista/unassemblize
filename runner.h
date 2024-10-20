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
#include "pdbreader.h"

namespace unassemblize
{
struct ExeOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    std::string output_file;
    std::string format_str;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

struct PdbOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

// TODO: Functionality to have 2 exe sources
// TODO: Functionality to discover and organize (*1) same functions on 2 sources
// TODO: Facility function asm matching (class AsmMatcher)

//(*1)
// struct FunctionMatch // finds matching function addresses from 2 sources
// {
//     Address64T address_left;
//     Address64T address_right;
//     Function function_left;
//     Function function_right;
// };
// struct FunctionBundleMatch // combines functions from same compiland or cpp
// {
//     std::vector<FunctionMatch> functions;
// };

class Runner
{
public:
    void print_sections(Executable &exe);
    void dump_function_to_file(const std::string &file_name, Executable &exe, uint64_t start, uint64_t end);

    // TODO: split into 2 functions, load exe, disassemble functions
    bool process_exe(const ExeOptions &o);
    bool process_pdb(const PdbOptions &o);

    std::string get_pdb_exe_file_name();

private:
    Executable m_executable;
    PdbReader m_pdbReader;
};
} // namespace unassemblize
