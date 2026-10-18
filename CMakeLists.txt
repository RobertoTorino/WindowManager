<<<<<<< HEAD
cmake_minimum_required(VERSION 3.21)

# Single source of truth for version — workflows read this with a regex
set(APP_VERSION "0.1.3")

project(WindowManager VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)

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
    # Slice 2 – Utilities
    src/core/Utilities.h
    src/core/Utilities.cpp
    # Slice 3 – MonitorHelper
    src/window/MonitorHelper.h
    src/window/MonitorHelper.cpp
    # Slice 4 – WindowManager core
    src/window/WindowManager.h
    src/window/WindowManager.cpp
    # Slice 5 – WindowStabilizer
    src/window/WindowStabilizer.h
    src/window/WindowStabilizer.cpp
    # Kill-scope decision logic (pure, unit-tested)
    src/window/KillPlanner.h
    src/window/KillPlanner.cpp
    # Fit-to-monitor maths and launch-settle trigger (pure, unit-tested)
    src/window/FitCalculator.h
    src/window/FitCalculator.cpp
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
    src/ui/ScaledView.cpp
    src/ui/InfoDialogs.h
    src/ui/InfoDialogs.cpp
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
)

target_include_directories(WindowManager PRIVATE src)

target_compile_definitions(WindowManager
    PRIVATE
        APP_VERSION_STR="${APP_VERSION}"
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

if(WIN32)
    target_compile_definitions(WindowManager PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(WindowManager PRIVATE user32 gdi32 dwmapi)
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
    add_wm_test(tst_supersample src/window/Supersample.cpp)
    add_wm_test(tst_appconfig   src/config/AppConfig.cpp src/core/Logger.cpp)
endif()

=======
cmake_minimum_required(VERSION 3.21)

# Single source of truth for version — workflows read this with a regex
set(APP_VERSION "0.1.3")

project(WindowManager VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)

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
    # Slice 2 – Utilities
    src/core/Utilities.h
    src/core/Utilities.cpp
    # Slice 3 – MonitorHelper
    src/window/MonitorHelper.h
    src/window/MonitorHelper.cpp
    # Slice 4 – WindowManager core
    src/window/WindowManager.h
    src/window/WindowManager.cpp
    # Slice 5 – WindowStabilizer
    src/window/WindowStabilizer.h
    src/window/WindowStabilizer.cpp
    # Kill-scope decision logic (pure, unit-tested)
    src/window/KillPlanner.h
    src/window/KillPlanner.cpp
    # Fit-to-monitor maths and launch-settle trigger (pure, unit-tested)
    src/window/FitCalculator.h
    src/window/FitCalculator.cpp
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
    src/ui/ScaledView.cpp
    src/ui/InfoDialogs.h
    src/ui/InfoDialogs.cpp
    src/ui/HelpDialog.h
    src/ui/HelpDialog.cpp
    src/ui/LogViewerDialog.h
    src/ui/LogViewerDialog.cpp
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
)

target_include_directories(WindowManager PRIVATE src)

target_compile_definitions(WindowManager
    PRIVATE
        APP_VERSION_STR="${APP_VERSION}"
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

if(WIN32)
    target_compile_definitions(WindowManager PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(WindowManager PRIVATE user32 gdi32 dwmapi)
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
    add_wm_test(tst_supersample src/window/Supersample.cpp)
    add_wm_test(tst_appconfig   src/config/AppConfig.cpp src/core/Logger.cpp)
endif()
>>>>>>> 34314f6 (see changelog)
