# CMakeLists.txt.
#
# @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
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

find_path(ETHERLAB_INCLUDE_DIR
	NAMES ecrt.h
)

find_library(ETHERLAB_LIBRARY
	NAMES ethercat
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set VILLASNODE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Etherlab DEFAULT_MSG
    ETHERLAB_LIBRARY ETHERLAB_INCLUDE_DIR)

mark_as_advanced(ETHERLAB_INCLUDE_DIR ETHERLAB_LIBRARY)

set(ETHERLAB_LIBRARIES ${ETHERLAB_LIBRARY})
set(ETHERLAB_INCLUDE_DIRS ${ETHERLAB_INCLUDE_DIR})
