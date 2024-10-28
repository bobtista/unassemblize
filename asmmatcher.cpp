/**
 * @file
 *
 * @brief Class to compare assembler texts
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "asmmatcher.h"
#include "strings.h"

namespace unassemblize
{
AsmInstructionVariant AsmMatcher::s_nullInstructionVariant(AsmInstructionNull{});
AsmInstruction AsmMatcher::s_dummyInstruction;
AsmInstructionLabel AsmMatcher::s_dummyInstructionLabel;

AsmComparisonResult AsmMatcher::run_comparison(const FunctionMatch &match)
{
    AsmComparisonResult result;

    const AsmInstructionVariants &instructions0 = match.functions[0].get_instructions();
    const AsmInstructionVariants &instructions1 = match.functions[1].get_instructions();
    assert(instructions0.size() != 0);
    assert(instructions1.size() != 0);

    // Creates all instruction splits in advance to avoid redundant splits when visiting instructions multiple times.
    const InstructionTextArrays arrays0 = split_instruction_texts(instructions0);
    const InstructionTextArrays arrays1 = split_instruction_texts(instructions1);
    const InstructionTextArray empty_array;

    {
        const size_t inst_count0 = match.functions[0].get_instruction_count();
        const size_t inst_count1 = match.functions[1].get_instruction_count();
        const size_t label_count0 = match.functions[0].get_label_count();
        const size_t label_count1 = match.functions[1].get_label_count();
        const size_t label_count_comb = label_count0 + label_count1;

        // Over-reserves with both label counts on both sides because labels can be unaligned in worst case mismatches.
        // On top of that, empty entries can take up space on either side, so this adds a guessed margin.
        size_t max_size = std::max(inst_count0 + label_count_comb, inst_count1 + label_count_comb);
        max_size = (size_t)((double)max_size * 1.2);
        result.records.reserve(max_size);
    }

    const size_t count0 = instructions0.size();
    const size_t count1 = instructions1.size();
    size_t i0 = 0;
    size_t i1 = 0;
    while (i0 < count0 || i1 < count1)
    {
        bool is_matching = false;
        bool do_lookahead = false;

        const InstructionTextArray &array0 = (i0 < count0) ? arrays0[i0] : empty_array;
        const InstructionTextArray &array1 = (i1 < count1) ? arrays1[i1] : empty_array;

        AsmTextMismatchInfo text_info;

        {
            const AsmInstructionVariant &variant0 = (i0 < count0) ? instructions0[i0] : s_nullInstructionVariant;
            const AsmInstructionVariant &variant1 = (i1 < count1) ? instructions1[i1] : s_nullInstructionVariant;

            // Check for any labels.

            const AsmInstructionLabel *label0 = std::get_if<AsmInstructionLabel>(&variant0);
            const AsmInstructionLabel *label1 = std::get_if<AsmInstructionLabel>(&variant1);

            if (label0 != nullptr || label1 != nullptr)
            {
                AsmLabel asm_label;
                if (label0 != nullptr)
                {
                    asm_label.label_pair[0] = label0;
                    ++i0;
                }
                else
                {
                    asm_label.label_pair[0] = &s_dummyInstructionLabel;
                }
                if (label1 != nullptr)
                {
                    asm_label.label_pair[1] = label1;
                    ++i1;
                }
                else
                {
                    asm_label.label_pair[1] = &s_dummyInstructionLabel;
                }
                result.records.emplace_back(std::move(asm_label));
                ++result.label_count;
                continue;
            }

            // Check for missing instruction on either side and mismatching instructions.

            const AsmInstruction *instruction0 = std::get_if<AsmInstruction>(&variant0);
            const AsmInstruction *instruction1 = std::get_if<AsmInstruction>(&variant1);

            if (instruction0 == nullptr || instruction1 == nullptr)
            {
                // Is missing instruction on one side.
                // Lookahead makes no sense then.
                is_matching = false;
                do_lookahead = false;
                text_info.mismatch_bits = ~uint16_t(0);
            }
            else if (has_mismatch(*instruction0, *instruction1, &array0, &array1, &text_info))
            {
                is_matching = false;
                do_lookahead = true;
            }
            else
            {
                is_matching = true;
                do_lookahead = false;
            }

            if (do_lookahead)
            {
                // Lookahead instructions to check if there is a match further ahead.
                assert(is_matching == false);
                assert(instruction0 != nullptr);
                assert(instruction1 != nullptr);

                size_t lookahead_limit0 = 20;
                size_t lookahead_limit1 = 20;
                size_t k0 = 0;
                size_t k1 = 0;
                ++k0;

                while (i0 + k0 < count0 && i1 + k1 < count1 && k0 < lookahead_limit0 && k1 < lookahead_limit1)
                {
                    // Lookahead takes turns on both sides.
                    // The first lookahead match will determine the side that skips ahead.
                    if (k0 > k1)
                    {
                        AsmInstructionVariants::const_iterator lookahead_base_it = instructions0.begin() + i0;
                        AsmInstructionVariants::const_iterator lookahead_last_it = lookahead_base_it + k0;
                        const InstructionTextArray &lookahead_last_array = arrays0[i0 + k0];
                        LookaheadResult lookahead_result = run_lookahead_comparison(
                            0, lookahead_base_it, lookahead_last_it, lookahead_last_array, *instruction1, array1, result);

                        if (lookahead_result.is_label)
                        {
                            // Skip over label.
                            ++k0;
                            ++lookahead_limit0;
                        }
                        else if (lookahead_result.is_matching)
                        {
                            // Set new base index and break.
                            is_matching = true;
                            i0 += k0;
                            break;
                        }
                        else
                        {
                            // Increment opposite side index to look at next.
                            ++k1;
                        }
                    }
                    else
                    {
                        AsmInstructionVariants::const_iterator lookahead_base_it = instructions1.begin() + i1;
                        AsmInstructionVariants::const_iterator lookahead_last_it = lookahead_base_it + k1;
                        const InstructionTextArray &lookahead_last_array = arrays1[i1 + k1];
                        LookaheadResult lookahead_result = run_lookahead_comparison(
                            1, lookahead_base_it, lookahead_last_it, lookahead_last_array, *instruction0, array0, result);

                        if (lookahead_result.is_label)
                        {
                            // Skip over label.
                            ++k1;
                            ++lookahead_limit1;
                        }
                        else if (lookahead_result.is_matching)
                        {
                            // Set new base index and break.
                            is_matching = true;
                            i1 += k1;
                            break;
                        }
                        else
                        {
                            // Increment opposite side index to look at next.
                            ++k0;
                        }
                    }
                }
            }
        }

        const AsmInstructionVariant &variant0 = (i0 < count0) ? instructions0[i0] : s_nullInstructionVariant;
        const AsmInstructionVariant &variant1 = (i1 < count1) ? instructions1[i1] : s_nullInstructionVariant;

        if (is_matching)
        {
            assert(std::holds_alternative<AsmInstruction>(variant0));
            assert(std::holds_alternative<AsmInstruction>(variant1));
            const AsmInstruction &instruction0 = std::get<AsmInstruction>(variant0);
            const AsmInstruction &instruction1 = std::get<AsmInstruction>(variant1);
            AsmMatch asm_match;
            asm_match.instruction_pair[0] = &instruction0;
            asm_match.instruction_pair[1] = &instruction1;
            result.records.emplace_back(std::move(asm_match));
            ++result.match_count;
            ++i0;
            ++i1;
        }
        else
        {
            const AsmInstruction *instruction0 = std::get_if<AsmInstruction>(&variant0);
            const AsmInstruction *instruction1 = std::get_if<AsmInstruction>(&variant1);
            AsmMismatch asm_mismatch;
            if (instruction0 != nullptr)
            {
                asm_mismatch.instruction_pair[0] = instruction0;
                ++i0;
            }
            else
            {
                asm_mismatch.instruction_pair[0] = &s_dummyInstruction;
            }
            if (instruction1 != nullptr)
            {
                asm_mismatch.instruction_pair[1] = instruction1;
                ++i1;
            }
            else
            {
                asm_mismatch.instruction_pair[1] = &s_dummyInstruction;
            }
            asm_mismatch.text_info = text_info;
            result.records.emplace_back(std::move(asm_mismatch));
            ++result.mismatch_count;
        }
    }

    return result;
}

AsmMatcher::LookaheadResult AsmMatcher::run_lookahead_comparison(
    size_t lookahead_side,
    AsmInstructionVariants::const_iterator lookahead_base_it,
    AsmInstructionVariants::const_iterator lookahead_last_it,
    const InstructionTextArray &lookahead_last_array,
    const AsmInstruction &opposite_base_instruction,
    const InstructionTextArray &opposite_base_array,
    AsmComparisonResult &comparison_result)
{
    assert(lookahead_side < 2);
    assert(lookahead_base_it < lookahead_last_it);

    const size_t opposite_side = (lookahead_side + 1) % 2;
    LookaheadResult lookahead_result;

    if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&*lookahead_last_it))
    {
        lookahead_result.is_label = true;
    }
    else
    {
        assert(std::holds_alternative<AsmInstruction>(*lookahead_last_it));
        const AsmInstruction &lookahead_last_instruction = std::get<AsmInstruction>(*lookahead_last_it);

        if (!has_mismatch(
                lookahead_last_instruction, opposite_base_instruction, &lookahead_last_array, &opposite_base_array))
        {
            // Lookahead instruction is matching with base instruction on the other side.
            lookahead_result.is_matching = true;

            for (auto it = lookahead_base_it; it < lookahead_last_it; ++it)
            {
                const AsmInstructionVariant &variant = *it;
                if (const AsmInstructionLabel *label = std::get_if<AsmInstructionLabel>(&variant))
                {
                    // Insert the label and go ahead.
                    AsmLabel asm_label;
                    asm_label.label_pair[lookahead_side] = label;
                    asm_label.label_pair[opposite_side] = &s_dummyInstructionLabel;
                    comparison_result.records.emplace_back(std::move(asm_label));
                    ++comparison_result.label_count;
                }
                else
                {
                    // These are all mismatches because above it hit the first match upon lookahead.
                    assert(std::holds_alternative<AsmInstruction>(variant));
                    const AsmInstruction &instruction = std::get<AsmInstruction>(variant);
                    AsmMismatch asm_mismatch;
                    AsmTextMismatchInfo text_info;
                    text_info.mismatch_bits = ~uint16_t(0);
                    asm_mismatch.instruction_pair[lookahead_side] = &instruction;
                    asm_mismatch.instruction_pair[opposite_side] = &s_dummyInstruction;
                    asm_mismatch.text_info = text_info;
                    comparison_result.records.emplace_back(std::move(asm_mismatch));
                    ++comparison_result.mismatch_count;
                }
            }
        }
    }

    return lookahead_result;
}

bool AsmMatcher::has_mismatch(
    const AsmInstruction &instruction0,
    const AsmInstruction &instruction1,
    const InstructionTextArray *array0,
    const InstructionTextArray *array1,
    AsmTextMismatchInfo *out_text_info)
{
    if (instruction0.isInvalid != instruction1.isInvalid)
    {
        return true;
    }
    AsmTextMismatchInfo text_info;
    if (array0 != nullptr && array1 != nullptr)
    {
        text_info = compare_asm_text(*array0, *array1);
    }
    else
    {
        text_info = compare_asm_text(instruction0.text, instruction1.text);
    }
    if (out_text_info != nullptr)
    {
        *out_text_info = text_info;
    }
    if (text_info.is_mismatch())
    {
        return true;
    }
    if (has_jump_len_mismatch(instruction0, instruction1))
    {
        return true;
    }
    return false;
}

bool AsmMatcher::has_jump_len_mismatch(const AsmInstruction &instruction0, const AsmInstruction &instruction1)
{
    return instruction0.isJump && instruction1.isJump && instruction0.jumpLen != instruction1.jumpLen;
}

AsmTextMismatchInfo AsmMatcher::compare_asm_text(const std::string &text0, const std::string &text1)
{
    const InstructionTextArray array0 = split_instruction_text(text0);
    const InstructionTextArray array1 = split_instruction_text(text1);

    return compare_asm_text(array0, array1);
}

AsmTextMismatchInfo AsmMatcher::compare_asm_text(const InstructionTextArray &array0, const InstructionTextArray &array1)
{
    // Note: All symbols, including pseudo symbols, are expected to be enclosed by quotes.

    AsmTextMismatchInfo result;

    std::string dummy;

    for (size_t i = 0; i < array0.size || i < array1.size; ++i)
    {
        const std::string &word0 = (i < array0.size) ? array0.inner[i] : dummy;
        const std::string &word1 = (i < array1.size) ? array1.inner[i] : dummy;
        const char *c0 = word0.c_str();
        const char *c1 = word1.c_str();
        int in_quote = -1;

        for (; *c0 != '\0' || *c1 != '\0'; ++c0, ++c1)
        {
            if (*c0 == '\"' && *c1 == '\"')
            {
                // String is entering or leaving a quoted symbol name.
                in_quote = in_quote < 0 ? 0 : -1;
                continue;
            }
            else if (in_quote >= 0)
            {
                ++in_quote;
            }

            if (in_quote == 1)
            {
                assert(*c0 != '\"');
                assert(*c1 != '\"');

                // Skip ahead unknown symbols, such as "unk_12A0".
                const bool skipped0 = skip_unknown_symbol(c0);
                const bool skipped1 = skip_unknown_symbol(c1);

                // If one side skipped an unknown symbol, then skip the other symbol as well.
                if (skipped0 && !skipped1)
                {
                    skip_known_symbol(c1);
                }
                else if (!skipped0 && skipped1)
                {
                    skip_known_symbol(c0);
                }

                // If at least one symbol was skipped, then this quote is done.
                if (skipped0 || skipped1)
                {
                    assert(*c0 == '\"');
                    assert(*c1 == '\"');
                    in_quote = -1;
                }
            }

            if (*c0 != *c1)
            {
                // This section is mismatching. Break.
                result.mismatch_bits |= (1 << i);
                break;
            }
        }
    }

    return result;
}

bool AsmMatcher::skip_unknown_symbol(const char *&str)
{
    bool skipped = false;
    for (const std::string_view prefix : s_prefix_array)
    {
        if (0 == strncasecmp(str, prefix.data(), prefix.size()))
        {
            str += prefix.size();
            while (*str != '\"')
            {
                ++str;
            }
            skipped = true;
            break;
        }
    }
    return skipped;
}

void AsmMatcher::skip_known_symbol(const char *&str)
{
    while (*str != '\"' && *str != '\0')
    {
        ++str;
    }
}

AsmMatcher::InstructionTextArrays AsmMatcher::split_instruction_texts(const AsmInstructionVariants &instructions)
{
    InstructionTextArrays arrays;
    const size_t size = instructions.size();
    arrays.resize(size);
    for (size_t i = 0; i < size; ++i)
    {
        if (const AsmInstruction *instruction = std::get_if<AsmInstruction>(&instructions[i]))
        {
            arrays[i] = split_instruction_text(instruction->text);
        }
    }
    return arrays;
}

AsmMatcher::InstructionTextArray AsmMatcher::split_instruction_text(const std::string &text)
{
    InstructionTextArray arr;
    size_t index = 0;
    char sep = ' ';
    bool in_quote = false;

    for (const char *c = text.c_str(); *c != '\0'; ++c)
    {
        if (*c == '\"')
        {
            // Does not look for separator inside quoted text.
            in_quote = !in_quote;
        }
        else if (!in_quote && *c == sep)
        {
            // Change word separator for operands.
            sep = ',';
            // Omit spaces between operands.
            while (*(c + 1) == ' ')
                ++c;
            // Increment word index.
            ++index;
            assert(index < arr.inner.size());
            continue;
        }
        arr.inner[index].push_back(*c);
    }
    arr.size = index + 1;
    return arr;
}

} // namespace unassemblize
