cmake_minimum_required(VERSION 3.10)
project(WirelessLinkAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(analyzer
    src/main.cpp
    src/frame.cpp
    src/analyzer.cpp
)
