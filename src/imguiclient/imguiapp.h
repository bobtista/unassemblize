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
    static constexpr uint32_t InvalidId = 0;

    using ProgramFileId = uint32_t;
    using ProgramFileRevisionId = uint32_t;
    using ProgramComparisonId = uint32_t;

    struct ProgramFileDescriptor;
    struct ProgramFileRevisionDescriptor;
    struct ProgramComparisonDescriptor;

    using ProgramFileDescriptorPtr = std::unique_ptr<ProgramFileDescriptor>;
    using ProgramFileRevisionDescriptorPtr = std::shared_ptr<ProgramFileRevisionDescriptor>;
    using ProgramComparisonDescriptorPtr = std::unique_ptr<ProgramComparisonDescriptor>;

    struct ProgramFileDescriptor
    {
        ProgramFileDescriptor();
        ~ProgramFileDescriptor();

        void invalidate_command_id();

        bool has_active_command() const;

        bool can_load_exe() const;
        bool can_load_pdb() const;
        bool can_load() const;
        bool can_save_exe_config() const;
        bool can_save_pdb_config() const;
        bool can_save_config() const;

        std::string evaluate_exe_filename() const;
        std::string evaluate_exe_config_filename() const;
        std::string evaluate_pdb_config_filename() const;

        std::string create_short_exe_name() const;
        std::string create_descriptor_name() const;
        std::string create_descriptor_name_with_file_info() const;

        ProgramFileRevisionId get_revision_id() const;

        // Note: All members must be modified by UI thread only

        const ProgramFileId m_id = InvalidId;

        // Has pending asynchronous command(s) running when not invalid.
        WorkQueueCommandId m_activeCommandId = InvalidWorkQueueCommandId; // #TODO Make vector of chained id's?

        // Must be not editable when the WorkQueue thread works on this descriptor.
        std::string m_exeFilename;
        std::string m_exeConfigFilename = auto_str;
        std::string m_pdbFilename;
        std::string m_pdbConfigFilename = auto_str;

        TextFilterDescriptor<const ExeSymbol *> m_exeSymbolsFilter = "exe_symbols_filter";
        TextFilterDescriptor<const PdbSymbolInfo *> m_pdbSymbolsFilter = "pdb_symbols_filter";
        TextFilterDescriptor<const PdbFunctionInfo *> m_pdbFunctionsFilter = "pdb_functions_filter";

        ProgramFileRevisionDescriptorPtr m_fileRevisionDescriptor;

    private:
        static ProgramFileId s_id;
    };

    // Note: Pass down a shared pointer of the ProgramSymbolsDescriptor when chaining async commands.
    struct ProgramFileRevisionDescriptor
    {
        ProgramFileRevisionDescriptor();
        ~ProgramFileRevisionDescriptor();

        bool can_load_exe() const;
        bool can_load_pdb() const;
        bool can_save_exe_config() const;
        bool can_save_pdb_config() const;

        std::string evaluate_exe_filename() const;
        std::string evaluate_exe_config_filename() const;
        std::string evaluate_pdb_config_filename() const;

        std::string create_short_exe_name() const;
        std::string create_descriptor_name() const;
        std::string create_descriptor_name_with_file_info() const;

        const ProgramFileRevisionId m_id = InvalidId;

        // String copies of the file descriptor at the time of async command chain creation.
        // These allows to evaluate async save load operations without a dependency to the file descriptor.
        std::string m_exeFilenameCopy;
        std::string m_exeConfigFilenameCopy;
        std::string m_pdbFilenameCopy;
        std::string m_pdbConfigFilenameCopy;

        std::unique_ptr<Executable> m_executable;
        std::unique_ptr<PdbReader> m_pdbReader;
        std::string m_exeFilenameFromPdb;
        std::string m_exeSaveConfigFilename;
        std::string m_pdbSaveConfigFilename;

        std::chrono::time_point<std::chrono::system_clock> m_exeLoadTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> m_exeSaveConfigTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> m_pdbLoadTimepoint = InvalidTimePoint;
        std::chrono::time_point<std::chrono::system_clock> m_pdbSaveConfigTimepoint = InvalidTimePoint;

        NamedFunctions m_namedFunctions;

    private:
        static ProgramFileRevisionId s_id;
    };

    struct ProgramComparisonDescriptor
    {
        struct File
        {
            size_t selectedFileIdx = 0; // Selected file index in list box. Does not necessarily link to actual loaded file.
            ProgramFileRevisionDescriptorPtr fileRevisionDescriptor;
            NamedFunctionBundles compilandBundles;
            NamedFunctionBundles sourceFileBundles;
            NamedFunctionBundle singleBundle;
        };

        ProgramComparisonDescriptor();
        ~ProgramComparisonDescriptor();

        const ProgramComparisonId id = InvalidId;

        bool has_open_window = true;

        std::array<File, 2> files;
        MatchedFunctions matchedFunctions;

    private:
        static ProgramFileId s_id;
    };

public:
    ImGuiApp();
    ~ImGuiApp();

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

    static WorkQueueCommandPtr create_load_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);
    static WorkQueueCommandPtr create_load_exe_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);
    static WorkQueueCommandPtr create_load_pdb_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);
    static WorkQueueCommandPtr create_load_pdb_and_exe_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);

    static WorkQueueCommandPtr create_save_exe_config_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);
    static WorkQueueCommandPtr create_save_pdb_config_command(
        ProgramFileDescriptor *fileDescriptor,
        ProgramFileRevisionDescriptorPtr &revisionDescriptor);

    ProgramFileDescriptor *get_program_file_descriptor(size_t program_file_idx);

    void load_async(ProgramFileDescriptor *descriptor);

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
    void FileManagerDescriptorSaveLoadStatus(const ProgramFileRevisionDescriptor &descriptor);
    void FileManagerGlobalButtons();
    void FileManagerInfo(ProgramFileDescriptor &fileDescriptor, const ProgramFileRevisionDescriptor &revisionDescriptor);
    void FileManagerInfoExeSections(const ProgramFileRevisionDescriptor &descriptor);
    void FileManagerInfoExeSymbols(
        ProgramFileDescriptor &fileDescriptor,
        const ProgramFileRevisionDescriptor &revisionDescriptor);
    void FileManagerInfoPdbCompilands(const ProgramFileRevisionDescriptor &descriptor);
    void FileManagerInfoPdbSourceFiles(const ProgramFileRevisionDescriptor &descriptor);
    void FileManagerInfoPdbSymbols(
        ProgramFileDescriptor &fileDescriptor,
        const ProgramFileRevisionDescriptor &revisionDescriptor);
    void FileManagerInfoPdbFunctions(
        ProgramFileDescriptor &fileDescriptor,
        const ProgramFileRevisionDescriptor &revisionDescriptor);
    void FileManagerInfoPdbExeInfo(const ProgramFileRevisionDescriptor &descriptor);

    void AsmOutputManagerBody();

    void AsmComparisonManagerBody(ProgramComparisonDescriptor &descriptor);

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
    std::vector<ProgramComparisonDescriptorPtr> m_programComparisons;
};

} // namespace unassemblize::gui
