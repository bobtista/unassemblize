/**
 * @file
 *
 * @brief Class encapsulating a single function disassembly.
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
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace unassemblize
{
class Function
{
    using Address64ToIndexMap = std::unordered_map<Address64T, IndexT>;

public:
    enum AsmFormat
    {
        FORMAT_DEFAULT,
        FORMAT_IGAS,
        FORMAT_AGAS,
        FORMAT_MASM,
    };

public:
    Function(const Executable &exe, const char *section_name, uint64_t start, uint64_t end) :
        m_executable(exe), m_section(section_name), m_startAddress(start), m_endAddress(end)
    {
    }
    void disassemble(AsmFormat fmt = FORMAT_DEFAULT); // Run the disassembly of the function.
    const std::string &dissassembly() const { return m_dissassembly; }
    const std::vector<std::string> &dependencies() const { return m_deps; }
    void add_dependency(const std::string &dep) { return m_deps.push_back(dep); }
    uint64_t start_address() const { return m_startAddress; }
    uint64_t end_address() const { return m_endAddress; }
    uint64_t section_address() const { return m_executable.section_address(m_section.c_str()); }
    uint64_t section_end() const
    {
        return m_executable.section_address(m_section.c_str()) + m_executable.section_size(m_section.c_str());
    }
    const Executable &executable() const { return m_executable; }
    const ExeSymbol &get_symbol(uint64_t addr) const;
    const ExeSymbol &get_nearest_symbol(uint64_t addr) const;

private:
    void add_pseudo_symbol(uint64_t address);

private:
    const Executable &m_executable;
    const std::string m_section; // #TODO: Unused, remove?
    const uint64_t m_startAddress; // Runtime start address of the function.
    const uint64_t m_endAddress; // Runtime end address of the function.

    ExeSymbols m_pseudoSymbols; // Symbols used in disassemble step.
    Address64ToIndexMap m_pseudoSymbolAddressToIndexMap;
    std::vector<std::string> m_deps; // Symbols this function depends on.
    std::string m_dissassembly; // Disassembly buffer for this function.
};
} // namespace unassemblize
