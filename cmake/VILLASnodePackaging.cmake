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

set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2018, Institute for Automation of Complex Power Systems, RWTH Aachen University")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR ${PROJECT_AUTHOR})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "This is VILLASnode, a gateway for processing and forwardning simulation data between real-time simulators.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_PATCH_VERSION})

set(CPACK_RPM_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_URL "http://www.fein-aachen.org/projects/dpsim/")
set(CPACK_RPM_PACKAGE_REQUIRES "openssl libconfig libnl3 libcurl jansson libwebsockets zeromq nanomsg libiec61850 librabbitmq mosquitto comedilib")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

# As close as possible to Fedoras naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.md")
set(CPACK_RESOURCE_FILE_README  "${CMAKE_SOURCE_DIR}/README.md")

set(CPACK_SOURCE_IGNORE_FILES "build/;\\.gitmodules;\\.git/;\\.vscode;\\.editorconfig;\\.gitlab-ci.yml;\\.(docker|git)ignore;\\.DS_Store")

if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "RPM;TGZ")
endif()

include(CPack)
