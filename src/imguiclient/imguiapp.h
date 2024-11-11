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

// clang-format off
#include <imgui_internal.h>
#include <imgui.h>
// clang-format on
#include "workqueue.h"

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
    struct ProgramFileDescriptor
    {
        void invalidate_command_id();

        bool has_active_command() const { return activeCommandId != InvalidWorkQueueCommandId; }

        bool can_load_exe() const { return !evaluate_exe_filename().empty(); }
        bool can_load_pdb() const { return !pdbFilename.empty(); }
        bool can_load() const { return can_load_exe() || can_load_pdb(); }
        bool can_save_exe_config() const { return executable != nullptr && !evaluate_exe_config_filename().empty(); }
        bool can_save_pdb_config() const { return pdbReader != nullptr && !evaluate_pdb_config_filename().empty(); }
        bool can_save_config() const { return can_save_exe_config() || can_save_pdb_config(); }

        std::string evaluate_exe_filename() const;
        std::string evaluate_exe_config_filename() const;
        std::string evaluate_pdb_config_filename() const;

        // All members must be modified by UI thread only

        // Must be not editable when the WorkQueue thread works on this descriptor.
        std::string exeFilename;
        std::string exeConfigFilename = auto_str;
        std::string pdbFilename;
        std::string pdbConfigFilename = auto_str;

        // Has pending asynchronous command(s) running when not invalid.
        WorkQueueCommandId activeCommandId = InvalidWorkQueueCommandId; // #TODO Make vector of chained id's?

        std::unique_ptr<Executable> executable;
        std::unique_ptr<PdbReader> pdbReader;
        std::string exeFilenameFromPdb;
    };
    using ProgramFileDescriptorPtr = std::unique_ptr<ProgramFileDescriptor>;

    struct WindowPlacement
    {
        ImVec2 pos = ImVec2(-FLT_MAX, -FLT_MAX);
        ImVec2 size = ImVec2(-FLT_MAX, -FLT_MAX);
    };

public:
    ImGuiApp() = default;
    ~ImGuiApp() = default;

    ImGuiStatus init(const CommandLineOptions &clo);

    void prepare_shutdown_wait();
    void prepare_shutdown_nowait();
    bool can_shutdown() const; // Signals that this app can shutdown.
    void shutdown();

    void set_window_pos(ImVec2 pos) { m_windowPos = pos; }
    void set_window_size(ImVec2 size) { m_windowSize = size; }
    ImGuiStatus update();

    const ImVec4 &get_clear_color() const { return m_clearColor; }

private:
    void update_app();

    void load_async(ProgramFileDescriptor *descriptor);
    void load_exe_async(ProgramFileDescriptor *descriptor);
    void load_pdb_async(ProgramFileDescriptor *descriptor);

    void save_config_async(ProgramFileDescriptor *descriptor);
    void save_exe_config_async(ProgramFileDescriptor *descriptor);
    void save_pdb_config_async(ProgramFileDescriptor *descriptor);

    static void TooltipText(const char *fmt, ...);
    static void TooltipTextV(const char *fmt, va_list args);
    static void TooltipTextUnformatted(const char *text, const char *text_end = nullptr);
    static void TooltipTextMarker(const char *fmt, ...);
    static void TooltipTextUnformattedMarker(const char *text, const char *text_end = nullptr);

    static void OverlayProgressBar(const ImRect &rect, float fraction, const char *overlay = nullptr);

    static void ApplyPlacementToNextWindow(WindowPlacement &placement);
    static void FetchPlacementFromWindowByName(WindowPlacement &placement, const char *window_name);

    void AddFileDialogButton(
        std::string *filePathName,
        const char *button_label,
        const std::string &vKey,
        const std::string &vTitle,
        const char *vFilters);

    void FileManagerWindow(bool *p_open);
    void AsmOutputManagerWindow(bool *p_open);
    void AsmComparisonManagerWindow(bool *p_open);

    void FileManagerBody();
    void FileManagerDescriptorExeFile(ProgramFileDescriptor &descriptor, size_t idx);
    void FileManagerDescriptorExeConfig(ProgramFileDescriptor &descriptor, size_t idx);
    void FileManagerDescriptorPdbFile(ProgramFileDescriptor &descriptor, size_t idx);
    void FileManagerDescriptorPdbConfig(ProgramFileDescriptor &descriptor, size_t idx);
    void FileManagerDescriptorActions(ProgramFileDescriptor &descriptor, bool &erased);
    void FileManagerFooter();

    void AsmOutputManagerBody();
    void AsmComparisonManagerBody();

private:
    ImVec2 m_windowPos = ImVec2(0, 0);
    ImVec2 m_windowSize = ImVec2(0, 0);
    ImVec4 m_clearColor = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);

    bool m_showDemoWindow = true;
    bool m_showProgramFileManager = true;
    bool m_showAsmOutputManager = true;
    bool m_showAsmComparisonManager = true;

    bool m_autoLoad = false;

    WindowPlacement m_lastFileDialogPlacement; // #TODO: Save this in a Json config file and restore on boot.

    WorkQueue m_workQueue;

    std::vector<ProgramFileDescriptorPtr> m_programFiles;
};

} // namespace unassemblize::gui
