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

namespace unassemblize
{
// #TODO: implement default value where exe object decides internally what to do.
enum class AsmFormat
{
    DEFAULT,
    IGAS,
    AGAS,
    MASM,
};

AsmFormat to_asm_format(const char *str);

} // namespace unassemblize
