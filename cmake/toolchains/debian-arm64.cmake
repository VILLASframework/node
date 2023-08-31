# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TRIPLET "aarch64-linux-gnu")

# specify the cross compiler
SET(CMAKE_C_COMPILER   "/usr/bin/${TRIPLET}-gcc")
SET(CMAKE_CXX_COMPILER "/usr/bin/${TRIPLET}-g++")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Buildroot CFLAGS")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "Buildroot CXXFLAGS")
set(CMAKE_EXE_LINKER_FLAGS " ${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Buildroot LDFLAGS")

set(CMAKE_LIBRARY_PATH "/usr/lib/${TRIPLET};/usr/local/lib/${TRIPLET}")

set(GOARCH "arm64")
