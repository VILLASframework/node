## CMakeLists.txt
#
# Author: Daniel Krebs <github@daniel-krebs.net>
# SPDX-FileCopyrightText: 2018-2022, Institute for Automation of Complex Power Systems, EONERC
# SPDX-License-Identifier: Apache-2.0
##############################################################################

set(SOURCES
	dma.cpp
	fifo.cpp
	fpga.cpp
	logging.cpp
	main.cpp
	rtds.cpp
	timer.cpp
)

#	rtds_rtt.cpp
#	hls.cpp
#	intc.cpp

add_executable(unit-tests-fpga ${SOURCES})

if (CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
	target_sources(unit-tests-fpga PRIVATE
		gpu.cpp rtds2gpu.cpp gpu_kernels.cu)
endif ()

target_include_directories(unit-tests-fpga PUBLIC
	../include
	${CRITERION_INCLUDE_DIRECTORIES}
)

target_link_libraries(unit-tests-fpga PUBLIC
	villas-fpga
	${CRITERION_LIBRARIES}
)
