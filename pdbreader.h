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

#include "pdbreadertypes.h"

struct IDiaDataSource;
struct IDiaEnumSourceFiles;
struct IDiaLineNumber;
struct IDiaSession;
struct IDiaSourceFile;
struct IDiaSymbol;

namespace unassemblize
{
class PdbReader
{
    using StringToIndexMapT = std::unordered_map<std::string, IndexT>;

public:
    PdbReader(bool verbose = false);

    bool read(const std::string &pdb_file);

    void load_json(const nlohmann::json &js);
    bool load_config(const std::string &file_name);
    void save_json(nlohmann::json &js, bool overwrite_sections = false);
    bool save_config(const std::string &file_name, bool overwrite_sections = false);

private:
    bool load(const std::string &pdb_file);
    void unload();

    bool read_symbols();
    bool read_global_scope();

    IDiaEnumSourceFiles *get_enum_source_files();
    bool read_source_files();
    void read_source_file_initial(IDiaSourceFile *pSourceFile);

    bool read_compilands();
    void read_compiland_symbol(PdbCompilandInfo &compilandInfo, IDiaSymbol *pSymbol);
    void read_compiland_function(
        PdbCompilandInfo &compilandInfo, PdbFunctionInfo &functionInfo, IndexT functionId, IDiaSymbol *pSymbol);
    void read_compiland_function_start(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);
    void read_compiland_function_end(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);

    void read_public_function(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);

    void read_source_file_for_compiland(PdbCompilandInfo &compilandInfo, IDiaSourceFile *pSourceFile);
    void read_source_file_for_function(PdbFunctionInfo &functionInfo, IndexT functionId, IDiaSourceFile *pSourceFile);
    void read_line(PdbFunctionInfo &function_info, IDiaLineNumber *pLine);

private:
    const bool m_verbose = false;
    IDiaDataSource *m_pDiaSource = nullptr;
    IDiaSession *m_pDiaSession = nullptr;
    IDiaSymbol *m_pDiaSymbol = nullptr;
    uint32_t m_dwMachineType = 0;
    bool m_coInitialized = false;

    // Compilands indices match DIA2 indices.
    // Source Files indices do not match DIA2 indices (aka "unique id").
    std::vector<PdbCompilandInfo> m_compilands;
    std::vector<PdbSourceFileInfo> m_sourceFiles;
    std::vector<PdbFunctionInfo> m_functions;
    PdbExeInfo m_exe;

    StringToIndexMapT m_sourceFileNameToIndexMap;
};

} // namespace unassemblize
