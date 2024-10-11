/**
 * @file
 *
 * @brief main function and option handling.
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "function.h"
#include "gitinfo.h"
#include "pdbreader.h"
#include "util.h"
#include <LIEF/LIEF.hpp>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <strings.h>

void print_help()
{
    char revision[12] = {0};
    const char *version = GitTag[0] == 'v' ? GitTag : GitShortSHA1;

    if (GitTag[0] != 'v') {
        snprintf(revision, sizeof(revision), "r%d ", GitRevision);
    }

    printf(
        "\nunassemblize %s%s%s\n"
        "    x86 Unassembly tool\n\n"
        "Usage:\n"
        "  unassemblize [OPTIONS] [INPUT]\n"
        "Options:\n"
        "  -o --output     Filename for single file output. Default is 'auto'\n"
        "  -f --format     Assembly output format.\n"
        "  -c --config     Configuration file describing how to disassemble the input. Default is 'auto'\n"
        "                  file and containing extra symbol info. Default: config.json\n"
        "  -s --start      Starting address of a single function to disassemble in\n"
        "                  hexadecimal notation.\n"
        "  -e --end        Ending address of a single function to disassemble in\n"
        "                  hexadecimal notation.\n"
        "  -v --verbose    Verbose output on current state of the program.\n"
        "  --section       Section to target for dissassembly, defaults to '.text'.\n"
        "  --listsections  Prints a list of sections in the exe then exits.\n"
        "  -d --dumpsyms   Dumps symbols stored in a executable or pdb to the config file.\n"
        "                  then exits.\n"
        "  -h --help       Displays this help.\n\n",
        revision,
        GitUncommittedChanges ? "~" : "",
        version);
}

void print_sections(unassemblize::Executable &exe)
{
    const unassemblize::Executable::SectionMap &map = exe.get_section_map();
    for (auto it = map.begin(); it != map.end(); ++it) {
        printf(
            "Name: %s, Address: 0x%" PRIx64 " Size: %" PRIu64 "\n", it->first.c_str(), it->second.address, it->second.size);
    }
}

void dump_function_to_file(
    const std::string &file_name, unassemblize::Executable &exe, const char *section_name, uint64_t start, uint64_t end)
{
    if (!file_name.empty()) {
        FILE *fp = fopen(file_name.c_str(), "w+");
        if (fp != nullptr) {
            fprintf(fp, ".intel_syntax noprefix\n\n");
            exe.dissassemble_function(fp, section_name, start, end);
            fclose(fp);
        }
    } else {
        exe.dissassemble_function(nullptr, section_name, start, end);
    }
}

const char *const auto_str = "auto"; // When output is set to "auto", then output name is chosen for input file name.

enum class InputType
{
    Exe,
    Pdb,
};

struct ExeOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    std::string output_file;
    std::string section_name = ".text"; // unused
    std::string format_str;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

bool process_exe(const ExeOptions &o)
{
    if (o.verbose) {
        printf("Parsing exe file '%s'...\n", o.input_file.c_str());
    }

    // TODO implement default value where exe object decides internally what to do.
    unassemblize::Executable::OutputFormats format = unassemblize::Executable::OUTPUT_IGAS;

    if (!o.format_str.empty()) {
        if (0 == strcasecmp(o.format_str.c_str(), "igas")) {
            format = unassemblize::Executable::OUTPUT_IGAS;
        } else if (0 == strcasecmp(o.format_str.c_str(), "masm")) {
            format = unassemblize::Executable::OUTPUT_MASM;
        }
    }

    unassemblize::Executable exe(o.input_file.c_str(), format, o.verbose);

    if (o.print_secs) {
        print_sections(exe);
        return true;
    }

    if (o.dump_syms) {
        exe.save_config(o.config_file.c_str());
        return true;
    }

    exe.load_config(o.config_file.c_str());

    if (o.start_addr == 0 && o.end_addr == 0) {
        for (const unassemblize::Executable::SymbolMap::value_type &pair : exe.get_symbol_map()) {
            const uint64_t address = pair.first;
            const unassemblize::Executable::Symbol &symbol = pair.second;
            std::string sanitized_symbol_name = symbol.name;
#if defined(WIN32)
            util::remove_characters(sanitized_symbol_name, "\\/:*?\"<>|");
#endif
            std::string file_name;
            if (!o.output_file.empty()) {
                // program.symbol.S
                file_name = util::get_remove_file_ext(o.output_file) + "." + sanitized_symbol_name + ".S";
            }
            dump_function_to_file(file_name, exe, o.section_name.c_str(), symbol.address, symbol.address + symbol.size);
        }
    } else {
        dump_function_to_file(o.output_file, exe, o.section_name.c_str(), o.start_addr, o.end_addr);
    }

    return true;
}

struct PdbOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

bool process_pdb(const PdbOptions &o)
{
    unassemblize::PdbReader pdb_reader(o.verbose);

    // Currently does not read back config file here.

    if (!pdb_reader.read(o.input_file)) {
        return false;
    }

    if (o.dump_syms) {
        pdb_reader.save_config(o.config_file);
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        print_help();
        return -1;
    }

    // When input_file_type is set to "auto", then input file type is chosen by file extension.
    const char *input_type = auto_str;
    // When output_file is set to "auto", then output file name is chosen for input file name.
    const char *output_file = auto_str;
    // When config file is set to "auto", then config file name is chosen for input file name.
    const char *config_file = auto_str;
    const char *section_name = ".text"; // unused
    const char *format_string = auto_str;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;

    while (true) {
        static struct option long_options[] = {
            {"input-type", required_argument, nullptr, 3},
            {"output", required_argument, nullptr, 'o'},
            {"format", required_argument, nullptr, 'f'},
            {"start", required_argument, nullptr, 's'},
            {"end", required_argument, nullptr, 'e'},
            {"config", required_argument, nullptr, 'c'},
            {"section", required_argument, nullptr, 1},
            {"listsections", no_argument, nullptr, 2},
            {"dumpsyms", no_argument, nullptr, 'd'},
            {"verbose", no_argument, nullptr, 'v'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, no_argument, nullptr, 0},
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, "+dhv?o:f:s:e:c:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 1:
                section_name = optarg;
                break;
            case 2:
                print_secs = true;
                break;
            case 3:
                input_type = optarg;
                break;
            case 'd':
                dump_syms = true;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'f':
                format_string = optarg;
                break;
            case 's':
                start_addr = strtoull(optarg, nullptr, 16);
                break;
            case 'e':
                end_addr = strtoull(optarg, nullptr, 16);
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                printf("\nOption %d not recognized.\n", optopt);
                print_help();
                return 0;
            case ':':
                printf("\nAn option is missing arguments.\n");
                print_help();
                return 0;
            case 'h':
                print_help();
                break;
            default:
                break;
        }
    }

    const char *input_file = argv[optind];

    if (input_file == nullptr || input_file[0] == '\0') {
        printf("Missing input file command line argument. Exiting...\n");
        return 1;
    }

    std::string input_file_str(input_file);
    std::string input_file_ext = util::get_file_ext(input_file_str);
    std::string config_file_str(config_file);
    std::string output_file_str(output_file);

    if (config_file_str == auto_str) {
        // program.config.json
        config_file_str = util::get_remove_file_ext(input_file_str) + ".config.json";
    }

    if (output_file_str == auto_str) {
        // program.S
        output_file_str = util::get_remove_file_ext(input_file_str) + ".S";
    }

    InputType type = InputType::Exe;

    if (0 == strcmp(input_type, auto_str)) {
        if (0 == strcasecmp(input_file_ext.c_str(), "pdb")) {
            type = InputType::Pdb;
        } else {
            type = InputType::Exe;
        }
    } else if (0 == strcasecmp(input_type, "exe")) {
        type = InputType::Exe;
    } else if (0 == strcasecmp(input_type, "pdb")) {
        type = InputType::Pdb;
    } else {
        printf("Unrecognized input file type '%s'. Exiting...\n", input_type);
        return 1;
    }

    if (InputType::Exe == type) {
        ExeOptions o;
        o.input_file = input_file_str;
        o.config_file = config_file_str;
        o.output_file = output_file_str;
        o.section_name = section_name;
        o.format_str = format_string;
        o.start_addr = start_addr;
        o.end_addr = end_addr;
        o.print_secs = print_secs;
        o.dump_syms = dump_syms;
        o.verbose = verbose;
        return process_exe(o) ? 0 : 1;
    } else if (InputType::Pdb == type) {
        PdbOptions o;
        o.input_file = input_file_str;
        o.config_file = config_file_str;
        o.print_secs = print_secs;
        o.dump_syms = dump_syms;
        o.verbose = verbose;
        return process_pdb(o) ? 0 : 1;
    } else {
        // Impossible
        return 1;
    }
}
