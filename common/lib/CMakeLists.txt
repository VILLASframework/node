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

add_library(villas-common SHARED
	plugin.cpp
	utils.cpp
	memory.cpp
	memory_manager.cpp

	kernel/kernel.c
	kernel/pci.c
	kernel/vfio.cpp

	utils.c
	list.c
	log.c
	log_config.c
	log_helper.c
)

target_include_directories(villas-common PUBLIC
	${villas-common_SOURCE_DIR}/include
	${villas-common_SOURCE_DIR}/thirdparty/spdlog/include
	${JANSSON_INCLUDE_DIRS}
)

target_link_libraries(villas-common PUBLIC
	${JANSSON_LIBRARIES}
	${CMAKE_DL_LIBS}
)

target_compile_definitions(villas-common PRIVATE
	BUILDID=\"abc\"
	_GNU_SOURCE
)
