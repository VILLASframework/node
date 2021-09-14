# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

find_path(OPAL_INCLUDE_DIR
	NAMES AsyncApi.h
	HINTS
		/usr/opalrt/common/include_target/
		${PROJECT_SOURCE_DIR}/libopal/include/opal/

)

find_library(OPAL_LIBRARY
	NAMES OpalAsyncApiCore
	HINTS
		/usr/opalrt/common/lib/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_IRC
	NAMES irc
	HINTS
		/usr/opalrt/common/lib/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_OPAL_UTILS
	NAMES OpalUtils
	HINTS
		/usr/opalrt/common/lib/redhawk/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_OPAL_CORE
	NAMES OpalCore
	HINTS
		/usr/opalrt/common/lib/redhawk/
		${PROJECT_SOURCE_DIR}/libopal/
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opal DEFAULT_MSG OPAL_LIBRARY OPAL_INCLUDE_DIR)

get_filename_component(OPAL_LIBRARY_DIR ${OPAL_LIBRARY} DIRECTORY)

mark_as_advanced(OPAL_INCLUDE_DIR OPAL_LIBRARY_DIR OPAL_LIBRARY)

set(OPAL_LIBRARIES ${OPAL_LIBRARY} ${OPAL_LIBRARY_OPALUTILS} ${OPAL_LIBRARY_OPALCORE} ${OPAL_LIBRARY_IRC} $ENV{OPAL_LIBPATH} $ENV{OPAL_LIBS})
set(OPAL_INCLUDE_DIRS ${OPAL_INCLUDE_DIR})
