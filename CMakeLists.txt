# Main CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

project(villas-node
    VERSION 1.0.0
    DESCRIPTION "VILLASnode"
    HOMEPAGE_URL "https://www.fein-aachen.org/projects/villas-node/"
    LANGUAGES C CXX
)

# Some more project settings
set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2014-2025 The VILLASframework Authors")

# Several CMake settings/defaults
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${PROJECT_SOURCE_DIR}/common/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(TOPLEVEL_PROJECT ON)
else()
    set(TOPLEVEL_PROJECT OFF)
endif()

# check compiler version
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # require at least gcc 8
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
        message(FATAL_ERROR "GCC version must be at least 8!")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # require at least clang 7
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
        message(FATAL_ERROR "Clang version must be at least 7!")
    endif()
endif()

include(FetchContent)
include(FindPkgConfig)
include(CheckIncludeFile)
include(FeatureSummary)
include(GNUInstallDirs)
include(GetVersion)
include(FindSymbol)
include(CMakeDependentOption)

add_definitions(-D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE)
add_compile_options(-Wall -Wno-unknown-pragmas -fdiagnostics-color=auto)

# Check OS
check_include_file("sys/eventfd.h" HAS_EVENTFD)
check_include_file("semaphore.h" HAS_SEMAPHORE)
check_include_file("sys/mman.h" HAS_MMAN)

# Use the switch NO_EVENTFD to deactivate eventfd usage indepentent of availability on OS
if(${NO_EVENTFD})
    set(HAS_EVENTFD OFF)
endif()

# Check packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(IBVerbs)
find_package(RDMACM)
find_package(Etherlab)
find_package(Lua)
find_package(LibDataChannel)
find_package(re)
find_package(OpenDSSC)

# Check for tools
find_program(PASTE NAMES paste)
if(NOT PASTE)
    message(SEND_ERROR "GNU paste is missing. Please install coreutils")
endif()

# Check programs
find_program(PROTOBUFC_COMPILER NAMES protoc-c)
find_program(PROTOBUF_COMPILER NAMES protoc)

# Build without any GPL-code
option(WITHOUT_GPL "Build VILLASnode without any GPL code" OFF)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:/usr/lib64/pkgconfig")

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.13)
pkg_check_modules(LIBWEBSOCKETS IMPORTED_TARGET REQUIRED libwebsockets>=3.1.0)
pkg_check_modules(PROTOBUF IMPORTED_TARGET protobuf>=2.6.0)
pkg_check_modules(PROTOBUFC IMPORTED_TARGET libprotobuf-c>=1.1.0)
pkg_check_modules(CRITERION IMPORTED_TARGET criterion>=2.3.1)
pkg_check_modules(LIBNL3_ROUTE IMPORTED_TARGET libnl-route-3.0>=3.2.27)
pkg_check_modules(LIBIEC61850 IMPORTED_TARGET libiec61850>=1.6.0)
pkg_check_modules(LIB60870 IMPORTED_TARGET lib60870>=2.3.1)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)
pkg_check_modules(MOSQUITTO IMPORTED_TARGET libmosquitto>=1.6.9)
pkg_check_modules(MODBUS IMPORTED_TARGET libmodbus>=3.1.0)
pkg_check_modules(RDKAFKA IMPORTED_TARGET rdkafka>=1.5.0)
pkg_check_modules(HIREDIS IMPORTED_TARGET hiredis>=1.0.0)
pkg_check_modules(REDISPP IMPORTED_TARGET redis++>=1.2.0)
pkg_check_modules(RABBITMQ_C IMPORTED_TARGET librabbitmq>=0.8.0)
pkg_check_modules(COMEDILIB IMPORTED_TARGET comedilib>=0.11.0)
pkg_check_modules(LIBZMQ IMPORTED_TARGET libzmq>=2.2.0)
pkg_check_modules(LIBULDAQ IMPORTED_TARGET libuldaq>=1.0.0)
pkg_check_modules(UUID IMPORTED_TARGET REQUIRED uuid>=2.23)
pkg_check_modules(CGRAPH IMPORTED_TARGET libcgraph>=2.30)
pkg_check_modules(GVC IMPORTED_TARGET libgvc>=2.30)
pkg_check_modules(LIBUSB IMPORTED_TARGET libusb-1.0>=1.0.23)
pkg_check_modules(NANOMSG IMPORTED_TARGET nanomsg)
if(NOT NANOMSG_FOUND)
    pkg_check_modules(NANOMSG IMPORTED_TARGET libnanomsg>=1.0.0)
