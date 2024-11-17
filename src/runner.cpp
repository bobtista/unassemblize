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

namespace unassemblize
{
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

std::unique_ptr<Executable> Runner::load_exe(const LoadExeOptions &o)
{
    assert(!o.input_file.empty());

    if (o.verbose)
    {
        printf("Loading executable file '%s'...\n", o.input_file.c_str());
    }

    auto executable = std::make_unique<Executable>();
    executable->set_verbose(o.verbose);

    if (!executable->load(o.input_file))
    {
        executable.reset();
        return executable;
    }

    constexpr bool pdb_symbols_overwrite_exe_symbols = true; // Make configurable?
    constexpr bool cfg_symbols_overwrite_exe_pdb_symbols = true; // Make configurable?

    if (o.pdb_reader != nullptr)
    {
        const PdbSymbolInfoVector &pdb_symbols = o.pdb_reader->get_symbols();

        if (!pdb_symbols.empty())
        {
            executable->add_symbols(pdb_symbols, pdb_symbols_overwrite_exe_symbols);
        }
    }

    if (!o.config_file.empty())
    {
        executable->load_config(o.config_file.c_str(), cfg_symbols_overwrite_exe_pdb_symbols);
    }

    return executable;
}

std::unique_ptr<PdbReader> Runner::load_pdb(const LoadPdbOptions &o)
{
    assert(!o.input_file.empty());

    if (o.verbose)
    {
        printf("Loading Pdb file '%s'...\n", o.input_file.c_str());
    }

    auto pdb_reader = std::make_unique<PdbReader>();
    pdb_reader->set_verbose(o.verbose);

    // Currently does not read back config file here.

    if (!pdb_reader->load(o.input_file))
    {
        pdb_reader.reset();
    }

    return pdb_reader;
}

bool Runner::save_exe_config(const SaveExeConfigOptions &o)
{
    assert(o.executable != nullptr);
    assert(!o.config_file.empty());

    return o.executable->save_config(o.config_file.c_str());
}

bool Runner::save_pdb_config(const SavePdbConfigOptions &o)
{
    assert(o.pdb_reader != nullptr);
    assert(!o.config_file.empty());

    return o.pdb_reader->save_config(o.config_file, o.overwrite_sections);
}

bool Runner::process_asm_output(const AsmOutputOptions &o)
{
    if (!(o.start_addr < o.end_addr))
    {
        return false;
    }

    if (o.format == AsmFormat::MASM)
    {
        return false;
    }

    if (!o.executable.is_loaded())
    {
        return false;
    }

    std::ofstream fs(o.output_file, std::ofstream::binary);
    if (!fs.is_open())
    {
        return false;
    }

    const FunctionSetup setup(o.executable, o.format);
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
    assert(o.executable_pair[0] != nullptr && o.executable_pair[0]->is_loaded());
    assert(o.executable_pair[1] != nullptr && o.executable_pair[1]->is_loaded());

    bool ok = true;

    MatchedFunctions matches;
    MatchBundles bundles;
    StringToIndexMapT functionNameToMatchIndexMap;

    build_function_matches(matches, functionNameToMatchIndexMap, o.executable_pair);

    build_function_bundles(bundles, matches, functionNameToMatchIndexMap, o.bundle_type, o.bundling_pdb_reader);

    disassemble_function_matches(matches, o.executable_pair, o.format);

    if (o.print_sourceline_len + o.print_sourcecode_len > 0)
    {
        build_function_source_lines(matches, functionNameToMatchIndexMap, o.pdb_reader_pair);
    }

    functionNameToMatchIndexMap.swap(StringToIndexMapT());

    build_comparison_records(matches, o.lookahead_limit);

    StringPair exe_filenames;
    for (size_t i = 0; i < o.executable_pair.size(); ++i)
        exe_filenames.pair[i] = o.executable_pair[i]->get_filename();

    ok = output_comparison_results(
        matches,
        bundles,
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

void Runner::build_function_matches(
    MatchedFunctions &matches, StringToIndexMapT &function_name_to_index_map, ExecutablePair executable_pair)
{
    const size_t less_idx = executable_pair[0]->get_symbols().size() < executable_pair[1]->get_symbols().size() ? 0 : 1;
    const size_t more_idx = (less_idx + 1) % executable_pair.size();
    const ExeSymbols &less_symbols = executable_pair[less_idx]->get_symbols();

    auto in_code_section = [&](size_t idx, const ExeSymbol &symbol) -> bool {
        const ExeSectionInfo *code_section = executable_pair[idx]->get_code_section();
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
        const ExeSymbol *p_more_symbol = executable_pair[more_idx]->get_symbol(less_symbol.name);
        if (p_more_symbol == nullptr || !in_code_section(more_idx, *p_more_symbol))
        {
            continue;
        }
        const ExeSymbol &more_symbol = *p_more_symbol;
        const IndexT index = matches.size();
        matches.emplace_back();
        MatchedFunction &match = matches.back();
        match.name = less_symbol.name;
        match.function_pair[less_idx].set_address_range(less_symbol.address, less_symbol.address + less_symbol.size);
        match.function_pair[more_idx].set_address_range(more_symbol.address, more_symbol.address + more_symbol.size);
        function_name_to_index_map[match.name] = index;
    }
}

void Runner::build_function_bundles(
    MatchBundles &bundles,
    const MatchedFunctions &matches,
    const StringToIndexMapT &function_name_to_index_map,
    MatchBundleType bundle_type,
    const PdbReader *bundling_pdb_reader)
{
    switch (bundle_type)
    {
        case MatchBundleType::Compiland: {
            assert(bundling_pdb_reader != nullptr);

            const PdbFunctionInfoVector &functions = bundling_pdb_reader->get_functions();
            const PdbCompilandInfoVector &compilands = bundling_pdb_reader->get_compilands();

            build_bundles(bundles, functions, compilands, function_name_to_index_map);
            break;
        }
        case MatchBundleType::SourceFile: {
            assert(bundling_pdb_reader != nullptr);

            const PdbFunctionInfoVector &functions = bundling_pdb_reader->get_functions();
            const PdbSourceFileInfoVector &sources = bundling_pdb_reader->get_source_files();

            build_bundles(bundles, functions, sources, function_name_to_index_map);
            break;
        }
    }

    if (bundles.empty())
    {
        // Create a dummy bundle with all function matches.
        bundles.resize(1);
        MatchBundle &bundle = bundles[0];
        const size_t count = matches.size();
        bundle.name = "all";
        bundle.matchedFunctions.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            bundle.matchedFunctions[i] = i;
        }
    }
}

template<class SourceInfoVectorT>
void Runner::build_bundles(
    MatchBundles &bundles,
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
            MatchBundle &bundle = bundles[source_idx];
            build_bundle(bundle, functions, source, function_name_to_index_map);
        }
    }
}

