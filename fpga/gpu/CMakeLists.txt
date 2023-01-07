## CMakeLists.txt
#
# Author: Daniel Krebs <github@daniel-krebs.net>
# SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, EONERC
# SPDX-License-Identifier: Apache-2.0
##############################################################################

cmake_minimum_required(VERSION 3.8)

project(villas-gpu
	VERSION 1.0
	DESCRIPTION "VILLASgpu"
	LANGUAGES C CXX CUDA)

add_subdirectory(thirdparty/gdrcopy)

add_library(villas-gpu
	SHARED
		src/gpu.cpp
		src/kernels.cu
)

target_compile_options(villas-gpu PRIVATE -g)

set_source_files_properties(src/gpu.cpp PROPERTIES
	LANGUAGE CUDA
)

target_include_directories(villas-gpu
	PRIVATE
		/opt/cuda/include
)

target_link_libraries(villas-gpu
	PUBLIC
		villas-common
		gdrapi
		cuda
)

target_include_directories(villas-gpu
	PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}
)
