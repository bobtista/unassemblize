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
    append_to_string(text, instructions);

    // The first variant is expected to be a label if it is the begin of a function.
    std::string name;
    if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&instructions[0])) {
        name = label->label;
    } else {
        name = "_unknown_";
    }

    fprintf(fp, ".intel_syntax noprefix\n\n");
    fprintf(fp, ".globl %s\n%s", name.c_str(), text.c_str());
}

void AsmPrinter::append_to_string(std::string &str, const AsmInstructionVariants &instructions)
{
    constexpr size_t indent_len = 4;

    str.reserve(instructions.size() * 32);

    for (const AsmInstructionVariant &variant : instructions) {
        if (const AsmInstruction *instruction = std::get_if<AsmInstruction>(&variant)) {
            str += to_string(*instruction, indent_len);
        } else if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&variant)) {
            str += to_string(*label);
        }

        str += "\n";
    }
}

std::string AsmPrinter::to_string(const AsmInstruction &instruction, size_t indent_len)
{
    constexpr std::string_view strip_quote = "\"";
    std::string str;

    if (instruction.isInvalid) {
        // Assign unrecognized instruction as comment.
        str = fmt::format("; Unrecognized opcode at address:{:08x} bytes:{:s}", instruction.address, instruction.text);
    } else {
        // Assign assembler text.
        str.reserve(indent_len + instruction.text.size());
        str.insert(str.end(), indent_len, ' ');
        str.append(instruction.text);
        util::strip_inplace(str, strip_quote);
    }

    if (instruction.isJump) {
        // Append jump distance as inline comment.
        str += fmt::format(" ; {:+d} bytes", instruction.jumpLen);
    }

    return str;
}

std::string AsmPrinter::to_string(const AsmInstructionLabel &label)
{
    return fmt::format("{:s}:", label.label);
}

void AsmPrinter::append_to_file(FILE *fp, const AsmComparisonResult &comparison)
{
    assert(fp != nullptr);

    if (comparison.records.empty()) {
        return;
    }

    std::string text;
    // text.reserve(comparison.records.size() * 64); // #TODO: reserve more?
    append_to_string(text, comparison);

    fprintf(fp, text.c_str());
}

