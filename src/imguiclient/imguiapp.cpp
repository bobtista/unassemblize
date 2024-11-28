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
ImGuiApp::ProgramFileRevisionId ImGuiApp::ProgramFileRevisionDescriptor::s_id = 1;
ImGuiApp::ProgramComparisonId ImGuiApp::ProgramComparisonDescriptor::s_id = 1;

ImGuiApp::ProgramFileDescriptor::ProgramFileDescriptor() : m_id(s_id++)
{
}

ImGuiApp::ProgramFileDescriptor::~ProgramFileDescriptor()
{
}

bool ImGuiApp::ProgramFileDescriptor::has_active_command() const
{
    return get_active_command_id() != InvalidWorkQueueCommandId;
}

WorkQueueCommandId ImGuiApp::ProgramFileDescriptor::get_active_command_id() const
{
    if (m_revisionDescriptor != nullptr && m_revisionDescriptor->has_active_command())
    {
        switch (m_revisionDescriptor->m_workReason)
        {
            case ProgramFileRevisionDescriptor::WorkReason::Load:
            case ProgramFileRevisionDescriptor::WorkReason::SaveConfig:
                return m_revisionDescriptor->get_active_command_id();
            case ProgramFileRevisionDescriptor::WorkReason::BuildNamedFunctions:
            default:
                break;
        }
    }

    return InvalidWorkQueueCommandId;
}

bool ImGuiApp::ProgramFileDescriptor::can_load_exe() const
{
    return !evaluate_exe_filename().empty();
}

bool ImGuiApp::ProgramFileDescriptor::can_load_pdb() const
{
    return !m_pdbFilename.empty();
}

bool ImGuiApp::ProgramFileDescriptor::can_load() const
{
    return can_load_exe() || can_load_pdb();
}

bool ImGuiApp::ProgramFileDescriptor::can_save_exe_config() const
{
    return exe_loaded() && !evaluate_exe_config_filename().empty();
}

bool ImGuiApp::ProgramFileDescriptor::can_save_pdb_config() const
{
    return pdb_loaded() && !evaluate_pdb_config_filename().empty();
}

bool ImGuiApp::ProgramFileDescriptor::can_save_config() const
{
    return can_save_exe_config() || can_save_pdb_config();
}

bool ImGuiApp::ProgramFileDescriptor::exe_loaded() const
{
    return m_revisionDescriptor != nullptr && m_revisionDescriptor->exe_loaded();
}

