cmake_minimum_required(VERSION 3.5)

project(VILLASfpga C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

include_directories(thirdparty/spdlog/include)

add_subdirectory(lib)
add_subdirectory(tests)
