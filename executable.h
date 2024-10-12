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

#include <list>
#include <map>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <stdio.h>
#include <string>

namespace LIEF
{
class Binary;
}

namespace unassemblize
{
class Executable
{
public:
    enum OutputFormats
    {
        OUTPUT_IGAS,
        OUTPUT_MASM,
    };

    enum SectionTypes
    {
        SECTION_DATA,
        SECTION_CODE,
    };

    struct SectionInfo
    {
        const uint8_t *data;
        uint64_t address;
        uint64_t size;
        SectionTypes type;
    };

    struct Symbol
    {
        std::string name;
        uint64_t address = 0;
        uint64_t size = 0;
    };

    struct ObjectSection
    {
        std::string name;
        uint64_t offset;
        uint64_t size;
    };

    struct Object
    {
        std::string name;
        std::list<ObjectSection> sections; // TODO: vector
    };

    struct ImageData
    {
        uint64_t imageBase = 0; // Default image base address if the ASLR is not enabled.
        uint64_t imageEnd = 0; // Image end address.
        uint32_t codeAlignment = sizeof(uint32_t);
        uint32_t dataAlignment = sizeof(uint32_t);
        uint8_t codePad = 0x90; // NOP
        uint8_t dataPad = 0x00;
    };

    using SectionMap = std::map<std::string, SectionInfo>; // TODO: unordered_map maybe
    using SymbolMap = std::map<uint64_t, Symbol>; // TODO: unordered_map maybe
    using Objects = std::list<Object>; // TODO: vector

public:
    Executable(OutputFormats format = OUTPUT_IGAS, bool verbose = false);
    ~Executable();

    bool read(const std::string &exe_file);

    void add_symbols(const SymbolMap &symbolMap);

    void load_config(const char *file_name);
    void save_config(const char *file_name);

    const SectionMap &get_section_map() const;
    const SectionInfo *find_section(uint64_t addr) const;
    const uint8_t *section_data(const char *name) const; // TODO: check how to improve this
    uint64_t section_address(const char *name) const; // TODO: check how to improve this
    uint64_t section_size(const char *name) const; // TODO: check how to improve this
    uint64_t base_address() const;
    uint64_t end_address() const;
    bool do_add_base() const;
    const Symbol &get_symbol(uint64_t addr) const;
    const Symbol &get_nearest_symbol(uint64_t addr) const;
    const SymbolMap &get_symbol_map() const;
    void add_symbol(const char *sym, uint64_t addr);

    /**
     * Disassembles a range of bytes and outputs the format as though it were a single function.
     * Addresses should be the absolute addresses when the binary is loaded at its preferred base address.
     */
    void dissassemble_function(FILE *fp, const char *section_name, uint64_t start, uint64_t end);

private:
    void dissassemble_gas_func(FILE *fp, const char *section_name, uint64_t start, uint64_t end);

    void load_symbols(nlohmann::json &js);
    void dump_symbols(nlohmann::json &js);

    void load_sections(nlohmann::json &js);
    void dump_sections(nlohmann::json &js);

    void load_objects(nlohmann::json &js);
    void dump_objects(nlohmann::json &js);

private:
    const OutputFormats m_outputFormat;
    const bool m_verbose;
    bool m_addBase = false;

    std::unique_ptr<LIEF::Binary> m_binary;
    SectionMap m_sectionMap;
    SymbolMap m_symbolMap;
    Objects m_targetObjects;
    ImageData m_imageData;

    static Symbol s_emptySymbol;
};

} // namespace unassemblize
