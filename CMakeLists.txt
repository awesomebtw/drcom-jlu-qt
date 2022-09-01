cmake_minimum_required(VERSION 3.21)
project(DrcomQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH C:/msys64/ucrt64/lib/)

find_package(Qt6 COMPONENTS Core Gui Widgets Network Core5Compat REQUIRED)

# 查找指定目录下的所有.cpp与.h文件 并存放到指定变量名SC_FILES中
FILE(GLOB SC_FILES "*.cpp" "*.h" "*.qrc")

# 指定生成目标
add_executable(${PROJECT_NAME} ${SC_FILES})

target_link_libraries(DrcomQt cryptopp)
target_link_libraries(DrcomQt Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Core5Compat)
