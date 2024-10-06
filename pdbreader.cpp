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
#include <dia2.h>

namespace unassemblize
{
PdbReader::PdbReader(const std::string &pdb_file, bool verbose) :
    m_filename(pdb_file), m_verbose(verbose), m_dwMachineType(CV_CFL_80386)
{
}

bool PdbReader::read()
{
    if (load()) {
        unload();
        return true;
    }

    return false;
}

void PdbReader::dump_symbols(nlohmann::json &js) {}

void PdbReader::save_config(const std::string &config_file) {}

bool PdbReader::load()
{
    HRESULT hr = CoInitialize(NULL);

    if (FAILED(hr)) {
        wprintf(L"CoInitialize failed - HRESULT = %08X\n", hr);
        return false;
    }

    // Obtain access to the provider

    hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)&m_pDiaSource);

    if (FAILED(hr)) {
        wprintf(L"CoCreateInstance failed - HRESULT = %08X\n", hr);
        return false;
    }

    // Open and prepare a program database (.pdb) file as a debug data source

    std::wstring wfilename(m_filename.begin(), m_filename.end());

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
    if (m_pDiaSession != nullptr) {
        m_pDiaSession->Release();
        m_pDiaSession = nullptr;
    }

    if (m_pDiaSymbol != nullptr) {
        m_pDiaSymbol->Release();
        m_pDiaSymbol = nullptr;
    }

    CoUninitialize();
}

} // namespace unassemblize