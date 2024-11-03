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
#include "runner.h"
#include "asmmatcher.h"
#include "asmprinter.h"
#include "util.h"
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <inttypes.h>
#include <strings.h>

namespace unassemblize
{
MatchBundleType to_match_bundle_type(const char *str)
{
    if (0 == strcasecmp(str, "compiland"))
    {
        return MatchBundleType::Compiland;
    }
    else if (0 == strcasecmp(str, "sourcefile"))
    {
        return MatchBundleType::SourceFile;
    }
    else
    {
        return MatchBundleType::None;
    }
    static_assert(size_t(MatchBundleType::None) == 2, "Enum was changed. Update conditions.");
}

Runner::FileContentStorage::FileContentStorage()
{
    m_lastFileIt = m_filesMap.end();
}

const TextFileContent *Runner::FileContentStorage::find_content(const std::string &name) const
{
    if (name.empty())
    {
        return nullptr;
    }

    // Fast path lookup.
    if (name == m_lastFileName)
    {
        assert(m_lastFileIt != m_filesMap.cend());
        return &m_lastFileIt->second;
    }

    // Search map.
    FileContentMap::const_iterator it = m_filesMap.find(name);
    if (it != m_filesMap.cend())
    {
        m_lastFileIt = it;
        m_lastFileName = name;
        return &it->second;
    }

    return nullptr;
}

bool Runner::FileContentStorage::load_content(const std::string &name)
{
    FileContentMap::iterator it = m_filesMap.find(name);
    if (it != m_filesMap.end())
    {
        // Is already loaded.
        return false;
    }

    std::ifstream fs(name);

    if (!fs.is_open())
    {
        // File open failed.
        return false;
    }

    TextFileContent content;
    content.filename = name;
    {
        std::string buf;
        while (std::getline(fs, buf))
        {
            content.lines.emplace_back(std::move(buf));
        }
    }
    m_lastFileIt = m_filesMap.insert(it, std::make_pair(name, std::move(content)));
    m_lastFileName = name;
    return true;
}

size_t Runner::FileContentStorage::size() const
{
    return m_filesMap.size();
}

void Runner::FileContentStorage::clear()
{
    m_filesMap.clear();
    m_lastFileIt = m_filesMap.cend();
    m_lastFileName.clear();
}

bool Runner::process_exe(const ExeSaveLoadOptions &o, size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    Executable &executable = m_executables[file_idx];

    if (o.verbose)
    {
        printf("Parsing exe file '%s'...\n", o.input_file.c_str());
    }

    executable.set_verbose(o.verbose);

    if (!executable.read(o.input_file))
    {
        return false;
    }

    if (o.print_secs)
    {
        print_sections(executable);
    }

    constexpr bool pdb_symbols_overwrite_exe_symbols = true;
    constexpr bool cfg_symbols_overwrite_exe_pdb_symbols = true;

    const PdbReader &pdbReader = m_pdbReaders[file_idx];
    const PdbSymbolInfoVector &pdb_symbols = pdbReader.get_symbols();

    if (!pdb_symbols.empty())
    {
        executable.add_symbols(pdb_symbols, pdb_symbols_overwrite_exe_symbols);
    }

    if (o.dump_syms)
    {
        executable.save_config(o.config_file.c_str());
    }
    else
    {
        executable.load_config(o.config_file.c_str(), cfg_symbols_overwrite_exe_pdb_symbols);
    }

    return true;
}

bool Runner::process_pdb(const PdbSaveLoadOptions &o, size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    PdbReader &pdbReader = m_pdbReaders[file_idx];

    pdbReader.set_verbose(o.verbose);

    // Currently does not read back config file here.

    if (!pdbReader.read(o.input_file))
    {
        return false;
    }

    if (o.dump_syms)
    {
        pdbReader.save_config(o.config_file);
    }

    return true;
}

bool Runner::process_asm_output(const AsmOutputOptions &o)
{
    if (o.format == AsmFormat::MASM)
    {
        return false;
    }

    if (!m_executables[0].is_ready())
    {
        return false;
    }

    std::ofstream fs(o.output_file, std::ofstream::binary);
    if (!fs.is_open())
    {
        return false;
    }

    const Executable &exe = m_executables[0];
    const FunctionSetup setup(exe, o.format);
    Function func;
    func.disassemble(setup, o.start_addr, o.end_addr);
    const AsmInstructionVariants &instructions = func.get_instructions();

    std::string text;
    AsmPrinter::append_to_string(text, instructions, o.print_indent_len);
    fs.write(text.data(), text.size());

    return true;
}

bool Runner::process_asm_comparison(const AsmComparisonOptions &o)
{
    if (!asm_comparison_ready())
    {
        return false;
    }

    bool ok = true;

    FunctionMatches matches;
    FunctionMatchBundles bundles;
    StringToIndexMapT functionNameToMatchIndexMap;

    build_function_matches(matches, functionNameToMatchIndexMap);

    build_function_bundles(bundles, matches, functionNameToMatchIndexMap, o.bundle_file_idx, o.bundle_type);

    disassemble_function_matches(matches, o.format);

    if (o.print_sourceline_len + o.print_sourcecode_len > 0)
    {
        build_function_source_lines(matches, functionNameToMatchIndexMap);
    }

    functionNameToMatchIndexMap.swap(StringToIndexMapT());

    AsmComparisonResultBundles result_bundles = build_comparison_results(matches, bundles, o.lookahead_limit);

    StringPair exe_filenames;
    exe_filenames.pair = {m_executables[0].get_filename(), m_executables[1].get_filename()};

    ok = output_comparison_results(
        result_bundles,
        o.bundle_type,
        o.output_file,
        exe_filenames,
        o.match_strictness,
        o.print_indent_len,
        o.print_asm_len,
        o.print_byte_count,
        o.print_sourcecode_len,
        o.print_sourceline_len);

    return ok;
}

void Runner::build_function_matches(FunctionMatches &matches, StringToIndexMapT &function_name_to_index_map) const
{
    assert(asm_comparison_ready());

    const size_t less_idx = m_executables[0].get_symbols().size() < m_executables[1].get_symbols().size() ? 0 : 1;
    const size_t more_idx = (less_idx + 1) % 2;
    const ExeSymbols &less_symbols = m_executables[less_idx].get_symbols();

    auto in_code_section = [&](size_t idx, const ExeSymbol &symbol) -> bool {
        const ExeSectionInfo *code_section = m_executables[idx].get_code_section();
        return symbol.address >= code_section->address && symbol.address < code_section->address + code_section->size;
    };

    // #TODO: If performance is a concern in UI, then build matches per some requested source files or compilands.
    // Currently all function matches are prepared, but not disassembled.

    matches.reserve(512);
    function_name_to_index_map.reserve(512);

    for (const ExeSymbol &less_symbol : less_symbols)
    {
        if (!in_code_section(less_idx, less_symbol))
        {
            continue;
        }
        const ExeSymbol &more_symbol = m_executables[more_idx].get_symbol(less_symbol.name);
        if (more_symbol.name.empty() || !in_code_section(more_idx, more_symbol))
        {
            continue;
        }
        const IndexT index = matches.size();
        matches.emplace_back();
        FunctionMatch &match = matches.back();
        match.name = less_symbol.name;
        match.functions[less_idx].set_address_range(less_symbol.address, less_symbol.address + less_symbol.size);
        match.functions[more_idx].set_address_range(more_symbol.address, more_symbol.address + more_symbol.size);
        function_name_to_index_map[match.name] = index;
    }
}

void Runner::build_function_bundles(
    FunctionMatchBundles &bundles,
    const FunctionMatches &matches,
    const StringToIndexMapT &function_name_to_index_map,
    size_t bundle_file_idx,
    MatchBundleType bundle_type) const
{
    if (bundle_file_idx < 2)
    {
        switch (bundle_type)
        {
            case MatchBundleType::Compiland: {
                const PdbFunctionInfoVector &functions = m_pdbReaders[bundle_file_idx].get_functions();
                const PdbCompilandInfoVector &compilands = m_pdbReaders[bundle_file_idx].get_compilands();

                build_bundles(bundles, functions, compilands, function_name_to_index_map);
                break;
            }
            case MatchBundleType::SourceFile: {
                const PdbFunctionInfoVector &functions = m_pdbReaders[bundle_file_idx].get_functions();
                const PdbSourceFileInfoVector &sources = m_pdbReaders[bundle_file_idx].get_source_files();

                build_bundles(bundles, functions, sources, function_name_to_index_map);
                break;
            }
        }
    }

    if (bundles.empty())
    {
        // Create a dummy bundle with all function matches.
        bundles.resize(1);
        FunctionMatchBundle &bundle = bundles[0];
        const size_t count = matches.size();
        bundle.name = "all";
        bundle.matches.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            bundle.matches[i] = i;
        }
    }
}

