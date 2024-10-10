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
#include "pdbreader.h"
#include "util.h"
#include <dia2.h>
#include <fstream>
#include <iostream>

namespace unassemblize
{
const char *const s_compilands = "pdb_compilands";
const char *const s_sourceFiles = "pdb_source_files";
const char *const s_functions = "pdb_functions";
const char *const s_exe = "pdb_exe";

PdbReader::PdbReader(bool verbose) : m_verbose(verbose), m_dwMachineType(CV_CFL_80386) {}

bool PdbReader::read(const std::string &pdb_file)
{
    bool success = false;

    if (load(pdb_file)) {
        success = read_symbols();
    }
    unload();

    return success;
}

void PdbReader::load_json(const nlohmann::json &js)
{
    js.at(s_compilands).get_to(m_compilands);
    js.at(s_sourceFiles).get_to(m_sourceFiles);
    js.at(s_functions).get_to(m_functions);
    js.at(s_exe).get_to(m_exe);
}

bool PdbReader::load_config(const std::string &file_name)
{
    if (m_verbose) {
        printf("Loading config file '%s'...\n", file_name.c_str());
    }

    nlohmann::json js;

    {
        std::ifstream fs(file_name);

        if (!fs.good()) {
            return false;
        }

        js = nlohmann::json::parse(fs);
    }

    load_json(js);

    return true;
}

void PdbReader::save_json(nlohmann::json &js, bool overwrite_sections)
{
    // Don't dump if we already have sections for these.

    if (overwrite_sections || js.find(s_compilands) == js.end()) {
        js[s_compilands] = m_compilands;
    }
    if (overwrite_sections || js.find(s_sourceFiles) == js.end()) {
        js[s_sourceFiles] = m_sourceFiles;
    }
    if (overwrite_sections || js.find(s_functions) == js.end()) {
        js[s_functions] = m_functions;
    }
    if (overwrite_sections || js.find(s_exe) == js.end()) {
        js[s_exe] = m_exe;
    }
}

bool PdbReader::save_config(const std::string &file_name, bool overwrite_sections)
{
    if (m_verbose) {
        printf("Saving config file '%s'...\n", file_name.c_str());
    }

    nlohmann::json js;

    // Parse the config file if it already exists and update it.
    {
        std::ifstream fs(file_name);

        if (fs.good()) {
            js = nlohmann::json::parse(fs);
        }
    }

    save_json(js, overwrite_sections);

    {
        std::ofstream fs(file_name);
        fs << std::setw(2) << js << std::endl;
    }

    return true;
}

bool PdbReader::load(const std::string &pdb_file)
{
    unload();

    HRESULT hr = CoInitialize(NULL);

    if (FAILED(hr)) {
        wprintf(L"CoInitialize failed - HRESULT = %08X\n", hr);
        return false;
    }

    m_coInitialized = true;

    // Obtain access to the provider

    hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)&m_pDiaSource);

    if (FAILED(hr)) {
        wprintf(L"CoCreateInstance failed - HRESULT = %08X\n", hr);
        return false;
    }

    // Open and prepare a program database (.pdb) file as a debug data source

    const std::wstring wfilename = util::to_utf16(pdb_file);

    hr = m_pDiaSource->loadDataFromPdb(wfilename.c_str());

    if (FAILED(hr)) {
        wprintf(L"loadDataFromPdb failed - HRESULT = %08X\n", hr);
        return false;
    }

    // Open a session for querying symbols

    hr = m_pDiaSource->openSession(&m_pDiaSession);

    if (FAILED(hr)) {
        wprintf(L"openSession failed - HRESULT = %08X\n", hr);
        return false;
    }

    // Retrieve a reference to the global scope

    hr = m_pDiaSession->get_globalScope(&m_pDiaSymbol);

    if (hr != S_OK) {
        wprintf(L"get_globalScope failed\n");
        return false;
    }

    // Set Machine type for getting correct register names

    DWORD dwMachType = 0;
    if (m_pDiaSymbol->get_machineType(&dwMachType) == S_OK) {
        switch (dwMachType) {
            case IMAGE_FILE_MACHINE_I386:
                m_dwMachineType = CV_CFL_80386;
                break;
            case IMAGE_FILE_MACHINE_IA64:
                m_dwMachineType = CV_CFL_IA64;
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                m_dwMachineType = CV_CFL_AMD64;
                break;
        }
    }

    return true;
}

