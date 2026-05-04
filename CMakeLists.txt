cmake_minimum_required(VERSION 3.21)

# Single source of truth for version — workflows read this with a regex
set(APP_VERSION "0.1.0")

project(WindowManager VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)

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

target_compile_definitions(WindowManager
    PRIVATE
        $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

if(WIN32)
    target_compile_definitions(WindowManager PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(WindowManager PRIVATE user32 gdi32 dwmapi)
endif()

install(TARGETS WindowManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)