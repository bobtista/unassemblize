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
#include <Zydis/Decoder.h>
#include <Zydis/Formatter.h>
#include <Zydis/SharedTypes.h>
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
    enum class AsmFormat
    {
        DEFAULT,
        IGAS,
        AGAS,
        MASM,
    };

public:
    Function(const Executable &exe, AsmFormat format = AsmFormat::DEFAULT);

    void disassemble(uint64_t start_address, uint64_t end_address);
    const std::string &dissassembly() const { return m_dissassembly; }
    const Executable &executable() const { return m_executable; }
    const ExeSymbol &get_symbol(uint64_t addr) const;
    const ExeSymbol &get_symbol_from_image_base(uint64_t addr) const;
    const ExeSymbol &get_nearest_symbol(uint64_t addr) const; // TODO: investigate

private:
    void add_pseudo_symbol(uint64_t address);

private:
    const AsmFormat m_format;
    const Executable &m_executable;

    ZydisStackWidth m_stack_width;
    ZydisFormatterStyle m_style;
    ZydisDecoder m_decoder;
    ZydisFormatter m_formatter;

    ExeSymbols m_pseudoSymbols; // Symbols used in disassemble step.
    Address64ToIndexMap m_pseudoSymbolAddressToIndexMap;
    std::string m_dissassembly; // Disassembly text buffer for this function.
};
} // namespace unassemblize
