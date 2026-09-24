cmake_minimum_required(VERSION 3.24)
project(NetControl LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "NetControl is a Windows-only application.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WIN32_EXECUTABLE TRUE)

add_executable(NetControl WIN32
    src/main.cpp
    src/App.cpp
    src/ProcessManager.cpp
    src/FirewallManager.cpp
    src/NetworkMonitor.cpp
    src/UsageStore.cpp
    src/Ui.cpp
    src/Theme.cpp
    src/Settings.cpp
    src/L10n.cpp
    src/ConnectionLog.cpp
    src/UsageHistory.cpp
    src/ScheduleManager.cpp
    src/BandwidthManager.cpp
    src/Notifier.cpp
    src/NetworkProfileMonitor.cpp
    src/AutoStart.cpp
    src/TrayIcon.cpp
    resources/app.rc
)

target_include_directories(NetControl PRIVATE include)

target_compile_definitions(NetControl PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000000
)

target_link_libraries(NetControl PRIVATE
    advapi32
    iphlpapi
    ole32
    oleaut32
    shell32
    shlwapi
    user32
    gdi32
    gdiplus
    comctl32
    comdlg32
    ws2_32
)

if(MSVC)
  target_compile_options(NetControl PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

set_target_properties(NetControl PROPERTIES
    VS_DPI_AWARE "PerMonitor"
)

if(MSVC)
  target_link_options(NetControl PRIVATE /MANIFEST:NO)
endif()

enable_testing()
add_executable(NetControlTests
    tests/test_core.cpp
    src/Theme.cpp
    src/Settings.cpp
    src/L10n.cpp
    src/UsageStore.cpp
    src/ScheduleManager.cpp
    src/BandwidthManager.cpp
    src/ConnectionLog.cpp
    src/UsageHistory.cpp
)
set_target_properties(NetControlTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(NetControlTests PRIVATE include)
target_compile_definitions(NetControlTests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
target_link_libraries(NetControlTests PRIVATE shell32 shlwapi user32)
if(MSVC)
  target_compile_options(NetControlTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(NetControlTests PRIVATE /SUBSYSTEM:CONSOLE)
endif()
add_test(NAME core COMMAND NetControlTests)