void PdbReader::unload()
{
    if (m_coInitialized) {
        if (m_pDiaSession != nullptr) {
            m_pDiaSession->Release();
            m_pDiaSession = nullptr;
        }

        if (m_pDiaSymbol != nullptr) {
            m_pDiaSymbol->Release();
            m_pDiaSymbol = nullptr;
        }

        CoUninitialize();

        m_coInitialized = false;
    }
}

bool PdbReader::read_symbols()
{
    m_compilands.clear();
    m_sourceFiles.clear();
    m_functions.clear();

    m_functions.reserve(1024 * 100);

    bool ok = true;
    read_global_scope();
    ok = ok && read_source_files();
    ok = ok && read_compilands();

    m_functions.shrink_to_fit();
    m_sourceFileNameToIndexMap.clear();

    return ok;
}

bool PdbReader::read_global_scope()
{
    IDiaSymbol *pSymbol;
    if (m_pDiaSession->get_globalScope(&pSymbol) == S_OK) {
        {
            BSTR name;
            if (pSymbol->get_name(&name) == S_OK) {
                m_exe.exeFileName = util::to_utf8(name);
                SysFreeString(name);
            }
        }
        {
            BSTR name;
            if (pSymbol->get_symbolsFileName(&name) == S_OK) {
                m_exe.pdbFilePath = util::to_utf8(name);
                SysFreeString(name);
            }
        }
        pSymbol->Release();
    }

    return !m_exe.exeFileName.empty() && !m_exe.pdbFilePath.empty();
}

IDiaEnumSourceFiles *PdbReader::get_enum_source_files()
{
    // https://learn.microsoft.com/en-us/visualstudio/debugger/debug-interface-access/idiaenumsourcefiles

    IDiaEnumSourceFiles *pUnknown = NULL;
    REFIID iid = __uuidof(IDiaEnumSourceFiles);
    IDiaEnumTables *pEnumTables = NULL;
    IDiaTable *pTable = NULL;
    ULONG celt = 0;

    if (m_pDiaSession->getEnumTables(&pEnumTables) != S_OK) {
        wprintf(L"ERROR - GetTable() getEnumTables\n");
        return NULL;
    }
    while (pEnumTables->Next(1, &pTable, &celt) == S_OK && celt == 1) {
        // There is only one table that matches the given iid
        HRESULT hr = pTable->QueryInterface(iid, (void **)&pUnknown);
        pTable->Release();
        if (hr == S_OK) {
            break;
        }
    }
    pEnumTables->Release();
    return pUnknown;
}

bool PdbReader::read_source_files()
{
    IDiaEnumSourceFiles *pEnumSourceFiles = get_enum_source_files();
    if (pEnumSourceFiles != nullptr) {
        {
            LONG count = 0;
            pEnumSourceFiles->get_Count(&count);
            m_sourceFiles.reserve(count);
            m_sourceFileNameToIndexMap.reserve(count);
        }

        IDiaSourceFile *pSourceFile;
        ULONG celt = 0;

        // Go through all the source files.

        while (SUCCEEDED(pEnumSourceFiles->Next(1, &pSourceFile, &celt)) && (celt == 1)) {
            read_source_file_initial(pSourceFile);
            pSourceFile->Release();
        }
        pEnumSourceFiles->Release();
        return true;
    }
    return false;
}

void PdbReader::read_source_file_initial(IDiaSourceFile *pSourceFile)
{
    // Note: get_uniqueId returns a random id. Do not trust it.

    const IndexT sourceFileIndex = m_sourceFiles.size();
    m_sourceFiles.emplace_back();
    PdbSourceFileInfo &fileInfo = m_sourceFiles.back();

    assert(fileInfo.name.empty());

    // Populate source file info.
    {
        BSTR name;
        if (pSourceFile->get_fileName(&name) == S_OK) {
            fileInfo.name = util::to_utf8(name);
            m_sourceFileNameToIndexMap[fileInfo.name] = sourceFileIndex;
            SysFreeString(name);
        }
    }
    {
        DWORD checksumType = CHKSUM_TYPE_NONE;
        if (pSourceFile->get_checksumType(&checksumType) == S_OK) {
            fileInfo.checksumType = static_cast<CV_Chksum>(checksumType);
        }
    }
    {
        BYTE checksum[256];
        DWORD cbChecksum = sizeof(checksum);
        if (pSourceFile->get_checksum(cbChecksum, &cbChecksum, checksum) == S_OK) {
            fileInfo.checksum.assign(checksum, checksum + cbChecksum);
        }
    }
    {
        IDiaEnumSymbols *pEnumCompilands;
        if (pSourceFile->get_compilands(&pEnumCompilands) == S_OK) {
            IDiaSymbol *pCompiland;
            ULONG celt = 0;

            {
                LONG count = 0;
                pEnumCompilands->get_Count(&count);
                fileInfo.compilandIds.reserve(count);
            }

            while (SUCCEEDED(pEnumCompilands->Next(1, &pCompiland, &celt)) && (celt == 1)) {
                DWORD indexId;
                if (pCompiland->get_symIndexId(&indexId) == S_OK) {
                    fileInfo.compilandIds.push_back(indexId);
                }
            }
        }
    }
}

