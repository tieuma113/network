cmake_minimum_required(VERSION 3.18.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_COMPILER "/home/linuxbrew/.linuxbrew/bin/g++-15")
set(CMAKE_C_COMPILER "/home/linuxbrew/.linuxbrew/bin/gcc-15")

project(network_dev VERSION 0.1.0 LANGUAGES C CXX)

add_library(net
    src/net/net.cpp
)
target_include_directories(net PUBLIC inc)

add_executable(network_dev main.cpp)
target_link_libraries(network_dev PRIVATE net)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
