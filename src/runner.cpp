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

    std::array<NamedFunctions, 2> named_functions;
    NamedFunctionsPair named_functions_pair = {&named_functions[0], &named_functions[1]};
    ConstNamedFunctionsPair const_named_functions_pair = {&named_functions[0], &named_functions[1]};

    for (size_t i = 0; i < named_functions.size(); ++i)
    {
        named_functions[i] = build_named_functions(o.get_executable(i));
    }

    MatchedFunctions matched_functions = build_matched_functions(named_functions_pair);

    MatchBundles bundles = build_match_bundles(
        named_functions[o.bundle_file_idx], matched_functions, o.bundling_pdb_reader(), o.bundle_type, o.bundle_file_idx);

    disassemble_matched_functions(named_functions_pair, matched_functions, o.executable_pair, o.format);

    if (o.print_sourceline_len + o.print_sourcecode_len > 0)
    {
        build_source_lines_for_matched_functions(named_functions_pair, matched_functions, o.pdb_reader_pair);
    }

    build_comparison_records(matched_functions, const_named_functions_pair, o.lookahead_limit);

    StringPair exe_filenames;
    for (size_t i = 0; i < o.executable_pair.size(); ++i)
    {
        exe_filenames.pair[i] = o.executable_pair[i]->get_filename();
    }

    ok = output_comparison_results(
        const_named_functions_pair,
        matched_functions,
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

bool Runner::in_code_section(const ExeSymbol &symbol, const Executable &executable)
{
    const ExeSectionInfo *code_section = executable.get_code_section();
    return symbol.address >= code_section->address && symbol.address < code_section->address + code_section->size;
}

StringToIndexMapT Runner::build_function_name_to_index_map(const NamedFunctions &named_functions)
{
    const size_t size = named_functions.size();
    StringToIndexMapT map;
    map.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        map[named_functions[i].name] = i;
    }
    return map;
}

Address64ToIndexMapT Runner::build_function_address_to_index_map(const NamedFunctions &named_functions)
{
    const size_t size = named_functions.size();
    Address64ToIndexMapT map;
    map.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        const Address64T address = named_functions[i].function.get_begin_address();
        map[address] = i;
    }
    return map;
}

NamedFunctions Runner::build_named_functions(const Executable &executable)
{
    const ExeSymbols &symbols = executable.get_symbols();

    NamedFunctions named_functions;
    named_functions.reserve(symbols.size());

    for (const ExeSymbol &symbol : symbols)
    {
        if (!in_code_section(symbol, executable))
        {
            continue;
        }

        const IndexT index = named_functions.size();
        named_functions.emplace_back();

        NamedFunction &named = named_functions.back();
        named.name = symbol.name;
        named.function.set_address_range(symbol.address, symbol.address + symbol.size);
    }

    named_functions.shrink_to_fit();

    return named_functions;
}

MatchedFunctions Runner::build_matched_functions(NamedFunctionsPair named_functions_pair)
{
    const size_t less_idx = named_functions_pair[0]->size() < named_functions_pair[1]->size() ? 0 : 1;
    const size_t more_idx = (less_idx + 1) % named_functions_pair.size();
    NamedFunctions &less_named_functions = *named_functions_pair[less_idx];
    NamedFunctions &more_named_functions = *named_functions_pair[more_idx];
    const StringToIndexMapT more_named_functions_to_index_map = build_function_name_to_index_map(more_named_functions);
    const size_t less_named_size = less_named_functions.size();
    const size_t more_named_size = more_named_functions.size();

    MatchedFunctions matched_functions;
    matched_functions.reserve(more_named_size);

    for (size_t less_named_idx = 0; less_named_idx < less_named_size; ++less_named_idx)
    {
        const NamedFunction &less_named_function = less_named_functions[less_named_idx];
        const StringToIndexMapT::const_iterator it = more_named_functions_to_index_map.find(less_named_function.name);

        if (it == more_named_functions_to_index_map.cend())
            continue;

        const IndexT matched_index = matched_functions.size();
        matched_functions.emplace_back();
        MatchedFunction &matched = matched_functions.back();
        matched.named_idx_pair[less_idx] = less_named_idx;
        matched.named_idx_pair[more_idx] = it->second;

        less_named_functions[less_named_idx].matched_index = matched_index;
        more_named_functions[it->second].matched_index = matched_index;
    }

    matched_functions.shrink_to_fit();

    return matched_functions;
}

