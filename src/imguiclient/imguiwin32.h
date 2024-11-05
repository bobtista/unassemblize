/**
 * @file
 *
 * @brief ImGui shell for win32
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#pragma once

#include <memory>

struct CommandLineOptions;

namespace unassemblize::gui
{
enum class ImGuiStatus;
class ImGuiApp;

class ImGuiWin32
{
public:
    ImGuiWin32();
    ~ImGuiWin32();

    ImGuiStatus run(const CommandLineOptions &clo);

private:
    std::unique_ptr<ImGuiApp> m_app;
};

} // namespace unassemblize::gui
