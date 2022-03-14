# determine GOARCH for target
set(GO_TARGET_ARCH_OVERRIDE
    ""
    CACHE STRING "overrides the 'GOARCH' variable")
if (GO_TARGET_ARCH_OVERRIDE)
    set(GO_TARGET_ARCH "${GO_TARGET_ARCH_OVERRIDE}")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(GO_TARGET_ARCH "amd64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(GO_TARGET_ARCH "386")
elseif (CMAKE_SYSTEM_NAME MATCHES "(ppc64|ppc64le|arm|arm64|s390x)")
    set(GO_TARGET_ARCH "${CMAKE_SYSTEM_NAME}")
else ()
    message(FATAL_ERROR "Unable to auto-determine GOARCH. Please set GO_TARGET_ARCH_OVERRIDE manually.")
endif ()

# determine GOOS for target
set(GO_TARGET_OS_OVERRIDE
    ""
    CACHE STRING "overrides the 'GOOS' variable")
if (GO_TARGET_OS_OVERRIDE)
    set(GO_TARGET_OS "${GO_TARGET_OS_OVERRIDE}")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(GO_TARGET_OS "linux")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(GO_TARGET_OS "android")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(GO_TARGET_OS "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(GO_TARGET_OS "freebsd")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(GO_TARGET_OS "darwin")
else ()
    message(FATAL_ERROR "Unable to auto-determine GOOS. Please set GO_TARGET_OS_OVERRIDE manually.")
endif ()

FetchContent_Declare(
    go
    URL      https://go.dev/dl/go1.17.8.${GO_TARGET_OS}-${GO_TARGET_ARCH}.tar.gz
)

message(STATUS "Downloading Go toolchain for ${GO_TARGET_OS}/${GO_TARGET_ARCH}")
FetchContent_MakeAvailable(go)

find_program(GO
    NAMES go
    NO_DEFAULT_PATH
    PATHS
        ${CMAKE_CURRENT_BINARY_DIR}/_deps/go-src/bin
        ${go_SOURCE_DIR}
)

