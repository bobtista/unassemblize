/**
 * @file
 *
 * @brief Class to print asm texts with
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "asmmatchertypes.h"
#include "function.h"

namespace unassemblize
{
class AsmPrinter
{
public:
    /*
     * Appends texts from instruction data to a file.
     */
    static void append_to_file(FILE *fp, const AsmInstructionVariants &instructions);

    /*
     * Appends texts from instruction data to a string.
     */
    static void append_to_string(std::string &str, const AsmInstructionVariants &instructions);

    /*
     * Appends texts from instruction data of a comparison result to a file.
     */
    static void append_to_file(FILE *fp, const AsmComparisonResult &comparison);

    /*
     * Appends texts from instruction data of a comparison result to a string.
     */
    static void append_to_string(std::string &str, const AsmComparisonResult &comparison);

private:
    static std::string to_string(const AsmInstruction &instruction, size_t indent_len);
    static std::string to_string(const AsmInstructionLabel &label);

    static void truncate_string_inplace(std::string &str, size_t max_len);
    static void pad_whitespace_inplace(std::string &str, size_t len);
};

} // namespace unassemblize
