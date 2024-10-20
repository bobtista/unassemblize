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
#include "gitinfo.h"
#include "runner.h"
#include "util.h"
#include <assert.h>
#include <getopt.h>
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
        unassemblize::Runner runner;
        unassemblize::ExeOptions o;
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
        unassemblize::Runner runner;
        bool success;
        {
            unassemblize::PdbOptions o;
            o.input_file = input_file;
            o.config_file = get_config_file_name(o.input_file, config_file);
            o.print_secs = print_secs;
            o.dump_syms = dump_syms;
            o.verbose = verbose;
            success = runner.process_pdb(o);
        }
        if (success) {
            unassemblize::ExeOptions o;
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
