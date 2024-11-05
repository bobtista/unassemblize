/**
 * @file
 *
 * @brief Option types
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "options.h"
#include "strings.h"
#include "util.h"
#include <filesystem>

std::string get_config_file_name(const std::string &input_file, const std::string &config_file)
{
    if (0 == strcasecmp(config_file.c_str(), auto_str))
    {
        // path/program.config.json
        std::filesystem::path path = input_file;
        path.replace_extension("config.json");
        return path.string();
    }
    return config_file;
}

std::string get_asm_output_file_name(const std::string &input_file, const std::string &output_file)
{
    if (0 == strcasecmp(output_file.c_str(), auto_str))
    {
        // path/program.S
        std::filesystem::path path = input_file;
        path.replace_extension("S");
        return path.string();
    }
    return output_file;
}

std::string
    get_cmp_output_file_name(const std::string &input_file0, const std::string &input_file1, const std::string &output_file)
{
    if (0 == strcasecmp(output_file.c_str(), auto_str))
    {
        // path0/program0_program1_cmp.txt
        std::filesystem::path path0 = input_file0;
        std::filesystem::path path1 = input_file1;
        std::filesystem::path path = path0.parent_path();
        path /= path0.stem();
        path += "_";
        path += path1.stem();
        path += "_cmp.txt";
        return path.string();
    }
    return output_file;
}

InputType get_input_type(const std::string &input_file, const std::string &input_type)
{
    InputType type = InputType::None;

    if (!input_file.empty())
    {
        if (0 == strcasecmp(input_type.c_str(), auto_str))
        {
            std::string input_file_ext = util::get_file_ext(input_file);
            if (0 == strcasecmp(input_file_ext.c_str(), "pdb"))
            {
                type = InputType::Pdb;
            }
            else
            {
                type = InputType::Exe;
            }
        }
        else if (0 == strcasecmp(input_type.c_str(), "exe"))
        {
            type = InputType::Exe;
        }
        else if (0 == strcasecmp(input_type.c_str(), "pdb"))
        {
            type = InputType::Pdb;
        }
    }

    return type;
}