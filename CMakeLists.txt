cmake_minimum_required(VERSION 3.10)
project(matrix_transpose_cache_associative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(associative main.cpp)

# Define PIN_TO_CORE to enable core pinning
target_compile_definitions(associative PRIVATE PIN_TO_CORE)