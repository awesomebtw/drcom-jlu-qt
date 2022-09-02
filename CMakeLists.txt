cmake_minimum_required(VERSION 3.21)
project(DrcomJluQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Gui Widgets Network Linguist REQUIRED)

FILE(GLOB SC_FILES "*.cpp" "*.h" "*.qrc")
FILE(GLOB UI_FILES "*.ui")

add_executable(${PROJECT_NAME} ${SC_FILES})

qt_add_lupdate(DrcomJluQt SOURCES ${SC_FILES} ${UI_FILES} TS_FILES ts/DrcomJluQt_zh_CN.ts)
qt_add_lrelease(DrcomJluQt TS_FILES ts/DrcomJluQt_zh_CN.ts)

target_link_libraries(DrcomJluQt cryptopp)
target_link_libraries(DrcomJluQt Qt6::Gui Qt6::Widgets Qt6::Network)
set_target_properties(DrcomJluQt PROPERTIES WIN32_EXECUTABLE ON)
