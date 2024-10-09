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
#include <IntSafe.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct IDiaDataSource;
struct IDiaLineNumber;
struct IDiaSession;
struct IDiaSourceFile;
struct IDiaSymbol;

namespace unassemblize
{
// A copy of DIA2's CV_SourceChksum_t
enum class CV_Chksum
{
    CHKSUM_TYPE_NONE = 0, // indicates no checksum is available
    CHKSUM_TYPE_MD5,
    CHKSUM_TYPE_SHA1,
    CHKSUM_TYPE_SHA_256,
};

// A copy of DIA2's CV_call_e
enum class CV_Call
{
    CV_CALL_NEAR_C = 0x00, // near right to left push, caller pops stack
    CV_CALL_FAR_C = 0x01, // far right to left push, caller pops stack
    CV_CALL_NEAR_PASCAL = 0x02, // near left to right push, callee pops stack
    CV_CALL_FAR_PASCAL = 0x03, // far left to right push, callee pops stack
    CV_CALL_NEAR_FAST = 0x04, // near left to right push with regs, callee pops stack
    CV_CALL_FAR_FAST = 0x05, // far left to right push with regs, callee pops stack
    CV_CALL_SKIPPED = 0x06, // skipped (unused) call index
    CV_CALL_NEAR_STD = 0x07, // near standard call
    CV_CALL_FAR_STD = 0x08, // far standard call
    CV_CALL_NEAR_SYS = 0x09, // near sys call
    CV_CALL_FAR_SYS = 0x0a, // far sys call
    CV_CALL_THISCALL = 0x0b, // this call (this passed in register)
    CV_CALL_MIPSCALL = 0x0c, // Mips call
    CV_CALL_GENERIC = 0x0d, // Generic call sequence
    CV_CALL_ALPHACALL = 0x0e, // Alpha call
    CV_CALL_PPCCALL = 0x0f, // PPC call
    CV_CALL_SHCALL = 0x10, // Hitachi SuperH call
    CV_CALL_ARMCALL = 0x11, // ARM call
    CV_CALL_AM33CALL = 0x12, // AM33 call
    CV_CALL_TRICALL = 0x13, // TriCore Call
    CV_CALL_SH5CALL = 0x14, // Hitachi SuperH-5 call
    CV_CALL_M32RCALL = 0x15, // M32R Call
    CV_CALL_CLRCALL = 0x16, // clr call
    CV_CALL_INLINE = 0x17, // Marker for routines always inlined and thus lacking a convention
    CV_CALL_NEAR_VECTOR = 0x18, // near left to right push with regs, callee pops stack
    CV_CALL_SWIFT = 0x19, // Swift calling convention
    CV_CALL_RESERVED = 0x20 // first unused call enumeration

    // Do NOT add any more machine specific conventions.  This is to be used for
    // calling conventions in the source only (e.g. __cdecl, __stdcall).
};

using Address64T = uint64_t;
using Address32T = uint32_t;
using IndexT = uint32_t;

struct PdbAddress
{
    Address64T absVirtual = 0; // Position of the function within the executable
    Address32T relVirtual = 0; // Relative position of the function within its block
    uint32_t section = 0; // Section part of location
    Address32T offset = 0; // Offset part of location
};

struct PdbSourceLineInfo
{
    uint32_t lineNumber = 0; // Line number in document
    uint32_t offset = 0; // Offset from function address
    uint32_t length = 0; // Length of asm bytes
};

struct PdbFunctionInfo
{
    bool has_valid_source_file_id() const { return sourceFileId != -1; }

    IndexT sourceFileId = -1; // Synonymous for index, can be -1, matches DIA2 index
    IndexT compilandId = -1; // Synonymous for index, can not be -1, matches DIA2 index

    PdbAddress address;
    PdbAddress debugStartAddress;
    PdbAddress debugEndAddress;
    uint32_t length = 0;
    CV_Call call = CV_Call(-1);

    std::string decoratedName; // ?nameToLowercaseKey@NameKeyGenerator@@QAE?AW4NameKeyType@@PBD@Z
    std::string undecoratedName; // public: enum NameKeyType __thiscall NameKeyGenerator::nameToLowercaseKey(char const *)
    std::string globalName; // NameKeyGenerator::nameToLowercaseKey

    std::vector<PdbSourceLineInfo> sourceLines;
};

struct PdbSourceFileInfo
{
    bool is_valid() const { return !name.empty(); }

    std::string name;
    CV_Chksum checksumType;
    std::vector<uint8_t> checksum;
    std::vector<IndexT> compilandIds; // Synonymous for indices
    std::vector<IndexT> functionIds; // Synonymous for indices
};

struct PdbCompilandInfo
{
    std::string name;
    std::vector<IndexT> sourceFileIds; // Synonymous for indices
    std::vector<IndexT> functionIds; // Synonymous for indices
};

class PdbReader
{
    using AddressToIndexMapT = std::unordered_map<Address32T, IndexT>;

public:
    PdbReader(const std::string &pdb_file, bool verbose = false);

    bool read();
    void save_config(const std::string &config_file);
    void dump_symbols(nlohmann::json &js);

private:
    bool load();
    void unload();
    bool read_symbols();

    bool read_compilands();
    void read_compiland_symbol(PdbCompilandInfo &compilandInfo, IDiaSymbol *pSymbol);
    void read_compiland_function(
        PdbCompilandInfo &compilandInfo, PdbFunctionInfo &functionInfo, IndexT functionId, IDiaSymbol *pSymbol);
    void read_compiland_function_start(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);
    void read_compiland_function_end(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);

    void read_public_function(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol);

    void read_source_file(PdbCompilandInfo &compilandInfo, IDiaSourceFile *pSourceFile);
    void read_source_file_from_line(PdbFunctionInfo &functionInfo, IndexT functionId, IDiaLineNumber *pLine);
    void read_line(PdbFunctionInfo &function_info, IDiaLineNumber *pLine);

    const std::string m_filename;
    const bool m_verbose = false;
    IDiaDataSource *m_pDiaSource = nullptr;
    IDiaSession *m_pDiaSession = nullptr;
    IDiaSymbol *m_pDiaSymbol = nullptr;
    uint32_t m_dwMachineType = 0;
    bool m_coInitialized = false;

    // Compilands and Source Files indices match DIA2 indices.
    // Source Files may contain unused entries.
    std::vector<PdbCompilandInfo> m_compilands;
    std::vector<PdbSourceFileInfo> m_sourceFiles;
    std::vector<PdbFunctionInfo> m_functions;
};

} // namespace unassemblize
