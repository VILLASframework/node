# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

if (NOT DEFINED DEPLOY_USER)
	set(DEPLOY_USER deploy)
endif()

if (NOT DEFINED DEPLOY_HOST)
	set(DEPLOY_USER acs-os-fein-website)
endif()

if (NOT DEFINED DEPLOY_PATH)
	set(DEPLOY_USER /var/www/villas/node)
endif()

add_custom_target(deploy-dist
	COMMAND rsync ${PROJECT_BINARY_DIR}/*.tar.gz ${DEPLOY_USER}@${DEPLOY_HOST}:${DEPLOY_PATH}/src
)
	
add_custom_target(deploy-rpm
	COMMAND rsync -a --progress ${PROJECT_BINARY_DIR}/*.rpm ${DEPLOY_USER}@${DEPLOY_HOST}:/var/www/packages/redhat/
	COMMAND ssh ${DEPLOY_USER}@${DEPLOY_HOST} createrepo /var/www/packages/redhat
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/libvillas.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/libvillas.pc
	@ONLY
)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/libvillas.pc
	COMPONENT devel
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
