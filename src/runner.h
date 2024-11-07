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
#include "options.h"
#include "pdbreader.h"

namespace unassemblize
{
struct ExeSaveLoadOptions
{
    ExeSaveLoadOptions(const std::string &input_file, const std::string &config_file) :
        input_file(input_file), config_file(config_file)
    {
    }

    const std::string input_file;
    const std::string config_file;
    const PdbReader *pdb_reader = nullptr;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

struct PdbSaveLoadOptions
{
    PdbSaveLoadOptions(const std::string &input_file, const std::string &config_file) :
        input_file(input_file), config_file(config_file)
    {
    }

    const std::string input_file;
    const std::string config_file;
    bool dump_syms = false;
    bool verbose = false;
};

struct AsmOutputOptions
{
    AsmOutputOptions(const Executable &executable, const std::string &output_file, uint64_t start_addr, uint64_t end_addr) :
        executable(executable), output_file(output_file), start_addr(start_addr), end_addr(end_addr)
    {
    }

    const Executable &executable;
    const std::string output_file;
    const uint64_t start_addr;
    const uint64_t end_addr;
    AsmFormat format = AsmFormat::IGAS;
    uint32_t print_indent_len = 4;
};

using ExecutablePair = std::array<const Executable *, 2>;
using PdbReaderPair = std::array<const PdbReader *, 2>;

struct AsmComparisonOptions
{
    AsmComparisonOptions(ExecutablePair executable_pair, PdbReaderPair pdb_reader_pair, const std::string &output_file) :
        executable_pair(executable_pair), pdb_reader_pair(pdb_reader_pair), output_file(output_file)
    {
    }

    const ExecutablePair executable_pair;
    const PdbReaderPair pdb_reader_pair;
    const std::string output_file;
    AsmFormat format = AsmFormat::IGAS;
    MatchBundleType bundle_type = MatchBundleType::None; // The method to group symbols with.
    // Pdb Reader used to group symbols with. Must not be null if bundle_type is not 'None'.
    const PdbReader *bundling_pdb_reader = nullptr;
    uint32_t print_indent_len = 4;
    uint32_t print_asm_len = 80;
    uint32_t print_byte_count = 11;
    uint32_t print_sourcecode_len = 80;
    uint32_t print_sourceline_len = 5;
    uint32_t lookahead_limit = 20;
    AsmMatchStrictness match_strictness = AsmMatchStrictness::Undecided;
};

class Runner
{
    class FileContentStorage
    {
        using FileContentMap = std::map<std::string, TextFileContent>;

    public:
        FileContentStorage();

        const TextFileContent *find_content(const std::string &name) const;
        bool load_content(const std::string &name);
        size_t size() const;
        void clear();

    private:
        FileContentMap m_filesMap;
        mutable FileContentMap::const_iterator m_lastFileIt;
        mutable std::string m_lastFileName;
    };

public:
    Runner() = delete;

    static std::unique_ptr<Executable> process_exe(const ExeSaveLoadOptions &o);
    static std::unique_ptr<PdbReader> process_pdb(const PdbSaveLoadOptions &o);

    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    static bool process_asm_output(const AsmOutputOptions &o);
    static bool process_asm_comparison(const AsmComparisonOptions &o);

    static std::string create_exe_filename(const PdbExeInfo &info);

private:
    // clang-format off

    /*
     * Builds function match collection.
     * All function objects are not disassembled for performance reasons, but are prepared.
     */
    static void build_function_matches(
        FunctionMatches &matches,
        StringToIndexMapT &function_name_to_index_map,
        ExecutablePair executable_pair);

    static void build_function_bundles(
        FunctionMatchBundles &bundles,
        const FunctionMatches &matches,
        const StringToIndexMapT &function_name_to_index_map,
        MatchBundleType bundle_type,
        const PdbReader *bundling_pdb_reader);

    template<class SourceInfoVectorT>
    static void build_bundles(
        FunctionMatchBundles &bundles,
        const PdbFunctionInfoVector &functions,
        const SourceInfoVectorT &sources,
        const StringToIndexMapT &function_name_to_index_map);

    template<class SourceInfoT>
    static void build_bundle(
        FunctionMatchBundle &bundle,
        const PdbFunctionInfoVector &functions,
        const SourceInfoT &source,
        const StringToIndexMapT &function_name_to_index_map);

    static void disassemble_function_matches(
        FunctionMatches &matches,
        ExecutablePair executable_pair,
        AsmFormat format);

    static void build_function_source_lines(
        FunctionMatches &matches,
        const StringToIndexMapT &function_name_to_index_map,
        PdbReaderPair pdb_reader_pair);

    static AsmComparisonResultBundles build_comparison_results(
        const FunctionMatches &matches,
        const FunctionMatchBundles &bundles,
        uint32_t lookahead_limit);

    static bool output_comparison_results(
        AsmComparisonResultBundles &result_bundles,
        MatchBundleType bundle_type,
        const std::string &output_file,
        const StringPair &exe_filenames,
        AsmMatchStrictness match_strictness,
        uint32_t indent_len,
        uint32_t asm_len,
        uint32_t byte_count,
        uint32_t sourcecode_len,
        uint32_t sourceline_len);

    static std::string build_cmp_output_path(
        size_t bundle_idx,
        const std::string &bundle_name,
        const std::string &output_file);

    static void print_sections(Executable &exe);

    // clang-format on
};

} // namespace unassemblize
