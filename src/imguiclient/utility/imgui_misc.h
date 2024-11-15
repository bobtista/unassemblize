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
#pragma once

// clang-format off
#include <imgui_internal.h>
#include <imgui.h>
// clang-format on
#include <string>
#include <string_view>

namespace unassemblize::gui
{
struct WindowPlacement
{
    ImVec2 pos = ImVec2(-FLT_MAX, -FLT_MAX);
    ImVec2 size = ImVec2(-FLT_MAX, -FLT_MAX);
};

extern WindowPlacement g_lastFileDialogPlacement;

void TextUnformatted(std::string_view view);

void TooltipText(const char *fmt, ...);
void TooltipTextV(const char *fmt, va_list args);
void TooltipTextUnformatted(const char *text, const char *text_end = nullptr);
void TooltipTextMarker(const char *fmt, ...);
void TooltipTextUnformattedMarker(const char *text, const char *text_end = nullptr);

void OverlayProgressBar(const ImRect &rect, float fraction, const char *overlay = nullptr);

void DrawInTextCircle(ImU32 color);

ImVec2 OuterSizeForTable(size_t show_table_len, size_t table_len);

void ApplyPlacementToNextWindow(WindowPlacement &placement);
void FetchPlacementFromWindowByName(WindowPlacement &placement, const char *window_name);

void AddFileDialogButton(
    std::string *file_path_name,
    std::string_view button_label,
    const std::string &key,
    const std::string &title,
    const char *filters);

} // namespace unassemblize::gui
