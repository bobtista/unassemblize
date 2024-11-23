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
#include "imguiapp.h"
#include "executable.h"
#include "options.h"
#include "pdbreader.h"
#include "util.h"
#include "utility/imgui_scoped.h"
#include <algorithm>
#include <filesystem>
#include <fmt/core.h>
#include <misc/cpp/imgui_stdlib.h>

namespace unassemblize::gui
{
ImGuiApp::ProgramFileId ImGuiApp::ProgramFileDescriptor::s_id = 1;
ImGuiApp::AsmComparisonId ImGuiApp::AsmComparisonDescriptor::s_id = 1;

ImGuiApp::ProgramFileDescriptor::ProgramFileDescriptor() : id(s_id++)
{
}

ImGuiApp::ProgramFileDescriptor::~ProgramFileDescriptor()
{
}

void ImGuiApp::ProgramFileDescriptor::invalidate_command_id()
{
    activeCommandId = InvalidWorkQueueCommandId;
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_exe_filename() const
{
    if (is_auto_str(exeFilename))
    {
        return exeFilenameFromPdb;
    }
    else
    {
        return exeFilename;
    }
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_exe_config_filename() const
{
    return ::get_config_file_name(evaluate_exe_filename(), exeConfigFilename);
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_pdb_config_filename() const
{
    return ::get_config_file_name(pdbFilename, pdbConfigFilename);
}

std::string ImGuiApp::ProgramFileDescriptor::create_short_exe_name() const
{
    std::string name = evaluate_exe_filename();
    if (name.empty())
        name = exeFilename;
    std::filesystem::path path(name);
    return path.filename().string();
}

std::string ImGuiApp::ProgramFileDescriptor::create_descriptor_name() const
{
    return fmt::format("File {:d}", id);
}

std::string ImGuiApp::ProgramFileDescriptor::create_descriptor_name_with_short_exe_name() const
{
    const std::string exe_name = create_short_exe_name();
    if (exe_name.empty())
        return create_descriptor_name();
    else
        return fmt::format("File {:d} - {:s}", id, exe_name);
}

ImGuiApp::AsmComparisonDescriptor::AsmComparisonDescriptor() : id(s_id++)
{
}

ImGuiApp::AsmComparisonDescriptor::~AsmComparisonDescriptor()
{
}

ImGuiApp::ImGuiApp()
{
}

ImGuiApp::~ImGuiApp()
{
}

ImGuiStatus ImGuiApp::init(const CommandLineOptions &clo)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
#ifdef RELEASE
    io.ConfigDebugHighlightIdConflicts = false;
    io.ConfigDebugIsDebuggerPresent = false;
#else
    io.ConfigDebugHighlightIdConflicts = true;
    io.ConfigDebugIsDebuggerPresent = true;
#endif
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
    // ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application
    // (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash
    io.Fonts->AddFontDefault();
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont *font =
    //    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr,
    //    io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When view ports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    m_workQueue.start();

    for (size_t i = 0; i < CommandLineOptions::MAX_INPUT_FILES; ++i)
    {
        if (clo.input_file[i].v.empty())
            continue;

        auto descriptor = std::make_unique<ProgramFileDescriptor>();
        InputType input_type = get_input_type(clo.input_file[i], clo.input_type[i]);
        switch (input_type)
        {
            case InputType::Exe:
                descriptor->exeFilename = clo.input_file[i];
                descriptor->exeConfigFilename = clo.config_file[i];
                break;
            case InputType::Pdb:
                descriptor->exeFilename = auto_str;
                descriptor->exeConfigFilename = clo.config_file[i];
                descriptor->pdbFilename = clo.input_file[i];
                descriptor->pdbConfigFilename = clo.config_file[i];
                break;
            default:
                break;
        }

        m_programFiles.emplace_back(std::move(descriptor));
    }

    add_asm_comparison();

    return ImGuiStatus::Ok;
}

void ImGuiApp::prepare_shutdown_wait()
{
    m_workQueue.stop(true);
}

void ImGuiApp::prepare_shutdown_nowait()
{
    m_workQueue.stop(false);
}

bool ImGuiApp::can_shutdown() const
{
    return !m_workQueue.is_busy();
}

void ImGuiApp::shutdown()
{
    assert(can_shutdown());
    ImGui::DestroyContext();
}

void ClampImGuiWindowToClientArea(ImVec2 &position, ImVec2 &size, const ImVec2 &clientPos, const ImVec2 &clientSize)
{
    // Clamp the position to ensure it stays within the client area
    if (position.x < clientPos.x)
        position.x = clientPos.x;
    if (position.y < clientPos.y)
        position.y = clientPos.y;

    if ((position.x + size.x) > (clientPos.x + clientSize.x))
        position.x = clientPos.x + clientSize.x - size.x;

    if ((position.y + size.y) > (clientPos.y + clientSize.y))
        position.y = clientPos.y + clientSize.y - size.y;
}

ImGuiStatus ImGuiApp::update()
{
    m_workQueue.update_callbacks();

    ImGui::NewFrame();

    update_app();

#if !RELEASE
    if (m_showDemoWindow)
    {
        // Show the big demo window.
        // Most of the sample code is in ImGui::ShowDemoWindow()!
        // You can browse its code to learn more about Dear ImGui!
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
#endif

    ImGui::EndFrame();

    return ImGuiStatus::Ok;
}

void ImGuiApp::update_app()
{
    BackgroundWindow();

    if (m_showFileManager)
        FileManagerWindow(&m_showFileManager);

    if (m_showAsmOutputManager)
        AsmOutputManagerWindow(&m_showAsmOutputManager);

    AsmComparisonManagerWindows();
}

WorkQueueCommandPtr ImGuiApp::create_load_exe_command(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_load_exe());

    if (descriptor->pdbReader == nullptr)
    {
        descriptor->exeFilenameFromPdb.clear();
    }

    const std::string exe_filename = descriptor->evaluate_exe_filename();
    auto command = std::make_unique<AsyncLoadExeCommand>(LoadExeOptions(exe_filename));

    command->options.config_file = descriptor->evaluate_exe_config_filename();
    command->options.pdb_reader = descriptor->pdbReader.get();
    command->callback = [descriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncLoadExeResult *>(result.get());
        descriptor->executable = std::move(res->executable);
        descriptor->exeLoadTimepoint = std::chrono::system_clock::now();
        descriptor->invalidate_command_id();
    };

    descriptor->exeSymbolsDescriptor.reset();
    descriptor->executable.reset();
    descriptor->exeLoadTimepoint = InvalidTimePoint;
    descriptor->exeSaveConfigFilename.clear();
    descriptor->exeSaveConfigTimepoint = InvalidTimePoint;
    descriptor->activeCommandId = command->command_id;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_load_pdb_command(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_load_pdb());

    auto command = std::make_unique<AsyncLoadPdbCommand>(LoadPdbOptions(descriptor->pdbFilename));

    command->callback = [descriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncLoadPdbResult *>(result.get());
        descriptor->pdbReader = std::move(res->pdbReader);
        descriptor->pdbLoadTimepoint = std::chrono::system_clock::now();
        descriptor->invalidate_command_id();
    };

    descriptor->pdbSymbolsDescriptor.reset();
    descriptor->pdbFunctionsDescriptor.reset();
    descriptor->pdbReader.reset();
    descriptor->pdbLoadTimepoint = InvalidTimePoint;
    descriptor->pdbSaveConfigFilename.clear();
    descriptor->pdbSaveConfigTimepoint = InvalidTimePoint;
    descriptor->exeFilenameFromPdb.clear();
    descriptor->activeCommandId = command->command_id;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_save_exe_config_command(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_save_exe_config());

    const std::string config_filename = descriptor->evaluate_exe_config_filename();
    auto command =
        std::make_unique<AsyncSaveExeConfigCommand>(SaveExeConfigOptions(*descriptor->executable, config_filename));

    command->callback = [descriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncSaveExeConfigResult *>(result.get());
        if (res->success)
        {
            auto com = static_cast<AsyncSaveExeConfigCommand *>(result->command.get());
            descriptor->exeSaveConfigFilename = util::abs_path(com->options.config_file);
            descriptor->exeSaveConfigTimepoint = std::chrono::system_clock::now();
        }
        descriptor->invalidate_command_id();
    };

    descriptor->exeSaveConfigFilename.clear();
    descriptor->exeSaveConfigTimepoint = InvalidTimePoint;
    descriptor->activeCommandId = command->command_id;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_save_pdb_config_command(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_save_pdb_config());

    const std::string config_filename = descriptor->evaluate_pdb_config_filename();
    auto command =
        std::make_unique<AsyncSavePdbConfigCommand>(SavePdbConfigOptions(*descriptor->pdbReader, config_filename));

    command->callback = [descriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncSavePdbConfigResult *>(result.get());
        if (res->success)
        {
            auto com = static_cast<AsyncSavePdbConfigCommand *>(result->command.get());
            descriptor->pdbSaveConfigFilename = util::abs_path(com->options.config_file);
            descriptor->pdbSaveConfigTimepoint = std::chrono::system_clock::now();
        }
        descriptor->invalidate_command_id();
    };

    descriptor->pdbSaveConfigFilename.clear();
    descriptor->pdbSaveConfigTimepoint = InvalidTimePoint;
    descriptor->activeCommandId = command->command_id;

    return command;
}

void ImGuiApp::load_async(ProgramFileDescriptor *descriptor)
{
    if (descriptor->can_load_pdb())
    {
        load_pdb_and_exe_async(descriptor);
    }
    else if (descriptor->can_load_exe())
    {
        load_exe_async(descriptor);
    }
    else
    {
        // Cannot load undefined file.
        assert(false);
    }
}

void ImGuiApp::load_exe_async(ProgramFileDescriptor *descriptor)
{
    m_workQueue.enqueue(create_load_exe_command(descriptor));
}

void ImGuiApp::load_pdb_and_exe_async(ProgramFileDescriptor *descriptor)
{
    auto command = create_load_pdb_command(descriptor);

    command->chain([descriptor](WorkQueueResultPtr &result) {
        if (descriptor->pdbReader == nullptr)
            return WorkQueueCommandPtr();

        const unassemblize::PdbExeInfo &exe_info = descriptor->pdbReader->get_exe_info();
        descriptor->exeFilenameFromPdb = unassemblize::Runner::create_exe_filename(exe_info);

        if (!descriptor->can_load_exe())
            return WorkQueueCommandPtr();

        return create_load_exe_command(descriptor);
    });

    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::save_config_async(ProgramFileDescriptor *descriptor)
{
    WorkQueueDelayedCommand head_command;
    WorkQueueDelayedCommand *next_command = &head_command;

    if (descriptor->can_save_exe_config())
    {
        next_command = next_command->chain(
            [descriptor](WorkQueueResultPtr &result) { return create_save_exe_config_command(descriptor); });
    }

    if (descriptor->can_save_pdb_config())
    {
        next_command = next_command->chain(
            [descriptor](WorkQueueResultPtr &result) { return create_save_pdb_config_command(descriptor); });
    }

    assert(head_command.next_delayed_command != nullptr);

    m_workQueue.enqueue(head_command);
}

void ImGuiApp::add_file()
{
    m_programFiles.emplace_back(std::make_unique<ProgramFileDescriptor>());
}

void ImGuiApp::remove_file(size_t idx)
{
    if (idx < m_programFiles.size())
    {
        m_programFiles.erase(m_programFiles.begin() + idx);
    }
}

void ImGuiApp::remove_all_files()
{
    m_programFiles.clear();
}

void ImGuiApp::add_asm_comparison()
{
    m_asmComparisons.emplace_back(std::make_unique<AsmComparisonDescriptor>());
}

void ImGuiApp::remove_closed_asm_comparisons()
{
    // Remove descriptor when window was closed.
    m_asmComparisons.erase(
        std::remove_if(
            m_asmComparisons.begin(),
            m_asmComparisons.end(),
            [](const AsmComparisonDescriptorPtr &p) { return !p->has_open_window; }),
        m_asmComparisons.end());
}

std::string ImGuiApp::create_section_string(uint32_t section_index, const ExeSections *sections)
{
    if (sections != nullptr && section_index < sections->size())
    {
        return (*sections)[section_index].name;
    }
    else
    {
        return fmt::format("{:d}", section_index + 1);
    }
}

std::string ImGuiApp::create_time_string(std::chrono::time_point<std::chrono::system_clock> time_point)
{
    if (time_point == InvalidTimePoint)
        return std::string();

    std::time_t time = std::chrono::system_clock::to_time_t(time_point);
    std::tm *local_time = std::localtime(&time);

    char buffer[32];
    if (0 == std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time))
        return std::string();

    return std::string(buffer);
}

void ImGuiApp::BackgroundWindow()
{
    // clang-format off
    constexpr ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;
    // clang-format on

    bool window_open = true;
    ImScoped::Window window("main", &window_open, window_flags);
    if (window.IsContentVisible)
    {
        ImGui::SetWindowPos("main", m_windowPos);
        ImGui::SetWindowSize("main", ImVec2(m_windowSize.x, 0.f));

        ImScoped::MenuBar menu_bar;
        if (menu_bar.IsOpen)
        {
            {
                ImScoped::Menu menu("File");
                if (menu.IsOpen)
                {
                    if (ImGui::MenuItem("Exit"))
                    {
                        // Will wait for all work to finish and then shutdown the app.
                        prepare_shutdown_nowait();
                    }
                    ImGui::SameLine();
                    TooltipTextUnformattedMarker("Graceful shutdown. Finishes all tasks before exiting.");
                }
            }

            {
                ImScoped::Menu menu("Tools");
                if (menu.IsOpen)
                {
                    ImGui::MenuItem("Program File Manager", nullptr, &m_showFileManager);
                    ImGui::MenuItem("Assembler Output", nullptr, &m_showAsmOutputManager);

                    if (ImGui::MenuItem("New Assembler Comparison"))
                    {
                        add_asm_comparison();
                    }
                    ImGui::SameLine();
                    TooltipTextUnformattedMarker("Opens a new Assembler Comparison window.");
                }
            }
        }
    }
}

void ImGuiApp::FileManagerWindow(bool *p_open)
{
    ImScoped::Window window("File Manager", p_open, ImGuiWindowFlags_MenuBar);
    if (window.IsContentVisible)
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Add File"))
                {
                    add_file();
                }
                if (ImGui::MenuItem("Remove All Files"))
                {
                    remove_all_files();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Show Tabs", nullptr, &m_showFileManagerWithTabs);
                ImGui::MenuItem("Show Exe Section Info", nullptr, &m_showFileManagerExeSectionInfo);
                ImGui::MenuItem("Show Exe Symbol Info", nullptr, &m_showFileManagerExeSymbolInfo);
                ImGui::MenuItem("Show Pdb Compiland Info", nullptr, &m_showFileManagerPdbCompilandInfo);
                ImGui::MenuItem("Show Pdb Source File Info", nullptr, &m_showFileManagerPdbSourceFileInfo);
                ImGui::MenuItem("Show Pdb Symbol Info", nullptr, &m_showFileManagerPdbSymbolInfo);
                ImGui::MenuItem("Show Pdb Function Info", nullptr, &m_showFileManagerPdbFunctionInfo);
                ImGui::MenuItem("Show Pdb Exe Info", nullptr, &m_showFileManagerPdbExeInfo);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        FileManagerBody();
    }
}

void ImGuiApp::AsmOutputManagerWindow(bool *p_open)
{
    ImScoped::Window window("Assembler Output Manager", p_open);
    if (window.IsContentVisible)
    {
        AsmOutputManagerBody();
    }
}

void ImGuiApp::AsmComparisonManagerWindows()
{
    const size_t count = m_asmComparisons.size();
    for (size_t i = 0; i < count; ++i)
    {
        AsmComparisonDescriptor &descriptor = *m_asmComparisons[i];

        const std::string title = fmt::format("Assembler Comparison {:d}", descriptor.id);

        ImScoped::Window window(title.c_str(), &descriptor.has_open_window);
        ImScoped::ID id(i);
        if (window.IsContentVisible && descriptor.has_open_window)
        {
            AsmComparisonManagerBody(descriptor);
        }
    }

    remove_closed_asm_comparisons();
}

namespace
{
const std::string_view g_browse_file_button_label = "Browse ..";
const std::string g_select_file_dialog_title = "Select File";
} // namespace

void ImGuiApp::FileManagerBody()
{
    FileManagerGlobalButtons();

    ImGui::SeparatorText("File List");

    size_t erase_idx = size_t(~0);

    bool show_files = !m_programFiles.empty();

    if (show_files && m_showFileManagerWithTabs)
    {
        show_files = ImGui::BeginTabBar("file_tabs");
    }

    if (show_files)
    {
        for (size_t i = 0; i < m_programFiles.size(); ++i)
        {
            ProgramFileDescriptor &descriptor = *m_programFiles[i];

            // Set a unique ID for each element to avoid ID conflicts
            ImScoped::ID id(i);

            bool is_open = false;

            if (m_showFileManagerWithTabs)
            {
                // Tab items cannot have dynamic labels without bugs. Force consistent names.
                const std::string title = descriptor.create_descriptor_name();
                is_open = ImGui::BeginTabItem(title.c_str());
                // Tooltip on hover tab.
                const std::string exe_name = descriptor.create_short_exe_name();
                if (!exe_name.empty())
                    TooltipTextUnformatted(exe_name);
            }
            else
            {
                const std::string title = descriptor.create_descriptor_name_with_short_exe_name();
                is_open = ImGui::TreeNodeEx("file_tree", ImGuiTreeNodeFlags_DefaultOpen, title.c_str());
            }

            if (is_open)
            {
                bool erased;
                FileManagerDescriptor(descriptor, erased);
                if (erased)
                    erase_idx = i;

                if (m_showFileManagerWithTabs)
                {
                    ImGui::EndTabItem();
                }
                else
                {
                    ImGui::TreePop();
                }
            }
        }

        if (m_showFileManagerWithTabs)
        {
            ImGui::EndTabBar();
        }

        // Erase at the end to avoid incomplete elements.
        remove_file(erase_idx);
    }
}

void ImGuiApp::FileManagerDescriptor(ProgramFileDescriptor &descriptor, bool &erased)
{
    {
        ImScoped::Group group;
        ImScoped::Disabled disabled(descriptor.has_active_command());
        ImScoped::ItemWidth item_width(ImGui::GetFontSize() * -12);

        FileManagerDescriptorExeFile(descriptor);

        FileManagerDescriptorExeConfig(descriptor);

        FileManagerDescriptorPdbFile(descriptor);

        FileManagerDescriptorPdbConfig(descriptor);

        FileManagerDescriptorActions(descriptor, erased);
    }

    // Draw command overlay
    if (descriptor.has_active_command())
    {
        ImRect group_rect;
        group_rect.Min = ImGui::GetItemRectMin();
        group_rect.Max = ImGui::GetItemRectMax();

        const std::string overlay = fmt::format("Processing command {:d} ..", descriptor.activeCommandId);

        OverlayProgressBar(group_rect, -1.0f * (float)ImGui::GetTime(), overlay.c_str());
    }

    FileManagerDescriptorSaveLoadStatus(descriptor);

    // Draw some details
    if (descriptor.executable != nullptr || descriptor.pdbReader != nullptr)
    {
        ImScoped::TreeNode tree("Info");
        if (tree.IsOpen)
        {
            FileManagerInfo(descriptor);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
}

void ImGuiApp::FileManagerDescriptorExeFile(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.exeFilename,
        g_browse_file_button_label,
        fmt::format("exe_file_dialog{:d}", descriptor.id),
        g_select_file_dialog_title,
        "Program (*.*){((.*))}"); // ((.*)) is regex for all files

    ImGui::SameLine();
    ImGui::InputTextWithHint("Program File", auto_str, &descriptor.exeFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.exeFilename) && !descriptor.has_active_command())
    {
        const std::string exe_filename = descriptor.evaluate_exe_filename();
        if (exe_filename.empty())
        {
            TooltipText("'%s' evaluates when the Pdb file is read", auto_str);
        }
        else
        {
            TooltipText("'%s' evaluates to '%s'", auto_str, exe_filename.c_str());
        }
    }
}

void ImGuiApp::FileManagerDescriptorExeConfig(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.exeConfigFilename,
        g_browse_file_button_label,
        fmt::format("exe_config_file_dialog{:d}", descriptor.id),
        g_select_file_dialog_title,
        "Config (*.json){.json}");

    ImGui::SameLine();
    ImGui::InputTextWithHint("Program Config File", auto_str, &descriptor.exeConfigFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.exeConfigFilename) && !descriptor.exeFilename.empty() && !descriptor.has_active_command())
    {
        const std::string exe_filename = descriptor.evaluate_exe_filename();
        if (exe_filename.empty())
        {
            TooltipText("'%s' evaluates when the Pdb file is read", auto_str);
        }
        else
        {
            const std::string config_filename = descriptor.evaluate_exe_config_filename();
            TooltipText("'%s' evaluates to '%s'", auto_str, config_filename.c_str());
        }
    }
}

void ImGuiApp::FileManagerDescriptorPdbFile(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.pdbFilename,
        g_browse_file_button_label,
        fmt::format("pdb_file_dialog{:d}", descriptor.id),
        g_select_file_dialog_title,
        "Program Database (*.pdb){.pdb}");

