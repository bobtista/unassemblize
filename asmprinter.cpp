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
void AsmPrinter::append_to_string(std::string &str, const AsmInstructionVariants &instructions, uint32_t indent_len)
{
    constexpr std::string_view eol = "\n";

    // The first variant is expected to be a label if it is the begin of a function.
    std::string name;
    if (const AsmLabel *label = std::get_if<AsmLabel>(&instructions[0]))
    {
        name = label->label;
    }
    else
    {
        name = "_unknown_";
    }

    std::string header = fmt::format(
        ".intel_syntax noprefix{:s}{:s}"
        ".globl {:s}{:s}",
        eol,
        eol,
        name,
        eol);

    // Simple estimation based on game.dat
    str.reserve(header.size() + instructions.size() * (indent_len + 24));
    str += header;

    for (const AsmInstructionVariant &variant : instructions)
    {
        if (const AsmInstruction *instruction = std::get_if<AsmInstruction>(&variant))
        {
            str += to_string(*instruction, indent_len);
        }
        else if (const AsmLabel *label = std::get_if<AsmLabel>(&variant))
        {
            str += to_string(*label);
        }

        str += eol;
    }
}

std::string AsmPrinter::to_string(const AsmInstruction &instruction, size_t indent_len)
{
    constexpr std::string_view strip_quote = "\"";
    std::string str;

    if (instruction.isInvalid)
    {
        // Assign unrecognized instruction as comment.
        str = fmt::format("; Unrecognized opcode at address:{:08x} bytes:{:s}", instruction.address, instruction.text);
    }
    else
    {
        // Assign assembler text.
        str.reserve(indent_len + instruction.text.size());
        append_whitespace_inplace(str, indent_len);
        str.append(instruction.text);
        util::strip_inplace(str, strip_quote);
    }

    if (instruction.isJump)
    {
        // Append jump distance as inline comment.
        str += fmt::format(" ; {:+d} bytes", instruction.jumpLen);
    }

    return str;
}

std::string AsmPrinter::to_string(const AsmLabel &label)
{
    return fmt::format("{:s}:", label.label);
}

