# CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(HOOK_SRC
    average.cpp
    cast.cpp
    decimate.cpp
    dp.cpp
    drop.cpp
    dump.cpp
    ebm.cpp
    fix.cpp
    gate.cpp
    jitter_calc.cpp
    limit_rate.cpp
    limit_value.cpp
    ma.cpp
    power.cpp
    pmu.cpp
    pmu_dft.cpp
    pmu_ipdft.cpp
    pps_ts.cpp
    print.cpp
    restart.cpp
    rms.cpp
    round.cpp
    scale.cpp
    shift_seq.cpp
    shift_ts.cpp
    skip_first.cpp
    stats.cpp
    ts.cpp
)

if(WITH_LUA)
    list(APPEND HOOK_SRC lua.cpp)
endif()

if(WITH_OPENMP)
    list(APPEND LIBRARIES OpenMP::OpenMP_CXX)
endif()

add_library(hooks STATIC ${HOOK_SRC})
target_include_directories(hooks PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hooks PUBLIC ${LIBRARIES})