    ImGui::SameLine();
    ImGui::InputText("Pdb File", &descriptor.pdbFilename);
}

void ImGuiApp::FileManagerDescriptorPdbConfig(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.pdbConfigFilename,
        g_browse_file_button_label,
        fmt::format("pdb_config_file_dialog{:d}", descriptor.id),
        g_select_file_dialog_title,
        "Config (*.json){.json}");

    ImGui::SameLine();
    ImGui::InputTextWithHint("Pdb Config File", auto_str, &descriptor.pdbConfigFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.pdbConfigFilename) && !descriptor.pdbFilename.empty())
    {
        const std::string config_filename = descriptor.evaluate_pdb_config_filename();
        TooltipText("'%s' evaluates to '%s'", auto_str, config_filename.c_str());
    }
}

void ImGuiApp::FileManagerDescriptorActions(ProgramFileDescriptor &descriptor, bool &erased)
{
    // Action buttons
    {
        // Change button color.
        ImScoped::StyleColor color1(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.2f));
        ImScoped::StyleColor color2(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
        ImScoped::StyleColor color3(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.4f));
        // Change text color too to make it readable with the light ImGui color theme.
        ImScoped::StyleColor text_color1(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImScoped::StyleColor text_color2(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        erased = ImGui::Button("Remove");

        // #TODO: Guard this button press with a confirmation dialog ?
        // There is an example for this in "Dear ImGui Demo" > "Popups & Modal windows" > "Modals".
    }

    ImGui::SameLine();
    {
        ImScoped::Disabled disabled(!descriptor.can_load());
        // Change button color.
        ImScoped::StyleColor color1(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.3f, 0.6f, 0.6f));
        ImScoped::StyleColor color2(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.3f, 0.8f, 0.8f));
        ImScoped::StyleColor color3(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.3f, 1.0f, 1.0f));
        // Change text color too to make it readable with the light ImGui color theme.
        ImScoped::StyleColor text_color1(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImScoped::StyleColor text_color2(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        if (ImGui::Button("Load"))
        {
            load_async(&descriptor);
        }
    }
    ImGui::SameLine();
    {
        ImScoped::Disabled disabled(!descriptor.can_save_config());
        // Change button color.
        ImScoped::StyleColor color1(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImScoped::StyleColor color2(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        ImScoped::StyleColor color3(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 1.0f, 1.0f));
        // Change text color too to make it readable with the light ImGui color theme.
        ImScoped::StyleColor text_color1(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImScoped::StyleColor text_color2(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        if (ImGui::Button("Save Config"))
        {
            save_config_async(&descriptor);
        }
    }
}

void ImGuiApp::FileManagerDescriptorSaveLoadStatus(const ProgramFileDescriptor &descriptor)
{
    constexpr ImU32 green = IM_COL32(0, 255, 0, 255);

    if (descriptor.executable != nullptr)
    {
        DrawInTextCircle(green);
        ImGui::Text(
            " Loaded Exe: [%s] %s",
            create_time_string(descriptor.exeLoadTimepoint).c_str(),
            descriptor.executable->get_filename().c_str());
    }

    if (descriptor.pdbReader != nullptr)
    {
        DrawInTextCircle(green);
        ImGui::Text(
            " Loaded Pdb: [%s] %s",
            create_time_string(descriptor.pdbLoadTimepoint).c_str(),
            descriptor.pdbReader->get_filename().c_str());
    }

    if (descriptor.exeSaveConfigTimepoint != InvalidTimePoint)
    {
        DrawInTextCircle(green);
        ImGui::Text(
            " Saved Exe Config: [%s] %s",
            create_time_string(descriptor.exeSaveConfigTimepoint).c_str(),
            descriptor.exeSaveConfigFilename.c_str());
    }

    if (descriptor.pdbSaveConfigTimepoint != InvalidTimePoint)
    {
        DrawInTextCircle(green);
        ImGui::Text(
            " Saved Pdb Config: [%s] %s",
            create_time_string(descriptor.pdbSaveConfigTimepoint).c_str(),
            descriptor.pdbSaveConfigFilename.c_str());
    }
    // #TODO: Also draw fail status.
}

void ImGuiApp::FileManagerGlobalButtons()
{
    if (ImGui::Button("Add File"))
    {
        add_file();
    }

    ImGui::SameLine();
    {
        const bool can_load_any =
            std::any_of(m_programFiles.begin(), m_programFiles.end(), [](const ProgramFileDescriptorPtr &descriptor) {
                return descriptor->can_load();
            });
        ImScoped::Disabled disabled(!can_load_any);
        // Change button color.
        ImScoped::StyleColor color1(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.3f, 0.6f, 0.6f));
        ImScoped::StyleColor color2(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.3f, 0.8f, 0.8f));
        ImScoped::StyleColor color3(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.3f, 1.0f, 1.0f));
        // Change text color too to make it readable with the light ImGui color theme.
        ImScoped::StyleColor text_color1(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImScoped::StyleColor text_color2(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        if (ImGui::Button("Load All"))
        {
            for (ProgramFileDescriptorPtr &descriptor : m_programFiles)
            {
                load_async(descriptor.get());
            }
        }
    }
}

void ImGuiApp::FileManagerInfo(ProgramFileDescriptor &descriptor)
{
    if (descriptor.executable != nullptr)
    {
        if (m_showFileManagerExeSectionInfo)
            FileManagerInfoExeSections(descriptor);

        if (m_showFileManagerExeSymbolInfo)
            FileManagerInfoExeSymbols(descriptor);
    }
    if (descriptor.pdbReader != nullptr)
    {
        if (m_showFileManagerPdbCompilandInfo)
            FileManagerInfoPdbCompilands(descriptor);

        if (m_showFileManagerPdbSourceFileInfo)
            FileManagerInfoPdbSourceFiles(descriptor);

        if (m_showFileManagerPdbSymbolInfo)
            FileManagerInfoPdbSymbols(descriptor);

        if (m_showFileManagerPdbFunctionInfo)
            FileManagerInfoPdbFunctions(descriptor);

        if (m_showFileManagerPdbExeInfo)
            FileManagerInfoPdbExeInfo(descriptor);
    }
}

void ImGuiApp::FileManagerInfoExeSections(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Exe Sections");

    ImGui::Text("Exe Image base: x%08x", down_cast<uint32_t>(descriptor.executable->image_base()));

    const ExeSections &sections = descriptor.executable->get_sections();

    ImGui::Text("Count: %zu", sections.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, sections.size() + 1);

    ImScoped::Child child("exe_sections_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("exe_sections", 3, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Name");
            ImGui::TableHeadersRow();

            for (const ExeSectionInfo &section : sections)
            {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("x%08x", down_cast<uint32_t>(section.address));

                ImGui::TableNextColumn();
                ImGui::Text("x%08x", down_cast<uint32_t>(section.size));

                ImGui::TableNextColumn();
                TextUnformatted(section.name);
            }
            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoExeSymbols(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Exe Symbols");

    const auto &filtered = descriptor.exeSymbolsDescriptor.filtered;
    {
        const ExeSymbols &symbols = descriptor.executable->get_symbols();

        UpdateFilter(
            descriptor.exeSymbolsDescriptor,
            symbols,
            [](const ImGuiTextFilterEx &filter, const ExeSymbol &symbol) -> bool { return filter.PassFilter(symbol.name); });

        ImGui::Text("Count: %zu, Filtered: %d", symbols.size(), filtered.size());
    }

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, filtered.size() + 1);

    ImScoped::Child child("exe_symbols_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("exe_symbols", 3, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Name");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(filtered.size());

            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
                {
                    const ExeSymbol &symbol = *filtered[n];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", down_cast<uint32_t>(symbol.address));

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", down_cast<uint32_t>(symbol.size));

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.name);
                }
            }

            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoPdbCompilands(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Compilands");

    const PdbCompilandInfoVector &compilands = descriptor.pdbReader->get_compilands();

    ImGui::Text("Count: %zu", compilands.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, compilands.size() + 1);

    ImScoped::Child child("pdb_compilands_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_compilands", 1, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Name");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(down_cast<int>(compilands.size()));

            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
                {
                    const PdbCompilandInfo &compiland = compilands[n];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    TextUnformatted(compiland.name);
                }
            }

            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoPdbSourceFiles(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Source Files");

    const PdbSourceFileInfoVector &source_files = descriptor.pdbReader->get_source_files();

    ImGui::Text("Count: %zu", source_files.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, source_files.size() + 1);

    ImScoped::Child child("pdb_source_files_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_source_files", 3, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Checksum Type");
            ImGui::TableSetupColumn("Checksum");
            ImGui::TableSetupColumn("Name");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(down_cast<int>(source_files.size()));

            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
                {
                    const PdbSourceFileInfo &source_file = source_files[n];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    switch (source_file.checksumType)
                    {
                        default:
                        case CV_Chksum::CHKSUM_TYPE_NONE:
                            TextUnformatted("none");
                            break;
                        case CV_Chksum::CHKSUM_TYPE_MD5:
                            TextUnformatted("md5");
                            break;
                        case CV_Chksum::CHKSUM_TYPE_SHA1:
                            TextUnformatted("sha1");
                            break;
                        case CV_Chksum::CHKSUM_TYPE_SHA_256:
                            TextUnformatted("sha256");
                            break;
                    }

                    const std::string checksum = util::to_hex_string(source_file.checksum); // #TODO: Cache this

                    ImGui::TableNextColumn();
                    TextUnformatted(checksum);

                    ImGui::TableNextColumn();
                    TextUnformatted(source_file.name);
                }
            }

            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoPdbSymbols(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Symbols");

    const auto &filtered = descriptor.pdbSymbolsDescriptor.filtered;
    {
        const PdbSymbolInfoVector &symbols = descriptor.pdbReader->get_symbols();

        UpdateFilter(
            descriptor.pdbSymbolsDescriptor,
            symbols,
            [](const ImGuiTextFilterEx &filter, const PdbSymbolInfo &symbol) -> bool {
                if (filter.PassFilter(symbol.decoratedName))
                    return true;
                if (filter.PassFilter(symbol.undecoratedName))
                    return true;
                if (filter.PassFilter(symbol.globalName))
                    return true;
                return false;
            });

        ImGui::Text("Count: %zu, Filtered: %d", symbols.size(), filtered.size());
    }

    const ExeSections *sections = nullptr;
    if (descriptor.executable != nullptr)
        sections = &descriptor.executable->get_sections();

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, filtered.size() + 1);

    ImScoped::Child child("pdb_symbols_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_symbols", 6, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Section");
            ImGui::TableSetupColumn("Decorated Name");
            ImGui::TableSetupColumn("Undecorated Name");
            ImGui::TableSetupColumn("Global Name");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(filtered.size());

            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
                {
                    const PdbSymbolInfo &symbol = *filtered[n];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", down_cast<uint32_t>(symbol.address.absVirtual));

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", symbol.length);

                    ImGui::TableNextColumn();
                    std::string section = create_section_string(symbol.address.section_as_index(), sections);
                    TextUnformatted(section);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.decoratedName);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.undecoratedName);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.globalName);
                }
            }

            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoPdbFunctions(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Functions");

    const auto &filtered = descriptor.pdbFunctionsDescriptor.filtered;
    {
        const PdbFunctionInfoVector &functions = descriptor.pdbReader->get_functions();

        UpdateFilter(
            descriptor.pdbFunctionsDescriptor,
            functions,
            [](const ImGuiTextFilterEx &filter, const PdbFunctionInfo &function) -> bool {
                if (filter.PassFilter(function.decoratedName))
                    return true;
                if (filter.PassFilter(function.undecoratedName))
                    return true;
                if (filter.PassFilter(function.globalName))
                    return true;
                return false;
            });

        ImGui::Text("Count: %zu, Filtered: %d", functions.size(), filtered.size());
    }

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, filtered.size() + 1);

    ImScoped::Child child("pdb_functions_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_functions", 5, s_fileManagerInfoTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Makes top row always visible.
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Decorated Name");
            ImGui::TableSetupColumn("Undecorated Name");
            ImGui::TableSetupColumn("Global Name");
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(filtered.size());

            while (clipper.Step())
            {
                for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
                {
                    const PdbFunctionInfo &symbol = *filtered[n];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", down_cast<uint32_t>(symbol.address.absVirtual));

                    ImGui::TableNextColumn();
                    ImGui::Text("x%08x", symbol.length);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.decoratedName);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.undecoratedName);

                    ImGui::TableNextColumn();
                    TextUnformatted(symbol.globalName);
                }
            }

            ImGui::EndTable();
        }
    }
}

void ImGuiApp::FileManagerInfoPdbExeInfo(ProgramFileDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Exe Info");

    const PdbExeInfo &exe_info = descriptor.pdbReader->get_exe_info();
    ImGui::Text("Exe File Name: %s", exe_info.exeFileName.c_str());
    ImGui::Text("Pdb File Path: %s", exe_info.pdbFilePath.c_str());
}

void ImGuiApp::AsmOutputManagerBody()
{
    // #TODO implement.

    ImGui::TextUnformatted("Not implemented");
}

void ImGuiApp::AsmComparisonManagerBody(AsmComparisonDescriptor &descriptor)
{
}

} // namespace unassemblize::gui
