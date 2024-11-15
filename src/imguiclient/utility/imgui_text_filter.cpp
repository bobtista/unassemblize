/**
 * @file
 *
 * @brief ImGui utility
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "imgui_text_filter.h"
#include <fmt/core.h>

bool ImGuiTextFilterEx::Draw(const char *key, const char *label, float width)
{
    return ImGuiTextFilter::Draw(fmt::format("{:s}##{:s}", label, key).c_str(), width);
}

bool ImGuiTextFilterEx::PassFilter(std::string_view view) const
{
    return ImGuiTextFilter::PassFilter(view.data(), view.data() + view.size());
}
