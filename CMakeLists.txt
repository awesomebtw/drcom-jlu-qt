cmake_minimum_required(VERSION 3.8)
project(DrcomJluQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Gui Widgets Network Linguist REQUIRED)

# add all source files
file(GLOB SC_FILES "*.cpp" "*.h" "*.qrc")
file(GLOB UI_FILES "*.ui")

# add application icon for windows and macos
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    qt_add_executable(DrcomJluQt ${SC_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/resources/DrcomJluQt.rc")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(${app_icon_macos} "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns")
    set_source_files_properties(${app_icon_macos} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    qt_add_executable(DrcomJluQt MACOSX_BUNDLE ${SC_FILES} ${app_icon_macos})
endif ()

set_source_files_properties(ts/DrcomJluQt_zh_CN.ts PROPERTIES OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/ts")

qt_add_lupdate(DrcomJluQt SOURCES ${SC_FILES} ${UI_FILES} TS_FILES ts/DrcomJluQt_zh_CN.ts)
qt_add_lrelease(DrcomJluQt TS_FILES ts/DrcomJluQt_zh_CN.ts)

target_link_libraries(DrcomJluQt PUBLIC cryptopp)
target_link_libraries(DrcomJluQt PUBLIC Qt6::Gui Qt6::Widgets Qt6::Network)
set_target_properties(DrcomJluQt PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
