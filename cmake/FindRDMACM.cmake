# CMakeLists.txt.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

find_path(RDMACM_INCLUDE_DIR
	NAMES rdma/rdma_cma.h
)

find_library(RDMACM_LIBRARY
	NAMES rdmacm
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set VILLASNODE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(RDMACM DEFAULT_MSG
	RDMACM_LIBRARY RDMACM_INCLUDE_DIR)

mark_as_advanced(RDMACM_INCLUDE_DIR RDMACM_LIBRARY)

set(RDMACM_LIBRARIES ${RDMACM_LIBRARY})
set(RDMACM_INCLUDE_DIRS ${RDMACM_INCLUDE_DIR})
