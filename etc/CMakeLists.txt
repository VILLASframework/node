# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMPONENT bin
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/villas/node
	FILES_MATCHING
		PATTERN "*.conf"
		PATTERN "*.json"
)
