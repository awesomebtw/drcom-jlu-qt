cmake_minimum_required(VERSION 3.21)
project(DrcomQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# TODO: remove this
set(CMAKE_PREFIX_PATH C:/msys64/ucrt64/lib/)

find_package(Qt6 COMPONENTS Gui Widgets Network REQUIRED)

FILE(GLOB SC_FILES "*.cpp" "*.h" "*.qrc")

add_executable(${PROJECT_NAME} ${SC_FILES})

target_link_libraries(DrcomQt cryptopp)
target_link_libraries(DrcomQt Qt6::Gui Qt6::Widgets Qt6::Network)
set_target_properties(DrcomQt PROPERTIES WIN32_EXECUTABLE ON)
