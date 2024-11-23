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
class FileContentStorage
{
    using FileContentMap = std::map<std::string, TextFileContent>;

public:
    enum class LoadResult
    {
        Failed,
        Loaded,
        AlreadyLoaded,
    };

public:
    FileContentStorage();

    const TextFileContent *find_content(const std::string &name) const;
    LoadResult load_content(const std::string &name);
    size_t size() const;
    void clear();

private:
    FileContentMap m_filesMap;
    mutable FileContentMap::const_iterator m_lastFileIt;
};

struct LoadExeOptions
{
    LoadExeOptions(const std::string &input_file) : input_file(input_file) {}

    const std::string input_file;
    std::string config_file;
    const PdbReader *pdb_reader = nullptr;
    bool verbose = false;
};

struct SaveExeConfigOptions
{
    SaveExeConfigOptions(const Executable *executable, const std::string &config_file) :
        executable(executable), config_file(config_file)
    {
    }

    const Executable *const executable;
    const std::string config_file;
};

struct LoadPdbOptions
{
    LoadPdbOptions(const std::string &input_file) : input_file(input_file) {}

    const std::string input_file;
    bool verbose = false;
};

struct SavePdbConfigOptions
{
    SavePdbConfigOptions(const PdbReader *pdb_reader, const std::string &config_file) :
        pdb_reader(pdb_reader), config_file(config_file)
    {
    }

    const PdbReader *const pdb_reader;
    const std::string config_file;
    bool overwrite_sections = false;
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

struct AsmComparisonOptions
{
    AsmComparisonOptions(
        ConstExecutablePair executable_pair, ConstPdbReaderPair pdb_reader_pair, const std::string &output_file) :
        executable_pair(executable_pair), pdb_reader_pair(pdb_reader_pair), output_file(output_file)
    {
    }

    const Executable &get_executable(size_t index) const { return *executable_pair[index]; }

    const PdbReader *bundling_pdb_reader() const
    {
        if (bundle_file_idx < pdb_reader_pair.size())
            return pdb_reader_pair[bundle_file_idx];
        else
            return nullptr;
    }

    const ConstExecutablePair executable_pair;
    const ConstPdbReaderPair pdb_reader_pair;
    const std::string output_file;
    AsmFormat format = AsmFormat::IGAS;
    MatchBundleType bundle_type = MatchBundleType::None; // The method to group symbols with.
    size_t bundle_file_idx = 0;
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
public:
    Runner() = delete;

    static std::unique_ptr<Executable> load_exe(const LoadExeOptions &o);
    static std::unique_ptr<PdbReader> load_pdb(const LoadPdbOptions &o);

    static bool save_exe_config(const SaveExeConfigOptions &o);
    static bool save_pdb_config(const SavePdbConfigOptions &o);

    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    static bool process_asm_output(const AsmOutputOptions &o);
    static bool process_asm_comparison(const AsmComparisonOptions &o);

    static std::string create_exe_filename(const PdbExeInfo &info);

private:
    // clang-format off

    static bool in_code_section(const ExeSymbol &symbol, const Executable& executable);

    static StringToIndexMapT build_function_name_to_index_map(const NamedFunctions& named_functions);
    static Address64ToIndexMapT build_function_address_to_index_map(const NamedFunctions& named_functions);

    static NamedFunctions build_named_functions(
        const Executable& executable);

    static MatchedFunctions build_matched_functions(
        NamedFunctionsPair named_functions_pair);

    static std::vector<IndexT> build_unmatched_functions(
        const NamedFunctions &named_functions,
        const MatchedFunctions &matched_functions);

    static MatchBundles build_match_bundles(
        const NamedFunctions &named_functions,
        const MatchedFunctions &matched_functions,
        const PdbReader *bundling_pdb_reader,
        MatchBundleType bundle_type,
        size_t bundle_file_idx);

    // Note: requires a prior call to build_matched_functions!
    static MatchBundles build_match_bundles_from_compilands(
        const NamedFunctions &named_functions,
        const PdbReader &pdb_reader);

    // Note: requires a prior call to build_matched_functions!
    static MatchBundles build_match_bundles_from_source_files(
        const NamedFunctions &named_functions,
        const PdbReader &pdb_reader);

    // Create a single bundle with all functions.
    static MatchBundle build_single_match_bundle(
        const NamedFunctions &named_functions,
        const MatchedFunctions &matched_functions,
        size_t bundle_file_idx);

    template<class SourceInfoVectorT>
    static MatchBundles build_match_bundles(
        const SourceInfoVectorT &sources,
        const PdbFunctionInfoVector &functions,
        const NamedFunctions &named_functions);

    template<class SourceInfoT>
    static MatchBundle build_match_bundle(
        const SourceInfoT &source,
        const PdbFunctionInfoVector &functions,
        const NamedFunctions &named_functions,
        const Address64ToIndexMapT &named_function_to_index_map);

    static void disassemble_function(
        NamedFunction &named,
        const FunctionSetup& setup);

    static void disassemble_matched_functions(
        NamedFunctionsPair named_functions_pair,
        const MatchedFunctions &matched_functions,
        ConstExecutablePair executable_pair,
        AsmFormat format);

    static void disassemble_bundled_functions(
        NamedFunctions &named_functions,
        MatchBundle& bundle,
        const Executable& executable,
        AsmFormat format);

    // Can be used to disassemble a complete bundle with two calls.
    static void disassemble_bundled_functions(
        NamedFunctions &named_functions,
        span<IndexT> named_function_indices,
        const Executable& executable,
        AsmFormat format);

    // Can be used to disassemble a single function too.
    static void disassemble_functions(
        span<NamedFunction> named_functions,
        const Executable& executable,
        AsmFormat format);

    static void build_source_lines_for_function(
        NamedFunction &named,
        const PdbReader &pdb_reader);

    static void build_source_lines_for_matched_functions(
        NamedFunctionsPair named_functions_pair,
        const MatchedFunctions &matched_functions,
        ConstPdbReaderPair pdb_reader_pair);

    static void build_source_lines_for_bundled_functions(
        NamedFunctions &named_functions,
        MatchBundle& bundle,
        const PdbReader& pdb_reader);

    // Can be used to build source lines of a complete bundle with two calls.
    static void build_source_lines_for_bundled_functions(
        NamedFunctions &named_functions,
        span<IndexT> named_function_indices,
        const PdbReader& pdb_reader);

    // Can be used to build source lines of a single function too.
    static void build_source_lines_for_functions(
        span<NamedFunction> named_functions,
        const PdbReader& pdb_reader);

    static void build_comparison_record(
        MatchedFunction &matched,
        ConstNamedFunctionsPair named_functions_pair,
        uint32_t lookahead_limit);

    static void build_comparison_records(
        MatchedFunctions &matched_functions,
        ConstNamedFunctionsPair named_functions_pair,
        uint32_t lookahead_limit);

    static void build_bundled_comparison_records(
        MatchedFunctions &matched_functions,
        ConstNamedFunctionsPair named_functions_pair,
        MatchBundle& bundle,
        uint32_t lookahead_limit);

    static void build_bundled_comparison_records(
        MatchedFunctions &matched_functions,
        ConstNamedFunctionsPair named_functions_pair,
        span<IndexT> matched_function_indices,
        uint32_t lookahead_limit);

    static bool output_comparison_results(
        ConstNamedFunctionsPair named_functions_pair,
        const MatchedFunctions& matched_functions,
        const MatchBundles &bundles,
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

    // clang-format on
};

} // namespace unassemblize