bool PdbReader::read_compilands()
{
    // Retrieve the compilands.

    IDiaEnumSymbols *pEnumCompilands;

    if (FAILED(m_pDiaSymbol->findChildren(SymTagCompiland, NULL, nsNone, &pEnumCompilands))) {
        return false;
    }

    {
        LONG count = 0;
        pEnumCompilands->get_Count(&count);
        m_compilands.reserve(count);
    }

    IDiaSymbol *pCompiland;
    ULONG celt = 0;

    // Go through all the compilands.

    while (SUCCEEDED(pEnumCompilands->Next(1, &pCompiland, &celt)) && (celt == 1)) {
        m_compilands.emplace_back();
        PdbCompilandInfo &compilandInfo = m_compilands.back();

        {
            BSTR name;
            if (pCompiland->get_name(&name) == S_OK) {
                compilandInfo.name = util::to_utf8(name);
                SysFreeString(name);
            }
        }

        {
            // Every compiland could contain multiple references to the source files which were used to build it.
            // Retrieve all source files by compiland by passing NULL for the name of the source file.

            IDiaEnumSourceFiles *pEnumSourceFiles;

            if (SUCCEEDED(m_pDiaSession->findFile(pCompiland, NULL, nsNone, &pEnumSourceFiles))) {
                IDiaSourceFile *pSourceFile;

                {
                    LONG count = 0;
                    pEnumSourceFiles->get_Count(&count);
                    compilandInfo.sourceFileIds.reserve(count);
                }

                while (SUCCEEDED(pEnumSourceFiles->Next(1, &pSourceFile, &celt)) && (celt == 1)) {
                    read_source_file_for_compiland(compilandInfo, pSourceFile);
                    pSourceFile->Release();
                }

                pEnumSourceFiles->Release();
            }
        }

        {
            // Go through all the symbols defined in this compiland.

            IDiaEnumSymbols *pEnumChildren;

            if (SUCCEEDED(pCompiland->findChildren(SymTagNull, NULL, nsNone, &pEnumChildren))) {
                {
                    LONG count = 0;
                    pEnumCompilands->get_Count(&count);
                    compilandInfo.functionIds.reserve(count);
                }

                IDiaSymbol *pSymbol;
                ULONG celtChildren = 0;

                while (SUCCEEDED(pEnumChildren->Next(1, &pSymbol, &celtChildren)) && (celtChildren == 1)) {
                    read_compiland_symbol(compilandInfo, pSymbol);
                    pSymbol->Release();
                }

                pEnumChildren->Release();
            }
        }

        pCompiland->Release();
    }

    pEnumCompilands->Release();

    m_compilands.shrink_to_fit();

    return true;
}