endif()


if (REDISPP_FOUND)
    file(READ "${REDISPP_INCLUDEDIR}/sw/redis++/tls.h" CONTENTS)
    if (CONTENTS MATCHES "SEWENEW_REDISPLUSPLUS_TLS_H")
        set(REDISPP_WITH_TLS ON)
    endif()
    unset(CONTENTS)

    # Redis++ had a breaking change between 1.3.6 and 1.3.7
    # Detect this change by checking if the new header exists
    #
    # See https://github.com/sewenew/redis-plus-plus/commit/192ebae13583ff0611e2d7666a9c1e98a2a0dc86
    if (EXISTS "${REDISPP_INCLUDEDIR}/sw/redis++/redis_uri.h")
        set(REDISPP_WITH_URI ON)
    endif()
endif()

if (RABBITMQ_C_FOUND)
    if (EXISTS "${RABBITMQ_C_INCLUDEDIR}/rabbitmq-c/amqp.h")
        set(RABBITMQ_C_NEW_INCLUDE_DIR ON)
    endif()
endif()

# Check if libwebsockets is build with deflate extension
if(${CMAKE_VERSION} VERSION_LESS "3.12.4")
    set(LWS_LOCATION "${LIBWEBSOCKETS_LIBRARY_DIRS}/lib${LIBWEBSOCKETS_LIBRARIES}.so")
else()
    set(LWS_LOCATION ${LIBWEBSOCKETS_LINK_LIBRARIES})
endif()
FindSymbol(${LWS_LOCATION} lws_extension_callback_pm_deflate LWS_DEFLATE_FOUND)

# Check if submodules for VILLASfpga are present
if(EXISTS "${PROJECT_SOURCE_DIR}/fpga/thirdparty/libxil/CMakeLists.txt")
    set(FOUND_FPGA_SUBMODULES ON)
else()
    set(FOUND_FPGA_SUBMODULES OFF)
endif()

# Build options
cmake_dependent_option(WITH_DEFAULTS        "Defaults for non required build options"               ON                 "" OFF)

