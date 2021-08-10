cmake_minimum_required(VERSION 3.0)
project (test)

add_subdirectory(go)

add_executable(test test.c)

target_link_libraries(test goshim pthread)
