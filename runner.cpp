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
bool Runner::process_exe(const ExeSaveLoadOptions &o, size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    Executable &executable = m_executable[file_idx];

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

    const PdbReader &pdbReader = m_pdbReader[file_idx];
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
    PdbReader &pdbReader = m_pdbReader[file_idx];

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

bool Runner::process_disassemble(const DisassembleOptions &o)
{
    // TODO: implement default value where exe object decides internally what to do.
    Executable::OutputFormat format = Executable::OUTPUT_IGAS;

    if (!o.format_str.empty()) {
        if (0 == strcasecmp(o.format_str.c_str(), "igas")) {
            format = Executable::OUTPUT_IGAS;
        } else if (0 == strcasecmp(o.format_str.c_str(), "masm")) {
            format = Executable::OUTPUT_MASM;
        }
    }

    Executable &executable = m_executable[0];
    executable.set_output_format(format);

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
            dump_function_to_file(file_name, executable, symbol.address, symbol.address + symbol.size);
        }
    } else {
        dump_function_to_file(o.output_file, executable, o.start_addr, o.end_addr);
    }

    return true;
}

const std::string &Runner::get_exe_filename(size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    return m_executable[file_idx].get_filename();
}

std::string Runner::get_exe_file_name_from_pdb(size_t file_idx)
{
    assert(file_idx < MAX_INPUT_FILES);
    const PdbExeInfo &exe_info = m_pdbReader[file_idx].get_exe_info();
    assert(!exe_info.exeFileName.empty());
    assert(!exe_info.pdbFilePath.empty());

    std::filesystem::path path = exe_info.pdbFilePath;
    path = path.parent_path();
    path /= exe_info.exeFileName;
    path += ".exe";

    return path.string();
}

} // namespace unassemblize