std::vector<IndexT>
    Runner::build_unmatched_functions(const NamedFunctions &named_functions, const MatchedFunctions &matched_functions)
{
    const size_t named_size = named_functions.size();
    const size_t matched_size = matched_functions.size();
    assert(named_size >= matched_size);
    const size_t unmatched_size = named_size - matched_size;

    std::vector<IndexT> unmatched_functions;
    unmatched_functions.resize(unmatched_size);
    size_t unmatched_idx = 0;

    for (size_t named_idx = 0; named_idx < named_size; ++named_idx)
    {
        if (!named_functions[named_idx].is_matched())
        {
            unmatched_functions[unmatched_idx++] = named_idx;
        }
    }

    return unmatched_functions;
}

MatchBundles Runner::build_match_bundles(
    const NamedFunctions &named_functions,
    const MatchedFunctions &matched_functions,
    const PdbReader *bundling_pdb_reader,
    MatchBundleType bundle_type,
    size_t bundle_file_idx)
{
    MatchBundles bundles;

    switch (bundle_type)
    {
        case MatchBundleType::Compiland: {
            bundles = build_match_bundles_from_compilands(named_functions, *bundling_pdb_reader);
            break;
        }
        case MatchBundleType::SourceFile: {
            bundles = build_match_bundles_from_source_files(named_functions, *bundling_pdb_reader);
            break;
        }
    }

    if (bundles.empty())
    {
        bundles.resize(1);
        bundles[0] = build_single_match_bundle(named_functions, matched_functions, bundle_file_idx);
    }

    return bundles;
}

MatchBundles Runner::build_match_bundles_from_compilands(const NamedFunctions &named_functions, const PdbReader &pdb_reader)
{
    const PdbCompilandInfoVector &compilands = pdb_reader.get_compilands();
    const PdbFunctionInfoVector &functions = pdb_reader.get_functions();

    return build_match_bundles(compilands, functions, named_functions);
}

MatchBundles
    Runner::build_match_bundles_from_source_files(const NamedFunctions &named_functions, const PdbReader &pdb_reader)
{
    const PdbSourceFileInfoVector &sources = pdb_reader.get_source_files();
    const PdbFunctionInfoVector &functions = pdb_reader.get_functions();

    return build_match_bundles(sources, functions, named_functions);
}

MatchBundle Runner::build_single_match_bundle(
    const NamedFunctions &named_functions, const MatchedFunctions &matched_functions, size_t bundle_file_idx)
{
    const size_t matched_count = matched_functions.size();

    MatchBundle bundle;
    bundle.name = "all";
    bundle.matchedFunctions.resize(matched_count);
    bundle.matchedNamedFunctions.resize(matched_count);

    for (size_t i = 0; i < matched_count; ++i)
    {
        bundle.matchedFunctions[i] = i;
        bundle.matchedNamedFunctions[i] = matched_functions[i].named_idx_pair[bundle_file_idx];
    }
    bundle.unmatchedNamedFunctions = build_unmatched_functions(named_functions, matched_functions);

    return bundle;
}

template<class SourceInfoVectorT>
MatchBundles Runner::build_match_bundles(
    const SourceInfoVectorT &sources, const PdbFunctionInfoVector &functions, const NamedFunctions &named_functions)
{
    const Address64ToIndexMapT named_function_to_index_map = build_function_address_to_index_map(named_functions);
    const IndexT sources_count = sources.size();
    MatchBundles bundles;
    bundles.resize(sources_count);

    for (IndexT source_idx = 0; source_idx < sources_count; ++source_idx)
    {
        bundles[source_idx] =
            build_match_bundle(sources[source_idx], functions, named_functions, named_function_to_index_map);
    }

    return bundles;
}

