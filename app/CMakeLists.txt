cmake_minimum_required(VERSION 3.16)

project(FMS VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Svg
    Multimedia
    MultimediaWidgets
    OpenGLWidgets
    Sql
    LinguistTools
)

set(ANT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QtAntDesign")

file(GLOB ANT_SRC "${ANT_DIR}/*.cpp")
file(GLOB ANT_HEADERS "${ANT_DIR}/*.h" "${ANT_DIR}/*.hpp")
file(GLOB ANT_QRC "${ANT_DIR}/*.qrc")
file(GLOB_RECURSE ANT_THIRD_PARTY_SRC "${ANT_DIR}/ThirdParty/*.cpp")
file(GLOB_RECURSE ANT_THIRD_PARTY_HEADERS "${ANT_DIR}/ThirdParty/*.h" "${ANT_DIR}/ThirdParty/*.hpp")

list(REMOVE_ITEM ANT_SRC
    "${ANT_DIR}/main.cpp"
    "${ANT_DIR}/QtAntDesign.cpp"
)
list(REMOVE_ITEM ANT_HEADERS
    "${ANT_DIR}/QtAntDesign.h"
)

set(PROJECT_SOURCES
    src/main.cpp
    resources/database.qrc
    src/ui/AppShell.cpp
    src/ui/AppShell.h
    src/ui/AppShell.ui
    ${ANT_SRC}
    ${ANT_HEADERS}
    ${ANT_QRC}
    ${ANT_THIRD_PARTY_SRC}
    ${ANT_THIRD_PARTY_HEADERS}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(FMS
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/core/DatabaseManager.h src/core/DatabaseManager.cpp
        src/services/AuthService.h src/services/AuthService.cpp
        src/services/FileService.h src/services/FileService.cpp
        src/services/FolderService.h src/services/FolderService.cpp

    )
else()
    add_executable(FMS
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(FMS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/services"
    "${ANT_DIR}"
    "${ANT_DIR}/ThirdParty/QR-Code-generator-master"
    "${ANT_DIR}/ThirdParty/FastGaussianBlur-main"
)

target_link_libraries(FMS PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Sql
)

if(WIN32)
    target_link_libraries(FMS PRIVATE Dwmapi)
endif()

set_target_properties(FMS PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS FMS
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FMS)
endif()