template<class SourceInfoVectorT>
void Runner::build_bundles(
    FunctionMatchBundles &bundles,
    const PdbFunctionInfoVector &functions,
    const SourceInfoVectorT &sources,
    const StringToIndexMapT &function_name_to_index_map)
{
    if (!sources.empty())
    {
        const IndexT sources_count = sources.size();
        bundles.resize(sources_count);

        for (IndexT source_idx = 0; source_idx < sources_count; ++source_idx)
        {
            const typename SourceInfoVectorT::value_type &source = sources[source_idx];
            FunctionMatchBundle &bundle = bundles[source_idx];
            build_bundle(bundle, functions, source, function_name_to_index_map);
        }
    }
}

template<class SourceInfoT>
void Runner::build_bundle(
    FunctionMatchBundle &bundle,
    const PdbFunctionInfoVector &functions,
    const SourceInfoT &source,
    const StringToIndexMapT &function_name_to_index_map)
{
    const IndexT function_count = source.functionIds.size();
    bundle.name = source.name;
    bundle.matches.reserve(function_count);

    for (IndexT function_idx = 0; function_idx < function_count; ++function_idx)
    {
        const PdbFunctionInfo &functionInfo = functions[source.functionIds[function_idx]];
        StringToIndexMapT::const_iterator it = function_name_to_index_map.find(functionInfo.decoratedName);
        if (it != function_name_to_index_map.end())
        {
            bundle.matches.push_back(it->second);
        }
        else
        {
            // Can track unmatched functions here ...
        }
    }
}

