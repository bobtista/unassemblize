#pragma once

#include <imgui.h>
#include "imguiapp.h"

namespace unassemblize::gui
{

class ImGuiLinux
{
public:
    ImGuiLinux() = default;
    ~ImGuiLinux() = default;

    bool init();
    void shutdown();
    bool update();
    void render();

private:
    // Linux-specific implementation details will go here
};

} // namespace unassemblize::gui
