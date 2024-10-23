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
#include <cxxopts.hpp>
#include <iostream>
#include <stdio.h>
#include <strings.h>

void print_version()
{
    char revision[12] = {0};
    const char *version = GitTag[0] == 'v' ? GitTag : GitShortSHA1;

    if (GitTag[0] != 'v') {
        snprintf(revision, sizeof(revision), "r%d ", GitRevision);
    }

    printf("unassemblize %s%s%s by The Assembly Armada\n", revision, GitUncommittedChanges ? "~" : "", version);
}

const char *const auto_str = "auto"; // When output is set to "auto", then output name is chosen for input file name.

enum class InputType
{
    None,
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
    InputType type = InputType::None;

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
    print_version();

    cxxopts::Options options("unassemblize", "x86 Unassembly tool");

#define OPT_INPUT "input"
#define OPT_INPUT_2 "input2"
#define OPT_INPUTTYPE "input-type"
#define OPT_INPUTTYPE_2 "input2-type"
#define OPT_OUTPUT "output"
#define OPT_FORMAT "format"
#define OPT_BUNDLE_FILE_ID "bundle-file-id"
#define OPT_BUNDLE_TYPE "bundle-type"
#define OPT_CONFIG "config"
#define OPT_CONFIG_2 "config2"
#define OPT_START "start"
#define OPT_END "end"
#define OPT_LISTSECTIONS "list-sections"
#define OPT_DUMPSYMS "dumpsyms"
#define OPT_VERBOSE "verbose"
#define OPT_HELP "help"

    // clang-format off
    options.add_options("main", {
        cxxopts::Option{"i," OPT_INPUT, "Input file", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_INPUT_2, "Input file 2", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_INPUTTYPE, "Input file type. Default is 'auto'", cxxopts::value<std::string>(), R"(["auto", "exe", "pdb"])"},
        cxxopts::Option{OPT_INPUTTYPE_2, "Input file 2 type. Default is 'auto'", cxxopts::value<std::string>(), R"(["auto", "exe", "pdb"])"},
        cxxopts::Option{"o," OPT_OUTPUT, "Filename for single file output. Default is 'auto'", cxxopts::value<std::string>()},
        cxxopts::Option{"f," OPT_FORMAT, "Assembly output format. Default is 'igas'", cxxopts::value<std::string>(), R"(["igas", "agas", "masm", "default"])"},
        cxxopts::Option{OPT_BUNDLE_FILE_ID, "Input file used to bundle match results with. Default is 1.", cxxopts::value<size_t>(), "[1: 1st input file, 2: 2nd input file]"},
        cxxopts::Option{OPT_BUNDLE_TYPE, "Method used to bundle match results with. Default is 'sourcefile'.", cxxopts::value<std::string>(), R"(["none", "sourcefile", "compiland"])"},
        cxxopts::Option{"c," OPT_CONFIG, "Configuration file describing how to disassemble the input file and containing extra symbol info. Default is 'auto'", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_CONFIG_2, "Configuration file describing how to disassemble the input file and containing extra symbol info. Default is 'auto'", cxxopts::value<std::string>()},
        cxxopts::Option{"s," OPT_START, "Starting address of a single function to disassemble in hexadecimal notation.", cxxopts::value<std::string>()},
        cxxopts::Option{"e," OPT_END, "Ending address of a single function to disassemble in hexadecimal notation.", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_LISTSECTIONS, "Prints a list of sections in the executable then exits."},
        cxxopts::Option{"d," OPT_DUMPSYMS, "Dumps symbols stored in a executable or pdb to the config file."},
        cxxopts::Option{"v," OPT_VERBOSE, "Verbose output on current state of the program."},
        cxxopts::Option{"h," OPT_HELP, "Displays this help."},
        });
    // clang-format on
    static_assert(size_t(unassemblize::AsmFormat::DEFAULT) == 3, "Enum was changed. Update command line options.");
    static_assert(size_t(unassemblize::MatchBundleType::None) == 2, "Enum was changed. Update command line options.");