cmake_dependent_option(WITH_API             "Build with remote control API"                         "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_CLIENTS         "Build client applications"                             "${WITH_DEFAULTS}" "TOPLEVEL_PROJECT" OFF)
cmake_dependent_option(WITH_CONFIG          "Build with support for libconfig configuration syntax" "${WITH_DEFAULTS}" "LIBCONFIG_FOUND" OFF)
cmake_dependent_option(WITH_FPGA            "Build with support for VILLASfpga"                     "${WITH_DEFAULTS}" "FOUND_FPGA_SUBMODULES" OFF)
cmake_dependent_option(WITH_GRAPHVIZ        "Build with Graphviz"                                   "${WITH_DEFAULTS}" "CGRAPH_FOUND; GVC_FOUND" OFF)
cmake_dependent_option(WITH_HOOKS           "Build with support for processing hook plugins"        "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_LUA             "Build with Lua"                                        "${WITH_DEFAULTS}" "LUA_FOUND" OFF)
cmake_dependent_option(WITH_OPENMP          "Build with support for OpenMP for parallel hooks"      "${WITH_DEFAULTS}" "OPENMP_FOUND" OFF)
cmake_dependent_option(WITH_PLUGINS         "Build plugins"                                         "${WITH_DEFAULTS}" "TOPLEVEL_PROJECT" OFF)
cmake_dependent_option(WITH_SRC             "Build executables"                                     "${WITH_DEFAULTS}" "TOPLEVEL_PROJECT" OFF)
cmake_dependent_option(WITH_TESTS           "Run tests"                                             "${WITH_DEFAULTS}" "TOPLEVEL_PROJECT" OFF)
cmake_dependent_option(WITH_TOOLS           "Build auxilary tools"                                  "${WITH_DEFAULTS}" "TOPLEVEL_PROJECT" OFF)
cmake_dependent_option(WITH_WEB             "Build with internal webserver"                         "${WITH_DEFAULTS}" "LIBWEBSOCKETS_FOUND" OFF)

cmake_dependent_option(WITH_NODE_AMQP       "Build with amqp node-type"                             "${WITH_DEFAULTS}" "RABBITMQ_C_FOUND" OFF)
cmake_dependent_option(WITH_NODE_CAN        "Build with can node-type"                              "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_COMEDI     "Build with comedi node-type"                           "${WITH_DEFAULTS}" "COMEDILIB_FOUND" OFF)
cmake_dependent_option(WITH_NODE_ETHERCAT   "Build with ethercat node-type"                         "${WITH_DEFAULTS}" "ETHERLAB_FOUND; NOT WITHOUT_GPL" OFF)
cmake_dependent_option(WITH_NODE_EXAMPLE    "Build with example node-type"                          "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_EXEC       "Build with exec node-type"                             "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_FILE       "Build with file node-type"                             "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_FPGA       "Build with fpga node-type"                             "${WITH_DEFAULTS}" "WITH_FPGA" OFF)
cmake_dependent_option(WITH_NODE_IEC61850   "Build with iec61850 node-types"                        "${WITH_DEFAULTS}" "LIBIEC61850_FOUND; NOT WITHOUT_GPL" OFF)
cmake_dependent_option(WITH_NODE_IEC60870   "Build with iec60870 node-types"                        "${WITH_DEFAULTS}" "LIB60870_FOUND; NOT WITHOUT_GPL" OFF)
cmake_dependent_option(WITH_NODE_INFINIBAND "Build with infiniband node-type"                       "${WITH_DEFAULTS}" "IBVerbs_FOUND; RDMACM_FOUND" OFF) # Infiniband node-type is currenly broken
cmake_dependent_option(WITH_NODE_INFLUXDB   "Build with influxdb node-type"                         "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_KAFKA      "Build with kafka node-type"                            "${WITH_DEFAULTS}" "RDKAFKA_FOUND" OFF)
cmake_dependent_option(WITH_NODE_LOOPBACK   "Build with loopback node-type"                         "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_MODBUS     "Build with modbus node-type"                           "${WITH_DEFAULTS}" "MODBUS_FOUND" OFF)
cmake_dependent_option(WITH_NODE_MQTT       "Build with mqtt node-type"                             "${WITH_DEFAULTS}" "MOSQUITTO_FOUND" OFF)
cmake_dependent_option(WITH_NODE_NANOMSG    "Build with nanomsg node-type"                          "${WITH_DEFAULTS}" "NANOMSG_FOUND" OFF)
cmake_dependent_option(WITH_NODE_NGSI       "Build with ngsi node-type"                             "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_REDIS      "Build with redis node-type"                            "${WITH_DEFAULTS}" "HIREDIS_FOUND; REDISPP_FOUND" OFF)
cmake_dependent_option(WITH_NODE_RTP        "Build with rtp node-type"                              "${WITH_DEFAULTS}" "re_FOUND" OFF)
cmake_dependent_option(WITH_NODE_SHMEM      "Build with shmem node-type"                            "${WITH_DEFAULTS}" "HAS_SEMAPHORE; HAS_MMAN" OFF)
cmake_dependent_option(WITH_NODE_SIGNAL     "Build with signal node-type"                           "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_SOCKET     "Build with socket node-type"                           "${WITH_DEFAULTS}" "LIBNL3_ROUTE_FOUND" OFF)
cmake_dependent_option(WITH_NODE_STATS      "Build with stats node-type"                            "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_TEMPER     "Build with temper node-type"                           "${WITH_DEFAULTS}" "LIBUSB_FOUND" OFF)
cmake_dependent_option(WITH_NODE_TEST_RTT   "Build with test_rtt node-type"                         "${WITH_DEFAULTS}" "" OFF)
cmake_dependent_option(WITH_NODE_ULDAQ      "Build with uldaq node-type"                            "${WITH_DEFAULTS}" "LIBULDAQ_FOUND" OFF)
cmake_dependent_option(WITH_NODE_WEBRTC     "Build with webrtc node-type"                           "${WITH_DEFAULTS}" "WITH_WEB; LibDataChannel_FOUND" OFF)
cmake_dependent_option(WITH_NODE_WEBSOCKET  "Build with websocket node-type"                        "${WITH_DEFAULTS}" "WITH_WEB" OFF)
cmake_dependent_option(WITH_NODE_ZEROMQ     "Build with zeromq node-type"                           "${WITH_DEFAULTS}" "LIBZMQ_FOUND; NOT WITHOUT_GPL" OFF)
cmake_dependent_option(WITH_NODE_OPENDSS    "Build with opendss node-type"                          "${WITH_DEFAULTS}" "OpenDSSC_FOUND" OFF)

# set a default for the build type
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Add more build configurations
include(cmake/config/Debug.cmake)
include(cmake/config/Release.cmake)
include(cmake/config/Coverage.cmake)
include(cmake/config/Profiling.cmake)

if(NOT DEFINED VILLAS_COMPILE_WARNING_AS_ERROR)
    set(VILLAS_COMPILE_WARNING_AS_ERROR ON)
endif()

if(VILLAS_COMPILE_WARNING_AS_ERROR)
    add_compile_options(-Werror)
else()
    add_compile_options(-Wno-error)
endif()

# Get version info and buildid from Git
GetVersion(${PROJECT_SOURCE_DIR} "CMAKE_PROJECT")

if(WITH_FPGA)
    add_subdirectory(fpga)
endif()

add_subdirectory(common)
add_subdirectory(etc)
add_subdirectory(lib)
add_subdirectory(packaging)
add_subdirectory(python)

if(WITH_SRC)
    add_subdirectory(src)
endif()

if(WITH_TOOLS)
    add_subdirectory(tools)
endif()

if(WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

if(WITH_CLIENTS)
    add_subdirectory(clients)
endif()

if(WITH_TESTS)
    add_subdirectory(tests)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/node/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/node/config.hpp
)

# Show feature summary
add_feature_info(API                    WITH_API                    "Build with remote control API")
add_feature_info(CLIENTS                WITH_CLIENTS                "Build client applications")
add_feature_info(CONFIG                 WITH_CONFIG                 "Build with support for libconfig configuration syntax")
add_feature_info(FPGA                   WITH_FPGA                   "Build with FPGA support")
add_feature_info(GRAPHVIZ               WITH_GRAPHVIZ               "Build with Graphviz support")
add_feature_info(HOOKS                  WITH_HOOKS                  "Build with support for processing hook plugins")
add_feature_info(LUA                    WITH_LUA                    "Build with Lua support")
add_feature_info(OPENMP                 WITH_OPENMP                 "Build with OpenMP support")
add_feature_info(PLUGINS                WITH_PLUGINS                "Build plugins")
add_feature_info(SRC                    WITH_SRC                    "Build executables")
add_feature_info(TESTS                  WITH_TESTS                  "Run tests")
add_feature_info(TOOLS                  WITH_TOOLS                  "Build auxilary tools")
add_feature_info(WEB                    WITH_WEB                    "Build with internal webserver")

add_feature_info(NODE_AMQP              WITH_NODE_AMQP              "Build with amqp node-type")
add_feature_info(NODE_CAN               WITH_NODE_CAN               "Build with can node-type")
add_feature_info(NODE_COMEDI            WITH_NODE_COMEDI            "Build with comedi node-type")
add_feature_info(NODE_ETHERCAT          WITH_NODE_ETHERCAT          "Build with ethercat node-type")
add_feature_info(NODE_EXAMPLE           WITH_NODE_EXAMPLE           "Build with example node-type")
add_feature_info(NODE_EXEC              WITH_NODE_EXEC              "Build with exec node-type")
add_feature_info(NODE_FILE              WITH_NODE_FILE              "Build with file node-type")
add_feature_info(NODE_FPGA              WITH_NODE_FPGA              "Build with fpga node-type")
add_feature_info(NODE_IEC61850          WITH_NODE_IEC61850          "Build with iec61850 node-types")
add_feature_info(NODE_IEC60870          WITH_NODE_IEC60870          "Build with iec60870 node-types")
add_feature_info(NODE_INFINIBAND        WITH_NODE_INFINIBAND        "Build with infiniband node-type")
add_feature_info(NODE_INFLUXDB          WITH_NODE_INFLUXDB          "Build with influxdb node-type")
add_feature_info(NODE_KAFKA             WITH_NODE_KAFKA             "Build with kafka node-type")
add_feature_info(NODE_LOOPBACK          WITH_NODE_LOOPBACK          "Build with loopback node-type")
add_feature_info(NODE_MODBUS            WITH_NODE_MODBUS            "Build with modbus node-type")
add_feature_info(NODE_MQTT              WITH_NODE_MQTT              "Build with mqtt node-type")
add_feature_info(NODE_NANOMSG           WITH_NODE_NANOMSG           "Build with nanomsg node-type")
add_feature_info(NODE_NGSI              WITH_NODE_NGSI              "Build with ngsi node-type")
add_feature_info(NODE_OPENDSS           WITH_NODE_OPENDSS           "Build with opendss node-type")
add_feature_info(NODE_REDIS             WITH_NODE_REDIS             "Build with redis node-type")
add_feature_info(NODE_RTP               WITH_NODE_RTP               "Build with rtp node-type")
add_feature_info(NODE_SHMEM             WITH_NODE_SHMEM             "Build with shmem node-type")
add_feature_info(NODE_SIGNAL            WITH_NODE_SIGNAL            "Build with signal node-type")
add_feature_info(NODE_SOCKET            WITH_NODE_SOCKET            "Build with socket node-type")
add_feature_info(NODE_STATS             WITH_NODE_STATS             "Build with stats node-type")
add_feature_info(NODE_TEMPER            WITH_NODE_TEMPER            "Build with temper node-type")
add_feature_info(NODE_TEST_RTT          WITH_NODE_TEST_RTT          "Build with test_rtt node-type")
add_feature_info(NODE_ULDAQ             WITH_NODE_ULDAQ             "Build with uldaq node-type")
add_feature_info(NODE_WEBRTC            WITH_NODE_WEBRTC            "Build with webrtc node-type")
add_feature_info(NODE_WEBSOCKET         WITH_NODE_WEBSOCKET         "Build with websocket node-type")
add_feature_info(NODE_ZEROMQ            WITH_NODE_ZEROMQ            "Build with zeromq node-type")

if(TOPLEVEL_PROJECT)
    feature_summary(WHAT ALL VAR FEATURES)
    message(STATUS "Building ${CMAKE_PROJECT_DESCRIPTION}:")
    message(STATUS "${FEATURES}")

    if(FOUND_GIT_VERSION)
        message(STATUS "  VERSION:         ${CMAKE_PROJECT_VERSION}")
        message(STATUS "  RELEASE:         ${CMAKE_PROJECT_RELEASE}")
        message(STATUS "  GIT_REV:         ${CMAKE_PROJECT_GIT_REV}")
        message(STATUS "  GIT_BRANCH:      ${CMAKE_PROJECT_GIT_BRANCH}")
        message(STATUS "  VARIANT:         ${CMAKE_PROJECT_VARIANT}")
        message(STATUS "  BUILD_ID:        ${CMAKE_PROJECT_BUILD_ID}")
        message(STATUS "  BUILD_DATE:      ${CMAKE_PROJECT_BUILD_DATE}")
    endif()

    message(STATUS "  ARCH:            ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "  OS:              ${CMAKE_SYSTEM_NAME}")
endif()

include(VILLASnodePackaging)
