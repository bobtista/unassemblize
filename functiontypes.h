/**
 * @file
 *
 * @brief Function types
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include <array>
#include <string_view>

namespace unassemblize
{
constexpr std::string_view s_prefix_sub = "sub_";
constexpr std::string_view s_prefix_off = "off_";
constexpr std::string_view s_prefix_unk = "unk_";
constexpr std::string_view s_prefix_loc = "loc_";
constexpr std::array<std::string_view, 4> s_prefix_array = {s_prefix_sub, s_prefix_off, s_prefix_unk, s_prefix_loc};

// #TODO: implement default value where exe object decides internally what to do.
enum class AsmFormat
{
    IGAS,
    AGAS,
    MASM,

    DEFAULT,
};

AsmFormat to_asm_format(const char *str);

} // namespace unassemblize