    options.parse_positional({"input", "input2"});

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return 1;
    }

    if (result.count("help") != 0) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    constexpr size_t MAX_INPUT_FILES = unassemblize::MAX_INPUT_FILES;

    std::string input_file[MAX_INPUT_FILES];
    // When input_file_type is set to "auto", then input file type is chosen by file extension.
    std::string input_type[MAX_INPUT_FILES];
    std::fill_n(input_type, MAX_INPUT_FILES, auto_str);
    // When output_file is set to "auto", then output file name is chosen for input file name.
    std::string output_file = auto_str;
    unassemblize::AsmFormat format = unassemblize::AsmFormat::IGAS;
    size_t bundle_file_idx = 0;
    unassemblize::MatchBundleType bundle_type = unassemblize::MatchBundleType::SourceFile;
    // When config file is set to "auto", then config file name is chosen for input file name.
    std::string config_file[MAX_INPUT_FILES];
    std::fill_n(config_file, MAX_INPUT_FILES, auto_str);
    uint64_t start_addr = 0x00000000;
    uint64_t end_addr = 0x00000000;
    bool print_secs = false;
    bool dump_syms = false;
    bool verbose = false;

    for (const cxxopts::KeyValue &kv : result.arguments()) {
        const std::string &v = kv.key();
        if (v == OPT_INPUT) {
            input_file[0] = kv.value();
        } else if (v == OPT_INPUT_2) {
            input_file[1] = kv.value();
        } else if (v == OPT_INPUTTYPE) {
            input_type[0] = kv.value();
        } else if (v == OPT_INPUTTYPE_2) {
            input_type[1] = kv.value();
        } else if (v == OPT_OUTPUT) {
            output_file = kv.value();
        } else if (v == OPT_FORMAT) {
            format = unassemblize::to_asm_format(kv.value().c_str());
        } else if (v == OPT_BUNDLE_FILE_ID) {
            bundle_file_idx = kv.as<size_t>() - 1;
        } else if (v == OPT_BUNDLE_TYPE) {
            bundle_type = unassemblize::to_match_bundle_type(kv.value().c_str());
        } else if (v == OPT_CONFIG) {
            config_file[0] = kv.value();
        } else if (v == OPT_CONFIG_2) {
            config_file[1] = kv.value();
        } else if (v == OPT_START) {
            start_addr = strtoull(kv.value().c_str(), nullptr, 16);
        } else if (v == OPT_END) {
            end_addr = strtoull(kv.value().c_str(), nullptr, 16);
        } else if (v == OPT_LISTSECTIONS) {
            print_secs = kv.as<bool>();
        } else if (v == OPT_DUMPSYMS) {
            dump_syms = kv.as<bool>();
        } else if (v == OPT_VERBOSE) {
            verbose = kv.as<bool>();
        }
    }

    if (input_file[0].empty()) {
        printf("Missing input file command line argument. Exiting...\n");
        return 1;
    }

    bool ok = true;
    unassemblize::Runner runner;

    for (size_t file_idx = 0; file_idx < MAX_INPUT_FILES && ok; ++file_idx) {
        const InputType type = get_input_type(input_file[file_idx], input_type[file_idx]);

        if (InputType::Exe == type) {
            unassemblize::ExeSaveLoadOptions o;
            o.input_file = input_file[file_idx];
            o.config_file = get_config_file_name(o.input_file, config_file[file_idx]);
            o.print_secs = print_secs;
            o.dump_syms = dump_syms;
            o.verbose = verbose;
            ok &= runner.process_exe(o, file_idx);
        } else if (InputType::Pdb == type) {
            {
                unassemblize::PdbSaveLoadOptions o;
                o.input_file = input_file[file_idx];
                o.config_file = get_config_file_name(o.input_file, config_file[file_idx]);
                o.dump_syms = dump_syms;
                o.verbose = verbose;
                ok &= runner.process_pdb(o, file_idx);
            }
            if (ok) {
                unassemblize::ExeSaveLoadOptions o;
                o.input_file = runner.get_exe_file_name_from_pdb(file_idx);
                o.config_file = get_config_file_name(o.input_file, config_file[file_idx]);
                o.print_secs = print_secs;
                o.dump_syms = dump_syms;
                o.verbose = verbose;
                ok &= runner.process_exe(o);
            }
        } else if (file_idx == 0) {
            printf("Unrecognized input file type '%s'. Exiting...\n", input_type[file_idx].c_str());
            return 1;
        }
    }

    if (ok) {
        {
            unassemblize::AsmOutputOptions o;
            o.output_file = get_output_file_name(runner.get_exe_filename(), output_file);
            o.format = format;
            o.start_addr = start_addr;
            o.end_addr = end_addr;
            ok &= runner.process_asm_output(o);
        }

        if (runner.asm_compare_ready()) {
            unassemblize::AsmCompareOptions o;
            o.format = format;
            o.bundle_file_idx = bundle_file_idx;
            o.bundle_type = bundle_type;
            ok &= runner.process_asm_compare(o);
        }
    }
    return ok ? 0 : 1;
}
