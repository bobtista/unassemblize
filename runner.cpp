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
#include "util.h"
#include <filesystem>
#include <inttypes.h>
#include <strings.h>

namespace unassemblize
{
MatchBundleType to_match_bundle_type(const char *str)
{
    if (0 == strcasecmp(str, "compiland")) {
        return MatchBundleType::Compiland;
    } else if (0 == strcasecmp(str, "sourcefile")) {
        return MatchBundleType::SourceFile;
    } else {
        return MatchBundleType::None;
    }
    static_assert(size_t(MatchBundleType::None) == 2, "Enum was changed. Update conditions.");
}

void Runner::print_sections(Executable &exe)
{
    const ExeSections &sections = exe.get_sections();
    for (const ExeSectionInfo &section : sections) {
        printf("Name: %s, Address: 0x%" PRIx64 " Size: %" PRIu64 "\n", section.name.c_str(), section.address, section.size);
    }
}

void Runner::dump_function_to_file(
    const std::string &file_name, Executable &exe, uint64_t start, uint64_t end, AsmFormat format)
{
    if (!file_name.empty()) {
        FILE *fp = fopen(file_name.c_str(), "w+");
        if (fp != nullptr) {
            exe.dissassemble_function(fp, start, end, format);
            fclose(fp);
        }
    } else {
        exe.dissassemble_function(nullptr, start, end, format);
    }
}

// #TODO: split into 2 functions, load exe, disassemble functions
bool Runner::process_exe(const ExeSaveLoadOptions &o, size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    Executable &executable = m_executables[file_idx];

    if (o.verbose) {
        printf("Parsing exe file '%s'...\n", o.input_file.c_str());
    }

    executable.set_verbose(o.verbose);

    if (!executable.read(o.input_file)) {
        return false;
    }

    if (o.print_secs) {
        print_sections(executable);
    }

    constexpr bool pdb_symbols_overwrite_exe_symbols = true;
    constexpr bool cfg_symbols_overwrite_exe_pdb_symbols = true;

    const PdbReader &pdbReader = m_pdbReaders[file_idx];
    const PdbSymbolInfoVector &pdb_symbols = pdbReader.get_symbols();

    if (!pdb_symbols.empty()) {
        executable.add_symbols(pdb_symbols, pdb_symbols_overwrite_exe_symbols);
    }

    if (o.dump_syms) {
        executable.save_config(o.config_file.c_str());
    } else {
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

    if (!pdbReader.read(o.input_file)) {
        return false;
    }

    if (o.dump_syms) {
        pdbReader.save_config(o.config_file);
    }

    return true;
}

bool Runner::process_asm_output(const AsmOutputOptions &o)
{
    Executable &executable = m_executables[0];

    // #TODO: Cleanup
    if (o.start_addr == 0 && o.end_addr == 0) {
        for (const ExeSymbol &symbol : executable.get_symbols()) {
            std::string sanitized_symbol_name = symbol.name;
#if defined(WIN32)
            util::remove_characters_inplace(sanitized_symbol_name, "\\/:*?\"<>|");
#endif
            std::string file_name;
            if (!o.output_file.empty()) {
                // program.symbol.S
                file_name = util::get_file_name_without_ext(o.output_file) + "." + sanitized_symbol_name + ".S";
            }
            dump_function_to_file(file_name, executable, symbol.address, symbol.address + symbol.size, o.format);
        }
    } else {
        dump_function_to_file(o.output_file, executable, o.start_addr, o.end_addr, o.format);
    }

    return true;
}

namespace
{
using StringToIndexMapT = std::unordered_map<std::string, IndexT>;

template<class SourceInfoVectorT>
void build_bundles(FunctionMatchBundles &bundles, const PdbFunctionInfoVector &functions, const SourceInfoVectorT &sources,
    const StringToIndexMapT &function_name_to_index)
{
    if (!sources.empty()) {
        const IndexT sources_count = sources.size();
        bundles.resize(sources_count);

        for (IndexT source_idx = 0; source_idx < sources_count; ++source_idx) {
            const typename SourceInfoVectorT::value_type &source = sources[source_idx];
            FunctionMatchBundle &bundle = bundles[source_idx];
            const IndexT function_count = source.functionIds.size();
            bundle.name = source.name;
            bundle.matches.reserve(function_count);

            for (IndexT function_idx = 0; function_idx < function_count; ++function_idx) {
                const PdbFunctionInfo &functionInfo = functions[source.functionIds[function_idx]];
                StringToIndexMapT::const_iterator it = function_name_to_index.find(functionInfo.decoratedName);
                if (it != function_name_to_index.end()) {
                    bundle.matches.push_back(it->second);
                } else {
                    // Can track unmatched functions here ...
                }
            }
        }
    }
}

} // namespace

bool Runner::process_asm_compare(const AsmCompareOptions &o)
{
    if (!asm_compare_ready())
        return false;

    const size_t less_idx = m_executables[0].get_symbols().size() < m_executables[1].get_symbols().size() ? 0 : 1;
    const size_t more_idx = less_idx == 0 ? 1 : 0;
    const FunctionSetup less_setup(m_executables[less_idx], o.format);
    const FunctionSetup more_setup(m_executables[more_idx], o.format);
    const ExeSymbols &less_symbols = m_executables[less_idx].get_symbols();

    auto in_code_section = [&](size_t idx, const ExeSymbol &symbol) -> bool {
        const ExeSectionInfo *code_section = m_executables[idx].get_code_section();
        return symbol.address >= code_section->address && symbol.address < code_section->address + code_section->size;
    };

    using StringToIndexMapT = std::unordered_map<std::string, IndexT>;
    StringToIndexMapT function_name_to_index;
    m_matches.reserve(512);
    function_name_to_index.reserve(512);

    for (const ExeSymbol &less_symbol : less_symbols) {
        if (!in_code_section(less_idx, less_symbol)) {
            continue;
        }
        const ExeSymbol &more_symbol = m_executables[more_idx].get_symbol(less_symbol.name);
        if (more_symbol.name.empty() || !in_code_section(more_idx, more_symbol)) {
            continue;
        }
        IndexT index = m_matches.size();
        m_matches.emplace_back();
        FunctionMatch &match = m_matches.back();
        match.name = less_symbol.name;
        match.functions[less_idx].disassemble(less_setup, less_symbol.address, less_symbol.address + less_symbol.size);
        match.functions[more_idx].disassemble(more_setup, more_symbol.address, more_symbol.address + more_symbol.size);
        function_name_to_index[match.name] = index;
    }

    if (o.bundle_file_idx < 2) {
        switch (o.bundle_type) {
            case MatchBundleType::Compiland: {
                const PdbFunctionInfoVector &functions = m_pdbReaders[o.bundle_file_idx].get_functions();
                const PdbCompilandInfoVector &compilands = m_pdbReaders[o.bundle_file_idx].get_compilands();

                build_bundles(m_matchBundles, functions, compilands, function_name_to_index);
                break;
            }
            case MatchBundleType::SourceFile: {
                const PdbFunctionInfoVector &functions = m_pdbReaders[o.bundle_file_idx].get_functions();
                const PdbSourceFileInfoVector &sources = m_pdbReaders[o.bundle_file_idx].get_source_files();

                build_bundles(m_matchBundles, functions, sources, function_name_to_index);
                break;
            }
        }
    }

    if (m_matchBundles.empty()) {
        // Create a dummy bundle with all function matches.
        m_matchBundles.resize(1);
        FunctionMatchBundle &bundle = m_matchBundles[0];
        const size_t count = m_matches.size();
        bundle.name = "all";
        bundle.matches.resize(count);
        for (size_t i = 0; i < count; ++i) {
            bundle.matches[i] = i;
        }
    }

    return true;
}

bool Runner::asm_compare_ready() const
{
    static_assert(MAX_INPUT_FILES >= 2, "Expects at least two input files to work with");

    return m_executables[0].is_ready() && m_executables[1].is_ready();
}

const std::string &Runner::get_exe_filename(size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    return m_executables[file_idx].get_filename();
}

std::string Runner::get_exe_file_name_from_pdb(size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    const PdbExeInfo &exe_info = m_pdbReaders[file_idx].get_exe_info();
    assert(!exe_info.exeFileName.empty());
    assert(!exe_info.pdbFilePath.empty());

    std::filesystem::path path = exe_info.pdbFilePath;
    path = path.parent_path();
    path /= exe_info.exeFileName;
    path += ".exe";

    return path.string();
}

} // namespace unassemblize
