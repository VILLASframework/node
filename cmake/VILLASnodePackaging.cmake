# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

include(CPackComponent)
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

set(CPACK_BUILD_SOURCE_DIRS ${PROJECT_SOURCE_DIR}/src;${PROJECT_SOURCE_DIR}/lib;${PROJECT_SOURCE_DIR}/include)

set(CPACK_PACKAGE_NAME "villas-node")
set(CPACK_PACKAGE_VENDOR ${PROJECT_AUTHOR})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Connecting real-time power grid simulation equipment")
set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_PATCH_VERSION})

set(CPACK_RPM_PACKAGE_RELEASE ${CMAKE_PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_MAIN_COMPONENT bin)

# TODO: Debuginfos are broken for some reason in CMake
# set(CPACK_RPM_DEBUGINFO_PACKAGE ON)
# set(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE ON)
# set(CPACK_RPM_DEBUGINGO_PACKAGE_NAME villas-node-debuginfo)
# set(CPACK_RPM_DEBUGINFO_PACKAGE "${CPACK_RPM_DEBUGINGO_PACKAGE_NAME}-${SUFFIX}")

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

set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES   "${CPACK_RPM_LIB_PACKAGE_NAME} >= ${CPACK_PACKAGE_VERSION} fmt-devel >= 5.2.0, spdlog-devel >= 1.3.1, openssl-devel >= 1.0.0, libuuid-devel, protobuf-devel >= 2.6.0, protobuf-c-devel >= 1.1.0, libconfig-devel >= 1.4.9, libnl3-devel >= 3.2.27, libcurl-devel >= 7.29.0, jansson-devel >= 2.7, libwebsockets-devel >= 2.3.0, zeromq-devel >= 2.2.0, nanomsg-devel >= 1.0.0, librabbitmq-devel >= 0.8.0, mosquitto-devel >= 1.4.15, libibverbs-devel >= 16.2, librdmacm-devel >= 16.2, libusb-devel >= 0.1.5, lua-devel >= 5.1, librdkafka-devel >= 1.5.0, hiredis-devel >= 1.0.0")
set(CPACK_RPM_LIB_PACKAGE_REQUIRES     "                                                          fmt >= 5.2.0,       spdlog >= 1.3.1,       openssl-libs >= 1.0.0,  libuuid,       protobuf >= 2.6.0,       protobuf-c >= 1.1.0,       libconfig >= 1.4.9,       libnl3 >= 3.2.27,       libcurl >= 7.29.0,       jansson >= 2.7,       libwebsockets >= 2.3.0,       zeromq >= 2.2.0,       nanomsg >= 1.0.0,       librabbitmq >= 0.8.0,       mosquitto >= 1.4.15,       libibverbs >= 16.2,       librdmacm >= 16.2,       libusb >= 0.1.5,       lua >= 5.1,       librdkafka >= 1.5.0,       hiredis >= 1.0.0")
set(CPACK_RPM_BIN_PACKAGE_REQUIRES     "${CPACK_RPM_LIB_PACKAGE_NAME} >= ${CPACK_PACKAGE_VERSION}")
set(CPACK_RPM_PLUGINS_PACKAGE_REQUIRES "${CPACK_RPM_LIB_PACKAGE_NAME} >= ${CPACK_PACKAGE_VERSION}")
set(CPACK_RPM_TOOLS_PACKAGE_REQUIRES   "${CPACK_RPM_LIB_PACKAGE_NAME} >= ${CPACK_PACKAGE_VERSION}")

set(CPACK_RPM_BIN_PACKAGE_SUGGESTS "villas-node-tools villas-node-plugins villas-node-doc")

set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README  "${PROJECT_SOURCE_DIR}/README.md")

set(CPACK_SOURCE_IGNORE_FILES "build/;\\\\.gitmodules;\\\\.git/;\\\\.vscode;\\\\.editorconfig;\\\\.gitlab-ci.yml;\\\\.(docker|git)ignore;\\\\.DS_Store")

if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "RPM;TGZ")
endif()

include(CPack)
