cmake_minimum_required(VERSION 3.16)

project(DesktopLLMHelper VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        taskwidget.cpp
        taskwidget.h
        taskwidget.ui
        hotkeymanager.cpp
        hotkeymanager.h
        taskwindow.cpp
        taskwindow.h
)

# ресурс Windows-иконки
set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
# Qt-ресурсы
set(main_resources "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DesktopLLMHelper
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
            ${app_icon_resource_windows}
            ${main_resources}
    )
else()
    if(ANDROID)
        add_library(DesktopLLMHelper SHARED
                ${PROJECT_SOURCES}
                ${main_resources}
        )
    else()
        add_executable(DesktopLLMHelper
                ${PROJECT_SOURCES}
                ${main_resources}
        )
    endif()
endif()

target_link_libraries(DesktopLLMHelper
        PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        user32
)

# Для MSVC: обеспечить корректное значение __cplusplus
if(MSVC)
    target_compile_options(DesktopLLMHelper PRIVATE "/Zc:__cplusplus")
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DesktopLLMHelper)
endif()
set_target_properties(DesktopLLMHelper PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS DesktopLLMHelper
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DesktopLLMHelper)
endif()