template<class SourceInfoT>
void Runner::build_bundle(
    MatchBundle &bundle,
    const PdbFunctionInfoVector &functions,
    const SourceInfoT &source,
    const StringToIndexMapT &function_name_to_index_map)
{
    const IndexT function_count = source.functionIds.size();
    bundle.name = source.name;
    bundle.matchedFunctions.reserve(function_count);

    for (IndexT function_idx = 0; function_idx < function_count; ++function_idx)
    {
        const PdbFunctionInfo &function_info = functions[source.functionIds[function_idx]];
        const std::string &function_name = to_exe_symbol_name(function_info);

        StringToIndexMapT::const_iterator it = function_name_to_index_map.find(function_name);
        if (it != function_name_to_index_map.end())
        {
            bundle.matchedFunctions.push_back(it->second);
        }
        else
        {
            // Can track unmatched functions here ...
        }
    }
}

void Runner::disassemble_function_matches(MatchedFunctions &matches, ExecutablePair executable_pair, AsmFormat format)
{
    const FunctionSetup setup0(*executable_pair[0], format);
    const FunctionSetup setup1(*executable_pair[1], format);

    for (MatchedFunction &match : matches)
    {
        match.function_pair[0].disassemble(setup0);
        match.function_pair[1].disassemble(setup1);
    }
}

void Runner::build_function_source_lines(
    MatchedFunctions &matches, const StringToIndexMapT &function_name_to_index_map, PdbReaderPair pdb_reader_pair)
{
    for (size_t i = 0; i < pdb_reader_pair.size(); ++i)
    {
        if (pdb_reader_pair[i] == nullptr)
            continue;

        const PdbFunctionInfoVector &functions = pdb_reader_pair[i]->get_functions();
        const PdbSourceFileInfoVector &sources = pdb_reader_pair[i]->get_source_files();

        for (const PdbSourceFileInfo &source : sources)
        {
            for (const IndexT function_idx : source.functionIds)
            {
                const PdbFunctionInfo &function_info = functions[function_idx];
                const std::string &function_name = to_exe_symbol_name(function_info);

                StringToIndexMapT::const_iterator it = function_name_to_index_map.find(function_name);
                if (it != function_name_to_index_map.end())
                {
                    MatchedFunction &match = matches[it->second];
                    match.function_pair[i].set_source_file(source, function_info.sourceLines);
                }
            }
        }
    }
}

void Runner::build_comparison_records(MatchedFunction &match, uint32_t lookahead_limit)
{
    match.comparison = AsmMatcher::run_comparison(match.function_pair, lookahead_limit);
}

void Runner::build_comparison_records(MatchedFunctions &matches, uint32_t lookahead_limit)
{
    for (MatchedFunction &match : matches)
    {
        build_comparison_records(match, lookahead_limit);
    }
}

bool Runner::output_comparison_results(
    const MatchedFunctions &matches,
    const MatchBundles &bundles,
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

    for (const MatchBundle &bundle : bundles)
    {
        for (IndexT i : bundle.matchedFunctions)
        {
            const MatchedFunction &match = matches[i];

            const std::string &source_file0 = match.function_pair[0].get_source_file_name();
            const std::string &source_file1 = match.function_pair[1].get_source_file_name();

            cpp_files.load_content(source_file0);
            cpp_files.load_content(source_file1);
        }

        std::string output_file_variant = build_cmp_output_path(bundle_idx, bundle.name, output_file);

        std::ofstream fs(output_file_variant, std::ofstream::binary);
        if (fs.is_open())
        {
            AsmPrinter printer;
            std::string text;
            text.reserve(1024 * 1024);

            for (IndexT i : bundle.matchedFunctions)
            {
                const MatchedFunction &match = matches[i];
                const std::string &source_file0 = match.function_pair[0].get_source_file_name();
                const std::string &source_file1 = match.function_pair[1].get_source_file_name();

                TextFileContentPair cpp_texts;
                cpp_texts.pair[0] = cpp_files.find_content(source_file0);
                cpp_texts.pair[1] = cpp_files.find_content(source_file1);

                text.clear();
                printer.append_to_string(
                    text,
                    match.comparison,
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

    return file_write_count == bundles.size();
}

std::string Runner::create_exe_filename(const PdbExeInfo &info)
{
    assert(!info.exeFileName.empty());
    assert(!info.pdbFilePath.empty());

    std::filesystem::path path = info.pdbFilePath;
    path = path.parent_path();
    path /= info.exeFileName;
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

} // namespace unassemblize
