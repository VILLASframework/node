cmake_minimum_required(VERSION 3.5)

project(VILLASfpga C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_subdirectory(lib)
add_subdirectory(tests)
add_subdirectory(src)
