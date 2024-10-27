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
     * Append texts from instruction data to a file.
     */
    static void append_to_file(FILE *fp, const AsmInstructionVariants &instructions);

    /*
     * Append texts from instruction data to a string.
     */
    static void append_to_string(std::string &str, const AsmInstructionVariants &instructions);
};

} // namespace unassemblize
