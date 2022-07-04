# CMakeLists.txt.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

set(CMAKE_CXX_FLAGS_PROFILING
    "${CMAKE_CXX_FLAGS} -pg"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE
)

set(CMAKE_C_FLAGS_PROFILING
    "${CMAKE_C_FLAGS} -pg"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE
)

set(CMAKE_EXE_LINKER_FLAGS_PROFILING
    "${CMAKE_EXE_LINKER_FLAGS} -pg"
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE
)

set(CMAKE_SHARED_LINKER_FLAGS_PROFILING
    "${CMAKE_SHARED_LINKER_FLAGS_COVERAGE} -pg"
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE
)

mark_as_advanced(
    CMAKE_CXX_FLAGS_PROFILING
    CMAKE_C_FLAGS_PROFILING
    CMAKE_EXE_LINKER_FLAGS_PROFILING
    CMAKE_SHARED_LINKER_FLAGS_PROFILING
)
