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
enum class AsmFormat
{
    DEFAULT,
    IGAS,
    AGAS,
    MASM,
};

class Function;

struct FunctionSetup
{
    friend class Function;

    explicit FunctionSetup(const Executable &executable, AsmFormat format = AsmFormat::DEFAULT);

private:
    const Executable &executable;
    const AsmFormat format;
    ZydisStackWidth stackWidth;
    ZydisFormatterStyle style;
    ZydisDecoder decoder;
    ZydisFormatter formatter;
};

class Function
{
    using Address64ToIndexMap = std::map<Address64T, IndexT>;

public:
    Function() = default;

    void disassemble(const FunctionSetup *setup, Address64T start_address, Address64T end_address);
    const std::string &dissassembly() const { return m_dissassembly; }
    const Executable &executable() const { return m_setup->executable; }
    const ExeSymbol &get_symbol(Address64T address) const;
    const ExeSymbol &get_symbol_from_image_base(Address64T address) const;
    const ExeSymbol &get_nearest_symbol(Address64T address) const; // TODO: investigate
    Address64T get_address() const;

private:
    void add_pseudo_symbol(Address64T address);

private:
    const FunctionSetup *m_setup;
    Address64T m_startAddress; // TODO: Perhaps is not necessary

    // Symbols used within disassemble step. Is cleared at the end of it.
    ExeSymbols m_pseudoSymbols;
    Address64ToIndexMap m_pseudoSymbolAddressToIndexMap;

    std::string m_dissassembly; // Disassembly text buffer for this function.
};
} // namespace unassemblize
