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
#include "utility/imgui_text_filter.h"
#include "utility/imgui_misc.h"
#include "runnerasync.h"
#include <chrono>

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
    // clang-format off
    static constexpr ImGuiTableFlags s_fileManagerInfoTableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Hideable |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX |
        ImGuiTableFlags_ScrollX |
        ImGuiTableFlags_ScrollY;
    // clang-format on

    static constexpr std::chrono::system_clock::time_point InvalidTimePoint = std::chrono::system_clock::time_point::min();
    using ProgramFileId = uint32_t;
    using AsmComparisonId = uint32_t;

    struct ProgramFileDescriptor
    {
        ProgramFileDescriptor() : id(s_id++) {}

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

        std::string create_short_exe_name() const;
        std::string create_descriptor_name() const;
        std::string create_descriptor_name_with_short_exe_name() const;

        // All members must be modified by UI thread only

        // Must be not editable when the WorkQueue thread works on this descriptor.
        std::string exeFilename;
        std::string exeConfigFilename = auto_str;
        std::string pdbFilename;
        std::string pdbConfigFilename = auto_str;

        TextFilterDescriptor<const ExeSymbol *> exeSymbolsDescriptor = "exe_symbols_descriptor";
        TextFilterDescriptor<const PdbSymbolInfo *> pdbSymbolsDescriptor = "pdb_symbols_descriptor";
        TextFilterDescriptor<const PdbFunctionInfo *> pdbFunctionsDescriptor = "pdb_functions_descriptor";

        const ProgramFileId id = 0;

        // Has pending asynchronous command(s) running when not invalid.
        WorkQueueCommandId activeCommandId = InvalidWorkQueueCommandId; // #TODO Make vector of chained id's?

        std::unique_ptr<Executable> executable;
        std::unique_ptr<PdbReader> pdbReader;
        std::string exeFilenameFromPdb;
        std::string exeSaveConfigFilename;
        std::string pdbSaveConfigFilename;

        std::chrono::time_point<std::chrono::system_clock> exeLoadTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> exeSaveConfigTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> pdbLoadTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> pdbSaveConfigTimepoint = InvalidTimePoint;

    private:
        static ProgramFileId s_id;
    };
    using ProgramFileDescriptorPtr = std::unique_ptr<ProgramFileDescriptor>;

    struct AsmComparisonDescriptor
    {
        AsmComparisonDescriptor() : id(s_id++) {}

        const AsmComparisonId id = 0;

        bool has_open_window = true;

    private:
        static ProgramFileId s_id;
    };
    using AsmComparisonDescriptorPtr = std::unique_ptr<AsmComparisonDescriptor>;

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

    static WorkQueueCommandPtr create_load_exe_command(ProgramFileDescriptor *descriptor);
    static WorkQueueCommandPtr create_load_pdb_command(ProgramFileDescriptor *descriptor);
    static WorkQueueCommandPtr create_save_exe_config_command(ProgramFileDescriptor *descriptor);
    static WorkQueueCommandPtr create_save_pdb_config_command(ProgramFileDescriptor *descriptor);

    void load_async(ProgramFileDescriptor *descriptor);
    void load_exe_async(ProgramFileDescriptor *descriptor);
    void load_pdb_and_exe_async(ProgramFileDescriptor *descriptor);

    void save_config_async(ProgramFileDescriptor *descriptor);

    void add_file();
    void remove_file(size_t idx);
    void remove_all_files();

    void add_asm_comparison();
    void remove_closed_asm_comparisons();

    static std::string create_section_string(uint32_t section_index, const ExeSections *sections);
    static std::string create_time_string(std::chrono::time_point<std::chrono::system_clock> time_point);

    void BackgroundWindow();
    void FileManagerWindow(bool *p_open);
    void AsmOutputManagerWindow(bool *p_open);
    void AsmComparisonManagerWindows();

    void FileManagerBody();
    void FileManagerDescriptor(ProgramFileDescriptor &descriptor, bool &erased);
    void FileManagerDescriptorExeFile(ProgramFileDescriptor &descriptor);
    void FileManagerDescriptorExeConfig(ProgramFileDescriptor &descriptor);
    void FileManagerDescriptorPdbFile(ProgramFileDescriptor &descriptor);
    void FileManagerDescriptorPdbConfig(ProgramFileDescriptor &descriptor);
    void FileManagerDescriptorActions(ProgramFileDescriptor &descriptor, bool &erased);
    void FileManagerDescriptorSaveLoadStatus(const ProgramFileDescriptor &descriptor);
    void FileManagerGlobalButtons();
    void FileManagerInfo(ProgramFileDescriptor &descriptor);
    void FileManagerInfoExeSections(ProgramFileDescriptor &descriptor);
    void FileManagerInfoExeSymbols(ProgramFileDescriptor &descriptor);
    void FileManagerInfoPdbCompilands(ProgramFileDescriptor &descriptor);
    void FileManagerInfoPdbSourceFiles(ProgramFileDescriptor &descriptor);
    void FileManagerInfoPdbSymbols(ProgramFileDescriptor &descriptor);
    void FileManagerInfoPdbFunctions(ProgramFileDescriptor &descriptor);
    void FileManagerInfoPdbExeInfo(ProgramFileDescriptor &descriptor);

    void AsmOutputManagerBody();

    void AsmComparisonManagerBody(AsmComparisonDescriptor &descriptor);

private:
    ImVec2 m_windowPos = ImVec2(0, 0);
    ImVec2 m_windowSize = ImVec2(0, 0);
    ImVec4 m_clearColor = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);

    bool m_showDemoWindow = true;
    bool m_showFileManager = true;
    bool m_showFileManagerWithTabs = false;
    bool m_showFileManagerExeSectionInfo = true;
    bool m_showFileManagerExeSymbolInfo = true;
    bool m_showFileManagerPdbCompilandInfo = true;
    bool m_showFileManagerPdbSourceFileInfo = true;
    bool m_showFileManagerPdbSymbolInfo = true;
    bool m_showFileManagerPdbFunctionInfo = true;
    bool m_showFileManagerPdbExeInfo = true;

    bool m_showAsmOutputManager = true;

    WorkQueue m_workQueue;

    std::vector<ProgramFileDescriptorPtr> m_programFiles;
    std::vector<AsmComparisonDescriptorPtr> m_asmComparisons;
};

} // namespace unassemblize::gui