bool ImGuiApp::ProgramFileDescriptor::pdb_loaded() const
{
    return m_revisionDescriptor != nullptr && m_revisionDescriptor->pdb_loaded();
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_exe_filename() const
{
    if (is_auto_str(m_exeFilename))
    {
        if (m_revisionDescriptor != nullptr)
            return m_revisionDescriptor->m_exeFilenameFromPdb;
        else
            return std::string();
    }
    else
    {
        return m_exeFilename;
    }
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_exe_config_filename() const
{
    return ::get_config_file_name(evaluate_exe_filename(), m_exeConfigFilename);
}

std::string ImGuiApp::ProgramFileDescriptor::evaluate_pdb_config_filename() const
{
    return ::get_config_file_name(m_pdbFilename, m_pdbConfigFilename);
}

std::string ImGuiApp::ProgramFileDescriptor::create_short_exe_name() const
{
    std::string name;
    if (m_revisionDescriptor != nullptr)
    {
        name = m_revisionDescriptor->create_short_exe_name();
    }
    else
    {
        name = evaluate_exe_filename();
        if (name.empty())
            name = m_exeFilename;
    }
    std::filesystem::path path(name);
    return path.filename().string();
}

std::string ImGuiApp::ProgramFileDescriptor::create_descriptor_name() const
{
    return fmt::format("File:{:d}", m_id);
}

std::string ImGuiApp::ProgramFileDescriptor::create_descriptor_name_with_file_info() const
{
    std::string revision;
    ProgramFileRevisionId revisionId = get_revision_id();
    if (revisionId != InvalidId)
    {
        revision = fmt::format(" - Revision:{:d}", revisionId);
    }

    const std::string name = create_short_exe_name();
    if (name.empty())
    {
        return create_descriptor_name();
    }
    else
    {
        return fmt::format("File:{:d}{:s} - {:s}", m_id, revision, name);
    }
}

ImGuiApp::ProgramFileRevisionId ImGuiApp::ProgramFileDescriptor::get_revision_id() const
{
    if (m_revisionDescriptor != nullptr)
        return m_revisionDescriptor->m_id;
    else
        return InvalidId;
}

void ImGuiApp::ProgramFileDescriptor::create_new_revision_descriptor()
{
    m_exeSymbolsFilter.reset();
    m_pdbSymbolsFilter.reset();
    m_pdbFunctionsFilter.reset();

    m_revisionDescriptor = std::make_shared<ProgramFileRevisionDescriptor>();
    m_revisionDescriptor->m_exeFilenameCopy = m_exeFilename;
    m_revisionDescriptor->m_exeConfigFilenameCopy = m_exeConfigFilename;
    m_revisionDescriptor->m_pdbFilenameCopy = m_pdbFilename;
    m_revisionDescriptor->m_pdbConfigFilenameCopy = m_pdbConfigFilename;
}

ImGuiApp::ProgramFileRevisionDescriptor::ProgramFileRevisionDescriptor() : m_id(s_id++)
{
}

ImGuiApp::ProgramFileRevisionDescriptor::~ProgramFileRevisionDescriptor()
{
}

void ImGuiApp::ProgramFileRevisionDescriptor::invalidate_command_id()
{
    m_activeCommandId = InvalidWorkQueueCommandId;
}

bool ImGuiApp::ProgramFileRevisionDescriptor::has_active_command() const
{
    return m_activeCommandId != InvalidWorkQueueCommandId;
}

WorkQueueCommandId ImGuiApp::ProgramFileRevisionDescriptor::get_active_command_id() const
{
    return m_activeCommandId;
}

bool ImGuiApp::ProgramFileRevisionDescriptor::can_load_exe() const
{
    return !evaluate_exe_filename().empty();
}

bool ImGuiApp::ProgramFileRevisionDescriptor::can_load_pdb() const
{
    return !m_pdbFilenameCopy.empty();
}

bool ImGuiApp::ProgramFileRevisionDescriptor::can_save_exe_config() const
{
    return exe_loaded() && !evaluate_exe_config_filename().empty();
}

bool ImGuiApp::ProgramFileRevisionDescriptor::can_save_pdb_config() const
{
    return pdb_loaded() && !evaluate_pdb_config_filename().empty();
}

bool ImGuiApp::ProgramFileRevisionDescriptor::exe_loaded() const
{
    return m_executable != nullptr;
}

bool ImGuiApp::ProgramFileRevisionDescriptor::pdb_loaded() const
{
    return m_pdbReader != nullptr;
}

bool ImGuiApp::ProgramFileRevisionDescriptor::named_functions_built() const
{
    return m_namedFunctionsBuilt;
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::evaluate_exe_filename() const
{
    if (is_auto_str(m_exeFilenameCopy))
    {
        return m_exeFilenameFromPdb;
    }
    else
    {
        return m_exeFilenameCopy;
    }
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::evaluate_exe_config_filename() const
{
    return ::get_config_file_name(evaluate_exe_filename(), m_exeConfigFilenameCopy);
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::evaluate_pdb_config_filename() const
{
    return ::get_config_file_name(m_pdbFilenameCopy, m_pdbConfigFilenameCopy);
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::create_short_exe_name() const
{
    std::string name;
    if (m_executable != nullptr)
    {
        name = m_executable->get_filename();
    }
    else
    {
        name = evaluate_exe_filename();
        if (name.empty())
            name = m_exeFilenameCopy;
    }
    std::filesystem::path path(name);
    return path.filename().string();
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::create_descriptor_name() const
{
    return fmt::format("Revision:{:d}", m_id);
}

std::string ImGuiApp::ProgramFileRevisionDescriptor::create_descriptor_name_with_file_info() const
{
    const std::string name = create_short_exe_name();
    if (name.empty())
    {
        return create_descriptor_name();
    }
    else
    {
        return fmt::format("Revision:{:d} - {:s}", m_id, name);
    }
}

ImGuiApp::ProgramComparisonDescriptor::ProgramComparisonDescriptor() : m_id(s_id++)
{
}

ImGuiApp::ProgramComparisonDescriptor::~ProgramComparisonDescriptor()
{
}

void ImGuiApp::ProgramComparisonDescriptor::File::invalidate_command_id()
{
    m_activeCommandId = InvalidWorkQueueCommandId;
}

bool ImGuiApp::ProgramComparisonDescriptor::File::has_active_command() const
{
    return get_active_command_id() != InvalidWorkQueueCommandId;
}

WorkQueueCommandId ImGuiApp::ProgramComparisonDescriptor::File::get_active_command_id() const
{
    if (m_revisionDescriptor != nullptr && m_revisionDescriptor->has_active_command())
    {
        switch (m_revisionDescriptor->m_workReason)
        {
            case ProgramFileRevisionDescriptor::WorkReason::Load:
            case ProgramFileRevisionDescriptor::WorkReason::BuildNamedFunctions:
                return m_revisionDescriptor->get_active_command_id();
            case ProgramFileRevisionDescriptor::WorkReason::SaveConfig:
            default:
                break;
        }
    }

    return m_activeCommandId;
}

bool ImGuiApp::ProgramComparisonDescriptor::File::exe_loaded() const
{
    return m_revisionDescriptor != nullptr && m_revisionDescriptor->exe_loaded();
}

bool ImGuiApp::ProgramComparisonDescriptor::File::pdb_loaded() const
{
    return m_revisionDescriptor != nullptr && m_revisionDescriptor->pdb_loaded();
}

bool ImGuiApp::ProgramComparisonDescriptor::File::named_functions_built() const
{
    return m_revisionDescriptor != nullptr && m_revisionDescriptor->named_functions_built();
}

bool ImGuiApp::ProgramComparisonDescriptor::File::bundles_built() const
{
    // Note: Compiland and Source Files bundles are only built when a Pdb was present.
    // Therefore, only the single bundle is always built for an executable.
    return m_singleBundleBuilt;
}

bool ImGuiApp::ProgramComparisonDescriptor::has_active_command() const
{
    for (const File &file : m_files)
    {
        if (file.has_active_command())
            return true;
    }
    return false;
}

bool ImGuiApp::ProgramComparisonDescriptor::executables_loaded() const
{
    int count = 0;

    for (const File &file : m_files)
    {
        if (file.exe_loaded())
            ++count;
    }
    return count == m_files.size();
}

bool ImGuiApp::ProgramComparisonDescriptor::named_functions_built() const
{
    int count = 0;

    for (const File &file : m_files)
    {
        if (file.named_functions_built())
            ++count;
    }
    return count == m_files.size();
}

bool ImGuiApp::ProgramComparisonDescriptor::matched_functions_built() const
{
    return m_matchedFunctionsBuilt;
}

bool ImGuiApp::ProgramComparisonDescriptor::bundles_built() const
{
    int count = 0;

    for (const File &file : m_files)
    {
        if (file.bundles_built())
            ++count;
    }
    return count == m_files.size();
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
                descriptor->m_exeFilename = clo.input_file[i];
                descriptor->m_exeConfigFilename = clo.config_file[i];
                break;
            case InputType::Pdb:
                descriptor->m_exeFilename = auto_str;
                descriptor->m_exeConfigFilename = clo.config_file[i];
                descriptor->m_pdbFilename = clo.input_file[i];
                descriptor->m_pdbConfigFilename = clo.config_file[i];
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

    if (m_showOutputManager)
        OutputManagerWindow(&m_showOutputManager);

    ComparisonManagerWindows();
}

WorkQueueCommandPtr ImGuiApp::create_load_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    if (revisionDescriptor->can_load_pdb())
    {
        return create_load_pdb_and_exe_command(revisionDescriptor);
    }
    else if (revisionDescriptor->can_load_exe())
    {
        return create_load_exe_command(revisionDescriptor);
    }
    else
    {
        // Cannot load undefined file.
        assert(false);
        return nullptr;
    }
}

WorkQueueCommandPtr ImGuiApp::create_load_exe_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    assert(revisionDescriptor->can_load_exe());

    if (revisionDescriptor->m_pdbReader == nullptr)
    {
        revisionDescriptor->m_exeFilenameFromPdb.clear();
    }

    const std::string exe_filename = revisionDescriptor->evaluate_exe_filename();
    auto command = std::make_unique<AsyncLoadExeCommand>(LoadExeOptions(exe_filename));

    command->options.config_file = revisionDescriptor->evaluate_exe_config_filename();
    command->options.pdb_reader = revisionDescriptor->m_pdbReader.get();
    command->callback = [revisionDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncLoadExeResult *>(result.get());
        revisionDescriptor->m_executable = std::move(res->executable);
        revisionDescriptor->m_exeLoadTimepoint = std::chrono::system_clock::now();
        revisionDescriptor->invalidate_command_id();
    };

    revisionDescriptor->m_executable.reset();
    revisionDescriptor->m_exeLoadTimepoint = InvalidTimePoint;
    revisionDescriptor->m_exeSaveConfigFilename.clear();
    revisionDescriptor->m_exeSaveConfigTimepoint = InvalidTimePoint;
    revisionDescriptor->m_activeCommandId = command->command_id;
    revisionDescriptor->m_workReason = ProgramFileRevisionDescriptor::WorkReason::Load;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_load_pdb_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    assert(revisionDescriptor->can_load_pdb());

    auto command = std::make_unique<AsyncLoadPdbCommand>(LoadPdbOptions(revisionDescriptor->m_pdbFilenameCopy));

    command->callback = [revisionDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncLoadPdbResult *>(result.get());
        revisionDescriptor->m_pdbReader = std::move(res->pdbReader);
        revisionDescriptor->m_pdbLoadTimepoint = std::chrono::system_clock::now();
        revisionDescriptor->invalidate_command_id();
    };

    revisionDescriptor->m_pdbReader.reset();
    revisionDescriptor->m_pdbLoadTimepoint = InvalidTimePoint;
    revisionDescriptor->m_pdbSaveConfigFilename.clear();
    revisionDescriptor->m_pdbSaveConfigTimepoint = InvalidTimePoint;
    revisionDescriptor->m_exeFilenameFromPdb.clear();
    revisionDescriptor->m_activeCommandId = command->command_id;
    revisionDescriptor->m_workReason = ProgramFileRevisionDescriptor::WorkReason::Load;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_load_pdb_and_exe_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    auto command = create_load_pdb_command(revisionDescriptor);

    command->chain([revisionDescriptor](WorkQueueResultPtr &result) mutable -> WorkQueueCommandPtr {
        if (revisionDescriptor->m_pdbReader == nullptr)
            return nullptr;

        const unassemblize::PdbExeInfo &exe_info = revisionDescriptor->m_pdbReader->get_exe_info();
        revisionDescriptor->m_exeFilenameFromPdb = unassemblize::Runner::create_exe_filename(exe_info);

        if (!revisionDescriptor->can_load_exe())
            return nullptr;

        return create_load_exe_command(revisionDescriptor);
    });

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_save_exe_config_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    assert(revisionDescriptor->can_save_exe_config());

    const std::string config_filename = revisionDescriptor->evaluate_exe_config_filename();
    auto command = std::make_unique<AsyncSaveExeConfigCommand>(
        SaveExeConfigOptions(*revisionDescriptor->m_executable, config_filename));

    command->callback = [revisionDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncSaveExeConfigResult *>(result.get());
        if (res->success)
        {
            auto com = static_cast<AsyncSaveExeConfigCommand *>(result->command.get());
            revisionDescriptor->m_exeSaveConfigFilename = util::abs_path(com->options.config_file);
            revisionDescriptor->m_exeSaveConfigTimepoint = std::chrono::system_clock::now();
        }
        revisionDescriptor->invalidate_command_id();
    };

    revisionDescriptor->m_exeSaveConfigFilename.clear();
    revisionDescriptor->m_exeSaveConfigTimepoint = InvalidTimePoint;
    revisionDescriptor->m_activeCommandId = command->command_id;
    revisionDescriptor->m_workReason = ProgramFileRevisionDescriptor::WorkReason::SaveConfig;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_save_pdb_config_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    assert(revisionDescriptor->can_save_pdb_config());

    const std::string config_filename = revisionDescriptor->evaluate_pdb_config_filename();
    auto command =
        std::make_unique<AsyncSavePdbConfigCommand>(SavePdbConfigOptions(*revisionDescriptor->m_pdbReader, config_filename));

    command->callback = [revisionDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncSavePdbConfigResult *>(result.get());
        if (res->success)
        {
            auto com = static_cast<AsyncSavePdbConfigCommand *>(result->command.get());
            revisionDescriptor->m_pdbSaveConfigFilename = util::abs_path(com->options.config_file);
            revisionDescriptor->m_pdbSaveConfigTimepoint = std::chrono::system_clock::now();
        }
        revisionDescriptor->invalidate_command_id();
    };

    revisionDescriptor->m_pdbSaveConfigFilename.clear();
    revisionDescriptor->m_pdbSaveConfigTimepoint = InvalidTimePoint;
    revisionDescriptor->m_activeCommandId = command->command_id;
    revisionDescriptor->m_workReason = ProgramFileRevisionDescriptor::WorkReason::SaveConfig;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_build_named_functions_command(ProgramFileRevisionDescriptorPtr &revisionDescriptor)
{
    assert(revisionDescriptor != nullptr);
    assert(revisionDescriptor->exe_loaded());

    auto command = std::make_unique<AsyncBuildFunctionsCommand>(BuildFunctionsOptions(*revisionDescriptor->m_executable));

    command->callback = [revisionDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncBuildFunctionsResult *>(result.get());
        revisionDescriptor->m_namedFunctions = std::move(res->named_functions);
        revisionDescriptor->m_namedFunctionsBuilt = true;
        revisionDescriptor->invalidate_command_id();
    };

    revisionDescriptor->m_namedFunctions.clear();
    revisionDescriptor->m_namedFunctionsBuilt = false;
    revisionDescriptor->m_activeCommandId = command->command_id;
    revisionDescriptor->m_workReason = ProgramFileRevisionDescriptor::WorkReason::BuildNamedFunctions;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_build_matched_functions_command(ProgramComparisonDescriptor *comparisonDescriptor)
{
    assert(comparisonDescriptor != nullptr);
    ProgramFileRevisionDescriptor *revisionDescriptor0 = comparisonDescriptor->m_files[0].m_revisionDescriptor.get();
    ProgramFileRevisionDescriptor *revisionDescriptor1 = comparisonDescriptor->m_files[1].m_revisionDescriptor.get();
    assert(revisionDescriptor0 != nullptr);
    assert(revisionDescriptor1 != nullptr);
    assert(revisionDescriptor0->named_functions_built());
    assert(revisionDescriptor1->named_functions_built());

    auto command = std::make_unique<AsyncBuildMatchedFunctionsCommand>(
        BuildMatchedFunctionsOptions({&revisionDescriptor0->m_namedFunctions, &revisionDescriptor1->m_namedFunctions}));

    command->callback = [comparisonDescriptor](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncBuildMatchedFunctionsResult *>(result.get());

        comparisonDescriptor->m_matchedFunctions = std::move(res->matchedFunctionsData.matchedFunctions);
        comparisonDescriptor->m_matchedFunctionsBuilt = true;

        for (int i = 0; i < 2; ++i)
        {
            ProgramComparisonDescriptor::File &file = comparisonDescriptor->m_files[i];

            file.m_namedFunctionsMatchInfos = std::move(res->matchedFunctionsData.namedFunctionMatchInfosArray[i]);
            file.invalidate_command_id();
        }
    };

    comparisonDescriptor->m_matchedFunctions.clear();
    comparisonDescriptor->m_matchedFunctionsBuilt = false;

    for (ProgramComparisonDescriptor::File &file : comparisonDescriptor->m_files)
    {
        file.m_namedFunctionsMatchInfos.clear();
        file.m_activeCommandId = command->command_id;
        file.m_workReason = ProgramComparisonDescriptor::File::WorkReason::BuildMatchedFunctions;
    }

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_build_bundles_from_compilands_command(ProgramComparisonDescriptor::File *file)
{
    assert(file != nullptr);
    assert(file->m_revisionDescriptor != nullptr);
    assert(file->m_revisionDescriptor->named_functions_built());
    assert(file->m_revisionDescriptor->pdb_loaded());

    auto command = std::make_unique<AsyncBuildBundlesFromCompilandsCommand>(BuildBundlesFromCompilandsOptions(
        file->m_revisionDescriptor->m_namedFunctions,
        file->m_namedFunctionsMatchInfos,
        *file->m_revisionDescriptor->m_pdbReader));

    command->callback = [file](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncBuildBundlesFromCompilandsResult *>(result.get());
        file->m_compilandBundles = std::move(res->bundles);
        file->m_compilandBundlesBuilt = true;
        file->invalidate_command_id();
    };

    file->m_compilandBundles.clear();
    file->m_compilandBundlesBuilt = false;
    file->m_activeCommandId = command->command_id;
    file->m_workReason = ProgramComparisonDescriptor::File::WorkReason::BuildCompilandBundles;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_build_bundles_from_source_files_command(ProgramComparisonDescriptor::File *file)
{
    assert(file != nullptr);
    assert(file->m_revisionDescriptor != nullptr);
    assert(file->m_revisionDescriptor->named_functions_built());
    assert(file->m_revisionDescriptor->pdb_loaded());

    auto command = std::make_unique<AsyncBuildBundlesFromSourceFilesCommand>(BuildBundlesFromSourceFilesOptions(
        file->m_revisionDescriptor->m_namedFunctions,
        file->m_namedFunctionsMatchInfos,
        *file->m_revisionDescriptor->m_pdbReader));

    command->callback = [file](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncBuildBundlesFromSourceFilesResult *>(result.get());
        file->m_sourceFileBundles = std::move(res->bundles);
        file->m_sourceFileBundlesBuilt = true;
        file->invalidate_command_id();
    };

    file->m_sourceFileBundles.clear();
    file->m_sourceFileBundlesBuilt = false;
    file->m_activeCommandId = command->command_id;
    file->m_workReason = ProgramComparisonDescriptor::File::WorkReason::BuildSourceFileBundles;

    return command;
}

WorkQueueCommandPtr ImGuiApp::create_build_single_bundle_command(
    ProgramComparisonDescriptor *comparisonDescriptor,
    size_t bundle_file_idx)
{
    assert(comparisonDescriptor != nullptr);
    assert(comparisonDescriptor->matched_functions_built());
    assert(bundle_file_idx < comparisonDescriptor->m_files.size());

    ProgramComparisonDescriptor::File *file = &comparisonDescriptor->m_files[bundle_file_idx];

    auto command = std::make_unique<AsyncBuildSingleBundleCommand>(BuildSingleBundleOptions(
        file->m_namedFunctionsMatchInfos,
        comparisonDescriptor->m_matchedFunctions,
        bundle_file_idx));

    command->callback = [file](WorkQueueResultPtr &result) {
        auto res = static_cast<AsyncBuildSingleBundleResult *>(result.get());
        file->m_singleBundle = std::move(res->bundle);
        file->m_singleBundleBuilt = true;
        file->invalidate_command_id();
    };

    file->m_sourceFileBundles.clear();
    file->m_singleBundleBuilt = false;
    file->m_activeCommandId = command->command_id;
    file->m_workReason = ProgramComparisonDescriptor::File::WorkReason::BuildSingleBundle;

    return command;
}

ImGuiApp::ProgramFileDescriptor *ImGuiApp::get_program_file_descriptor(size_t program_file_idx)
{
    if (program_file_idx < m_programFiles.size())
    {
        return m_programFiles[program_file_idx].get();
    }
    return nullptr;
}

void ImGuiApp::load_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor != nullptr);

    descriptor->create_new_revision_descriptor();

    auto command = create_load_command(descriptor->m_revisionDescriptor);

    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::save_config_async(ProgramFileDescriptor *descriptor)
{
    assert(descriptor != nullptr);
    assert(descriptor->m_revisionDescriptor != nullptr);

    WorkQueueDelayedCommand head_command;
    WorkQueueDelayedCommand *next_command = &head_command;

    if (descriptor->can_save_exe_config())
    {
        descriptor->m_revisionDescriptor->m_exeConfigFilenameCopy = descriptor->m_exeConfigFilename;
        next_command = next_command->chain(
            [descriptor, revisionDescriptor = descriptor->m_revisionDescriptor](WorkQueueResultPtr &result) mutable
            -> WorkQueueCommandPtr { return create_save_exe_config_command(revisionDescriptor); });
    }

    if (descriptor->can_save_pdb_config())
    {
        descriptor->m_revisionDescriptor->m_pdbConfigFilenameCopy = descriptor->m_pdbConfigFilename;
        next_command = next_command->chain(
            [descriptor, revisionDescriptor = descriptor->m_revisionDescriptor](WorkQueueResultPtr &result) mutable
            -> WorkQueueCommandPtr { return create_save_pdb_config_command(revisionDescriptor); });
    }

    assert(head_command.next_delayed_command != nullptr);

    m_workQueue.enqueue(head_command);
}

void ImGuiApp::load_and_compare_async(
    ProgramFileDescriptorPair fileDescriptorPair,
    ProgramComparisonDescriptor *comparisonDescriptor)
{
    assert(fileDescriptorPair[0]->can_load() || fileDescriptorPair[0]->exe_loaded());
    assert(fileDescriptorPair[1]->can_load() || fileDescriptorPair[1]->exe_loaded());

    bool isAnyncLoading = false;

    const bool isComparingSameFile = fileDescriptorPair[0] == fileDescriptorPair[1];
    const int loadFileCount = isComparingSameFile ? 1 : 2;

    for (int i = 0; i < loadFileCount; ++i)
    {
        ProgramComparisonDescriptor::File &file = comparisonDescriptor->m_files[i];

        if (file.m_revisionDescriptor == nullptr || file.m_revisionDescriptor != fileDescriptorPair[i]->m_revisionDescriptor)
        {
            // Force rebuild matched functions when at least one of the files needs to be loaded first or has changed.
            comparisonDescriptor->m_matchedFunctionsBuilt = false;
            break;
        }
    }

    for (int i = 0; i < loadFileCount; ++i)
    {
        ProgramFileDescriptor *fileDescriptor = fileDescriptorPair[i];

        if (fileDescriptor->exe_loaded())
        {
            // Executable is already loaded. Use it.

            if (isComparingSameFile)
            {
                comparisonDescriptor->m_files[0].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
                comparisonDescriptor->m_files[1].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
            }
            else
            {
                comparisonDescriptor->m_files[i].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
            }

            if (!comparisonDescriptor->matched_functions_built())
            {
                // Force rebuild bundles because bundles are dependent on matched functions.
                comparisonDescriptor->m_files[i].m_compilandBundlesBuilt = false;
                comparisonDescriptor->m_files[i].m_sourceFileBundlesBuilt = false;
                comparisonDescriptor->m_files[i].m_singleBundleBuilt = false;
            }
        }
        else
        {
            // Executable is not yet loaded. Load it first.

            fileDescriptor->create_new_revision_descriptor();

            if (isComparingSameFile)
            {
                comparisonDescriptor->m_files[0].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
                comparisonDescriptor->m_files[1].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
            }
            else
            {
                comparisonDescriptor->m_files[i].m_revisionDescriptor = fileDescriptor->m_revisionDescriptor;
            }

            auto command = create_load_command(fileDescriptor->m_revisionDescriptor);

            WorkQueueDelayedCommand *next_command = get_last_delayed_command(command.get());

            next_command->chain([this, comparisonDescriptor](WorkQueueResultPtr &result) -> WorkQueueCommandPtr {
                if (comparisonDescriptor->executables_loaded())
                {
                    compare_async(comparisonDescriptor);
                }
                return nullptr;
            });

            m_workQueue.enqueue(std::move(command));
            isAnyncLoading = true;
        }
    }

    if (!isAnyncLoading)
    {
        compare_async(comparisonDescriptor);
    }
}

void ImGuiApp::compare_async(ProgramComparisonDescriptor *comparisonDescriptor)
{
    assert(comparisonDescriptor != nullptr);

    if (!comparisonDescriptor->named_functions_built())
    {
        build_named_functions_async(comparisonDescriptor);
    }
    else if (!comparisonDescriptor->matched_functions_built())
    {
        build_matched_functions_async(comparisonDescriptor);
    }
    else if (!comparisonDescriptor->bundles_built())
    {
        build_bundled_functions_async(comparisonDescriptor);
    }
    else
    {
        // Nothing to do.
    }
}

void ImGuiApp::build_named_functions_async(ProgramComparisonDescriptor *comparisonDescriptor)
{
    std::array<ProgramFileRevisionDescriptorPtr, 2> revisionDescriptorPair = {
        comparisonDescriptor->m_files[0].m_revisionDescriptor,
        comparisonDescriptor->m_files[1].m_revisionDescriptor};

    const bool isComparingSameFile = revisionDescriptorPair[0] == revisionDescriptorPair[1];
    const int loadFileCount = isComparingSameFile ? 1 : 2;

    for (size_t i = 0; i < loadFileCount; ++i)
    {
        assert(revisionDescriptorPair[i] != nullptr);

        if (!revisionDescriptorPair[i]->named_functions_built())
        {
            auto command = create_build_named_functions_command(revisionDescriptorPair[i]);

            WorkQueueDelayedCommand *next_command = get_last_delayed_command(command.get());

            next_command->chain([this, comparisonDescriptor](WorkQueueResultPtr &result) -> WorkQueueCommandPtr {
                if (comparisonDescriptor->named_functions_built())
                {
                    // Go to next async task.
                    compare_async(comparisonDescriptor);
                }
                return nullptr;
            });

            m_workQueue.enqueue(std::move(command));
        }
    }
}

void ImGuiApp::build_matched_functions_async(ProgramComparisonDescriptor *comparisonDescriptor)
{
    auto command = create_build_matched_functions_command(comparisonDescriptor);

    WorkQueueDelayedCommand *next_command = get_last_delayed_command(command.get());

    next_command->chain([this, comparisonDescriptor](WorkQueueResultPtr &result) -> WorkQueueCommandPtr {
        assert(comparisonDescriptor->matched_functions_built());

        // Go to next async task.
        compare_async(comparisonDescriptor);

        return nullptr;
    });

    m_workQueue.enqueue(std::move(command));
}

void ImGuiApp::build_bundled_functions_async(ProgramComparisonDescriptor *comparisonDescriptor)
{
    for (size_t i = 0; i < 2; ++i)
    {
        ProgramComparisonDescriptor::File &file = comparisonDescriptor->m_files[i];
        assert(file.m_revisionDescriptor != nullptr);

        if (!file.bundles_built())
        {
            if (file.pdb_loaded())
            {
                auto command1 = create_build_bundles_from_compilands_command(&file);
                auto command2 = create_build_bundles_from_source_files_command(&file);
                m_workQueue.enqueue(std::move(command1));
                m_workQueue.enqueue(std::move(command2));
            }

            auto command3 = create_build_single_bundle_command(comparisonDescriptor, i);
            m_workQueue.enqueue(std::move(command3));
        }
    }
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
    m_programComparisons.emplace_back(std::make_unique<ProgramComparisonDescriptor>());
}

void ImGuiApp::remove_closed_asm_comparisons()
{
    // Remove descriptor when window was closed.
    m_programComparisons.erase(
        std::remove_if(
            m_programComparisons.begin(),
            m_programComparisons.end(),
            [](const ProgramComparisonDescriptorPtr &p) { return !p->m_has_open_window; }),
        m_programComparisons.end());
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
                    ImGui::MenuItem("Assembler Output", nullptr, &m_showOutputManager);

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

void ImGuiApp::OutputManagerWindow(bool *p_open)
{
    ImScoped::Window window("Assembler Output Manager", p_open);
    if (window.IsContentVisible)
    {
        OutputManagerBody();
    }
}

void ImGuiApp::ComparisonManagerWindows()
{
    const size_t count = m_programComparisons.size();
    for (size_t i = 0; i < count; ++i)
    {
        ProgramComparisonDescriptor &descriptor = *m_programComparisons[i];

        const std::string title = fmt::format("Assembler Comparison {:d}", descriptor.m_id);

        ImScoped::Window window(title.c_str(), &descriptor.m_has_open_window);
        ImScoped::ID id(i);
        if (window.IsContentVisible && descriptor.m_has_open_window)
        {
            ComparisonManagerBody(descriptor);
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
                const std::string title = descriptor.create_descriptor_name_with_file_info();
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

        const std::string overlay = fmt::format("Processing command {:d} ..", descriptor.get_active_command_id());

        OverlayProgressBar(group_rect, -1.0f * (float)ImGui::GetTime(), overlay.c_str());
    }

    ProgramFileRevisionDescriptor *revisionDescriptor = descriptor.m_revisionDescriptor.get();

    if (revisionDescriptor != nullptr)
    {
        FileManagerDescriptorSaveLoadStatus(*revisionDescriptor);

        // Draw some details

        if (revisionDescriptor->m_executable != nullptr || revisionDescriptor->m_pdbReader != nullptr)
        {
            ImScoped::TreeNode tree("Info");
            if (tree.IsOpen)
            {
                FileManagerInfo(descriptor, *revisionDescriptor);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
}

void ImGuiApp::FileManagerDescriptorExeFile(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.m_exeFilename,
        g_browse_file_button_label,
        fmt::format("exe_file_dialog{:d}", descriptor.m_id),
        g_select_file_dialog_title,
        "Program (*.*){((.*))}"); // ((.*)) is regex for all files

    ImGui::SameLine();
    ImGui::InputTextWithHint("Program File", auto_str, &descriptor.m_exeFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.m_exeFilename) && !descriptor.has_active_command())
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
        &descriptor.m_exeConfigFilename,
        g_browse_file_button_label,
        fmt::format("exe_config_file_dialog{:d}", descriptor.m_id),
        g_select_file_dialog_title,
        "Config (*.json){.json}");

    ImGui::SameLine();
    ImGui::InputTextWithHint("Program Config File", auto_str, &descriptor.m_exeConfigFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.m_exeConfigFilename) && !descriptor.m_exeFilename.empty() && !descriptor.has_active_command())
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
        &descriptor.m_pdbFilename,
        g_browse_file_button_label,
        fmt::format("pdb_file_dialog{:d}", descriptor.m_id),
        g_select_file_dialog_title,
        "Program Database (*.pdb){.pdb}");

    ImGui::SameLine();
    ImGui::InputText("Pdb File", &descriptor.m_pdbFilename);
}

void ImGuiApp::FileManagerDescriptorPdbConfig(ProgramFileDescriptor &descriptor)
{
    AddFileDialogButton(
        &descriptor.m_pdbConfigFilename,
        g_browse_file_button_label,
        fmt::format("pdb_config_file_dialog{:d}", descriptor.m_id),
        g_select_file_dialog_title,
        "Config (*.json){.json}");

    ImGui::SameLine();
    ImGui::InputTextWithHint("Pdb Config File", auto_str, &descriptor.m_pdbConfigFilename);

    // Tooltip on hover 'auto'
    if (is_auto_str(descriptor.m_pdbConfigFilename) && !descriptor.m_pdbFilename.empty())
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

void ImGuiApp::FileManagerDescriptorSaveLoadStatus(const ProgramFileRevisionDescriptor &descriptor)
{
    FileManagerDescriptorLoadStatus(descriptor);
    FileManagerDescriptorSaveStatus(descriptor);
}

void ImGuiApp::FileManagerDescriptorLoadStatus(const ProgramFileRevisionDescriptor &descriptor)
{
    if (descriptor.m_executable != nullptr)
    {
        DrawInTextCircle(GreenColor);
        ImGui::Text(
            " Loaded Exe: [Revision:%u] [%s] %s",
            descriptor.m_id,
            create_time_string(descriptor.m_exeLoadTimepoint).c_str(),
            descriptor.m_executable->get_filename().c_str());
    }

    if (descriptor.m_pdbReader != nullptr)
    {
        DrawInTextCircle(GreenColor);
        ImGui::Text(
            " Loaded Pdb: [Revision:%u] [%s] %s",
            descriptor.m_id,
            create_time_string(descriptor.m_pdbLoadTimepoint).c_str(),
            descriptor.m_pdbReader->get_filename().c_str());
    }
    // #TODO: Also draw fail status.
}

void ImGuiApp::FileManagerDescriptorSaveStatus(const ProgramFileRevisionDescriptor &descriptor)
{
    if (descriptor.m_exeSaveConfigTimepoint != InvalidTimePoint)
    {
        DrawInTextCircle(GreenColor);
        ImGui::Text(
            " Saved Exe Config: [Revision:%u] [%s] %s",
            descriptor.m_id,
            create_time_string(descriptor.m_exeSaveConfigTimepoint).c_str(),
            descriptor.m_exeSaveConfigFilename.c_str());
    }

    if (descriptor.m_pdbSaveConfigTimepoint != InvalidTimePoint)
    {
        DrawInTextCircle(GreenColor);
        ImGui::Text(
            " Saved Pdb Config: [Revision:%u] [%s] %s",
            descriptor.m_id,
            create_time_string(descriptor.m_pdbSaveConfigTimepoint).c_str(),
            descriptor.m_pdbSaveConfigFilename.c_str());
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

void ImGuiApp::FileManagerInfo(
    ProgramFileDescriptor &fileDescriptor,
    const ProgramFileRevisionDescriptor &revisionDescriptor)
{
    if (revisionDescriptor.m_executable != nullptr)
    {
        if (m_showFileManagerExeSectionInfo)
            FileManagerInfoExeSections(revisionDescriptor);

        if (m_showFileManagerExeSymbolInfo)
            FileManagerInfoExeSymbols(fileDescriptor, revisionDescriptor);
    }
    if (revisionDescriptor.m_pdbReader != nullptr)
    {
        if (m_showFileManagerPdbCompilandInfo)
            FileManagerInfoPdbCompilands(revisionDescriptor);

        if (m_showFileManagerPdbSourceFileInfo)
            FileManagerInfoPdbSourceFiles(revisionDescriptor);

        if (m_showFileManagerPdbSymbolInfo)
            FileManagerInfoPdbSymbols(fileDescriptor, revisionDescriptor);

        if (m_showFileManagerPdbFunctionInfo)
            FileManagerInfoPdbFunctions(fileDescriptor, revisionDescriptor);

        if (m_showFileManagerPdbExeInfo)
            FileManagerInfoPdbExeInfo(revisionDescriptor);
    }
}

void ImGuiApp::FileManagerInfoExeSections(const ProgramFileRevisionDescriptor &descriptor)
{
    ImGui::SeparatorText("Exe Sections");

    ImGui::Text("Exe Image base: x%08x", down_cast<uint32_t>(descriptor.m_executable->image_base()));

    const ExeSections &sections = descriptor.m_executable->get_sections();

    ImGui::Text("Count: %zu", sections.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, sections.size() + 1);

    ImScoped::Child child("exe_sections_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("exe_sections", 3, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoExeSymbols(
    ProgramFileDescriptor &fileDescriptor,
    const ProgramFileRevisionDescriptor &revisionDescriptor)
{
    ImGui::SeparatorText("Exe Symbols");

    const auto &filtered = fileDescriptor.m_exeSymbolsFilter.filtered;
    {
        const ExeSymbols &symbols = revisionDescriptor.m_executable->get_symbols();

        UpdateFilter(
            fileDescriptor.m_exeSymbolsFilter,
            symbols,
            [](const ImGuiTextFilterEx &filter, const ExeSymbol &symbol) -> bool { return filter.PassFilter(symbol.name); });

        ImGui::Text("Count: %zu, Filtered: %d", symbols.size(), filtered.size());
    }

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, filtered.size() + 1);

    ImScoped::Child child("exe_symbols_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("exe_symbols", 3, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoPdbCompilands(const ProgramFileRevisionDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Compilands");

    const PdbCompilandInfoVector &compilands = descriptor.m_pdbReader->get_compilands();

    ImGui::Text("Count: %zu", compilands.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, compilands.size() + 1);

    ImScoped::Child child("pdb_compilands_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_compilands", 1, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoPdbSourceFiles(const ProgramFileRevisionDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Source Files");

    const PdbSourceFileInfoVector &source_files = descriptor.m_pdbReader->get_source_files();

    ImGui::Text("Count: %zu", source_files.size());

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, source_files.size() + 1);

    ImScoped::Child child("pdb_source_files_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_source_files", 3, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoPdbSymbols(
    ProgramFileDescriptor &fileDescriptor,
    const ProgramFileRevisionDescriptor &revisionDescriptor)
{
    ImGui::SeparatorText("Pdb Symbols");

    const auto &filtered = fileDescriptor.m_pdbSymbolsFilter.filtered;
    {
        const PdbSymbolInfoVector &symbols = revisionDescriptor.m_pdbReader->get_symbols();

        UpdateFilter(
            fileDescriptor.m_pdbSymbolsFilter,
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
    if (revisionDescriptor.m_executable != nullptr)
        sections = &revisionDescriptor.m_executable->get_sections();

    const ImVec2 outer_size = OuterSizeForTable(10 + 1, filtered.size() + 1);

    ImScoped::Child child("pdb_symbols_container", outer_size, ImGuiChildFlags_ResizeY);
    if (child.IsContentVisible)
    {
        if (ImGui::BeginTable("pdb_symbols", 6, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoPdbFunctions(
    ProgramFileDescriptor &fileDescriptor,
    const ProgramFileRevisionDescriptor &revisionDescriptor)
{
    ImGui::SeparatorText("Pdb Functions");

    const auto &filtered = fileDescriptor.m_pdbFunctionsFilter.filtered;
    {
        const PdbFunctionInfoVector &functions = revisionDescriptor.m_pdbReader->get_functions();

        UpdateFilter(
            fileDescriptor.m_pdbFunctionsFilter,
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
        if (ImGui::BeginTable("pdb_functions", 5, FileManagerInfoTableFlags))
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

void ImGuiApp::FileManagerInfoPdbExeInfo(const ProgramFileRevisionDescriptor &descriptor)
{
    ImGui::SeparatorText("Pdb Exe Info");

    const PdbExeInfo &exe_info = descriptor.m_pdbReader->get_exe_info();
    ImGui::Text("Exe File Name: %s", exe_info.exeFileName.c_str());
    ImGui::Text("Pdb File Path: %s", exe_info.pdbFilePath.c_str());
}

void ImGuiApp::OutputManagerBody()
{
    // #TODO implement.

    ImGui::TextUnformatted("Not implemented");
}

void ImGuiApp::ComparisonManagerBody(ProgramComparisonDescriptor &descriptor)
{
    {
        ImScoped::Group group;
        ImScoped::Disabled disabled(descriptor.has_active_command());
        {
            const ImVec2 outer_size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8);
            ImScoped::Child child("list_box_container", outer_size);
            ImScoped::Table table("list_box_table", 2, ImGuiTableFlags_SizingStretchSame);
            if (table.IsContentVisible)
            {
                for (size_t i = 0; i < 2; ++i)
                {
                    ImGui::TableNextColumn();
                    ComparisonManagerProgramFileSelection(descriptor, i);
                }
            }
        }

        const IndexT selectedFileIdx0 = descriptor.m_files[0].m_selectedFileIdx;
        const IndexT selectedFileIdx1 = descriptor.m_files[1].m_selectedFileIdx;
        ProgramFileDescriptor *fileDescriptor0 = get_program_file_descriptor(selectedFileIdx0);
        ProgramFileDescriptor *fileDescriptor1 = get_program_file_descriptor(selectedFileIdx1);

        if (fileDescriptor0 != nullptr && fileDescriptor1 != nullptr)
        {
            const bool canCompare0 = fileDescriptor0->can_load() || fileDescriptor0->exe_loaded();
            const bool canCompare1 = fileDescriptor1->can_load() || fileDescriptor1->exe_loaded();
            ImScoped::Disabled disabled(!(canCompare0 && canCompare1));
            // Change button color.
            ImScoped::StyleColor color1(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.8f, 0.6f, 0.6f));
            ImScoped::StyleColor color2(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.8f, 0.8f, 0.8f));
            ImScoped::StyleColor color3(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.8f, 1.0f, 1.0f));
            // Change text color too to make it readable with the light ImGui color theme.
            ImScoped::StyleColor text_color1(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
            ImScoped::StyleColor text_color2(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
            if (ImGui::Button("Compare"))
            {
                load_and_compare_async({fileDescriptor0, fileDescriptor1}, &descriptor);
            }
        }
    }

    // Draw command overlay
    if (descriptor.has_active_command())
    {
        ImRect group_rect;
        group_rect.Min = ImGui::GetItemRectMin();
        group_rect.Max = ImGui::GetItemRectMax();

        const std::string overlay = fmt::format(
            "Processing commands {:d}:{:d} ..",
            descriptor.m_files[0].get_active_command_id(),
            descriptor.m_files[1].get_active_command_id());

        OverlayProgressBar(group_rect, -1.0f * (float)ImGui::GetTime(), overlay.c_str());
    }

    {
        ImScoped::Child child("file_content_container");
        ImScoped::Table table("file_content_table", 2, ImGuiTableFlags_SizingStretchSame);
        if (table.IsContentVisible)
        {
            for (size_t i = 0; i < 2; ++i)
            {
                ImGui::TableNextColumn();
                const ProgramFileRevisionDescriptorPtr &revisionDescriptor = descriptor.m_files[i].m_revisionDescriptor;

                if (revisionDescriptor != nullptr)
                {
                    FileManagerDescriptorLoadStatus(*revisionDescriptor);
                }
            }
        }
    }
}

void ImGuiApp::ComparisonManagerProgramFileSelection(ProgramComparisonDescriptor &descriptor, size_t list_idx)
{
    constexpr char *const list_name[2] = {"Select left file", "Select right file"};

    ImScoped::ID id(static_cast<int>(list_idx));
    ImScoped::Child child("file_list_container");

    ProgramComparisonDescriptor::File &file = descriptor.m_files[list_idx];
    {
        ImGui::Text(list_name[list_idx]);
        const float column_width = ImGui::GetContentRegionAvail().x;
        const float column_height = ImGui::GetContentRegionAvail().y;
        const ImVec2 size(column_width, column_height);
        ImScoped::ListBox list_box("file_list", size);

        if (list_box.IsContentVisible)
        {
            const size_t file_count = m_programFiles.size();
            for (size_t file_idx = 0; file_idx < file_count; ++file_idx)
            {
                ProgramFileDescriptor *program_file = m_programFiles[file_idx].get();
                const std::string name = program_file->create_descriptor_name_with_file_info();
                const bool is_selected = (file.m_selectedFileIdx == file_idx);

                if (ImGui::Selectable(name.c_str(), is_selected))
                {
                    file.m_selectedFileIdx = file_idx;
                }
            }
        }
    }
}

} // namespace unassemblize::gui
