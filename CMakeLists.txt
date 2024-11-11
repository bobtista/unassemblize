cmake_minimum_required(VERSION 3.16)

# Use packagename_ROOT for FindPackage.
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Disable default MSVC warning level so we can set it ourselves.
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

# Disable default MSVC runtime hardcoding.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(unassemblize LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

# Set up a format target to do automated clang format checking.
find_package(ClangFormat)
include(ClangFormat)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    set(WINDOWS TRUE)
endif()

include(FetchContent)

# TODO: How can we move this to 3rdparty without error?
FetchContent_Declare(
    zydis
    GIT_REPOSITORY https://github.com/zyantific/zydis.git
    GIT_TAG        1ba75aeefae37094c7be8eba07ff81d4fe0f1f20
)
set(ZYDIS_BUILD_EXAMPLES OFF)
set(ZYDIS_FEATURE_ENCODER OFF)
FetchContent_MakeAvailable(zydis)

# TODO: How can we move this to 3rdparty without error?
FetchContent_Declare(
    lief
    GIT_REPOSITORY https://github.com/lief-project/LIEF.git
    GIT_TAG        2d9855fc7f9d4ce6325245f8b75c98eb7663db60
)
set(LIEF_EXAMPLES OFF)
set(LIEF_INSTALL OFF)
set(LIEF_C_API OFF)
set(LIEF_PYTHON_API OFF)
set(LIEF_TESTS OFF)
set(LIEF_ART OFF)
set(LIEF_DEX OFF)
set(LIEF_VDEX OFF)
FetchContent_MakeAvailable(lief)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nlohmann_json
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        0c9fce2ffefecfdce794e1859584e25877b7b592
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        3bf268481da8208d171d8908e6491459de3651d7
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cxxopts
)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(readerwriterqueue
    GIT_REPOSITORY https://github.com/xezon/readerwriterqueue
    GIT_TAG        master
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/readerwriterqueue
)
FetchContent_MakeAvailable(readerwriterqueue)

set(GIT_PRE_CONFIGURE_FILE "gitinfo.cpp.in")
set(GIT_POST_CONFIGURE_FILE "${CMAKE_CURRENT_BINARY_DIR}/gitinfo.cpp")

include(GitWatcher)
include(imgui.cmake)
include(imguifiledialog.cmake)
if(WINDOWS)
include(FindDiaSDK.cmake)
endif()

if(WINDOWS)
# No console by default.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

add_executable(unassemblize)

target_sources(unassemblize PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/gitinfo.cpp
    gitinfo.h
    FindDiaSDK.cmake
    imgui.cmake
    imguifiledialog.cmake
    src/asmmatcher.cpp
    src/asmmatcher.h
    src/asmmatchertypes.cpp
    src/asmmatchertypes.h
    src/asmprinter.cpp
    src/asmprinter.h
    src/commontypes.h
    src/executable.cpp
    src/executable.h
    src/executabletypes.cpp
    src/executabletypes.h
    src/function.cpp
    src/function.h
    src/functiontypes.cpp
    src/functiontypes.h
    src/main.cpp
    src/options.cpp
    src/options.h
    src/pdbreader.cpp
    src/pdbreader.h
    src/pdbreadertypes.cpp
    src/pdbreadertypes.h
    src/runner.cpp
    src/runner.h
    src/util.cpp
    src/util.h
    src/version.cpp
    src/version.h
    src/workqueue.cpp
    src/workqueue.h
    src/imguiclient/imguiapp.cpp
    src/imguiclient/imguiapp.h
    src/imguiclient/imguiwin32.cpp
    src/imguiclient/imguiwin32.h
    src/imguiclient/utility/imgui_scoped.h
)

target_link_libraries(unassemblize PRIVATE
    Zydis
    LIEF::LIEF
    nlohmann_json
    fmt::fmt
    cxxopts::cxxopts
    readerwriterqueue
)

target_include_directories(unassemblize PRIVATE
    .
    src
)

if(WINDOWS)
    target_sources(unassemblize PRIVATE
        wincompat/strings.h
    )

    target_link_libraries(unassemblize PRIVATE
        d3d9.lib # How does it find it???
        imgui_win32
        ImGuiFileDialog
        ${DIASDK_LIBRARIES}
    )

    target_include_directories(unassemblize PRIVATE
        wincompat
        ${DIASDK_INCLUDE_DIRS}
    )
endif()

target_compile_definitions(unassemblize PRIVATE
    $<$<CONFIG:MinSizeRel,Release,RelWithDebInfo>:RELEASE=1> # Can we do this nicer?
)