void AsmPrinter::append_to_string(
    std::string &str,
    const AsmComparisonResult &comparison,
    const StringPair &exe_filenames,
    AsmMatchStrictness match_strictness,
    uint32_t asm_len,
    uint32_t indent_len)
{
    if (comparison.records.empty())
    {
        return;
    }

    constexpr std::string_view eol = "\n";
    constexpr std::string_view header = "> ";
    constexpr std::string_view equal = " == ";
    constexpr std::string_view unequal = " xx ";
    constexpr std::string_view maybe_equal = " ?? ";
    constexpr std::string_view left_missing = " >> ";
    constexpr std::string_view right_missing = " << ";
    constexpr size_t address_len = 10;
    constexpr size_t cmp_len = equal.size();
    constexpr size_t side_count = 2;
    constexpr size_t end_eol_count = 4;
    const size_t line_len = side_count * (address_len + indent_len + asm_len) + cmp_len + eol.size();
    static_assert(equal.size() == unequal.size(), "Expects same length");
    static_assert(equal.size() == maybe_equal.size(), "Expects same length");
    static_assert(equal.size() == left_missing.size(), "Expects same length");
    static_assert(equal.size() == right_missing.size(), "Expects same length");

    auto pad_side_line = [=](std::string &line_buf, size_t side_idx) {
        pad_whitespace_inplace(line_buf, (side_idx + 1) * (address_len + indent_len + asm_len) + side_idx * cmp_len);
    };

    std::string misc_buf;
    std::string line_buf;
    misc_buf.reserve(1024);
    line_buf.reserve(line_len);

    {
        // Create misc info.
        // The first line is expected to be a label if it is the begin of a function.
        const AsmLabelPair *label = std::get_if<AsmLabelPair>(&comparison.records[0]);
        const std::string name = label != nullptr ? label->pair[0]->label : "_unknown_";

        const uint32_t match_count = comparison.get_match_count(match_strictness);
        const uint32_t max_match_count = comparison.get_max_match_count(match_strictness);
        const uint32_t mismatch_count = comparison.get_mismatch_count(match_strictness);
        const uint32_t max_mismatch_count = comparison.get_max_mismatch_count(match_strictness);
        const float similarity = comparison.get_similarity(match_strictness);
        const float max_similarity = comparison.get_max_similarity(match_strictness);

        misc_buf += fmt::format("{:s}{:s}{:s}", header, name, eol);
        misc_buf += fmt::format("{:s}match count: {:d}", header, match_count);
        if (max_match_count != match_count)
        {
            misc_buf += fmt::format(" or {:d}", max_match_count);
        }
        misc_buf += eol;
        misc_buf += fmt::format("{:s}mismatch count: {:d}", header, mismatch_count);
        if (max_mismatch_count != mismatch_count)
        {
            misc_buf += fmt::format(" or {:d}", max_mismatch_count);
        }
        misc_buf += eol;
        misc_buf += fmt::format("{:s}similarity: {:.1f} %%", header, similarity * 100.f);
        if (max_similarity != similarity)
        {
            misc_buf += fmt::format(" or {:.1f} %%", max_similarity * 100.f);
        }
        misc_buf += eol;
        misc_buf += header;
        misc_buf += eol;
    }

    // Create file names.
    for (size_t i = 0; i < side_count; ++i)
    {
        std::string filename_copy = exe_filenames[i];
        front_truncate_inplace(filename_copy, address_len + indent_len + asm_len - header.size());
        line_buf += header;
        line_buf += filename_copy;
        pad_side_line(line_buf, i);
        if (i != side_count - 1)
        {
            append_whitespace_inplace(line_buf, cmp_len);
        }
        else
        {
            line_buf += eol;
        }
    }

    line_buf += header;
    line_buf += eol;
    misc_buf.append(line_buf);
    line_buf.clear();

    const size_t reserved_len =
        str.size() + comparison.records.size() * line_len + end_eol_count * eol.size() + misc_buf.size();

    str.reserve(reserved_len);
    str.append(misc_buf);

    for (const AsmComparisonRecord &record : comparison.records)
    {
        misc_buf.clear();
        line_buf.clear();

        if (const AsmLabelPair *label_pair = std::get_if<AsmLabelPair>(&record))
        {
            for (size_t i = 0; i < side_count; ++i)
            {
                const AsmLabel *label = label_pair->pair[i];
                if (label != nullptr)
                {
                    append_whitespace_inplace(line_buf, address_len);
                    misc_buf = to_string(*label);
                    truncate_inplace(misc_buf, asm_len);
                    line_buf.append(misc_buf);
                }
                pad_side_line(line_buf, i);
                if (i != side_count - 1)
                {
                    append_whitespace_inplace(line_buf, cmp_len);
                }
            }
        }
        else if (const AsmInstructionPair *instruction_pair = std::get_if<AsmInstructionPair>(&record))
        {
            for (size_t i = 0; i < side_count; ++i)
            {
                const AsmInstruction *instruction = instruction_pair->pair[i];
                if (instruction != nullptr)
                {
                    line_buf.append(fmt::format("{:08x}  ", instruction->address));
                    misc_buf = to_string(*instruction, indent_len);
                    truncate_inplace(misc_buf, asm_len + indent_len);
                    line_buf.append(misc_buf);
                }
                pad_side_line(line_buf, i);
                if (i != side_count - 1)
                {
                    const AsmMatchValue match_value = instruction_pair->mismatch_info.get_match_value(match_strictness);

                    switch (match_value)
                    {
                        case AsmMatchValue::IsMatch:
                            line_buf.append(equal);
                            break;

                        case AsmMatchValue::IsMaybeMatch:
                            line_buf.append(maybe_equal);
                            break;

                        case AsmMatchValue::IsMismatch: {
                            const AsmInstruction *right_instruction = instruction_pair->pair[i + 1];
                            if (instruction != nullptr && right_instruction != nullptr)
                            {
                                line_buf.append(unequal);
                            }
                            else if (instruction == nullptr && right_instruction != nullptr)
                            {
                                line_buf.append(left_missing);
                            }
                            else if (instruction != nullptr && right_instruction == nullptr)
                            {
                                line_buf.append(right_missing);
                            }
                            else
                            {
                                assert(false);
                            }
                            break;
                        }
                        default:
                            assert(false);
                            break;
                    }
                }
            }
        }
        else
        {
            assert(false);
        }

        str.append(line_buf);
        str.append(eol);
    }

    // Add line breaks at the end.
    for (size_t i = 0; i < end_eol_count; ++i)
    {
        str.append(eol);
    }

    assert(str.size() == reserved_len);
}

void AsmPrinter::truncate_inplace(std::string &str, size_t max_len)
{
    max_len = std::max<size_t>(max_len, 2);
    if (str.size() > max_len)
    {
        str.resize(max_len);
        // End string on 2 dots to clarify that it has been truncated.
        str[max_len - 1] = '.';
        str[max_len - 2] = '.';
    }
}

void AsmPrinter::front_truncate_inplace(std::string &str, size_t max_len)
{
    max_len = std::max<size_t>(max_len, 2);
    if (str.size() > max_len)
    {
        str.erase(0, str.size() - max_len);
        // Begin string on 2 dots to clarify that it has been truncated.
        str[0] = '.';
        str[1] = '.';
    }
}

void AsmPrinter::pad_whitespace_inplace(std::string &str, size_t len)
{
    if (str.size() < len)
    {
        const size_t pad_count = len - str.size();
        append_whitespace_inplace(str, pad_count);
    }
}

void AsmPrinter::append_whitespace_inplace(std::string &str, size_t len)
{
    str.insert(str.end(), len, ' ');
}

} // namespace unassemblize
