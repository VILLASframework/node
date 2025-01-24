cmake_minimum_required(VERSION 3.15...3.29)
project(villas-python-wrapper LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

execute_process(
  COMMAND "${Python3_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('stdlib') + '/lib-dynload')"
  OUTPUT_VARIABLE PYTHON_LIB_DYNLOAD_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Found Python version: ${Python_VERSION}")
message(STATUS "Python major version: ${Python_VERSION_MAJOR}")
message(STATUS "Python minor version: ${Python_VERSION_MINOR}")
message(STATUS "Python .so install directory: ${PYTHON_LIB_DYNLOAD_DIR}")

pybind11_add_module(villas_node villas-python-wrapper.cpp)
target_link_libraries(villas_node PUBLIC villas)

install(
  TARGETS villas_node
  COMPONENT lib
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${PYTHON_LIB_DYNLOAD_DIR}
)