template<class SourceInfoT>
MatchBundle Runner::build_match_bundle(
    const SourceInfoT &source,
    const PdbFunctionInfoVector &functions,
    const NamedFunctions &named_functions,
    const Address64ToIndexMapT &named_function_to_index_map)
{
    const IndexT function_count = source.functionIds.size();
    MatchBundle bundle;
    bundle.name = source.name;
    bundle.matchedFunctions.reserve(function_count);
    bundle.matchedNamedFunctions.reserve(function_count);
    bundle.unmatchedNamedFunctions.reserve(function_count);

    for (IndexT function_idx = 0; function_idx < function_count; ++function_idx)
    {
        const PdbFunctionInfo &function_info = functions[source.functionIds[function_idx]];
        const Address64ToIndexMapT::const_iterator it = named_function_to_index_map.find(function_info.address.absVirtual);

        if (it != named_function_to_index_map.cend())
        {
            IndexT named_idx = it->second;
            const NamedFunction &named = named_functions[named_idx];
            if (named.is_matched())
            {
                bundle.matchedFunctions.push_back(named_functions[named_idx].matched_index);
                bundle.matchedNamedFunctions.push_back(named_idx);
            }
            else
            {
                bundle.unmatchedNamedFunctions.push_back(named_idx);
            }
        }
        else
        {
            assert(false);
        }
    }

    bundle.matchedFunctions.shrink_to_fit();
    bundle.matchedNamedFunctions.shrink_to_fit();
    bundle.unmatchedNamedFunctions.shrink_to_fit();

    return bundle;
}

void Runner::disassemble_function(NamedFunction &named, const FunctionSetup &setup)
{
    if (named.is_disassembled())
        return;

    named.function.disassemble(setup);
}

void Runner::disassemble_matched_functions(
    NamedFunctionsPair named_functions_pair,
    const MatchedFunctions &matched_functions,
    ConstExecutablePair executable_pair,
    AsmFormat format)
{
    const FunctionSetup setup0(*executable_pair[0], format);
    const FunctionSetup setup1(*executable_pair[1], format);

    for (const MatchedFunction &matched : matched_functions)
    {
        NamedFunctionPair named_pair = to_named_function_pair(named_functions_pair, matched);
        disassemble_function(*named_pair[0], setup0);
        disassemble_function(*named_pair[1], setup1);
    }
}

void Runner::disassemble_bundled_functions(
    NamedFunctions &named_functions, MatchBundle &bundle, const Executable &executable, AsmFormat format)
{
    disassemble_bundled_functions(named_functions, span<IndexT>{bundle.matchedNamedFunctions}, executable, format);
    disassemble_bundled_functions(named_functions, span<IndexT>{bundle.unmatchedNamedFunctions}, executable, format);
    bundle.update_disassembled_count(named_functions);
}

void Runner::disassemble_bundled_functions(
    NamedFunctions &named_functions, span<IndexT> named_function_indices, const Executable &executable, AsmFormat format)
{
    const FunctionSetup setup(executable, format);

    for (IndexT index : named_function_indices)
    {
        disassemble_function(named_functions[index], setup);
    }
}

void Runner::disassemble_functions(span<NamedFunction> named_functions, const Executable &executable, AsmFormat format)
{
    const FunctionSetup setup(executable, format);

    for (NamedFunction &named : named_functions)
    {
        disassemble_function(named, setup);
    }
}

void Runner::build_source_lines_for_function(NamedFunction &named, const PdbReader &pdb_reader)
{
    if (named.is_linked_to_source_file() || !named.can_link_to_source_file)
        return;

    const Address64T address = named.function.get_begin_address();
    const PdbFunctionInfo *pdb_function = pdb_reader.find_function_by_address(address);

    if (pdb_function != nullptr && pdb_function->has_valid_source_file_id())
    {
        const PdbSourceFileInfoVector &source_files = pdb_reader.get_source_files();
        const PdbSourceFileInfo &source_file = source_files[pdb_function->sourceFileId];
        named.function.set_source_file(source_file, pdb_function->sourceLines);
    }
    else
    {
        named.can_link_to_source_file = false;
    }
}

