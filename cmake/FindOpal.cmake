# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

set(OPAL_PREFIX /usr/opalrt/common)

find_path(OPAL_INCLUDE_DIR
	NAMES AsyncApi.h
	HINTS
		${OPAL_PREFIX}/include_target/
		${PROJECT_SOURCE_DIR}/libopal/include/opal/

)

find_library(OPAL_LIBRARY
	NAMES OpalAsyncApiCore
	HINTS
		${OPAL_PREFIX}/lib/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_IRC
	NAMES irc
	HINTS
	${OPAL_PREFIX}/lib/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_UTILS
	NAMES OpalUtils
	HINTS
		${OPAL_PREFIX}/lib/redhawk/
		${OPAL_PREFIX}/lib/redhawk64/
		${PROJECT_SOURCE_DIR}/libopal/
)

find_library(OPAL_LIBRARY_CORE
	NAMES OpalCore
	HINTS
		${OPAL_PREFIX}/lib/redhawk/
		${OPAL_PREFIX}/lib/redhawk64/
		${PROJECT_SOURCE_DIR}/libopal/
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opal DEFAULT_MSG OPAL_LIBRARY OPAL_LIBRARY_UTILS OPAL_LIBRARY_CORE OPAL_LIBRARY_IRC OPAL_INCLUDE_DIR)

mark_as_advanced(OPAL_INCLUDE_DIR OPAL_LIBRARY)

set(OPAL_LIBRARIES ${OPAL_LIBRARY} ${OPAL_LIBRARY_UTILS} ${OPAL_LIBRARY_CORE} ${OPAL_LIBRARY_IRC} $ENV{OPAL_LIBPATH} $ENV{OPAL_LIBS})
set(OPAL_INCLUDE_DIRS ${OPAL_INCLUDE_DIR})
