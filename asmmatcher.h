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
#pragma once

#include "asmmatchertypes.h"

namespace unassemblize
{
class AsmMatcher
{
    template<class E, size_t Size>
    struct SizeableArray
    {
        std::array<E, Size> inner;
        size_t size = 0;
    };
    using InstructionTextArray = SizeableArray<std::string, 4>;
    using InstructionTextArrays = std::vector<InstructionTextArray>;

    struct LookaheadResult
    {
        bool is_label = false;
        bool is_matching = false;
    };

public:
    /*
     * Runs a comparison on the given FunctionMatch.
     * The returned result will retain a dependency on that FunctionMatch object.
     */
    static AsmComparisonResult run_comparison(const FunctionMatch &match);

private:
    // clang-format off

    /*
     * Looks ahead one side of the instruction list and compares
     * its last instruction with the base instruction of the opposite side.
     */
    static LookaheadResult run_lookahead_comparison(size_t lookahead_side,
        AsmInstructionVariants::const_iterator lookahead_base_it,
        AsmInstructionVariants::const_iterator lookahead_last_it,
        const InstructionTextArray &lookahead_last_array,
        const AsmInstruction &opposite_base_instruction,
        const InstructionTextArray &opposite_base_array,
        AsmComparisonResult &comparison_result);

    static bool has_mismatch(
        const AsmInstruction &instruction0,
        const AsmInstruction &instruction1,
        const InstructionTextArray *array0 = nullptr,
        const InstructionTextArray *array1 = nullptr,
        AsmTextMismatchInfo *out_text_info = nullptr);

    // clang-format on

    static bool has_jump_len_mismatch(const AsmInstruction &instruction0, const AsmInstruction &instruction1);
    static AsmTextMismatchInfo compare_asm_text(const std::string &text0, const std::string &text1);
    static AsmTextMismatchInfo compare_asm_text(const InstructionTextArray &array0, const InstructionTextArray &array1);
    static bool skip_unknown_symbol(const char *&str);
    static void skip_known_symbol(const char *&str);

    /*
     * Splits instruction text string to text array.
     * "mov dword ptr[eax], 0x10" becomes {"mov", "dword ptr[eax]", "0x10"}
     */
    static InstructionTextArrays split_instruction_texts(const AsmInstructionVariants &instructions);
    static InstructionTextArray split_instruction_text(const std::string &text);

    static AsmInstructionVariant s_nullInstructionVariant;
    static AsmInstruction s_dummyInstruction;
    static AsmInstructionLabel s_dummyInstructionLabel;
};

} // namespace unassemblize