void PdbReader::read_compiland_symbol(PdbCompilandInfo &compilandInfo, IDiaSymbol *pSymbol)
{
    // This function provides skeleton functionality, as seen in DIA2DUMP app.

    DWORD dwSymTag;

    if (pSymbol->get_symTag(&dwSymTag) != S_OK) {
        wprintf(L"ERROR - get_symTag() failed\n");
        return;
    }

    const auto symTag = static_cast<enum SymTagEnum>(dwSymTag);

    switch (symTag) {
        case SymTagCompilandDetails:
            break;

        case SymTagCompilandEnv:
            break;

        case SymTagData:
            break;

        case SymTagFunction: {
            const auto compilandId = static_cast<IndexT>(m_compilands.size() - 1);
            const auto functionId = static_cast<IndexT>(m_functions.size());
            compilandInfo.functionIds.push_back(functionId);
            m_functions.emplace_back();
            PdbFunctionInfo &functionInfo = m_functions.back();
            assert(functionInfo.compilandId == -1);
            functionInfo.compilandId = compilandId;
            read_compiland_function(compilandInfo, functionInfo, functionId, pSymbol);
            break;
        }

        case SymTagBlock:
            break;

        case SymTagAnnotation:
            break;

        case SymTagLabel:
            break;

        case SymTagEnum:
        case SymTagTypedef:
        case SymTagUDT:
        case SymTagBaseClass:
            break;

        case SymTagFuncDebugStart:
            read_compiland_function_start(m_functions.back(), pSymbol);
            break;

        case SymTagFuncDebugEnd:
            read_compiland_function_end(m_functions.back(), pSymbol);
            break;

        case SymTagFunctionArgType:
        case SymTagFunctionType:
        case SymTagPointerType:
        case SymTagArrayType:
        case SymTagBaseType:
            break;

        case SymTagThunk:
            break;

        case SymTagCallSite:
            break;

        case SymTagHeapAllocationSite:
            break;

        case SymTagCoffGroup:
            break;

        default:
            break;
    }

    switch (symTag) {
        case SymTagFunction:
        case SymTagBlock:
        case SymTagAnnotation:
        case SymTagUDT: {
            IDiaEnumSymbols *pEnumChildren;

            if (SUCCEEDED(pSymbol->findChildren(SymTagNull, NULL, nsNone, &pEnumChildren))) {
                IDiaSymbol *pChild;
                ULONG celt = 0;

                while (SUCCEEDED(pEnumChildren->Next(1, &pChild, &celt)) && (celt == 1)) {
                    read_compiland_symbol(compilandInfo, pChild);
                    pChild->Release();
                }

                pEnumChildren->Release();
            }
        }
    }
}

void PdbReader::read_compiland_function(
    PdbCompilandInfo &compiland_info, PdbFunctionInfo &functionInfo, IndexT functionId, IDiaSymbol *pSymbol)
{
    ULONGLONG dwVA = ~ULONGLONG(0);
    DWORD dwRVA = ~DWORD(0);
    DWORD dwSec = 0;
    DWORD dwOff = 0;
    ULONGLONG ulLen = 0;
    DWORD dwCall = DWORD(-1);

    pSymbol->get_virtualAddress(&dwVA);
    pSymbol->get_relativeVirtualAddress(&dwRVA);
    pSymbol->get_addressSection(&dwSec);
    pSymbol->get_addressOffset(&dwOff);
    pSymbol->get_length(&ulLen);
    pSymbol->get_callingConvention(&dwCall);

    functionInfo.address.absVirtual = static_cast<uint64_t>(dwVA);
    functionInfo.address.relVirtual = static_cast<uint32_t>(dwRVA);
    functionInfo.address.section = static_cast<uint32_t>(dwSec);
    functionInfo.address.offset = static_cast<uint32_t>(dwOff);
    functionInfo.length = static_cast<uint32_t>(ulLen);
    functionInfo.call = static_cast<CV_Call>(dwCall);

    {
        BSTR name;
        if (pSymbol->get_undecoratedName(&name) == S_OK) {
            functionInfo.undecoratedName = util::to_utf8(name);
            SysFreeString(name);
        }
    }

    {
        BSTR name;
        if (pSymbol->get_name(&name) == S_OK) {
            functionInfo.globalName = util::to_utf8(name);
            SysFreeString(name);
        }
    }

    {
        IDiaEnumSymbols *pEnumSymbols;

        if (SUCCEEDED(m_pDiaSymbol->findChildrenExByVA(SymTagPublicSymbol, NULL, nsNone, dwVA, &pEnumSymbols))) {
            IDiaSymbol *pSymbol;
            ULONG celt = 0;

            while (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && (celt == 1)) {
                read_public_function(functionInfo, pSymbol);
                pSymbol->Release();
            }

            pEnumSymbols->Release();
        }
    }

    {
        IDiaEnumLineNumbers *pLines;

        if (SUCCEEDED(m_pDiaSession->findLinesByVA(dwVA, DWORD(ulLen), &pLines))) {
            IDiaLineNumber *pLine;
            DWORD celt;

            {
                LONG count = 0;
                pLines->get_Count(&count);
                functionInfo.sourceLines.reserve(count);
            }

            int line_index = 0;

            while (SUCCEEDED(pLines->Next(1, &pLine, &celt)) && (celt == 1)) {
                if (line_index++ == 0) {
                    IDiaSourceFile *pSource;
                    if (pLine->get_sourceFile(&pSource) == S_OK) {
                        read_source_file_for_function(functionInfo, functionId, pSource);

                        pSource->Release();
                    }
                }
                read_line(functionInfo, pLine);
                pLine->Release();
            }

            pLines->Release();
        }
    }
}

