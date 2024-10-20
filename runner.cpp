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
#include "function.h"
#include "util.h"
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

void Runner::dump_function_to_file(const std::string &file_name, Executable &exe, uint64_t start, uint64_t end)
{
    if (!file_name.empty()) {
        FILE *fp = fopen(file_name.c_str(), "w+");
        if (fp != nullptr) {
            exe.dissassemble_function(fp, start, end);
            fclose(fp);
        }
    } else {
        exe.dissassemble_function(nullptr, start, end);
    }
}

// TODO: split into 2 functions, load exe, disassemble functions
bool Runner::process_exe(const ExeOptions &o)
{
    if (o.verbose) {
        printf("Parsing exe file '%s'...\n", o.input_file.c_str());
    }

    // TODO implement default value where exe object decides internally what to do.
    Executable::OutputFormat format = Executable::OUTPUT_IGAS;

    if (!o.format_str.empty()) {
        if (0 == strcasecmp(o.format_str.c_str(), "igas")) {
            format = Executable::OUTPUT_IGAS;
        } else if (0 == strcasecmp(o.format_str.c_str(), "masm")) {
            format = Executable::OUTPUT_MASM;
        }
    }

    m_executable.set_output_format(format);
    m_executable.set_verbose(o.verbose);

    if (!m_executable.read(o.input_file)) {
        return false;
    }

    if (o.print_secs) {
        print_sections(m_executable);
        return true;
    }

    constexpr bool pdb_symbols_overwrite_exe_symbols = true;
    constexpr bool cfg_symbols_overwrite_exe_pdb_symbols = true;

    const PdbSymbolInfoVector &pdb_symbols = m_pdbReader.get_symbols();

    if (!pdb_symbols.empty()) {
        m_executable.add_symbols(pdb_symbols, pdb_symbols_overwrite_exe_symbols);
    }

    if (o.dump_syms) {
        m_executable.save_config(o.config_file.c_str());
        return true;
    }

    m_executable.load_config(o.config_file.c_str(), cfg_symbols_overwrite_exe_pdb_symbols);

    if (o.start_addr == 0 && o.end_addr == 0) {
        for (const ExeSymbol &symbol : m_executable.get_symbols()) {
            std::string sanitized_symbol_name = symbol.name;
#if defined(WIN32)
            util::remove_characters_inplace(sanitized_symbol_name, "\\/:*?\"<>|");
#endif
            std::string file_name;
            if (!o.output_file.empty()) {
                // program.symbol.S
                file_name = util::get_file_name_without_ext(o.output_file) + "." + sanitized_symbol_name + ".S";
            }
            dump_function_to_file(file_name, m_executable, symbol.address, symbol.address + symbol.size);
        }
    } else {
        dump_function_to_file(o.output_file, m_executable, o.start_addr, o.end_addr);
    }

    return true;
}

bool Runner::process_pdb(const PdbOptions &o)
{
    m_pdbReader.set_verbose(o.verbose);

    // Currently does not read back config file here.

    if (!m_pdbReader.read(o.input_file)) {
        return false;
    }

    if (o.dump_syms) {
        m_pdbReader.save_config(o.config_file);
    }

    return true;
}

std::string Runner::get_pdb_exe_file_name()
{
    PdbExeInfo exe_info = m_pdbReader.get_exe_info();
    assert(!exe_info.exeFileName.empty());
    assert(!exe_info.pdbFilePath.empty());

    return util::get_file_path(exe_info.pdbFilePath) + "/" + exe_info.exeFileName + ".exe";
}

} // namespace unassemblize
