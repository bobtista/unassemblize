#pragma once

#include <imgui.h>
#include "imguiapp.h"

namespace unassemblize::gui
{

class ImGuiLinux
{
public:
    ImGuiLinux();
    ~ImGuiLinux();

    ImGuiStatus run(const CommandLineOptions &clo);

private:
    bool init();
    void shutdown();
    bool update();
    void render();

    std::unique_ptr<ImGuiApp> m_app;
    GLFWwindow* m_window;
    bool m_initialized;
};

} // namespace unassemblize::gui
