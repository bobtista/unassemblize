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
#include "options.h"
#include "util.h"
#include "utility/imgui_scoped.h"
#include <ImGuiFileDialog.h>
#include <fmt/core.h>
#include <misc/cpp/imgui_stdlib.h>

namespace unassemblize::gui
{
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
    bool open = true;
    // clang-format off
    const int flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;
    // clang-format on
    ImGui::Begin("main", &open, flags);
    ImGui::SetWindowPos("main", m_windowPos);
    ImGui::SetWindowSize("main", ImVec2(m_windowSize.x, 0.f));

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                // Will wait for all work to finish and then shutdown the app.
                prepare_shutdown_nowait();
            }
            ImGui::SameLine();
            TooltipTextUnformattedMarker("Graceful shutdown. Finishes all tasks before exiting.");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Program File Manager", nullptr, &m_showProgramFileManager);
            ImGui::MenuItem("Assembler Output", nullptr, &m_showAsmOutputManager);
            ImGui::MenuItem("Assembler Comparison", nullptr, &m_showAsmComparisonManager);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (m_showProgramFileManager)
        FileManagerWindow(&m_showProgramFileManager);

    if (m_showAsmOutputManager)
        AsmOutputManagerWindow(&m_showAsmOutputManager);

    if (m_showAsmComparisonManager)
        AsmComparisonManagerWindow(&m_showAsmComparisonManager);

    ImGui::End();
}

void ImGuiApp::load_async(ProgramFileDescriptor *descriptor)
{
    if (descriptor->can_load_pdb())
    {
        load_pdb_async(descriptor);
    }
    else if (descriptor->can_load_exe())
    {
        load_exe_async(descriptor);
    }
    else
    {
        assert("Cannot load undefined file");
    }
}

void ImGuiApp::load_exe_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_load_exe());

    if (descriptor->pdbReader == nullptr)
    {
        descriptor->exeFilenameFromPdb.clear();
    }

    const std::string exe_filename = descriptor->evaluate_exe_filename();
    auto command = std::make_unique<WorkQueueCommandLoadExe>(exe_filename);
    command->options.config_file = descriptor->evaluate_exe_config_filename();
    command->options.pdb_reader = descriptor->pdbReader.get();
    command->callback = [descriptor](WorkQueueResultPtr &result) {
        assert(result->type() == WorkCommandType::LoadExe);
        auto res = static_cast<WorkQueueResultLoadExe *>(result.get());
        descriptor->executable = std::move(res->executable);
        descriptor->invalidate_command_id();
        result.reset();
    };

    descriptor->executable.reset();
    descriptor->activeCommandId = command->command_id;
    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::load_pdb_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_load_pdb());

    auto command = std::make_unique<WorkQueueCommandLoadPdb>(descriptor->pdbFilename);
    command->callback = [this, descriptor](WorkQueueResultPtr &result) {
        assert(result->type() == WorkCommandType::LoadPdb);
        auto res = static_cast<WorkQueueResultLoadPdb *>(result.get());
        descriptor->pdbReader = std::move(res->pdbReader);
        result.reset();

        if (descriptor->pdbReader != nullptr)
        {
            // Chain load executable when Pdb load is done.
            const unassemblize::PdbExeInfo &exe_info = descriptor->pdbReader->get_exe_info();
            descriptor->exeFilenameFromPdb = unassemblize::Runner::create_exe_filename(exe_info);

            if (descriptor->can_load_exe())
            {
                load_exe_async(descriptor);
            }
        }
        else
        {
            descriptor->invalidate_command_id();
        }
    };

    descriptor->pdbReader.reset();
    descriptor->exeFilenameFromPdb.clear();
    descriptor->activeCommandId = command->command_id;
    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::save_config_async(ProgramFileDescriptor *descriptor)
{
    if (descriptor->can_save_pdb_config())
    {
        save_pdb_config_async(descriptor);
    }
    else if (descriptor->can_save_exe_config())
    {
        save_exe_config_async(descriptor);
    }
    else
    {
        assert("Cannot save undefined config file");
    }
}

