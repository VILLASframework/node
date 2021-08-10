# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
