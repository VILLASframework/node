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
	logging.cpp
	graph.cpp
)

add_executable(unit-tests-common ${SOURCES})

find_package(Criterion REQUIRED)

target_include_directories(unit-tests-common PUBLIC
	${PROJECT_SOURCE_DIR}/include
	${CRITERION_INCLUDE_DIRECTORIES}
)

target_link_libraries(unit-tests-common PUBLIC
	villas-common
	${CRITERION_LIBRARIES}
)
