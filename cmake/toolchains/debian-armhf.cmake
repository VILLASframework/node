set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(TRIPLET "arm-linux-gnueabihf")

# specify the cross compiler
SET(CMAKE_C_COMPILER   "/usr/bin/${TRIPLET}-gcc")
SET(CMAKE_CXX_COMPILER "/usr/bin/${TRIPLET}-g++")

set(CMAKE_C_FLAGS "-mfloat-abi=hard ${CMAKE_C_FLAGS}" CACHE STRING "Buildroot CFLAGS")
set(CMAKE_CXX_FLAGS "-mfloat-abi=hard ${CMAKE_CXX_FLAGS}" CACHE STRING "Buildroot CXXFLAGS")
set(CMAKE_EXE_LINKER_FLAGS " ${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Buildroot LDFLAGS")

set(CMAKE_LIBRARY_PATH "/usr/lib/${TRIPLET};/usr/local/lib/${TRIPLET}")

set(GOARCH "arm")
set(GOARM "7")
