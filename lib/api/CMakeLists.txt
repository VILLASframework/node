
# CMakeLists.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

set(API_SRC
    session.cpp
    request.cpp
    response.cpp

    requests/node.cpp
    requests/path.cpp
    requests/universal.cpp

    requests/status.cpp
    requests/capabiltities.cpp
    requests/config.cpp
    requests/shutdown.cpp
    requests/restart.cpp
    requests/nodes.cpp
    requests/node_info.cpp
    requests/node_action.cpp
    requests/node_stats.cpp
    requests/node_stats_reset.cpp
    requests/node_file.cpp
    requests/paths.cpp
    requests/path_info.cpp
    requests/path_action.cpp

    requests/universal/config.cpp
    requests/universal/info.cpp
    requests/universal/signal.cpp
    requests/universal/signals.cpp
)

if(WITH_GRAPHVIZ)
    list(APPEND API_SRC requests/graph.cpp)
    list(APPEND LIBRARIES PkgConfig::CGRAPH PkgConfig::GVC)
endif()

add_library(api STATIC ${API_SRC})
target_include_directories(api PUBLIC ${INCLUDE_DIRS})
target_link_libraries(api PUBLIC ${LIBRARIES})
