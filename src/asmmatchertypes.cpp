/**
 * @file
 *
 * @brief Types to store relevant data for asm matching
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "asmmatchertypes.h"
#include <strings.h>

namespace unassemblize
{
MatchBundleType to_match_bundle_type(const char *str)
{
    if (0 == strcasecmp(str, "compiland"))
    {
        return MatchBundleType::Compiland;
    }
    else if (0 == strcasecmp(str, "sourcefile"))
    {
        return MatchBundleType::SourceFile;
    }
    else if (0 == strcasecmp(str, "none"))
    {
        return MatchBundleType::None;
    }
    else
    {
        printf("Unrecognized match bundle type '%s'. Defaulting to 'None'", str);
        return MatchBundleType::None;
    }
    static_assert(size_t(MatchBundleType::None) == 2, "Enum was changed. Update conditions.");
}

AsmMatchValue AsmMismatchInfo::get_match_value(AsmMatchStrictness strictness) const
{
    switch (strictness)
    {
        case AsmMatchStrictness::Lenient:
            if (mismatch_bits == 0 && mismatch_reasons == 0)
                return AsmMatchValue::IsMatch;
            else
                return AsmMatchValue::IsMismatch;

        default:
        case AsmMatchStrictness::Undecided:
            if (mismatch_bits == 0 && maybe_mismatch_bits == 0 && mismatch_reasons == 0)
                return AsmMatchValue::IsMatch;
            else if (maybe_mismatch_bits != 0)
                return AsmMatchValue::IsMaybeMatch;
            else
                return AsmMatchValue::IsMismatch;

        case AsmMatchStrictness::Strict:
            if (mismatch_bits == 0 && maybe_mismatch_bits == 0 && mismatch_reasons == 0)
                return AsmMatchValue::IsMatch;
            else
                return AsmMatchValue::IsMismatch;
    }
}

bool AsmMismatchInfo::is_match() const
{
    return mismatch_bits == 0 && maybe_mismatch_bits == 0 && mismatch_reasons == 0;
}

bool AsmMismatchInfo::is_mismatch() const
{
    return mismatch_bits != 0 || mismatch_reasons != 0;
}

bool AsmMismatchInfo::is_maybe_match() const
{
    return mismatch_bits == 0 && maybe_mismatch_bits != 0 && mismatch_reasons == 0;
}

bool AsmMismatchInfo::is_maybe_mismatch() const
{
    return is_maybe_match();
}

AsmMatchStrictness to_asm_match_strictness(const char *str)
{
    if (0 == strcasecmp(str, "lenient"))
    {
        return AsmMatchStrictness::Lenient;
    }
    else if (0 == strcasecmp(str, "undecided"))
    {
        return AsmMatchStrictness::Undecided;
    }
    else if (0 == strcasecmp(str, "strict"))
    {
        return AsmMatchStrictness::Strict;
    }
    else
    {
        printf("Unrecognized asm match to_asm_match_strictness '%s'. Defaulting to 'Undecided'", str);
        return AsmMatchStrictness::Undecided;
    }
}

uint32_t AsmComparisonResult::get_instruction_count() const
{
    return match_count + maybe_match_count + mismatch_count;
}

uint32_t AsmComparisonResult::get_match_count(AsmMatchStrictness strictness) const
{
    switch (strictness)
    {
        case AsmMatchStrictness::Lenient:
            return match_count + maybe_match_count;
        default:
        case AsmMatchStrictness::Undecided:
            return match_count;
        case AsmMatchStrictness::Strict:
            return match_count;
    }
}

uint32_t AsmComparisonResult::get_max_match_count(AsmMatchStrictness strictness) const
{
    switch (strictness)
    {
        case AsmMatchStrictness::Lenient:
            return match_count + maybe_match_count;
        default:
        case AsmMatchStrictness::Undecided:
            return match_count + maybe_match_count;
        case AsmMatchStrictness::Strict:
            return match_count;
    }
}

uint32_t AsmComparisonResult::get_mismatch_count(AsmMatchStrictness strictness) const
{
    switch (strictness)
    {
        case AsmMatchStrictness::Lenient:
            return mismatch_count;
        default:
        case AsmMatchStrictness::Undecided:
            return mismatch_count;
        case AsmMatchStrictness::Strict:
            return mismatch_count + maybe_match_count;
    }
}

uint32_t AsmComparisonResult::get_max_mismatch_count(AsmMatchStrictness strictness) const
{
    switch (strictness)
    {
        case AsmMatchStrictness::Lenient:
            return mismatch_count;
        default:
        case AsmMatchStrictness::Undecided:
            return mismatch_count + maybe_match_count;
        case AsmMatchStrictness::Strict:
            return mismatch_count + maybe_match_count;
    }
}

float AsmComparisonResult::get_similarity(AsmMatchStrictness strictness) const
{
    return float(get_match_count(strictness)) / float(get_instruction_count());
}

float AsmComparisonResult::get_max_similarity(AsmMatchStrictness strictness) const
{
    return float(get_max_match_count(strictness)) / float(get_instruction_count());
}

} // namespace unassemblize