void Runner::build_source_lines_for_matched_functions(
    NamedFunctionsPair named_functions_pair, const MatchedFunctions &matched_functions, ConstPdbReaderPair pdb_reader_pair)
{
    for (size_t i = 0; i < pdb_reader_pair.size(); ++i)
    {
        if (const PdbReader *pdb_reader = pdb_reader_pair[i])
        {
            for (const MatchedFunction &matched : matched_functions)
            {
                NamedFunction &named = named_functions_pair[i]->at(matched.named_idx_pair[i]);
                build_source_lines_for_function(named, *pdb_reader);
            }
        }
    }
}

void Runner::build_source_lines_for_bundled_functions(
    NamedFunctions &named_functions, MatchBundle &bundle, const PdbReader &pdb_reader)
{
    build_source_lines_for_bundled_functions(named_functions, span<IndexT>{bundle.matchedNamedFunctions}, pdb_reader);
    build_source_lines_for_bundled_functions(named_functions, span<IndexT>{bundle.unmatchedNamedFunctions}, pdb_reader);
    bundle.update_linked_source_file_count(named_functions);
}

void Runner::build_source_lines_for_bundled_functions(
    NamedFunctions &named_functions, span<IndexT> named_function_indices, const PdbReader &pdb_reader)
{
    for (IndexT index : named_function_indices)
    {
        build_source_lines_for_function(named_functions[index], pdb_reader);
    }
}

void Runner::build_source_lines_for_functions(span<NamedFunction> named_functions, const PdbReader &pdb_reader)
{
    for (NamedFunction &named : named_functions)
    {
        build_source_lines_for_function(named, pdb_reader);
    }
}

void Runner::build_comparison_record(
    MatchedFunction &matched, ConstNamedFunctionsPair named_functions_pair, uint32_t lookahead_limit)
{
    if (matched.is_compared())
        return;

    ConstFunctionPair function_pair = to_const_function_pair(named_functions_pair, matched);
    matched.comparison = AsmMatcher::run_comparison(function_pair, lookahead_limit);
}

void Runner::build_comparison_records(
    MatchedFunctions &matched_functions, ConstNamedFunctionsPair named_functions_pair, uint32_t lookahead_limit)
{
    for (MatchedFunction &matched : matched_functions)
    {
        build_comparison_record(matched, named_functions_pair, lookahead_limit);
    }
}

void Runner::build_bundled_comparison_records(
    MatchedFunctions &matched_functions,
    ConstNamedFunctionsPair named_functions_pair,
    MatchBundle &bundle,
    uint32_t lookahead_limit)
{
    build_bundled_comparison_records(
        matched_functions, named_functions_pair, span<IndexT>{bundle.matchedFunctions}, lookahead_limit);
    bundle.update_compared_count(matched_functions);
}

void Runner::build_bundled_comparison_records(
    MatchedFunctions &matched_functions,
    ConstNamedFunctionsPair named_functions_pair,
    span<IndexT> matched_function_indices,
    uint32_t lookahead_limit)
{
    for (IndexT index : matched_function_indices)
    {
        build_comparison_record(matched_functions[index], named_functions_pair, lookahead_limit);
    }
}

bool Runner::output_comparison_results(
    ConstNamedFunctionsPair named_functions_pair,
    const MatchedFunctions &matched_functions,
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
            const MatchedFunction &matched = matched_functions[i];
            ConstFunctionPair function_pair = to_const_function_pair(named_functions_pair, matched);
            const std::string &source_file0 = function_pair[0]->get_source_file_name();
            const std::string &source_file1 = function_pair[1]->get_source_file_name();

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
                const MatchedFunction &matched = matched_functions[i];
                ConstFunctionPair function_pair = to_const_function_pair(named_functions_pair, matched);
                const std::string &source_file0 = function_pair[0]->get_source_file_name();
                const std::string &source_file1 = function_pair[1]->get_source_file_name();

                TextFileContentPair cpp_texts;
                cpp_texts.pair[0] = cpp_files.find_content(source_file0);
                cpp_texts.pair[1] = cpp_files.find_content(source_file1);

                text.clear();
                printer.append_to_string(
                    text,
                    matched.comparison,
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
