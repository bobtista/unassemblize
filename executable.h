/**
 * @file
 *
 * @brief Class encapsulating the executable being disassembled.
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "executabletypes.h"
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <stdio.h>

namespace LIEF
{
class Binary;
}

namespace unassemblize
{
class Executable
{
    using Address64ToIndexMap = std::unordered_map<Address64T, IndexT>;

public:
    enum OutputFormats
    {
        OUTPUT_IGAS,
        OUTPUT_MASM,
    };

public:
    Executable(OutputFormats format = OUTPUT_IGAS, bool verbose = false);
    ~Executable();

    bool read(const std::string &exe_file);

    void load_config(const char *file_name);
    void save_config(const char *file_name);

    const ExeSectionMap &get_section_map() const;
    const ExeSectionInfo *find_section(uint64_t addr) const;
    const uint8_t *section_data(const char *name) const; // TODO: check how to improve this
    uint64_t section_address(const char *name) const; // TODO: check how to improve this
    uint64_t section_size(const char *name) const; // TODO: check how to improve this
    uint64_t base_address() const;
    uint64_t end_address() const;
    bool do_add_base() const;
    const ExeSymbol &get_symbol(uint64_t addr) const;
    const ExeSymbol &get_nearest_symbol(uint64_t addr) const;
    const ExeSymbols &get_symbols() const;
    void add_symbols(const ExeSymbols &symbols);
    void add_symbol(const ExeSymbol &symbol);

    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    void dissassemble_function(FILE *fp, const char *section_name, uint64_t start, uint64_t end);

private:
    void dissassemble_gas_func(FILE *fp, const char *section_name, uint64_t start, uint64_t end);

    void load_symbols(nlohmann::json &js);
    void dump_symbols(nlohmann::json &js) const;

    void load_sections(nlohmann::json &js);
    void dump_sections(nlohmann::json &js) const;

    void load_objects(nlohmann::json &js);
    void dump_objects(nlohmann::json &js) const;

private:
    const OutputFormats m_outputFormat;
    const bool m_verbose;
    bool m_addBase = false;

    std::unique_ptr<LIEF::Binary> m_binary;
    ExeSectionMap m_sectionMap;
    ExeSymbols m_symbols;
    Address64ToIndexMap m_symbolAddressToIndexMap;
    ExeObjects m_targetObjects;
    ExeImageData m_imageData;

    static ExeSymbol s_emptySymbol;
};

} // namespace unassemblize
