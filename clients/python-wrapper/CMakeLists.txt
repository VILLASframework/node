cmake_minimum_required(VERSION 3.15...3.29)
project(villas-python-wrapper LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

pybind11_add_module(villas-python-wrapper villas-python-wrapper.cpp)
target_link_libraries(villas-python-wrapper PUBLIC villas)

install(
  TARGETS villas-python-wrapper
  COMPONENT bin
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
