# CMakeLists.
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

set(NODE_SRC
    influxdb.c
    stats.c
    signal_generator.c
    loopback.c
)

if(LIBNL3_ROUTE_FOUND)
    list(APPEND LIBRARIES PkgConfig::LIBNL3_ROUTE)
    list(APPEND INCLUDE_DIRS LIBNL3_ROUTE_INCLUDE_DIRS)
endif()

if(WITH_IO)
    list(APPEND NODE_SRC
        test_rtt.c
        file.c
        socket.c
    )
endif()

if(HAS_EVENTFD)
    list(APPEND NODE_SRC
        cbuilder.c
    )
endif()

# Enable shared memory node-type
if(HAS_SEMAPHORE AND HAS_MMAN)
    list(APPEND NODE_SRC shmem.c)

    if(CMAKE_SUSTEM_NAME STREQUAL Linux)
        list(APPEND LIBRARIES rt)
    endif()
endif()

# Enable IEC61850 node-types when libiec61850 is available
if(LIBIEC61850_FOUND)
    list(APPEND NODE_SRC iec61850_sv.c iec61850.c)
    list(APPEND INCLUDE_DIRS ${LIBIEC61850_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBIEC61850)
endif()

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
if(OPAL_FOUND AND BUILD32)
    list(APPEND NODE_SRC opal.c)
    list(APPEND INCLUDE_DIRS ${OPAL_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${OPAL_LIBRARIES})
endif()

# Enable nanomsg node type when libnanomsg is available
if(NANOMSG_FOUND AND WITH_IO)
    list(APPEND NODE_SRC nanomsg.c)
    list(APPEND INCLUDE_DIRS ${NANOMSG_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::NANOMSG)
endif()

# Enable ZeroMQ node type when libzmq is available
if(LIBZMQ_FOUND AND WITH_IO)
    list(APPEND NODE_SRC zeromq.c)
    list(APPEND INCLUDE_DIRS ${LIBZMQ_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBZMQ)
endif()

# Enable NGSI support
if(CURL_FOUND)
    list(APPEND NODE_SRC ngsi.c)
    list(APPEND INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${CURL_LIBRARIES})
endif()

# Enable WebSocket support
if(Libwebsockets_FOUND AND WITH_WEB AND WITH_IO)
    list(APPEND NODE_SRC websocket.c)
    list(APPEND INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIRS})
    list(APPEND LIBRARIES websockets_shared)
endif()

# Enable AMQP support
if(RABBITMQ_C_FOUND AND WITH_IO)
    list(APPEND NODE_SRC amqp.c)
    list(APPEND INCLUDE_DIRS ${RABBITMQ_C_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::RABBITMQ_C)
endif()

# Enable MQTT support
if(MOSQUITTO_FOUND AND WITH_IO)
    list(APPEND NODE_SRC mqtt.c)
    list(APPEND INCLUDE_DIRS ${MOSQUITTO_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${MOSQUITTO_LIBRARIES})
endif()

# Enable Comedi support
if(COMEDILIB_FOUND)
    list(APPEND NODE_SRC comedi.c)
    list(APPEND INCLUDE_DIRS ${COMEDILIB_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::COMEDILIB)
endif()

# Enable infiniband support
if(IBVERBS_FOUND AND RDMACM_FOUND)
    list(APPEND NODE_SRC infiniband.c)
    list(APPEND INCLUDE_DIRS ${IBVERBS_INCLUDE_DIRS} ${RDMACM_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${IBVERBS_LIBRARIES} ${RDMACM_LIBRARIES})
endif()

add_library(nodes STATIC ${NODE_SRC})
target_include_directories(nodes PUBLIC ${INCLUDE_DIRS})
target_link_libraries(nodes LINK_PRIVATE ${LIBRARIES})
