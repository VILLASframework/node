## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2018, RWTH Institute for Automation of Complex Power Systems (ACS)
# @license GNU General Public License (version 3)
#
# VILLASfpga
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
