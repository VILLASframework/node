cmake_minimum_required(VERSION 3.7)

project(villas-common)

add_library(villas-common SHARED
	plugin.cpp
	utils.cpp
	memory.cpp
	memory_manager.cpp)

target_include_directories(villas-common
	PUBLIC
	    ${CMAKE_CURRENT_LIST_DIR}/../../include/villas)
