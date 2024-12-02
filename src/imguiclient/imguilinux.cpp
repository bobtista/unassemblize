#include "imguilinux.h"
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace unassemblize::gui
{

ImGuiLinux::ImGuiLinux() : m_window(nullptr), m_initialized(false)
{
}

ImGuiLinux::~ImGuiLinux()
{
    shutdown();
}

ImGuiStatus ImGuiLinux::run(const CommandLineOptions &clo)
{
    m_app = std::make_unique<ImGuiApp>();
    
    if (!init())
        return ImGuiStatus::Error;

    // Initialize ImGui app
    ImGuiStatus status = m_app->init(clo);
    if (status != ImGuiStatus::Ok)
        return status;

    // Main loop
    while (!glfwWindowShouldClose(m_window))
    {
        if (!update())
            break;
        render();
    }

    // Cleanup
    shutdown();
    return ImGuiStatus::Ok;
}

bool ImGuiLinux::init()
{
    if (m_initialized)
        return true;

    // Initialize GLFW
    if (!glfwInit())
        return false;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    m_window = glfwCreateWindow(1280, 720, "Unassemblize", NULL, NULL);
    if (m_window == nullptr)
        return false;
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    m_initialized = true;
    return true;
}

void ImGuiLinux::shutdown()
{
    if (!m_initialized)
        return;

    if (m_app)
    {
        m_app->prepare_shutdown_wait();
        while (!m_app->can_shutdown())
        {
            update();
        }
        m_app->shutdown();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    
    glfwTerminate();
    m_initialized = false;
}

bool ImGuiLinux::update()
{
    if (!m_initialized)
        return false;

    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Update app
    ImGuiStatus status = m_app->update();
    if (status != ImGuiStatus::Ok)
        return false;

    return true;
}

void ImGuiLinux::render()
{
    if (!m_initialized)
        return;

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    const ImVec4& clear_color = m_app->get_clear_color();
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

} // namespace unassemblize::gui
