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
#include "asmprinter.h"
#include "util.h"
#include <fmt/core.h>

namespace unassemblize
{
void AsmPrinter::append_to_file(FILE *fp, const AsmInstructionVariants &instructions)
{
    assert(fp != nullptr);

    if (instructions.empty()) {
        return;
    }

    std::string text;
    text.reserve(instructions.size() * 32);
    append_to_string(text, instructions);

    // The first variant is expected to be a label if it is the begin of a function.
    std::string sym;
    if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&instructions[0])) {
        sym = label->label;
    } else {
        sym = "_unknown_";
    }

    fprintf(fp, ".intel_syntax noprefix\n\n");
    fprintf(fp, ".globl %s\n%s", sym.c_str(), text.c_str());
}

void AsmPrinter::append_to_string(std::string &str, const AsmInstructionVariants &instructions)
{
    const std::string strip_quote = "\"";

    for (const AsmInstructionVariant &variant : instructions) {
        if (const AsmInstruction *instruction = std::get_if<AsmInstruction>(&variant)) {
            // Print instruction.
            if (instruction->isInvalid) {
                // Add unrecognized instruction as comment.
                str += fmt::format(
                    "; Unrecognized opcode at runtime-address:0x{:08X} bytes:{:s}", instruction->address, instruction->text);
            } else {
                str += fmt::format("    {:s}", util::strip(instruction->text, strip_quote));
            }

            if (instruction->isJump) {
                // Add jump distance as inline comment.
                str += fmt::format(" ; {:+d} bytes", instruction->jumpLen);
            }
        } else if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&variant)) {
            // Print label.
            str += fmt::format("{:s}:", label->label);
        }

        str += "\n";
    }
}

} // namespace unassemblize