void AsmPrinter::append_to_string(std::string &str, const AsmComparisonResult &comparison)
{
    if (comparison.records.empty()) {
        return;
    }

    // #TODO: Make part of this configurable
    constexpr std::string_view eol = "\n";
    constexpr std::string_view equal = " == ";
    constexpr std::string_view unequal = " xx ";
    constexpr std::string_view left_missing = " >> ";
    constexpr std::string_view right_missing = " << ";
    constexpr size_t address_len = 10;
    constexpr size_t indent_len = 4;
    constexpr size_t asm_len = 80;
    constexpr size_t cmp_len = equal.size();
    constexpr size_t side_count = 2;
    constexpr size_t end_eol_count = 4;
    constexpr size_t line_len = side_count * (address_len + indent_len + asm_len) + cmp_len + eol.size();
    static_assert(equal.size() == unequal.size(), "Expects same length");
    static_assert(equal.size() == left_missing.size(), "Expects same length");
    static_assert(equal.size() == right_missing.size(), "Expects same length");

    std::string asm_buf;
    std::string line_buf;
    asm_buf.reserve(1024);
    line_buf.reserve(line_len);

    // The first line is expected to be a label if it is the begin of a function.
    std::string name;
    if (const AsmLabel *label = std::get_if<AsmLabel>(&comparison.records[0])) {
        name = label->label_pair[0]->label;
    } else {
        name = "_unknown_";
    }
    const float match_percent = (float)comparison.match_count / (float)(comparison.get_instruction_count()) * 100.f;

    asm_buf = fmt::format(
        "::  {:s}{:s}"
        "::    matches: {:d}{:s}"
        "::    mismatches: {:d}{:s}"
        "::    match percent: {:.1f}{:s}{:s}",
        name,
        eol,
        comparison.match_count,
        eol,
        comparison.mismatch_count,
        eol,
        match_percent,
        eol,
        eol);

    const size_t reserved_len =
        str.size() + comparison.records.size() * line_len + end_eol_count * eol.size() + asm_buf.size();

    str.reserve(reserved_len);
    str.append(asm_buf);

    auto pad_line = [=](std::string &line_buf, size_t side_idx) {
        pad_whitespace_inplace(line_buf, (side_idx + 1) * (address_len + indent_len + asm_len) + side_idx * cmp_len);
    };

    for (const AsmComparisonRecord &record : comparison.records) {
        asm_buf.clear();
        line_buf.clear();

        if (const AsmLabel *asm_label = std::get_if<AsmLabel>(&record)) {
            for (size_t i = 0; i < side_count; ++i) {
                const AsmInstructionLabel &label = *asm_label->label_pair[i];
                if (!label.label.empty()) {
                    line_buf.insert(line_buf.end(), address_len, ' ');
                    asm_buf = to_string(label);
                    truncate_string_inplace(asm_buf, asm_len);
                    line_buf.append(asm_buf);
                }
                pad_line(line_buf, i);
                if (i != side_count - 1) {
                    line_buf.insert(line_buf.end(), cmp_len, ' ');
                }
            }
        } else if (const AsmMatch *asm_match = std::get_if<AsmMatch>(&record)) {
            for (size_t i = 0; i < side_count; ++i) {
                const AsmInstruction &instruction = *asm_match->instruction_pair[i];
                if (!instruction.is_empty()) {
                    line_buf.append(fmt::format("{:08x}  ", instruction.address));
                    asm_buf = to_string(instruction, indent_len);
                    truncate_string_inplace(asm_buf, asm_len + indent_len);
                    line_buf.append(asm_buf);
                }
                pad_line(line_buf, i);
                if (i != side_count - 1) {
                    line_buf.append(equal);
                }
            }
        } else if (const AsmMismatch *asm_mismatch = std::get_if<AsmMismatch>(&record)) {
            for (size_t i = 0; i < side_count; ++i) {
                const AsmInstruction &instruction = *asm_mismatch->instruction_pair[i];
                if (!instruction.is_empty()) {
                    line_buf.append(fmt::format("{:08x}  ", instruction.address));
                    asm_buf = to_string(instruction, indent_len);
                    truncate_string_inplace(asm_buf, asm_len + indent_len);
                    line_buf.append(asm_buf);
                }
                pad_line(line_buf, i);
                if (i != side_count - 1) {
                    const AsmInstruction &right_instruction = *asm_mismatch->instruction_pair[i + 1];
                    if (!instruction.is_empty() && !right_instruction.is_empty()) {
                        line_buf.append(unequal);
                    } else if (instruction.is_empty() && !right_instruction.is_empty()) {
                        line_buf.append(left_missing);
                    } else if (!instruction.is_empty() && right_instruction.is_empty()) {
                        line_buf.append(right_missing);
                    } else {
                        assert(false);
                    }
                }
            }
        } else {
            assert(false);
        }

        str.append(line_buf);
        str.append(eol);
    }

    // Add line breaks at the end.
    for (size_t i = 0; i < end_eol_count; ++i) {
        str.append(eol);
    }

    assert(str.size() == reserved_len);
}

void AsmPrinter::truncate_string_inplace(std::string &str, size_t max_len)
{
    max_len = std::max<size_t>(max_len, 2);
    if (str.size() > max_len) {
        str.resize(max_len);
        // End string on 2 dots to clarify that it has been truncated.
        str[max_len - 1] = '.';
        str[max_len - 2] = '.';
    }
}

void AsmPrinter::pad_whitespace_inplace(std::string &str, size_t len)
{
    if (str.size() < len) {
        const size_t pad_count = len - str.size();
        str.insert(str.end(), pad_count, ' ');
    }
}

} // namespace unassemblize