void Runner::disassemble_function_matches(FunctionMatches &matches, AsmFormat format) const
{
    const FunctionSetup setup0(m_executables[0], format);
    const FunctionSetup setup1(m_executables[1], format);

    for (FunctionMatch &match : matches)
    {
        match.functions[0].disassemble(setup0);
        match.functions[1].disassemble(setup1);
    }
}

void Runner::build_function_source_lines(FunctionMatches &matches, const StringToIndexMapT &function_name_to_index_map)
{
    for (size_t i = 0; i < MAX_INPUT_FILES; ++i)
    {
        const PdbFunctionInfoVector &functions = m_pdbReaders[i].get_functions();
        const PdbSourceFileInfoVector &sources = m_pdbReaders[i].get_source_files();

        for (const PdbSourceFileInfo &source : sources)
        {
            for (const IndexT function_idx : source.functionIds)
            {
                const PdbFunctionInfo &functionInfo = functions[function_idx];
                StringToIndexMapT::const_iterator it = function_name_to_index_map.find(functionInfo.decoratedName);
                if (it != function_name_to_index_map.end())
                {
                    FunctionMatch &match = matches[it->second];
                    match.functions[i].set_source_file(source, functionInfo.sourceLines);
                }
            }
        }
    }
}

AsmComparisonResultBundles Runner::build_comparison_results(
    const FunctionMatches &matches, const FunctionMatchBundles &bundles, uint32_t lookahead_limit) const
{
    AsmComparisonResultBundles result_bundles;

    const size_t bundle_count = bundles.size();

    result_bundles.resize(bundles.size());

    for (size_t bundle_idx = 0; bundle_idx < bundle_count; ++bundle_idx)
    {
        const FunctionMatchBundle &match_bundle = bundles[bundle_idx];
        AsmComparisonResultBundle &result_bundle = result_bundles[bundle_idx];
        const size_t match_count = match_bundle.matches.size();
        result_bundle.name = match_bundle.name;
        result_bundle.results.resize(match_count);
        size_t result_index = 0;

        for (IndexT match_idx : match_bundle.matches)
        {
            const FunctionMatch &match = matches[match_idx];
            result_bundle.results[result_index++] = AsmMatcher::run_comparison(match, lookahead_limit);
        }
        assert(result_index == result_bundle.results.size());
    }
    return result_bundles;
}

