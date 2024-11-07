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
#include "runner.h"
#include "version.h"
#include <assert.h>
#include <cxxopts.hpp>
#include <iostream>
#include <stdio.h>

#ifdef WIN32
#include "imguiclient/imguiwin32.h"
#endif

int main(int argc, char **argv)
{
    printf(create_version_string().c_str());

    cxxopts::Options options("unassemblize", "x86 Unassembly tool");

#define OPT_INPUT "input"
#define OPT_INPUT_2 "input2"
#define OPT_INPUTTYPE "input-type"
#define OPT_INPUTTYPE_2 "input2-type"
#define OPT_ASM_OUTPUT "output"
#define OPT_CMP_OUTPUT "cmp-output"
#define OPT_LOOKAHEAD_LIMIT "lookahead-limit"
#define OPT_MATCH_STRICTNESS "match-strictness"
#define OPT_PRINT_INDENT_LEN "print-indent-len"
#define OPT_PRINT_ASM_LEN "print-asm-len"
#define OPT_PRINT_BYTE_COUNT "print-byte-count"
#define OPT_PRINT_SOURCECODE_LEN "print-sourcecode-len"
#define OPT_PRINT_SOURCELINE_LEN "print-sourceline-len"
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
#define OPT_GUI "gui"

    // clang-format off
    options.add_options("main", {
        cxxopts::Option{"i," OPT_INPUT, "Input file", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_INPUT_2, "Input file 2", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_INPUTTYPE, "Input file type. Default is 'auto'", cxxopts::value<std::string>(), R"(["auto", "exe", "pdb"])"},
        cxxopts::Option{OPT_INPUTTYPE_2, "Input file 2 type. Default is 'auto'", cxxopts::value<std::string>(), R"(["auto", "exe", "pdb"])"},
        cxxopts::Option{"o," OPT_ASM_OUTPUT, "Filename for single file output. Default is 'auto'", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_CMP_OUTPUT, "Filename for comparison file output. Default is 'auto'", cxxopts::value<std::string>()},
        cxxopts::Option{OPT_LOOKAHEAD_LIMIT, "Max instruction count for trying to find a matching assembler line ahead. Default is 20.", cxxopts::value<uint32_t>()},
        cxxopts::Option{OPT_MATCH_STRICTNESS, "Assembler matching strictness. If 'lenient', then unknown to known/unknown symbol pairs are treated as match. If 'strict', then unknown to known/unknown symbol pairs are treated as mismatch. Default is 'undecided'.", cxxopts::value<std::string>(), R"(["lenient", "undecided", "strict"])"},
        cxxopts::Option{OPT_PRINT_INDENT_LEN, "Character count for indentation of assembler instructions in output report(s). Default is 4.", cxxopts::value<uint32_t>()},
        cxxopts::Option{OPT_PRINT_ASM_LEN, "Max character count for assembler instructions in comparison report(s). Text is truncated if longer. Default is 80.", cxxopts::value<uint32_t>()},
        cxxopts::Option{OPT_PRINT_BYTE_COUNT, "Max byte count in comparison report(s). Text is truncated if longer. Default is 11.", cxxopts::value<uint32_t>()},
        cxxopts::Option{OPT_PRINT_SOURCECODE_LEN, "Max character count for source code in comparison report(s). Text is truncated if longer. Default is 80.", cxxopts::value<uint32_t>()},
        cxxopts::Option{OPT_PRINT_SOURCELINE_LEN, "Max character count for source line in comparison report(s). Text is truncated if longer. Default is 5.", cxxopts::value<uint32_t>()},
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
        cxxopts::Option{"g," OPT_GUI, "Open with graphical user interface"},
        });
    // clang-format on
    static_assert(size_t(unassemblize::AsmFormat::DEFAULT) == 3, "Enum was changed. Update command line options.");
    static_assert(size_t(unassemblize::MatchBundleType::None) == 2, "Enum was changed. Update command line options.");

    options.parse_positional({"input", "input2"});

    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return 1;
    }

    if (result.count("help") != 0)
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    CommandLineOptions clo;

    for (const cxxopts::KeyValue &kv : result.arguments())
    {
        const std::string &v = kv.key();
        if (v == OPT_INPUT)
        {
            clo.input_file[0].set_from_command_line(kv.value());
        }
        else if (v == OPT_INPUT_2)
        {
            clo.input_file[1].set_from_command_line(kv.value());
        }
        else if (v == OPT_INPUTTYPE)
        {
            clo.input_type[0].set_from_command_line(kv.value());
        }
        else if (v == OPT_INPUTTYPE_2)
        {
            clo.input_type[1].set_from_command_line(kv.value());
        }
        else if (v == OPT_ASM_OUTPUT)
        {
            clo.output_file.set_from_command_line(kv.value());
        }
        else if (v == OPT_CMP_OUTPUT)
        {
            clo.cmp_output_file.set_from_command_line(kv.value());
        }
        else if (v == OPT_LOOKAHEAD_LIMIT)
        {
            clo.lookahead_limit.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_MATCH_STRICTNESS)
        {
            const auto type = unassemblize::to_asm_match_strictness(kv.value().c_str());
            clo.match_strictness.set_from_command_line(type);
        }
        else if (v == OPT_PRINT_INDENT_LEN)
        {
            clo.print_indent_len.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_PRINT_ASM_LEN)
        {
            clo.print_asm_len.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_PRINT_BYTE_COUNT)
        {
            clo.print_byte_count.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_PRINT_SOURCECODE_LEN)
        {
            clo.print_sourcecode_len.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_PRINT_SOURCELINE_LEN)
        {
            clo.print_sourceline_len.set_from_command_line(kv.as<uint32_t>());
        }
        else if (v == OPT_FORMAT)
        {
            const auto type = unassemblize::to_asm_format(kv.value().c_str());
            clo.format.set_from_command_line(type);
        }
        else if (v == OPT_BUNDLE_FILE_ID)
        {
            clo.bundle_file_idx.set_from_command_line(kv.as<size_t>() - 1);
        }
        else if (v == OPT_BUNDLE_TYPE)
        {
            const auto type = unassemblize::to_match_bundle_type(kv.value().c_str());
            clo.bundle_type.set_from_command_line(type);
        }
        else if (v == OPT_CONFIG)
        {
            clo.config_file[0].set_from_command_line(kv.value());
        }
        else if (v == OPT_CONFIG_2)
        {
            clo.config_file[1].set_from_command_line(kv.value());
        }
        else if (v == OPT_START)
        {
            clo.start_addr.set_from_command_line(strtoull(kv.value().c_str(), nullptr, 16));
        }
        else if (v == OPT_END)
        {
            clo.end_addr.set_from_command_line(strtoull(kv.value().c_str(), nullptr, 16));
        }
        else if (v == OPT_LISTSECTIONS)
        {
            clo.print_secs.set_from_command_line(kv.as<bool>());
        }
        else if (v == OPT_DUMPSYMS)
        {
            clo.dump_syms.set_from_command_line(kv.as<bool>());
        }
        else if (v == OPT_VERBOSE)
        {
            clo.verbose.set_from_command_line(kv.as<bool>());
        }
        else if (v == OPT_GUI)
        {
            clo.gui.set_from_command_line(kv.as<bool>());
        }
    }

    if (clo.gui)
    {
#ifdef WIN32
        unassemblize::gui::ImGuiWin32 gui;
        unassemblize::gui::ImGuiStatus error = gui.run(clo);
        return int(error);
#else
        printf("Gui not implemented.\n");
        return 1;
#endif
    }

    if (clo.input_file[0].v.empty())
    {
        printf("Missing input file command line argument. Exiting...\n");
        return 1;
    }

    std::array<std::unique_ptr<unassemblize::Executable>, 2> executable_pair;
    std::array<std::unique_ptr<unassemblize::PdbReader>, 2> pdb_reader_pair;

    bool ok = true;

    for (size_t idx = 0; idx < executable_pair.size() && ok; ++idx)
    {
        const InputType type = get_input_type(clo.input_file[idx], clo.input_type[idx]);

        if (InputType::Exe == type)
        {
            const std::string &input_file = clo.input_file[idx];
            const std::string config_file = get_config_file_name(input_file, clo.config_file[idx]);
            unassemblize::ExeSaveLoadOptions o(input_file, config_file);
            o.print_secs = clo.print_secs;
            o.dump_syms = clo.dump_syms;
            o.verbose = clo.verbose;
            executable_pair[idx] = unassemblize::Runner::process_exe(o);
            ok &= executable_pair[idx] != nullptr;
        }
        else if (InputType::Pdb == type)
        {
            {
                const std::string &input_file = clo.input_file[idx];
                const std::string config_file = get_config_file_name(input_file, clo.config_file[idx]);
                unassemblize::PdbSaveLoadOptions o(input_file, config_file);
                o.dump_syms = clo.dump_syms;
                o.verbose = clo.verbose;
                pdb_reader_pair[idx] = unassemblize::Runner::process_pdb(o);
                ok &= pdb_reader_pair[idx] != nullptr;
            }
            if (ok)
            {
                const unassemblize::PdbExeInfo &exe_info = pdb_reader_pair[idx]->get_exe_info();
                const std::string &input_file = unassemblize::Runner::create_exe_filename(exe_info);
                const std::string config_file = get_config_file_name(input_file, clo.config_file[idx]);
                unassemblize::ExeSaveLoadOptions o(input_file, config_file);
                o.pdb_reader = pdb_reader_pair[idx].get();
                o.print_secs = clo.print_secs;
                o.dump_syms = clo.dump_syms;
                o.verbose = clo.verbose;
                executable_pair[idx] = unassemblize::Runner::process_exe(o);
                ok &= executable_pair[idx] != nullptr;
            }
        }
        else if (idx == 0)
        {
            printf("Unrecognized input file type '%s'. Exiting...\n", clo.input_type[idx].v.c_str());
            return 1;
        }
    }

    if (ok)
    {
        const unassemblize::Executable *executable0 = executable_pair[0].get();
        const unassemblize::Executable *executable1 = executable_pair[1].get();

        if (executable0 != nullptr && !clo.output_file.v.empty())
        {
            const std::string output_file = get_asm_output_file_name(executable0->get_filename(), clo.output_file);
            unassemblize::AsmOutputOptions o(*executable0, output_file, clo.start_addr, clo.end_addr);
            o.format = clo.format;
            o.print_indent_len = clo.print_indent_len;
            ok &= unassemblize::Runner::process_asm_output(o);
        }

        if (executable0 != nullptr && executable1 != nullptr)
        {
            assert(executable0->is_loaded());
            assert(executable1->is_loaded());

            const std::string output_file =
                get_cmp_output_file_name(executable0->get_filename(), executable1->get_filename(), clo.output_file);

            unassemblize::ExecutablePair executable_pair2 = {executable0, executable1};
            unassemblize::PdbReaderPair pdb_reader_pair2 = {pdb_reader_pair[0].get(), pdb_reader_pair[1].get()};

            unassemblize::AsmComparisonOptions o(executable_pair2, pdb_reader_pair2, output_file);
            o.format = clo.format;
            if (clo.bundle_file_idx < 2)
                o.bundling_pdb_reader = pdb_reader_pair[clo.bundle_file_idx].get();
            if (o.bundling_pdb_reader != nullptr)
                o.bundle_type = clo.bundle_type;
            o.print_indent_len = clo.print_indent_len;
            o.print_asm_len = clo.print_asm_len;
            o.print_byte_count = clo.print_byte_count;
            o.print_sourcecode_len = clo.print_sourcecode_len;
            o.print_sourceline_len = clo.print_sourceline_len;
            o.lookahead_limit = clo.lookahead_limit;
            o.match_strictness = clo.match_strictness;
            ok &= unassemblize::Runner::process_asm_comparison(o);
        }
    }
    return ok ? 0 : 1;
}
