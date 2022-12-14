# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

find_path(IBVERBS_INCLUDE_DIR
	NAMES infiniband/verbs.h
)

find_library(IBVERBS_LIBRARY
	NAMES ibverbs
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set VILLASNODE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(IBVerbs DEFAULT_MSG
	IBVERBS_LIBRARY IBVERBS_INCLUDE_DIR)

mark_as_advanced(IBVERBS_INCLUDE_DIR IBVERBS_LIBRARY)

set(IBVERBS_LIBRARIES ${IBVERBS_LIBRARY})
set(IBVERBS_INCLUDE_DIRS ${IBVERBS_INCLUDE_DIR})
