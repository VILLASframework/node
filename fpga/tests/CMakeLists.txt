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
	main.cpp
	fpga.cpp
	logging.cpp
	dma.cpp
	fifo.cpp
	rtds.cpp
	rtds2gpu.cpp
	timer.cpp
)

if(CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
	list(APPEND SOURCES gpu.cpp gpu_kernels.cu)
endif()

add_executable(unit-tests ${SOURCES})

find_package(Criterion REQUIRED)

target_include_directories(unit-tests PUBLIC
	../include
	${CRITERION_INCLUDE_DIRECTORIES}
)

target_link_libraries(unit-tests PUBLIC
	villas-fpga
	${CRITERION_LIBRARIES}
)
