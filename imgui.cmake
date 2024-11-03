FetchContent_Populate(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.4-docking.zip
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

if(WINDOWS)
    add_library(imgui_win32 STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_dx9.cpp
        imgui/backends/imgui_impl_win32.cpp
    )

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        target_sources(imgui_win32 PUBLIC
            imgui/misc/debuggers/imgui.natstepfilter
            imgui/misc/debuggers/imgui.natvis
        )
    endif()

    target_sources(imgui_win32 PRIVATE
        imgui/imconfig.h
        imgui/imgui.h
        imgui/backends/imgui_impl_dx9.h
        imgui/backends/imgui_impl_win32.h
    )

    target_include_directories(imgui_win32 PUBLIC
        imgui
        imgui/backends
    )
endif()
