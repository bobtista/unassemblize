/**
 * @file
 *
 * @brief ImGui App core
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include "imgui.h"

struct CommandLineOptions;

namespace unassemblize::gui
{
enum class ImGuiStatus
{
    Ok,
    Error,
};

class ImGuiApp
{
public:
    ImGuiApp() = default;
    ~ImGuiApp() = default;

    ImGuiStatus pre_platform_init(const CommandLineOptions &clo);
    ImGuiStatus post_platform_init();
    void shutdown();
    ImGuiStatus update();

    const ImVec4 &get_clear_color() const { return m_clear_color; }

private:
    void show_demo_filedialog();
    void show_app();

    ImVec4 m_clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool m_show_another_window = false;
    bool m_show_demo_window = true;
};

} // namespace unassemblize::gui
