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

set(CPACK_BUILD_SOURCE_DIRS ${PROJECT_SOURCE_DIR}/src;${PROJECT_SOURCE_DIR}/lib;${PROJECT_SOURCE_DIR}/include)

set(CPACK_PACKAGE_NAME "villas-node")
set(CPACK_PACKAGE_VENDOR ${PROJECT_AUTHOR})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "This is VILLASnode, a gateway for processing and forwardning simulation data between real-time simulators.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_PATCH_VERSION})

set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_MAIN_COMPONENT bin)

#set(CPACK_RPM_DEBUGINFO_PACKAGE ON)
#set(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE ON)
#set(CPACK_RPM_DEBUGINGO_PACKAGE_NAME villas-node-debuginfo)
#set(CPACK_RPM_DEBUGINFO_PACKAGE "${CPACK_RPM_DEBUGINGO_PACKAGE_NAME}-${SUFFIX}")

set(CPACK_RPM_LIB_PACKAGE_NAME libvillas)
set(CPACK_RPM_DEVEL_PACKAGE_NAME libvillas-devel)
set(CPACK_RPM_BIN_PACKAGE_NAME villas-node)
set(CPACK_RPM_PLUGINS_PACKAGE_NAME villas-node-plugins)
set(CPACK_RPM_TOOLS_PACKAGE_NAME villas-node-tools)
set(CPACK_RPM_DOC_PACKAGE_NAME villas-node-doc)

set(SUFFIX "${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")
set(CPACK_RPM_LIB_FILE_NAME     "${CPACK_RPM_LIB_PACKAGE_NAME}-${SUFFIX}")
set(CPACK_RPM_DEVEL_FILE_NAME   "${CPACK_RPM_DEVEL_PACKAGE_NAME}-${SUFFIX}")
set(CPACK_RPM_BIN_FILE_NAME     "${CPACK_RPM_BIN_PACKAGE_NAME}-${SUFFIX}")
set(CPACK_RPM_PLUGINS_FILE_NAME "${CPACK_RPM_PLUGINS_PACKAGE_NAME}-${SUFFIX}")
set(CPACK_RPM_TOOLS_FILE_NAME   "${CPACK_RPM_TOOLS_PACKAGE_NAME}-${SUFFIX}")
set(CPACK_RPM_DOC_FILE_NAME     "${CPACK_RPM_DOC_PACKAGE_NAME}-${SUFFIX}")

set(CPACK_RPM_LIB_PACKAGE_REQUIRES "openssl libconfig libnl3 libcurl jansson libwebsockets zeromq nanomsg libiec61850 librabbitmq mosquitto comedilib")
set(CPACK_RPM_BIN_PACKAGE_REQUIRES     ${CPACK_RPM_LIB_PACKAGE_NAME})
set(CPACK_RPM_PLUGINS_PACKAGE_REQUIRES ${CPACK_RPM_LIB_PACKAGE_NAME})
set(CPACK_RPM_TOOLS_PACKAGE_REQUIRES   ${CPACK_RPM_LIB_PACKAGE_NAME})
set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES   "${CPACK_RPM_LIB_PACKAGE_NAME} openssl openssl-devel libconfig-devel libnl3-devel libcurl-devel jansson-devel zeromq-devel nanomsg-devel protobuf-devel protobuf-c-devel libiec61850-devel librabbitmq-devel mosquitto-devel comedilib-devel comedilib libibverbs-devel librdmacm-devel")

set(CPACK_RPM_BIN_PACKAGE_SUGGESTS "villas-node-tools villas-node-plugins villas-node-doc")

set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_URL "http://www.fein-aachen.org/projects/villas-node/")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

set(CPACK_RESOURCE_FILE_LICENSE "${VILLASnode_SOURCE_DIR}/COPYING.md")
set(CPACK_RESOURCE_FILE_README  "${VILLASnode_SOURCE_DIR}/README.md")

set(CPACK_SOURCE_IGNORE_FILES "build/;\\\\.gitmodules;\\\\.git/;\\\\.vscode;\\\\.editorconfig;\\\\.gitlab-ci.yml;\\\\.(docker|git)ignore;\\\\.DS_Store")

if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "RPM;TGZ")
endif()

include(CPack)
