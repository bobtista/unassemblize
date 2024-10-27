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

#include "asmmatchertypes.h"
#include "executable.h"
#include "pdbreader.h"

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

struct AsmComparisonOptions
{
    std::string output_file;
    AsmFormat format = AsmFormat::IGAS;
    size_t bundle_file_idx = 0; // The executable file that will be used to group symbols with.
    MatchBundleType bundle_type = MatchBundleType::None; // The method to group symbols with.
};

class Runner
{
public:
    bool process_exe(const ExeSaveLoadOptions &o, size_t file_idx);
    bool process_pdb(const PdbSaveLoadOptions &o, size_t file_idx);

    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    bool process_asm_output(const AsmOutputOptions &o);
    bool process_asm_comparison(const AsmComparisonOptions &o);

    bool asm_comparison_ready() const;
    const std::string &get_exe_filename(size_t file_idx) const;
    std::string get_exe_file_name_from_pdb(size_t file_idx) const;

private:
    /*
     * Builds function match collection.
     * All function objects are not disassembled for performance reasons, but are prepared.
     */
    FunctionMatchCollection build_function_match_collection(size_t bundle_file_idx, MatchBundleType bundle_type) const;
    void disassemble_function_match_collection(FunctionMatchCollection &collection, AsmFormat format) const;
    AsmComparisonResultBundles build_comparison_results(const FunctionMatchCollection &collection) const;
    static bool output_comparison_results(AsmComparisonResultBundles &result_bundles, const std::string &output_file);
    static std::string
        build_cmp_output_path(size_t bundle_idx, const std::string &bundle_name, const std::string &output_file);

    static void print_sections(Executable &exe);

private:
    std::array<Executable, MAX_INPUT_FILES> m_executables;
    std::array<PdbReader, MAX_INPUT_FILES> m_pdbReaders;
};

} // namespace unassemblize
