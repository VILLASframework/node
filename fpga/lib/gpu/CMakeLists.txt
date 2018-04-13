cmake_minimum_required(VERSION 3.7)

project(villas-gpu
	VERSION 1.0
	DESCRIPTION "VILLASgpu"
	LANGUAGES C CXX)

# fail if CUDA not found
find_package(CUDA QUIET REQUIRED)

cuda_add_library(villas-gpu SHARED gpu.cpp)

target_include_directories(villas-gpu
	PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
