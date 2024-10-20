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
#include "executable.h"
#include "function.h"
#include <LIEF/LIEF.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <strings.h>

namespace unassemblize
{
const char *const s_symbolSection = "symbols";
const char *const s_sectionsSection = "sections";
const char *const s_configSection = "config";
const char *const s_objectSection = "objects";

ExeSymbol Executable::s_emptySymbol;

Executable::Executable() {}

Executable::~Executable() {}

bool Executable::read(const std::string &exe_file)
{
    if (m_verbose) {
        printf("Loading section info...\n");
    }

    m_binary = LIEF::Parser::parse(exe_file);

    if (m_binary.get() == nullptr) {
        return false;
    }

    m_imageData.imageBase = m_binary->imagebase();

    bool checked_image_base = false;

    for (auto it = m_binary->sections().begin(); it != m_binary->sections().end(); ++it) {
        if (!it->name().empty() && it->size() != 0) {
            ExeSectionInfo &section = m_sectionMap[it->name()];
            section.data = it->content().data();

            // For PE format virtual_address appears to be an offset, in ELF/Mach-O it appears to be absolute.
            // TODO: Check if ELF/Mach-O works correctly with this code - if necessary.
            section.address = it->virtual_address();
            section.size = it->size();

            m_imageData.sectionsBegin = std::min(m_imageData.sectionsBegin, section.address);
            m_imageData.sectionsEnd = std::max(m_imageData.sectionsEnd, section.address + section.size);

            // Naive split on whether section contains data or code... have entrypoint? Code, else data.
            // Needs to be refined by providing a config file with section types specified.
            const uint64_t entrypoint = m_binary->entrypoint() - m_binary->imagebase();
            if (section.address < entrypoint && section.address + section.size >= entrypoint) {
                section.type = SECTION_CODE;
            } else {
                section.type = SECTION_DATA;
            }
        }
    }

    if (m_verbose) {
        printf("Indexing embedded symbols...\n");
    }

    {
        auto exe_syms = m_binary->symbols();

        const size_t newSize = m_symbols.size() + exe_syms.size();
        m_symbols.reserve(newSize);
        m_symbolAddressToIndexMap.reserve(newSize);

        for (auto it = exe_syms.begin(); it != exe_syms.end(); ++it) {
            ExeSymbol symbol;
            symbol.name = it->name();
            symbol.address = it->value();
            symbol.size = it->size();

            add_symbol(symbol);
        }
    }

    {
        auto exe_imports = m_binary->imported_functions();

        const size_t newSize = m_symbols.size() + exe_imports.size();
        m_symbols.reserve(newSize);
        m_symbolAddressToIndexMap.reserve(newSize);

        for (auto it = exe_imports.begin(); it != exe_imports.end(); ++it) {
            ExeSymbol symbol;
            symbol.name = it->name();
            symbol.address = it->value();
            symbol.size = it->size();

            add_symbol(symbol);
        }
    }

    if (m_targetObjects.empty()) {
        m_targetObjects.push_back(
            {m_binary->name().substr(m_binary->name().find_last_of("/\\") + 1), std::list<ExeObjectSection>()});
        ExeObject &obj = m_targetObjects.back();

        for (auto it = m_binary->sections().begin(); it != m_binary->sections().end(); ++it) {
            if (it->name().empty() || it->size() == 0) {
                continue;
            }

            obj.sections.push_back({it->name(), it->offset(), it->size()});
        }
    }

    return true;
}

const ExeSectionMap &Executable::get_section_map() const
{
    return m_sectionMap;
}

const ExeSectionInfo *Executable::find_section(uint64_t address) const
{
    for (const ExeSectionMap::value_type &pair : m_sectionMap) {
        const ExeSectionInfo &info = pair.second;
        if (address >= info.address && address < info.address + info.size) {
            return &info;
        }
    }
    return nullptr;
}

const uint8_t *Executable::section_data(const char *name) const
{
    ExeSectionMap::const_iterator it = m_sectionMap.find(name);
    return it != m_sectionMap.end() ? it->second.data : nullptr;
}

uint64_t Executable::section_address(const char *name) const
{
    ExeSectionMap::const_iterator it = m_sectionMap.find(name);
    return it != m_sectionMap.end() ? it->second.address : UINT64_MAX;
}

uint64_t Executable::section_size(const char *name) const
{
    ExeSectionMap::const_iterator it = m_sectionMap.find(name);
    return it != m_sectionMap.end() ? it->second.size : 0;
}

uint64_t Executable::image_base() const
{
    return m_imageData.imageBase;
}

uint64_t Executable::text_section_begin_from_image_base() const
{
    return section_address(".text") + m_imageData.imageBase; // Slow
}

uint64_t Executable::text_section_end_from_image_base() const
{
    return section_address(".text") + m_imageData.imageBase; // Slow
}

uint64_t Executable::all_sections_begin_from_image_base() const
{
    return m_imageData.sectionsBegin + m_imageData.imageBase;
}

uint64_t Executable::all_sections_end_from_image_base() const
{
    return m_imageData.sectionsEnd + m_imageData.imageBase;
}

const ExeSymbol &Executable::get_symbol(uint64_t address) const
{
    Address64ToIndexMap::const_iterator it = m_symbolAddressToIndexMap.find(address);

    if (it != m_symbolAddressToIndexMap.end()) {
        return m_symbols[it->second];
    }

    return s_emptySymbol;
}

const ExeSymbol &Executable::get_symbol_from_image_base(uint64_t address) const
{
    return get_symbol(address - image_base());
}

const ExeSymbol &Executable::get_nearest_symbol(uint64_t address) const
{
    Address64ToIndexMap::const_iterator it = m_symbolAddressToIndexMap.lower_bound(address);

    if (it != m_symbolAddressToIndexMap.end()) {
        const ExeSymbol &symbol = m_symbols[it->second];
        if (symbol.address == address) {
            return symbol;
        } else {
            const ExeSymbol &prevSymbol = m_symbols[std::prev(it)->second];
            return prevSymbol;
        }
    }

    return s_emptySymbol;
}

const ExeSymbols &Executable::get_symbols() const
{
    return m_symbols;
}

void Executable::add_symbols(const ExeSymbols &symbols)
{
    const size_t size = m_symbols.size() + symbols.size();
    m_symbols.reserve(size);
    m_symbolAddressToIndexMap.reserve(size);

    for (const ExeSymbol &symbol : symbols) {
        add_symbol(symbol);
    }
}

void Executable::add_symbol(const ExeSymbol &symbol)
{
    Address64ToIndexMap::iterator it = m_symbolAddressToIndexMap.find(symbol.address);

    if (it == m_symbolAddressToIndexMap.end()) {
        const uint32_t index = static_cast<uint32_t>(m_symbols.size());
        m_symbols.push_back(symbol);
        m_symbolAddressToIndexMap[symbol.address] = index;
    }
}

void Executable::load_config(const char *file_name)
{
    if (m_verbose) {
        printf("Loading config file '%s'...\n", file_name);
    }

    std::ifstream fs(file_name);

    if (!fs.good()) {
        return;
    }

    nlohmann::json j = nlohmann::json::parse(fs);

    if (j.find(s_configSection) != j.end()) {
        nlohmann::json &conf = j.at(s_configSection);
        conf.at("codealign").get_to(m_imageData.codeAlignment);
        conf.at("dataalign").get_to(m_imageData.dataAlignment);
        conf.at("codepadding").get_to(m_imageData.codePad);
        conf.at("datapadding").get_to(m_imageData.dataPad);
    }

    if (j.find(s_symbolSection) != j.end()) {
        load_symbols(j.at(s_symbolSection));
    }

    if (j.find(s_sectionsSection) != j.end()) {
        load_sections(j.at(s_sectionsSection));
    }

    if (j.find(s_objectSection) != j.end()) {
        load_objects(j.at(s_objectSection));
    }
}

void Executable::save_config(const char *file_name)
{
    if (m_verbose) {
        printf("Saving config file '%s'...\n", file_name);
    }

    nlohmann::json j;

    // Parse the config file if it already exists and update it.
    {
        std::ifstream fs(file_name);

        if (fs.good()) {
            j = nlohmann::json::parse(fs);
        }
    }

    if (j.find(s_configSection) == j.end()) {
        j[s_configSection] = nlohmann::json();
    }

    nlohmann::json &conf = j.at(s_configSection);
    conf["codealign"] = m_imageData.codeAlignment;
    conf["dataalign"] = m_imageData.dataAlignment;
    conf["codepadding"] = m_imageData.codePad;
    conf["datapadding"] = m_imageData.dataPad;

    // Don't dump if we already have a sections for these.
    if (j.find(s_symbolSection) == j.end()) {
        j[s_symbolSection] = nlohmann::json();
        dump_symbols(j.at(s_symbolSection));
    }

    if (j.find(s_sectionsSection) == j.end()) {
        j[s_sectionsSection] = nlohmann::json();
        dump_sections(j.at(s_sectionsSection));
    }

    if (j.find(s_objectSection) == j.end()) {
        j[s_objectSection] = nlohmann::json();
        dump_objects(j.at(s_objectSection));
    }

    std::ofstream fs(file_name);
    fs << std::setw(4) << j << std::endl;
}

void Executable::load_symbols(nlohmann::json &js)
{
    if (m_verbose) {
        printf("Loading external symbols...\n");
    }

    size_t newSize = m_symbols.size() + js.size();
    m_symbols.reserve(newSize);
    m_symbolAddressToIndexMap.reserve(newSize);

    for (auto it = js.begin(); it != js.end(); ++it) {
        ExeSymbol symbol;

        it->at("name").get_to(symbol.name);
        if (symbol.name.empty()) {
            continue;
        }

        it->at("address").get_to(symbol.address);
        if (symbol.address == 0) {
            continue;
        }

        it->at("size").get_to(symbol.size);

        add_symbol(symbol);
    }
}

void Executable::dump_symbols(nlohmann::json &js) const
{
    if (m_verbose) {
        printf("Saving symbols...\n");
    }

    for (const ExeSymbol &symbol : m_symbols) {
        js.push_back({{"name", symbol.name}, {"address", symbol.address}, {"size", symbol.size}});
    }
}

void Executable::load_sections(nlohmann::json &js)
{
    if (m_verbose) {
        printf("Loading section info...\n");
    }

    for (auto it = js.begin(); it != js.end(); ++it) {
        std::string name;
        it->at("name").get_to(name);

        // Don't try and load an empty section.
        if (!name.empty()) {
            ExeSectionMap::iterator section = m_sectionMap.find(name);

            if (section == m_sectionMap.end() && m_verbose) {
                printf("Tried to load section info for section not present in this binary!\n");
                printf("Section '%s' info was ignored.\n", name.c_str());
            }

            std::string type;
            it->at("type").get_to(type);

            if (strcasecmp(type.c_str(), "code") == 0) {
                section->second.type = SECTION_CODE;
            } else if (strcasecmp(type.c_str(), "data") == 0) {
                section->second.type = SECTION_DATA;
            } else if (m_verbose) {
                printf("Incorrect type specified for section '%s'.\n", name.c_str());
            }

            auto it_address = it->find("address");
            if (it_address != it->end()) {
                it_address->get_to(section->second.address);
            }
            auto it_size = it->find("size");
            if (it_size != it->end()) {
                it_size->get_to(section->second.size);
            }
        }
    }
}

void Executable::dump_sections(nlohmann::json &js) const
{
    if (m_verbose) {
        printf("Saving section info...\n");
    }

    for (ExeSectionMap::const_iterator it = m_sectionMap.begin(); it != m_sectionMap.end(); ++it) {
        const char *type_str = it->second.type == SECTION_CODE ? "code" : "data";
        js.push_back({{"name", it->first}, {"type", type_str}, {"address", it->second.address}, {"size", it->second.size}});
    }
}

void Executable::load_objects(nlohmann::json &js)
{
    if (m_verbose) {
        printf("Loading objects...\n");
    }

    for (auto it = js.begin(); it != js.end(); ++it) {
        std::string obj_name;
        it->at("name").get_to(obj_name);

        if (obj_name.empty()) {
            continue;
        }

        {
            // Skip if entry already exists.
            ExeObjects::const_iterator it_object = std::find_if(m_targetObjects.begin(),
                m_targetObjects.end(),
                [&](const ExeObject &object) { return object.name == obj_name; });
            if (it_object != m_targetObjects.end()) {
                continue;
            }
        }

        m_targetObjects.push_back({obj_name, std::list<ExeObjectSection>()});
        ExeObject &obj = m_targetObjects.back();
        auto &sections = js.back().at("sections");

        for (auto sec = sections.begin(); sec != sections.end(); ++sec) {
            obj.sections.emplace_back();
            ExeObjectSection &section = obj.sections.back();
            sec->at("name").get_to(section.name);
            sec->at("offset").get_to(section.offset);
            sec->at("size").get_to(section.size);
        }
    }
}

void Executable::dump_objects(nlohmann::json &js) const
{
    if (m_verbose) {
        printf("Saving objects...\n");
    }

    for (ExeObjects::const_iterator it = m_targetObjects.begin(); it != m_targetObjects.end(); ++it) {
        js.push_back({{"name", it->name}, {"sections", nlohmann::json()}});
        auto &sections = js.back().at("sections");

        for (auto it2 = it->sections.begin(); it2 != it->sections.end(); ++it2) {
            sections.push_back({{"name", it2->name}, {"offset", it2->offset}, {"size", it2->size}});
        }
    }
}

void Executable::dissassemble_function(FILE *fp, uint64_t start, uint64_t end)
{
    if (m_outputFormat != OUTPUT_MASM) {
        dissassemble_gas_func(fp, start, end);
    }
}

void Executable::dissassemble_gas_func(FILE *fp, uint64_t start, uint64_t end)
{
    if (start != 0 && end != 0) {
        const AsmFormat format = m_outputFormat == OUTPUT_IGAS ? AsmFormat::IGAS : AsmFormat::AGAS;
        const FunctionSetup setup(*this, format); // TODO: Optimize by creating just once

        std::string str;
        {
            Function func;
            func.disassemble(setup, start, end);
            const InstructionDataVector &instructions = func.get_instructions();
            str.reserve(instructions.size() * 32);
            append_as_text(str, instructions);
        }

        const std::string &sym = get_symbol(start).name;

        if (fp != nullptr) {
            fprintf(fp, ".intel_syntax noprefix\n\n");
            if (!sym.empty()) {
                fprintf(fp, ".globl %s\n%s", sym.c_str(), str.c_str());
            } else {
                fprintf(fp, ".globl sub_%" PRIx64 "\n%s", start, str.c_str());
            }
        }
    }
}

} // namespace unassemblize
