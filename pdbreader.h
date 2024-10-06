/**
 * @file
 *
 * @brief Class to extract relevant symbols from PDB files
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
#include <nlohmann/json.hpp>
#include <string>

struct IDiaDataSource;
struct IDiaSession;
struct IDiaSymbol;

namespace unassemblize
{
class PdbReader
{
public:
    PdbReader(const std::string &pdb_file, bool verbose = false);

    bool read();
    void save_config(const std::string &config_file);
    void dump_symbols(nlohmann::json &js);

private:
    bool load();
    void unload();

    const std::string m_filename;
    const bool m_verbose = false;
    IDiaDataSource *m_pDiaSource = nullptr;
    IDiaSession *m_pDiaSession = nullptr;
    IDiaSymbol *m_pDiaSymbol = nullptr;
    uint32_t m_dwMachineType = 0;
    std::map<uint64_t, Executable::Symbol> m_symbolMap;
    std::list<std::string> m_loadedSymbols;
};

} // namespace unassemblize