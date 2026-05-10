cmake_minimum_required(VERSION 3.28)
project(llob CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(benchmark REQUIRED)
find_package(GTest REQUIRED)

add_library(llob INTERFACE)
target_include_directories(llob INTERFACE include/)

enable_testing()

add_subdirectory(benchmarks)
add_subdirectory(tests)
