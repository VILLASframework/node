# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
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
###################################################################################

add_custom_target(doc
	COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

configure_file(
	Doxyfile.in
	${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
)

install(
	DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
	DESTINATION share/villas/node/doc
	COMPONENT doc
)
