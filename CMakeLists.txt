cmake_minimum_required(VERSION 3.20)

project(DesktopIconCenter VERSION 1.0.3 LANGUAGES CXX RC)

add_executable(DesktopIconCenter WIN32
    src/main.cpp
    src/App.cpp
    src/Config.cpp
    src/DesktopWatcher.cpp
    src/Desktop360Compat.cpp
    src/ExplorerDesktop.cpp
    src/DesktopListView.cpp
    src/IconManager.cpp
    src/GridManager.cpp
    src/TrayIcon.cpp
    src/Logger.cpp
    resource/resource.rc
)

target_include_directories(DesktopIconCenter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/resource
)

target_compile_features(DesktopIconCenter PRIVATE cxx_std_17)
target_compile_definitions(DesktopIconCenter PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    set_property(TARGET DesktopIconCenter PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(DesktopIconCenter PRIVATE /W3 /permissive-)
endif()

target_link_libraries(DesktopIconCenter PRIVATE
    Comctl32
    Shell32
    Ole32
    OleAut32
    Advapi32
    Shlwapi
    Uuid
)
