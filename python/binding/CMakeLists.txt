# Python-binding CMakeLists.
#
# Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG)

if(pybind11_FOUND)
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('purelib'))"
    OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  message(STATUS "Found Python version: ${Python_VERSION}")
  message(STATUS "Python major version: ${Python_VERSION_MAJOR}")
  message(STATUS "Python minor version: ${Python_VERSION_MINOR}")

  pybind11_add_module(python_binding capi_python_binding.cpp)
  target_link_libraries(python_binding PUBLIC villas)

  install(
    TARGETS python_binding
    COMPONENT lib
    LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}/villas/node/
  )
else()
  message(STATUS "pybind11 not found. Skipping Python wrapper build.")
endif()
