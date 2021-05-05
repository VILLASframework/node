# CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
    loopback_internal.cpp
)

if(LIBNL3_ROUTE_FOUND)
    list(APPEND LIBRARIES PkgConfig::LIBNL3_ROUTE)
endif()

if(WITH_NODE_INFLUXDB)
    list(APPEND NODE_SRC influxdb.cpp)
endif()

if(WITH_NODE_STATS)
    list(APPEND NODE_SRC stats.cpp)
endif()

if(WITH_NODE_SIGNAL)
    list(APPEND NODE_SRC signal_generator.cpp)
endif()

if(WITH_NODE_LOOPBACK)
    list(APPEND NODE_SRC loopback.cpp)
endif()

if(WITH_NODE_TEST_RTT)
    list(APPEND NODE_SRC test_rtt.cpp)
endif()

if(WITH_NODE_SOCKET)
    list(APPEND NODE_SRC socket.cpp)
endif()

if(WITH_NODE_FILE)
    list(APPEND NODE_SRC file.cpp)
endif()

if(WITH_NODE_EXEC)
    list(APPEND NODE_SRC exec.cpp)
endif()

# Enable Universal Library for Linux DAQ devices (libuldaq)
if(WITH_NODE_ULDAQ)
    list(APPEND NODE_SRC uldaq.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBULDAQ)
endif()

# Enable shared memory node-type
if(WITH_NODE_SHMEM)
    list(APPEND NODE_SRC shmem.cpp)
endif()

# Enable IEC61850 node-types when libiec61850 is available
if(WITH_NODE_IEC61850)
    list(APPEND NODE_SRC iec61850_sv.cpp iec61850.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBIEC61850)
endif()

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
if(WITH_NODE_OPAL)
    list(APPEND NODE_SRC opal.cpp)
    list(APPEND INCLUDE_DIRS ${OPAL_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${OPAL_LIBRARIES})
endif()

# Enable nanomsg node type when libnanomsg is available
if(WITH_NODE_NANOMSG)
    list(APPEND NODE_SRC nanomsg.cpp)
    list(APPEND LIBRARIES PkgConfig::NANOMSG)
endif()

# Enable ZeroMQ node type when libzmq is available
if(WITH_NODE_ZEROMQ)
    list(APPEND NODE_SRC zeromq.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBZMQ)
endif()

# Enable NGSI support
if(WITH_NODE_NGSI)
    list(APPEND NODE_SRC ngsi.cpp)
    list(APPEND INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${CURL_LIBRARIES})
endif()

# Enable WebSocket support
if(WITH_NODE_WEBSOCKET)
    list(APPEND NODE_SRC websocket.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBWEBSOCKETS)
endif()

# Enable AMQP support
if(WITH_NODE_AMQP)
    list(APPEND NODE_SRC amqp.cpp)
    list(APPEND LIBRARIES PkgConfig::RABBITMQ_C)
endif()

# Enable MQTT support
if(WITH_NODE_MQTT)
    list(APPEND NODE_SRC mqtt.cpp)
    list(APPEND LIBRARIES PkgConfig::MOSQUITTO)
endif()

# Enable KAFKA support
if(WITH_NODE_KAFKA)
    list(APPEND NODE_SRC kafka.cpp)
    list(APPEND LIBRARIES PkgConfig::RDKAFKA)
endif()

# Enable Comedi support
if(WITH_NODE_COMEDI)
    list(APPEND NODE_SRC comedi.cpp)
    list(APPEND INCLUDE_DIRS ${COMEDILIB_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${COMEDILIB_LDFLAGS})
endif()

# Enable Infiniband support
if(WITH_NODE_INFINIBAND)
    list(APPEND NODE_SRC infiniband.cpp)
    list(APPEND INCLUDE_DIRS ${IBVERBS_INCLUDE_DIRS} ${RDMACM_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${IBVERBS_LIBRARIES} ${RDMACM_LIBRARIES})
endif()

# Enable RTP node type when libre is available
if(WITH_NODE_RTP)
    list(APPEND NODE_SRC rtp.cpp)
    list(APPEND LIBRARIES PkgConfig::RE)
endif()

# Enable CAN node type
if(WITH_NODE_CAN)
    list(APPEND NODE_SRC can.cpp)
endif()

# Enable Example node type
if(WITH_NODE_EXAMPLE)
    list(APPEND NODE_SRC example.cpp)

# Add your library dependencies for the new node-type here
#    list(APPEND LIBRARIES PkgConfig::EXAMPLELIB)
endif()

# Enable Ethercat support
if(WITH_NODE_ETHERCAT)
    list(APPEND NODE_SRC ethercat.cpp)
    list(APPEND INCLUDE_DIRS ${ETHERLAB_INCLUDE_DIRS})
    list(APPEND LIBRARIES ${ETHERLAB_LIBRARIES})
endif()

# Enable VILLASfpga support
if(WITH_NODE_FPGA)
    list(APPEND NODE_SRC fpga.cpp)
    list(APPEND INCLUDE_DIRS $<TARGET_PROPERTY:villas-fpga,INCLUDE_DIRECTORIES>)
endif()

add_library(nodes STATIC ${NODE_SRC})
target_include_directories(nodes PUBLIC ${INCLUDE_DIRS})
target_link_libraries(nodes PUBLIC ${LIBRARIES})
