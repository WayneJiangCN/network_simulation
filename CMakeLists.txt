cmake_minimum_required(VERSION 3.10)
project(SimpleAsyncSimMultiFile CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动查找src目录下的所有.cpp源文件
file(GLOB SOURCES "src/*.cpp")

# 创建可执行文件，源文件包括main.cpp和所有在src目录下的文件
add_executable(simulator main.cpp ${SOURCES})

# 告诉编译器头文件的查找路径在include目录
target_include_directories(simulator PUBLIC include)

message(STATUS "Executable is available at: ${CMAKE_BINARY_DIR}/simulator")