void ImGuiApp::save_exe_config_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_save_exe_config());

    const std::string config_filename = descriptor->evaluate_exe_config_filename();
    auto command = std::make_unique<WorkQueueCommandSaveExeConfig>(descriptor->executable.get(), config_filename);
    command->callback = [descriptor](WorkQueueResultPtr &result) {
        assert(result->type() == WorkCommandType::SaveExeConfig);
        auto res = static_cast<WorkQueueResultSaveExeConfig *>(result.get());
        // #TODO: Use result.
        descriptor->invalidate_command_id();
        result.reset();
    };

    descriptor->activeCommandId = command->command_id;
    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::save_pdb_config_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor->can_save_pdb_config());

    const std::string config_filename = descriptor->evaluate_pdb_config_filename();
    auto command = std::make_unique<WorkQueueCommandSavePdbConfig>(descriptor->pdbReader.get(), config_filename);
    command->callback = [this, descriptor](WorkQueueResultPtr &result) {
        assert(result->type() == WorkCommandType::SavePdbConfig);
        auto res = static_cast<WorkQueueResultSavePdbConfig *>(result.get());
        // #TODO: Use result.
        result.reset();

        if (descriptor->can_save_exe_config())
        {
            // Chain save next config file when this one is done,
            // because config file contents can be saved into the same file.
            save_exe_config_async(descriptor);
        }
        else
        {
            descriptor->invalidate_command_id();
        }
    };

    descriptor->activeCommandId = command->command_id;
    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::TooltipText(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TooltipTextV(fmt, args);
    va_end(args);
}

void ImGuiApp::TooltipTextV(const char *fmt, va_list args)
{
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextV(fmt, args);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ImGuiApp::TooltipTextUnformatted(const char *text, const char *text_end)
{
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text, text_end);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ImGuiApp::TooltipTextMarker(const char *fmt, ...)
{
    ImGui::TextDisabled("(?)");
    va_list args;
    va_start(args, fmt);
    TooltipTextV(fmt, args);
    va_end(args);
}

void ImGuiApp::TooltipTextUnformattedMarker(const char *text, const char *text_end)
{
    ImGui::TextDisabled("(?)");
    TooltipTextUnformatted(text, text_end);
}

void ImGuiApp::OverlayProgressBar(const ImRect &rect, float fraction, const char *overlay)
{
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    // Define a translucent overlay color
    const ImVec4 dimBgVec4 = ImGui::GetStyleColorVec4(ImGuiCol_ModalWindowDimBg);
    const ImU32 dimBgU32 = ImGui::ColorConvertFloat4ToU32(dimBgVec4);

    // Draw a filled rectangle over the group area
    drawList->AddRectFilled(rect.Min, rect.Max, dimBgU32);

    // Calculate the center position for the ImGui ProgressBar
    ImVec2 center = ImVec2((rect.Min.x + rect.Max.x) * 0.5f, (rect.Min.y + rect.Max.y) * 0.5f);
    ImVec2 progressBarSize = ImVec2(rect.Max.x - rect.Min.x, 20.0f);

    // Set cursor position to the center of the overlay rectangle
    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(center.x - progressBarSize.x * 0.5f, center.y - progressBarSize.y * 0.5f));

    {
        // Set a custom background color with transparency
        ImScoped::StyleColor frameBg(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));

        // Render the ImGui progress bar
        ImGui::ProgressBar(fraction, progressBarSize, overlay);
    }

    // Set cursor position back
    ImGui::SetCursorScreenPos(cursorScreenPos);
}

