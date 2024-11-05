set(IMGUIFILEDIALOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imguifiledialog)

FetchContent_Populate(imguifiledialog DOWNLOAD_EXTRACT_TIMESTAMP
    URL https://github.com/xezon/ImGuiFileDialog/archive/refs/tags/2024-11-05.zip
    SOURCE_DIR ${IMGUIFILEDIALOG_DIR}
)

# Note: Is in part a copy of /3rdparty/imguifiledialog/CMakeLists.txt

add_library(ImGuiFileDialog STATIC
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.h
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialogConfig.h
)

target_include_directories(ImGuiFileDialog PUBLIC ${IMGUIFILEDIALOG_DIR})
target_include_directories(ImGuiFileDialog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui)

if(UNIX)
    target_compile_options(ImGuiFileDialog PUBLIC -Wno-unknown-pragmas)
endif()
