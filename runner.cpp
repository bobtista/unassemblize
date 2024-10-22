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
void Runner::print_sections(Executable &exe)
{
    const ExeSectionMap &map = exe.get_section_map();
    for (auto it = map.begin(); it != map.end(); ++it) {
        printf(
            "Name: %s, Address: 0x%" PRIx64 " Size: %" PRIu64 "\n", it->first.c_str(), it->second.address, it->second.size);
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

bool Runner::process_asm_compare(const AsmCompareOptions &o)
{
    if (!asm_compare_ready())
        return false;

    // The Executable class currently has no function symbol classification.
    // Therefore, take the function symbols from the Pdb reader if applicable.

    std::array<ExeSymbols, 2> pdb_function_symbols;

    for (size_t i = 0; i < pdb_function_symbols.size(); ++i) {
        const auto &pdb_functions = m_pdbReaders[i].get_functions();
        pdb_function_symbols[i] = std::move(to_exe_symbols(pdb_functions.begin(), pdb_functions.end()));
    }

    std::array<const ExeSymbols *, 2> function_symbols;

    for (size_t i = 0; i < function_symbols.size(); ++i) {
        if (!pdb_function_symbols[i].empty()) {
            function_symbols[i] = &pdb_function_symbols[i];
        } else {
            function_symbols[i] = &m_executables[i].get_symbols();
        }
    }

    const size_t less_idx = function_symbols[0]->size() < function_symbols[1]->size() ? 0 : 1;
    const size_t more_idx = less_idx == 0 ? 1 : 0;
    const FunctionSetup less_setup(m_executables[less_idx], o.format);
    const FunctionSetup more_setup(m_executables[more_idx], o.format);
    const ExeSymbols &less_symbols = *function_symbols[less_idx];
    m_matches.reserve(less_symbols.size());

    for (const ExeSymbol &less_symbol : less_symbols) {
        const ExeSymbol &more_symbol = m_executables[more_idx].get_symbol(less_symbol.name);
        if (!more_symbol.name.empty()) {
            m_matches.emplace_back();
            FunctionMatch &match = m_matches.back();
            match.name = less_symbol.name;
            match.functions[less_idx].disassemble(less_setup, less_symbol.address, less_symbol.address + less_symbol.size);
            match.functions[more_idx].disassemble(more_setup, more_symbol.address, more_symbol.address + more_symbol.size);
        }
    }

    // ...

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
