# CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(NODE_SRC
    loopback_internal.cpp
)

if(WITH_WEB)
    list(APPEND NODE_SRC api.cpp)
endif()

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
    list(APPEND NODE_SRC signal.cpp signal_v1.cpp)
endif()

if(WITH_NODE_LOOPBACK)
    list(APPEND NODE_SRC loopback.cpp)
endif()

if(WITH_NODE_TEST_RTT)
    list(APPEND NODE_SRC test_rtt.cpp)
endif()

if(WITH_NODE_SMU)
    list(APPEND NODE_SRC smu.cpp)
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

# Enable IEC60870 node-types when lib60870 is available
if(WITH_NODE_IEC60870)
    list(APPEND NODE_SRC iec60870.cpp)
    list(APPEND LIBRARIES PkgConfig::LIB60870)
endif()

# Enable IEC61850 node-types when libiec61850 is available
if(WITH_NODE_IEC61850)
    list(APPEND NODE_SRC iec61850_goose.cpp iec61850.cpp iec61850_sv.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBIEC61850)
endif()

if(WITH_NODE_MODBUS)
    list(APPEND NODE_SRC modbus.cpp)
    list(APPEND LIBRARIES PkgConfig::MODBUS)
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

# Enable Kafka support
if(WITH_NODE_KAFKA)
    list(APPEND NODE_SRC kafka.cpp)
    list(APPEND LIBRARIES  PkgConfig::RDKAFKA)
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
    list(APPEND LIBRARIES re)
endif()

# Enable CAN node type
if(WITH_NODE_CAN)
    list(APPEND NODE_SRC can.cpp)
endif()

# Enable Example node type
if(WITH_NODE_EXAMPLE)
    list(APPEND NODE_SRC example.cpp)

    # Add your library dependencies for the new node-type here
    # list(APPEND LIBRARIES PkgConfig::EXAMPLELIB)
endif()

# Enable OpenDSS node type
if(WITH_NODE_OPENDSS)
    list(APPEND NODE_SRC opendss.cpp)
    list(APPEND LIBRARIES OpenDSSC)
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

# Enable TEMPer node-type support
if(WITH_NODE_TEMPER)
    list(APPEND NODE_SRC temper.cpp)
    list(APPEND LIBRARIES PkgConfig::LIBUSB)
endif()

# Enable Redis support
if(WITH_NODE_REDIS)
    list(APPEND NODE_SRC redis.cpp)
    list(APPEND LIBRARIES PkgConfig::HIREDIS PkgConfig::REDISPP)
endif()

# Enable WebRTC support
if(WITH_NODE_WEBRTC)
    list(APPEND NODE_SRC webrtc.cpp webrtc/signaling_client.cpp webrtc/signaling_message.cpp webrtc/peer_connection.cpp)
    list(APPEND LIBRARIES LibDataChannel::LibDataChannel)
endif()

add_library(nodes STATIC ${NODE_SRC})
target_include_directories(nodes PUBLIC ${INCLUDE_DIRS})
target_link_libraries(nodes PUBLIC ${LIBRARIES})
