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
    COMMAND "${Python3_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('stdlib') + '/lib-dynload')"
    OUTPUT_VARIABLE PYTHON_LIB_DYNLOAD_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  message(STATUS "Found Python version: ${Python_VERSION}")
  message(STATUS "Python major version: ${Python_VERSION_MAJOR}")
  message(STATUS "Python minor version: ${Python_VERSION_MINOR}")
  message(STATUS "Python .so install directory: ${PYTHON_LIB_DYNLOAD_DIR}")

  pybind11_add_module(python-binding villas-python-binding.cpp)
  set_target_properties(python-binding PROPERTIES OUTPUT_NAME villas_node)
  target_link_libraries(python-binding PUBLIC villas)

  install(
    TARGETS python-binding
    COMPONENT lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${PYTHON_LIB_DYNLOAD_DIR}
  )
else()
  message(STATUS "pybind11 not found. Skipping Python wrapper build.")
endif()
