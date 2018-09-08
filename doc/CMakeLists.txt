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

find_package(Doxygen)

if(DOXYGEN_FOUND)
	set(DOXYGEN_PROJECT_LOGO doc/pictures/villas_node.svg)
	set(DOXYGEN_WARN_LOGFILE ${CMAKE_CURRENT_BINARY_DIR}/warnings.log)
	set(DOXYGEN_TAB_SIZE 8)
	set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
	set(DOXYGEN_LAYOUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/theme/layout.xml)
	set(DOXYGEN_RECURSIVE YES)
	set(DOXYGEN_EXAMPLE_PATH etc/)
	set(DOXYGEN_EXAMPLE_RECURSIVE YES)
	set(DOXYGEN_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/pictures)
	set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
	set(DOXYGEN_SOURCE_BROWSER YES)
	set(DOXYGEN_REFERENCED_BY_RELATION YES)
	set(DOXYGEN_HTML_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/theme/header.html)
	set(DOXYGEN_HTML_EXTRA_STYLESHEET ${CMAKE_CURRENT_SOURCE_DIR}/theme/style.css)
	set(DOXYGEN_HTML_EXTRA_FILES doc${CMAKE_CURRENT_SOURCE_DIR}/pictures/acs_eonerc_logo.svg)
	set(DOXYGEN_HTML_COLORSTYLE_HUE 209)
	set(DOXYGEN_HTML_COLORSTYLE_SAT 88)
	set(DOXYGEN_HTML_TIMESTAMP YES)
	set(DOXYGEN_HTML_DYNAMIC_SECTIONS YES)
	set(DOXYGEN_GENERATE_TREEVIEW YES)
	set(DOXYGEN_TREEVIEW_WIDTH 280)
	set(DOXYGEN_UML_LOOK YES)
	set(DOXYGEN_CALL_GRAPH YES)
	set(DOXYGEN_CALLER_GRAPH YES)
	set(DOXYGEN_DOT_IMAGE_FORMAT svg)
	set(DOXYGEN_INTERACTIVE_SVG YES)
	set(DOXYGEN_DIAFILE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/figures/)

	doxygen_add_docs(doc
		README.md CONTRIBUTING.md COPYING.md src/ lib/ tests/ include/ doc/
		WORKING_DIRECTORY ${PROJECT_DIR}
	)

	install(
		DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/villas/node
		COMPONENT doc
	)
endif()
