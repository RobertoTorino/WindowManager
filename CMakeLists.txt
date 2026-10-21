cmake_minimum_required(VERSION 3.21)

# Single source of truth for version — workflows read this with a regex
set(APP_VERSION "0.1.11")

project(WindowManager VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Network)

option(WINDOWMANAGER_BUILD_TESTS "Build unit tests" ON)

set(WINDOW_MANAGER_SOURCES
    resources/resources.qrc
    $<$<PLATFORM_ID:Windows>:resources/app.rc>
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/WindowManagerController.cpp
    src/WindowManagerController.h
    # Slice 1 – Logger
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/BuildInfo.h
    # Slice 2 – Utilities
    src/core/Utilities.h
    src/core/Utilities.cpp
    # Ini text editing in place (pure, unit-tested through tst_helix and tst_gameconfig)
    src/core/IniText.h
    src/core/IniText.cpp
    # Slice 3 – MonitorHelper
    src/window/MonitorHelper.h
    src/window/MonitorHelper.cpp
    # Slice 4 – WindowManager core
    src/window/WindowManager.h
    src/window/WindowManager.cpp
    # Slice 5 – WindowStabilizer (implemented per platform below)
    src/window/WindowStabilizer.h
    # Operating-system access behind one interface; one backend per platform
    src/platform/PlatformBackend.h
    src/platform/PlatformBackend.cpp
    $<$<PLATFORM_ID:Windows>:src/platform/win32/Win32Backend.cpp>
    $<$<PLATFORM_ID:Windows>:src/platform/win32/Win32WindowStabilizer.cpp>
    $<$<PLATFORM_ID:Linux>:src/platform/x11/X11Backend.cpp>
    $<$<PLATFORM_ID:Linux>:src/platform/PollingWindowStabilizer.cpp>
    $<$<PLATFORM_ID:Linux>:src/platform/unsupported/ScaledViewUnsupported.cpp>
    # System display settings (Display tab): pure model (unit-tested), interface, per-game session
    src/display/DisplayModel.h
    src/display/DisplayModel.cpp
    src/display/DisplayBackend.h
    src/display/DisplaySession.h
    src/display/DisplaySession.cpp
    $<$<PLATFORM_ID:Windows>:src/platform/win32/Win32Display.cpp>
    $<$<NOT:$<PLATFORM_ID:Windows>>:src/platform/unsupported/DisplayUnsupported.cpp>
    # A game's own config files (Game config tab): pure model (unit-tested) and the file writing
    src/gameconfig/GameConfigModel.h
    src/gameconfig/GameConfigModel.cpp
    src/gameconfig/GameConfigFiles.h
    src/gameconfig/GameConfigFiles.cpp
    # Focus switch: who has the keyboard, the hotkey (pure, unit-tested)
    src/window/FocusSwitch.h
    src/window/FocusSwitch.cpp
    # Game stats bar: pure model (unit-tested) and the reading per platform
    src/stats/StatsModel.h
    src/stats/StatsModel.cpp
    src/stats/PictureInfo.h
    src/stats/PictureInfo.cpp
    src/stats/SessionReport.h
    src/stats/SessionReport.cpp
    src/stats/StatsBackend.h
    src/stats/DriverModel.h
    src/stats/DriverModel.cpp
    src/stats/DriverCheck.h
    src/stats/DriverCheck.cpp
    $<$<PLATFORM_ID:Windows>:src/platform/win32/Win32Stats.cpp>
    $<$<NOT:$<PLATFORM_ID:Windows>>:src/platform/unsupported/StatsUnsupported.cpp>
    # Kill-scope decision logic (pure, unit-tested)
    src/window/KillPlanner.h
    src/window/KillPlanner.cpp
    # Diagnostic log of the game's windows, focus and mouse (line logic pure, unit-tested)
    src/window/GameWatch.h
    src/window/GameWatchModel.h
    src/window/GameWatchModel.cpp
    $<$<PLATFORM_ID:Windows>:src/platform/win32/Win32GameWatch.cpp>
    $<$<NOT:$<PLATFORM_ID:Windows>>:src/platform/unsupported/GameWatchUnsupported.cpp>
    # Fit-to-monitor maths and launch-settle trigger (pure, unit-tested)
    src/window/FitCalculator.h
    src/window/FitCalculator.cpp
    # Steam game identification by AppID (pure, unit-tested)
    src/window/SteamMatch.h
    src/window/SteamMatch.cpp
    # Games installed through Steam: library and manifest files, main executable (pure parts unit-tested)
    src/window/SteamLibrary.h
    src/window/SteamLibrary.cpp
    # Downscaled-view geometry (pure, unit-tested)
    src/window/Supersample.h
    src/window/Supersample.cpp
    # UI
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/GameRegistry.h
    src/ui/GameRegistry.cpp
    src/ui/GameBar.h
    src/ui/GameBar.cpp
    src/ui/WindowTable.h
    src/ui/WindowTable.cpp
    src/ui/SnapshotCapture.h
    src/ui/SnapshotCapture.cpp
    src/ui/ControlPanels.h
    src/ui/ControlPanels.cpp
    src/ui/FitDialog.h
    src/ui/FitDialog.cpp
    src/ui/ScaledView.h
    $<$<NOT:$<PLATFORM_ID:Linux>>:src/ui/ScaledView.cpp>
    src/ui/InfoDialogs.h
    src/ui/InfoDialogs.cpp
    src/ui/HelpDialog.h
    src/ui/HelpDialog.cpp
    src/ui/LogViewerDialog.h
    src/ui/LogViewerDialog.cpp
    src/ui/HelixPanel.h
    src/ui/HelixPanel.cpp
    src/ui/MagpiePanel.h
    src/ui/MagpiePanel.cpp
    src/ui/MagpieSession.h
    src/ui/MagpieSession.cpp
    src/ui/DisplayPanel.h
    src/ui/DisplayPanel.cpp
    src/ui/GameConfigPanel.h
    src/ui/GameConfigPanel.cpp
    src/ui/FocusMonitor.h
    src/ui/FocusMonitor.cpp
    src/ui/GameOverlay.h
    src/ui/GameOverlay.cpp
    src/ui/PictureCompare.h
    src/ui/PictureCompare.cpp
    src/ui/ReportCharts.h
    src/ui/ReportCharts.cpp
    src/ui/ReportsPanel.h
    src/ui/ReportsPanel.cpp
    src/ui/SessionRecorder.h
    src/ui/SessionRecorder.cpp
    src/ui/StatsMonitor.h
    src/ui/StatsMonitor.cpp
    src/ui/MiniBar.h
    src/ui/MiniBar.cpp
    src/ui/GpuPanel.h
    src/ui/GpuPanel.cpp
    # HelixSR: release/version parsing, game detection, on-disk layout (pure, unit-tested)
    src/helix/HelixRelease.h
    src/helix/HelixRelease.cpp
    src/helix/HelixGame.h
    src/helix/HelixGame.cpp
    src/helix/HelixPaths.h
    src/helix/HelixPaths.cpp
    src/helix/HelixIni.h
    src/helix/HelixIni.cpp
    src/helix/HelixSystem.h
    src/helix/HelixSystem.cpp
    src/helix/HelixBridge.h
    src/helix/HelixBridge.cpp
    src/helix/SystemProbe.h
    src/helix/SystemProbe.cpp
    # Magpie (window upscaler): its config.json, profiles that scale a game by themselves (pure, unit-tested)
    src/magpie/MagpieConfig.h
    src/magpie/MagpieConfig.cpp
    src/magpie/MagpieRelease.h
    src/magpie/MagpieRelease.cpp
    # Magpie: the app's own copy (download, check, unpack, first start) and its process
    src/magpie/MagpieProcess.h
    src/magpie/MagpieProcess.cpp
    src/magpie/MagpieSetup.h
    src/magpie/MagpieSetup.cpp
    # HelixSR: download, build and per-game install (Qt Network + processes)
    src/helix/HelixSetup.h
    src/helix/HelixSetup.cpp
    # AppConfig (replaces ConfigManager, keyed by exe path)
    src/config/AppConfig.h
    src/config/AppConfig.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_standard_project_setup()

    qt_add_executable(WindowManager
        WIN32
        ${WINDOW_MANAGER_SOURCES}
    )
else()
    add_executable(WindowManager WIN32 ${WINDOW_MANAGER_SOURCES})
endif()

target_link_libraries(WindowManager
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

# Build number and date: a header regenerated on every build (only rewritten when it changes).
set(BUILDINFO_DIR ${CMAKE_BINARY_DIR}/generated)
set(BUILDINFO_HEADER ${BUILDINFO_DIR}/build_stamp.h)
set(BUILDINFO_COMMAND ${CMAKE_COMMAND} -DOUT=${BUILDINFO_HEADER} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                      -P ${CMAKE_SOURCE_DIR}/cmake/BuildInfo.cmake)
execute_process(COMMAND ${BUILDINFO_COMMAND})   # exists from the first configure on
add_custom_target(buildinfo ALL
    COMMAND ${BUILDINFO_COMMAND}
    BYPRODUCTS ${BUILDINFO_HEADER}
    COMMENT "Updating build info")
add_dependencies(WindowManager buildinfo)

target_include_directories(WindowManager PRIVATE src ${BUILDINFO_DIR})

target_compile_definitions(WindowManager
    PRIVATE
        APP_VERSION_STR="${APP_VERSION}"
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
    target_link_libraries(WindowManager PRIVATE PkgConfig::XCB)
endif()

if(WIN32)
    target_compile_definitions(WindowManager PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(WindowManager PRIVATE user32 gdi32 dwmapi d3d12 dxgi dxva2 wbemuuid ole32 oleaut32
                                                pdh powrprof advapi32)
endif()

# Copy the Qt runtime next to the exe after every build so it starts without
# Qt's bin directory on PATH (e.g. when double-clicked from Explorer).
if(WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET WindowManager POST_BUILD
        COMMAND Qt6::windeployqt --no-translations --no-compiler-runtime
                "$<TARGET_FILE:WindowManager>"
        COMMENT "Deploying Qt runtime")
endif()

install(TARGETS WindowManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# ---------------------------------------------------------------------------
# Tests (QtTest + CTest):  ctest --test-dir <build> --output-on-failure
# ---------------------------------------------------------------------------
if(WINDOWMANAGER_BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    enable_testing()

    function(add_wm_test name)
        add_executable(${name} tests/${name}.cpp ${ARGN})
        target_include_directories(${name} PRIVATE src)
        target_link_libraries(${name} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Core)
        add_test(NAME ${name} COMMAND ${name})
        if(WIN32)
            # Let the test find Qt DLLs without a globally configured PATH
            set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>")
        endif()
    endfunction()

    add_wm_test(tst_utilities   src/core/Utilities.cpp)
    add_wm_test(tst_killplanner src/window/KillPlanner.cpp)
    add_wm_test(tst_fit         src/window/FitCalculator.cpp)
    add_wm_test(tst_gamewatch   src/window/GameWatchModel.cpp)
    add_wm_test(tst_supersample src/window/Supersample.cpp)
    add_wm_test(tst_steammatch  src/window/SteamMatch.cpp)
    add_wm_test(tst_focusswitch src/window/FocusSwitch.cpp)
    # Game config: keys per file, folder layout, JConfig's file; the file writing against a fake game folder
    add_wm_test(tst_gameconfig  src/gameconfig/GameConfigModel.cpp src/gameconfig/GameConfigFiles.cpp
                                src/core/IniText.cpp src/core/Logger.cpp)
    add_wm_test(tst_appconfig   src/config/AppConfig.cpp src/core/Logger.cpp src/display/DisplayModel.cpp
                                src/gameconfig/GameConfigModel.cpp src/core/IniText.cpp
                                src/magpie/MagpieConfig.cpp)
    # WindowManager and MonitorHelper against a fake PlatformBackend (no real windows)
    add_wm_test(tst_windowmanager
                src/window/WindowManager.cpp src/window/MonitorHelper.cpp
                src/window/KillPlanner.cpp src/window/FitCalculator.cpp src/window/SteamMatch.cpp
                src/platform/PlatformBackend.cpp
                src/config/AppConfig.cpp src/core/Logger.cpp src/display/DisplayModel.cpp
                src/gameconfig/GameConfigModel.cpp src/core/IniText.cpp src/magpie/MagpieConfig.cpp)
    target_link_libraries(tst_windowmanager PRIVATE Qt${QT_VERSION_MAJOR}::Gui)
    # Steam library parsing, a fake Steam folder, and the sync into the saved games
    add_wm_test(tst_steamlibrary
                src/window/SteamLibrary.cpp src/ui/GameRegistry.cpp
                src/config/AppConfig.cpp src/core/Utilities.cpp src/core/Logger.cpp
                src/display/DisplayModel.cpp src/gameconfig/GameConfigModel.cpp src/core/IniText.cpp
                src/magpie/MagpieConfig.cpp)
    target_link_libraries(tst_steamlibrary PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    if(UNIX AND NOT APPLE)
        # The real X11 backend against an xterm (skips itself without a display)
        add_wm_test(tst_x11backend src/platform/x11/X11Backend.cpp src/platform/PlatformBackend.cpp)
        target_link_libraries(tst_x11backend PRIVATE Qt${QT_VERSION_MAJOR}::Gui PkgConfig::XCB)
    endif()
    add_wm_test(tst_picture    src/stats/PictureInfo.cpp src/stats/StatsModel.cpp)
    add_wm_test(tst_report     src/stats/SessionReport.cpp)
    add_wm_test(tst_magpie     src/magpie/MagpieConfig.cpp src/magpie/MagpieRelease.cpp src/helix/HelixRelease.cpp)
    add_wm_test(tst_helix      src/helix/HelixRelease.cpp src/helix/HelixGame.cpp
                                src/helix/HelixPaths.cpp src/helix/HelixSystem.cpp
                                src/helix/HelixIni.cpp src/helix/SystemProbe.cpp src/core/IniText.cpp
                                src/helix/HelixBridge.cpp)
    if(WIN32)
        target_link_libraries(tst_helix PRIVATE d3d12 dxgi)   # SystemProbe asks the GPU driver
    endif()
    # Display model (modes, scale steps, brightness, per-game plan / undo); on Windows also a
    # read-only probe of this PC's displays (nothing is changed)
    if(WIN32)
        add_wm_test(tst_display src/display/DisplayModel.cpp src/platform/win32/Win32Display.cpp)
        target_link_libraries(tst_display PRIVATE user32 dxva2 wbemuuid ole32 oleaut32)
    else()
        add_wm_test(tst_display src/display/DisplayModel.cpp src/platform/unsupported/DisplayUnsupported.cpp)
    endif()
    # Stats model (GPU counter names, CPU / frame-rate maths, warnings); on Windows also a
    # read-only sample of the test process itself
    if(WIN32)
        add_wm_test(tst_stats src/stats/StatsModel.cpp src/stats/DriverModel.cpp src/platform/win32/Win32Stats.cpp)
        target_link_libraries(tst_stats PRIVATE dxgi pdh powrprof advapi32 ole32)
    else()
        add_wm_test(tst_stats src/stats/StatsModel.cpp src/stats/DriverModel.cpp
                    src/platform/unsupported/StatsUnsupported.cpp)
    endif()
endif()








