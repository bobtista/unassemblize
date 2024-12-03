/**
 * @file
 *
 * @brief ImGui shell for GLFW (Linux/macOS)
 *
 * @copyright Unassemblize is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "imguiglfw.h"
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "version.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace unassemblize::gui
{

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

ImGuiGLFW::ImGuiGLFW() 
    : m_window(nullptr)
    , m_app(nullptr)
    , m_initialized(false)
    , m_dpiScale(1.0f)
{
}

ImGuiGLFW::~ImGuiGLFW()
{
    shutdown();
}

bool ImGuiGLFW::init()
{
    if (m_initialized)
        return true;

    fprintf(stderr, "Starting GLFW initialization...\n");

    // Setup GLFW error callback
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    fprintf(stderr, "GLFW initialized successfully\n");

    // Decide GL+GLSL versions
#ifdef __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    fprintf(stderr, "Setting up OpenGL 3.2 Core Profile for macOS...\n");
    
    // Required for macOS
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    
    // Additional macOS hints for better menu bar handling
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);      // Enable Retina display support
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);      // Enable graphics switching
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);                      // Create window hidden initially
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);                     // Enable window decorations
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);      // Disable transparency for menu bar
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);             // Better DPI handling
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    fprintf(stderr, "Creating GLFW window...\n");
    // Create window with graphics context
    m_window = glfwCreateWindow(1280, 800, create_version_string().c_str(), nullptr, nullptr);
    if (m_window == nullptr)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    fprintf(stderr, "GLFW window created successfully\n");

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

#ifdef __APPLE__
    // Set up window for macOS
    glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);  // Enable window decorations
    glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_TRUE);  // Allow resizing
    glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_FALSE);  // Not always on top
    
    // Center window on screen
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode) {
            int x = (mode->width - 1280) / 2;
            int y = (mode->height - 800) / 2;
            glfwSetWindowPos(m_window, x, y);
        }
    }
    
    // Show window explicitly
    glfwShowWindow(m_window);
    glfwFocusWindow(m_window);
#endif

    fprintf(stderr, "Setting up ImGui context...\n");
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    fprintf(stderr, "ImGui context created\n");

    ImGuiIO& io = ImGui::GetIO();
    fprintf(stderr, "Configuring ImGui flags...\n");
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

#ifdef __APPLE__
    // macOS-specific input handling
    io.ConfigMacOSXBehaviors = true;                         // Enable macOS-style keyboard shortcuts
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;    // Enable custom mouse cursors
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;     // Enable mouse position
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports; // Enable multi-viewport support
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // Enable renderer viewport support
#endif

    fprintf(stderr, "ImGui flags configured\n");

    // Setup Platform/Renderer backends
    fprintf(stderr, "Initializing GLFW backend...\n");
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true))  // Install callbacks
    {
        fprintf(stderr, "Failed to initialize GLFW backend\n");
        return false;
    }
    fprintf(stderr, "GLFW backend initialized\n");

    fprintf(stderr, "Initializing OpenGL3 backend...\n");
    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        fprintf(stderr, "Failed to initialize OpenGL3 backend\n");
        return false;
    }
    fprintf(stderr, "OpenGL3 backend initialized\n");

    // Setup style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Get initial DPI scale
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    m_dpiScale = (xscale > yscale) ? xscale : yscale;
    fprintf(stderr, "DPI scale: %f (x: %f, y: %f)\n", m_dpiScale, xscale, yscale);

    fprintf(stderr, "GLFW initialization complete\n");
    m_initialized = true;
    return true;
}

void ImGuiGLFW::shutdown()
{
    if (!m_initialized)
        return;

    // Cleanup ImGui app
    if (m_app)
    {
        m_app.reset();
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup GLFW
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    m_initialized = false;
}

bool ImGuiGLFW::update()
{
    if (!m_initialized || !m_window)
        return false;

    // Poll and handle events (inputs, window resize, etc.)
    glfwPollEvents();

    // Check if window should close
    if (glfwWindowShouldClose(m_window))
        return false;

    // Handle window iconification/minimization
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED))
    {
        glfwWaitEventsTimeout(0.1); // Sleep to reduce CPU usage
        return true;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Update display scale if needed
    updateDisplayScale();

    // Update window size and position
    int window_x, window_y;
    glfwGetWindowPos(m_window, &window_x, &window_y);
    
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    // Set window info for the app
    m_app->set_window_pos(ImVec2(static_cast<float>(window_x), static_cast<float>(window_y)));
    m_app->set_window_size(ImVec2(static_cast<float>(width), static_cast<float>(height)));

    // Update the app
    ImGuiStatus status = m_app->update();
    if (status != ImGuiStatus::Ok)
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        return false;
    }

    return true;
}

void ImGuiGLFW::render()
{
    if (!m_initialized || !m_window)
        return;

    // Rendering
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    ImVec4 clear_color = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(m_window);
}

void ImGuiGLFW::updateDisplayScale()
{
    fprintf(stderr, "updateDisplayScale called\n");
    
    if (!m_window) {
        fprintf(stderr, "Window not available\n");
        return;
    }
    
    if (!m_initialized) {
        fprintf(stderr, "Not yet initialized\n");
        return;
    }
    
    if (!glfwGetWindowAttrib(m_window, GLFW_VISIBLE)) {
        fprintf(stderr, "Window not visible\n");
        return;
    }

    fprintf(stderr, "Getting new DPI scale...\n");
    // Get new DPI scale
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    float newScale = (xscale > yscale) ? xscale : yscale;
    fprintf(stderr, "New scale: %f (x: %f, y: %f)\n", newScale, xscale, yscale);

    // Only update if scale changed
    if (newScale != m_dpiScale)
    {
        fprintf(stderr, "Scale changed from %f to %f\n", m_dpiScale, newScale);
        m_dpiScale = newScale;
        
        fprintf(stderr, "Updating style...\n");
        // Update style
        ImGui::GetStyle().ScaleAllSizes(m_dpiScale);
        
        fprintf(stderr, "Updating fonts...\n");
        // Update fonts
        try {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->Clear();
            ImFontConfig font_cfg;
            font_cfg.SizePixels = 13.0f * m_dpiScale;
            io.Fonts->AddFontDefault(&font_cfg);
            bool built = io.Fonts->Build();
            fprintf(stderr, "Fonts %s\n", built ? "built successfully" : "failed to build");
        } catch (const std::exception& e) {
            fprintf(stderr, "Exception during font update: %s\n", e.what());
        } catch (...) {
            fprintf(stderr, "Unknown exception during font update\n");
        }
    } else {
        fprintf(stderr, "Scale unchanged at %f\n", m_dpiScale);
    }
    
    fprintf(stderr, "updateDisplayScale complete\n");
}

ImGuiStatus ImGuiGLFW::run(const CommandLineOptions& clo)
{
    if (!init())
    {
        fprintf(stderr, "Failed to initialize ImGui GLFW backend\n");
        return ImGuiStatus::Error;
    }

    // Initialize the app after ImGui is fully set up
    m_app = std::make_unique<ImGuiApp>();
    if (m_app->init(clo) != ImGuiStatus::Ok)
    {
        fprintf(stderr, "Failed to initialize ImGui app\n");
        return ImGuiStatus::Error;
    }

    // Main loop
    while (!glfwWindowShouldClose(m_window))
    {
        if (!update())
            break;
        render();
    }

    shutdown();
    return ImGuiStatus::Ok;
}

} // namespace unassemblize::gui
