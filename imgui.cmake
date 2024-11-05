set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui)

FetchContent_Populate(imgui DOWNLOAD_EXTRACT_TIMESTAMP
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.4-docking.zip
    SOURCE_DIR ${IMGUI_DIR}
)

if(WINDOWS)
    add_library(imgui_win32 STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    )

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        target_sources(imgui_win32 PUBLIC
            ${IMGUI_DIR}/misc/debuggers/imgui.natstepfilter
            ${IMGUI_DIR}/misc/debuggers/imgui.natvis
        )
    endif()

    target_sources(imgui_win32 PRIVATE
        ${IMGUI_DIR}/imconfig.h
        ${IMGUI_DIR}/imgui.h
        ${IMGUI_DIR}/backends/imgui_impl_dx9.h
        ${IMGUI_DIR}/backends/imgui_impl_win32.h
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.h
    )

    target_include_directories(imgui_win32 PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
endif()