void ImGuiApp::ApplyPlacementToNextWindow(WindowPlacement &placement)
{
    if (placement.pos.x != -FLT_MAX)
    {
        ImGui::SetNextWindowPos(placement.pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(placement.size, ImGuiCond_Always);
    }
}

void ImGuiApp::FetchPlacementFromWindowByName(WindowPlacement &placement, const char *window_name)
{
    // Note: Is using internals of ImGui.
    if (const ImGuiWindow *window = ImGui::FindWindowByName(window_name))
    {
        placement.pos = window->Pos;
        placement.size = window->Size;
    }
}

void ImGuiApp::AddFileDialogButton(
    std::string *filePathName,
    const char *button_label,
    const std::string &vKey,
    const std::string &vTitle,
    const char *vFilters)
{
    IGFD::FileDialog *instance = ImGuiFileDialog::Instance();

    const std::string button_label_key = fmt::format("{:s}##{:s}", button_label, vKey);
    if (ImGui::Button(button_label_key.c_str()))
    {
        // Restore position and size of any last file dialog.
        ApplyPlacementToNextWindow(m_lastFileDialogPlacement);

        IGFD::FileDialogConfig config;
        config.path = ".";
        config.flags = ImGuiFileDialogFlags_Modal;
        instance->OpenDialog(vKey, vTitle, vFilters, config);
    }

    if (instance->Display(vKey, ImGuiWindowFlags_NoCollapse, ImVec2(400, 200)))
    {
        // Note: Is using internals of ImGuiFileDialog
        const std::string window_name = vTitle + "##" + vKey;
        FetchPlacementFromWindowByName(m_lastFileDialogPlacement, window_name.c_str());

        if (instance->IsOk())
        {
            *filePathName = instance->GetFilePathName();
        }
        instance->Close();
    }
}

void ImGuiApp::FileManagerWindow(bool *p_open)
{
    ImGui::Begin("File Manager", p_open);
    FileManagerBody();
    ImGui::End();
}

void ImGuiApp::AsmOutputManagerWindow(bool *p_open)
{
    ImGui::Begin("Assembler Output Manager", p_open);
    AsmOutputManagerBody();
    ImGui::End();
}

void ImGuiApp::AsmComparisonManagerWindow(bool *p_open)
{
    ImGui::Begin("Assembler Comparison Manager", p_open);
    AsmComparisonManagerBody();
    ImGui::End();
}

namespace
{
const char *const g_browse_file_button_label = "Browse ..";
const char *const g_select_file_dialog_title = "Select File";
} // namespace

void ImGuiApp::FileManagerBody()
{
    // clang-format off
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_BordersH |
        ImGuiTableFlags_BordersOuterV |
        ImGuiTableFlags_NoBordersInBodyUntilResize;
    // clang-format on

    ImGui::SeparatorText("Files List Begin");

    size_t erase_idx = size_t(~0);

    for (size_t i = 0; i < m_programFiles.size(); ++i)
    {
        ProgramFileDescriptor &descriptor = *m_programFiles[i];

        // Set a unique ID for each element to avoid ID conflicts
        ImScoped::ID push_id(i);

        const std::string exe_filename = descriptor.evaluate_exe_filename();
        const ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_DefaultOpen;
        ImScoped::TreeNodeEx tree_node(
            "file_tree", tree_flags, "File %02d (%s) (%s)", i, exe_filename.c_str(), descriptor.pdbFilename.c_str());

        if (!tree_node.IsOpen)
        {
            continue;
        }

        {
            ImScoped::Group group;
            ImScoped::Disabled disabled(descriptor.has_active_command());
            ImScoped::ItemWidth push_item_width(ImGui::GetFontSize() * -12);

            FileManagerDescriptorExeFile(descriptor, i);

            FileManagerDescriptorExeConfig(descriptor, i);

            FileManagerDescriptorPdbFile(descriptor, i);

            FileManagerDescriptorPdbConfig(descriptor, i);

            bool erased;
            FileManagerDescriptorActions(descriptor, erased);
            if (erased)
                erase_idx = i;
        }

        // Draw overlay
        if (descriptor.has_active_command())
        {
            ImRect group_rect;
            group_rect.Min = ImGui::GetItemRectMin();
            group_rect.Max = ImGui::GetItemRectMax();

            const std::string overlay = fmt::format("Processing command {:d} ..", descriptor.activeCommandId);

            OverlayProgressBar(group_rect, -1.0f * (float)ImGui::GetTime(), overlay.c_str());
        }

        ImGui::Spacing();
        ImGui::Spacing();
    }

    // Erase at the end to avoid incomplete elements.
    if (erase_idx < m_programFiles.size())
    {
        m_programFiles.erase(m_programFiles.begin() + erase_idx);
    }

    ImGui::SeparatorText("Files List End");

    FileManagerFooter();
}

void ImGuiApp::FileManagerDescriptorExeFile(ProgramFileDescriptor &descriptor, size_t idx)
{
    AddFileDialogButton(
        &descriptor.exeFilename,
        g_browse_file_button_label,
        fmt::format("exe_file_dialog{:d}", idx),
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

void ImGuiApp::FileManagerDescriptorExeConfig(ProgramFileDescriptor &descriptor, size_t idx)
{
    AddFileDialogButton(
        &descriptor.exeConfigFilename,
        g_browse_file_button_label,
        fmt::format("exe_config_file_dialog{:d}", idx),
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

void ImGuiApp::FileManagerDescriptorPdbFile(ProgramFileDescriptor &descriptor, size_t idx)
{
    AddFileDialogButton(
        &descriptor.pdbFilename,
        g_browse_file_button_label,
        fmt::format("pdb_file_dialog{:d}", idx),
        g_select_file_dialog_title,
        "Program Database (*.pdb){.pdb}");

    ImGui::SameLine();
    ImGui::InputText("Pdb File", &descriptor.pdbFilename);
}

void ImGuiApp::FileManagerDescriptorPdbConfig(ProgramFileDescriptor &descriptor, size_t idx)
{
    AddFileDialogButton(
        &descriptor.pdbConfigFilename,
        g_browse_file_button_label,
        fmt::format("pdb_config_file_dialog{:d}", idx),
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

void ImGuiApp::FileManagerFooter()
{
    if (ImGui::Button("Add File"))
    {
        m_programFiles.emplace_back(std::make_unique<ProgramFileDescriptor>());
    }

    ImGui::SameLine();
    {
        ImScoped::Disabled disabled(true);
        ImGui::Checkbox("Auto Load", &m_autoLoad); // #TODO: Implement ?
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

void ImGuiApp::AsmOutputManagerBody()
{
}

void ImGuiApp::AsmComparisonManagerBody()
{
}

} // namespace unassemblize::gui