bool Runner::output_comparison_results(
    AsmComparisonResultBundles &result_bundles,
    MatchBundleType bundle_type,
    const std::string &output_file,
    const StringPair &exe_filenames,
    AsmMatchStrictness match_strictness,
    uint32_t indent_len,
    uint32_t asm_len,
    uint32_t byte_count,
    uint32_t sourcecode_len,
    uint32_t sourceline_len)
{
    size_t file_write_count = 0;
    size_t bundle_idx = 0;

    FileContentStorage cpp_files;

    for (const AsmComparisonResultBundle &result_bundle : result_bundles)
    {
        for (const AsmComparisonResult &result : result_bundle.results)
        {
            assert(result.function_pair[0] != nullptr);
            assert(result.function_pair[1] != nullptr);

            const std::string &source_file0 = result.function_pair[0]->get_source_file_name();
            const std::string &source_file1 = result.function_pair[1]->get_source_file_name();

            cpp_files.load_content(source_file0);
            cpp_files.load_content(source_file1);
        }

        std::string output_file_variant = build_cmp_output_path(bundle_idx, result_bundle.name, output_file);

        std::ofstream fs(output_file_variant, std::ofstream::binary);
        if (fs.is_open())
        {
            AsmPrinter printer;
            std::string text;
            text.reserve(1024 * 1024);

            for (const AsmComparisonResult &result : result_bundle.results)
            {
                const std::string &source_file0 = result.function_pair[0]->get_source_file_name();
                const std::string &source_file1 = result.function_pair[1]->get_source_file_name();

                TextFileContentPair cpp_texts;
                cpp_texts.pair[0] = cpp_files.find_content(source_file0);
                cpp_texts.pair[1] = cpp_files.find_content(source_file1);

                text.clear();
                printer.append_to_string(
                    text,
                    result,
                    exe_filenames,
                    cpp_texts,
                    match_strictness,
                    indent_len,
                    asm_len,
                    byte_count,
                    sourcecode_len,
                    sourceline_len);
                fs.write(text.data(), text.size());
            }
            ++file_write_count;
        }

        if (bundle_type == MatchBundleType::SourceFile)
        {
            // Concurrent cpp file count for source file bundles is expected to be less than 2.
            assert(cpp_files.size() < 2);
            cpp_files.clear();
        }
    }

    return file_write_count == result_bundles.size();
}

bool Runner::asm_comparison_ready() const
{
    static_assert(MAX_INPUT_FILES >= 2, "Expects at least two input files to work with");

    return m_executables[0].is_ready() && m_executables[1].is_ready();
}

const std::string &Runner::get_exe_filename(size_t file_idx) const
{
    assert(file_idx < MAX_INPUT_FILES);
    return m_executables[file_idx].get_filename();
}

std::string Runner::get_exe_file_name_from_pdb(size_t file_idx) const
{
    assert(file_idx < MAX_INPUT_FILES);
    const PdbExeInfo &exe_info = m_pdbReaders[file_idx].get_exe_info();
    assert(!exe_info.exeFileName.empty());
    assert(!exe_info.pdbFilePath.empty());

    std::filesystem::path path = exe_info.pdbFilePath;
    path = path.parent_path();
    path /= exe_info.exeFileName;
    if (!path.has_extension())
    {
        path += ".exe";
    }

    return path.string();
}

std::string Runner::build_cmp_output_path(size_t bundle_idx, const std::string &bundle_name, const std::string &output_file)
{
    const std::filesystem::path bundle_path = bundle_name;
    const std::filesystem::path output_path = output_file;

    const std::string filename = fmt::format(
        "{:s}.{:s}.{:d}{:s}",
        output_path.stem().string(),
        bundle_path.filename().string(),
        bundle_idx++,
        output_path.extension().string());

    const std::filesystem::path path = output_path.parent_path() / filename;
    return path.string();
}

void Runner::print_sections(Executable &exe)
{
    const ExeSections &sections = exe.get_sections();
    for (const ExeSectionInfo &section : sections)
    {
        printf("Name: %s, Address: 0x%" PRIx64 " Size: %" PRIu64 "\n", section.name.c_str(), section.address, section.size);
    }
}

} // namespace unassemblize