void PdbReader::read_compiland_function_start(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol)
{
    DWORD dwLocType;
    assert(pSymbol->get_locationType(&dwLocType) == S_OK);
    assert(dwLocType == LocIsStatic);

    ULONGLONG dwVA = ~ULONGLONG(0);
    DWORD dwRVA = ~DWORD(0);
    DWORD dwSec = 0;
    DWORD dwOff = 0;

    pSymbol->get_virtualAddress(&dwVA);
    pSymbol->get_relativeVirtualAddress(&dwRVA);
    pSymbol->get_addressSection(&dwSec);
    pSymbol->get_addressOffset(&dwOff);

    functionInfo.debugStartAddress.absVirtual = static_cast<uint64_t>(dwVA);
    functionInfo.debugStartAddress.relVirtual = static_cast<uint32_t>(dwRVA);
    functionInfo.debugStartAddress.section = static_cast<uint32_t>(dwSec);
    functionInfo.debugStartAddress.offset = static_cast<uint32_t>(dwOff);
}

void PdbReader::read_compiland_function_end(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol)
{
    DWORD dwLocType;
    assert(pSymbol->get_locationType(&dwLocType) == S_OK);
    assert(dwLocType == LocIsStatic);

    ULONGLONG dwVA = ~ULONGLONG(0);
    DWORD dwRVA = ~DWORD(0);
    DWORD dwSec = 0;
    DWORD dwOff = 0;

    pSymbol->get_virtualAddress(&dwVA);
    pSymbol->get_relativeVirtualAddress(&dwRVA);
    pSymbol->get_addressSection(&dwSec);
    pSymbol->get_addressOffset(&dwOff);

    functionInfo.debugEndAddress.absVirtual = static_cast<uint64_t>(dwVA);
    functionInfo.debugEndAddress.relVirtual = static_cast<uint32_t>(dwRVA);
    functionInfo.debugEndAddress.section = static_cast<uint32_t>(dwSec);
    functionInfo.debugEndAddress.offset = static_cast<uint32_t>(dwOff);
}

void PdbReader::read_public_function(PdbFunctionInfo &functionInfo, IDiaSymbol *pSymbol)
{
    BSTR name;
    if (pSymbol->get_name(&name) == S_OK) {
        functionInfo.decoratedName = util::to_utf8(name);
        SysFreeString(name);
    }
}

void PdbReader::read_source_file_for_compiland(PdbCompilandInfo &compilandInfo, IDiaSourceFile *pSourceFile)
{
    BSTR name;
    if (pSourceFile->get_fileName(&name) == S_OK) {
        std::string n = util::to_utf8(name);
        StringToIndexMapT::iterator it = m_sourceFileNameToIndexMap.find(n);
        assert(it != m_sourceFileNameToIndexMap.end());

        compilandInfo.sourceFileIds.push_back(it->second);

        SysFreeString(name);
    }
}

void PdbReader::read_source_file_for_function(PdbFunctionInfo &functionInfo, IndexT functionId, IDiaSourceFile *pSourceFile)
{
    BSTR name;
    if (pSourceFile->get_fileName(&name) == S_OK) {
        std::string n = util::to_utf8(name);
        StringToIndexMapT::iterator it = m_sourceFileNameToIndexMap.find(n);
        assert(it != m_sourceFileNameToIndexMap.end());

        assert(functionInfo.sourceFileId == -1);
        functionInfo.sourceFileId = static_cast<IndexT>(it->second);
        m_sourceFiles[it->second].functionIds.push_back(functionId);

        SysFreeString(name);
    }
}

void PdbReader::read_line(PdbFunctionInfo &functionInfo, IDiaLineNumber *pLine)
{
    DWORD dwRVA;
    DWORD dwLinenum;
    DWORD dwLength;

    bool ok = true;

    ok = ok && pLine->get_relativeVirtualAddress(&dwRVA) == S_OK;
    ok = ok && pLine->get_lineNumber(&dwLinenum) == S_OK;
    ok = ok && pLine->get_length(&dwLength) == S_OK;

    if (ok) {
        functionInfo.sourceLines.emplace_back();
        PdbSourceLineInfo &lineInfo = functionInfo.sourceLines.back();
        lineInfo.lineNumber = static_cast<uint32_t>(dwLinenum);
        lineInfo.offset = static_cast<uint32_t>(dwRVA) - functionInfo.address.relVirtual;
        lineInfo.length = static_cast<uint32_t>(dwLength);
    }
}

} // namespace unassemblize
