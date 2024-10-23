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
#include "functiontypes.h"
#include "strings.h"

namespace unassemblize
{
AsmFormat to_asm_format(const char *str)
{
    if (0 == strcasecmp(str, "igas")) {
        return AsmFormat::IGAS;
    } else if (0 == strcasecmp(str, "agas")) {
        return AsmFormat::AGAS;
    } else if (0 == strcasecmp(str, "masm")) {
        return AsmFormat::MASM;
    } else {
        return AsmFormat::DEFAULT;
    }
    static_assert(size_t(AsmFormat::DEFAULT) == 3, "Enum was changed. Update switch case.");
}

} // namespace unassemblize
