# CMakeLists.txt.
#
# Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

find_path(ETHERLAB_INCLUDE_DIR
	NAMES ecrt.h
	PATHS
		/opt/etherlab/include
)

find_library(ETHERLAB_LIBRARY
	NAMES ethercat
	PATHS
		/opt/etherlab/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Etherlab DEFAULT_MSG ETHERLAB_LIBRARY ETHERLAB_INCLUDE_DIR)

mark_as_advanced(ETHERLAB_INCLUDE_DIR ETHERLAB_LIBRARY)

set(ETHERLAB_LIBRARIES ${ETHERLAB_LIBRARY})
set(ETHERLAB_INCLUDE_DIRS ${ETHERLAB_INCLUDE_DIR})
