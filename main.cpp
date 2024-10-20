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
#include "executable.h"
#include "function.h"
#include "gitinfo.h"
#include "pdbreader.h"
#include "util.h"
#include <assert.h>
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

const char *const auto_str = "auto"; // When output is set to "auto", then output name is chosen for input file name.

enum class InputType
{
    Unknown,
    Exe,
    Pdb,
};

struct ExeOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    std::string output_file;
    std::string format_str;
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

struct PdbOptions
{
    std::string input_file;
    std::string config_file = "config.json";
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;
};

class Runner
{
public:
    void print_sections(unassemblize::Executable &exe)
    {
        const unassemblize::ExeSectionMap &map = exe.get_section_map();
        for (auto it = map.begin(); it != map.end(); ++it) {
            printf("Name: %s, Address: 0x%" PRIx64 " Size: %" PRIu64 "\n",
                it->first.c_str(),
                it->second.address,
                it->second.size);
        }
    }

    void dump_function_to_file(const std::string &file_name, unassemblize::Executable &exe, uint64_t start, uint64_t end)
    {
        if (!file_name.empty()) {
            FILE *fp = fopen(file_name.c_str(), "w+");
            if (fp != nullptr) {
                exe.dissassemble_function(fp, start, end);
                fclose(fp);
            }
        } else {
            exe.dissassemble_function(nullptr, start, end);
        }
    }

    bool process_exe(const ExeOptions &o)
    {
        if (o.verbose) {
            printf("Parsing exe file '%s'...\n", o.input_file.c_str());
        }

        // TODO implement default value where exe object decides internally what to do.
        unassemblize::Executable::OutputFormat format = unassemblize::Executable::OUTPUT_IGAS;

        if (!o.format_str.empty()) {
            if (0 == strcasecmp(o.format_str.c_str(), "igas")) {
                format = unassemblize::Executable::OUTPUT_IGAS;
            } else if (0 == strcasecmp(o.format_str.c_str(), "masm")) {
                format = unassemblize::Executable::OUTPUT_MASM;
            }
        }

        m_executable.set_output_format(format);
        m_executable.set_verbose(o.verbose);

        if (!m_executable.read(o.input_file)) {
            return false;
        }

        if (o.print_secs) {
            print_sections(m_executable);
            return true;
        }

        constexpr bool pdb_symbols_overwrite_exe_symbols = true;
        constexpr bool cfg_symbols_overwrite_exe_pdb_symbols = true;

        const unassemblize::PdbSymbolInfoVector &pdb_symbols = m_pdbReader.get_symbols();

        if (!pdb_symbols.empty()) {
            m_executable.add_symbols(pdb_symbols, pdb_symbols_overwrite_exe_symbols);
        }

        if (o.dump_syms) {
            m_executable.save_config(o.config_file.c_str());
            return true;
        }

        m_executable.load_config(o.config_file.c_str(), cfg_symbols_overwrite_exe_pdb_symbols);

        if (o.start_addr == 0 && o.end_addr == 0) {
            for (const unassemblize::ExeSymbol &symbol : m_executable.get_symbols()) {
                std::string sanitized_symbol_name = symbol.name;
#if defined(WIN32)
                util::remove_characters_inplace(sanitized_symbol_name, "\\/:*?\"<>|");
#endif
                std::string file_name;
                if (!o.output_file.empty()) {
                    // program.symbol.S
                    file_name = util::get_file_name_without_ext(o.output_file) + "." + sanitized_symbol_name + ".S";
                }
                dump_function_to_file(file_name, m_executable, symbol.address, symbol.address + symbol.size);
            }
        } else {
            dump_function_to_file(o.output_file, m_executable, o.start_addr, o.end_addr);
        }

        return true;
    }

    bool process_pdb(const PdbOptions &o)
    {
        m_pdbReader.set_verbose(o.verbose);

        // Currently does not read back config file here.

        if (!m_pdbReader.read(o.input_file)) {
            return false;
        }

        if (o.dump_syms) {
            m_pdbReader.save_config(o.config_file);
        }

        return true;
    }

    std::string get_pdb_exe_file_name()
    {
        unassemblize::PdbExeInfo exe_info = m_pdbReader.get_exe_info();
        assert(!exe_info.exeFileName.empty());
        assert(!exe_info.pdbFilePath.empty());

        return util::get_file_path(exe_info.pdbFilePath) + "/" + exe_info.exeFileName + ".exe";
    }

private:
    unassemblize::Executable m_executable;
    unassemblize::PdbReader m_pdbReader;
};

std::string get_config_file_name(const std::string &input_file, const std::string &config_file)
{
    if (0 == strcasecmp(config_file.c_str(), auto_str)) {
        // program.config.json
        return util::get_file_name_without_ext(input_file) + ".config.json";
    }
    return config_file;
}

std::string get_output_file_name(const std::string &input_file, const std::string &output_file)
{
    if (0 == strcasecmp(output_file.c_str(), auto_str)) {
        // program.S
        return util::get_file_name_without_ext(input_file) + ".S";
    }
    return output_file;
}

InputType get_input_type(const std::string &input_file, const std::string &input_type)
{
    InputType type = InputType::Unknown;

    if (0 == strcasecmp(input_type.c_str(), auto_str)) {
        std::string input_file_ext = util::get_file_ext(input_file);
        if (0 == strcasecmp(input_file_ext.c_str(), "pdb")) {
            type = InputType::Pdb;
        } else {
            type = InputType::Exe;
        }
    } else if (0 == strcasecmp(input_type.c_str(), "exe")) {
        type = InputType::Exe;
    } else if (0 == strcasecmp(input_type.c_str(), "pdb")) {
        type = InputType::Pdb;
    }

    return type;
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
                // unused
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

    const InputType type = get_input_type(input_file, input_type);

    if (InputType::Exe == type) {
        Runner runner;
        ExeOptions o;
        o.input_file = input_file;
        o.config_file = get_config_file_name(o.input_file, config_file);
        o.output_file = get_output_file_name(o.input_file, output_file);
        o.format_str = format_string;
        o.start_addr = start_addr;
        o.end_addr = end_addr;
        o.print_secs = print_secs;
        o.dump_syms = dump_syms;
        o.verbose = verbose;
        return runner.process_exe(o) ? 0 : 1;
    } else if (InputType::Pdb == type) {
        Runner runner;
        bool success;
        {
            PdbOptions o;
            o.input_file = input_file;
            o.config_file = get_config_file_name(o.input_file, config_file);
            o.print_secs = print_secs;
            o.dump_syms = dump_syms;
            o.verbose = verbose;
            success = runner.process_pdb(o);
        }
        if (success) {
            ExeOptions o;
            o.input_file = runner.get_pdb_exe_file_name();
            o.config_file = get_config_file_name(o.input_file, config_file);
            o.output_file = get_output_file_name(o.input_file, output_file);
            o.format_str = format_string;
            o.start_addr = start_addr;
            o.end_addr = end_addr;
            o.print_secs = print_secs;
            o.dump_syms = dump_syms;
            o.verbose = verbose;
            success = runner.process_exe(o);
        }
        return success ? 0 : 1;
    } else {
        printf("Unrecognized input file type '%s'. Exiting...\n", input_type);
        return 1;
    }
}
