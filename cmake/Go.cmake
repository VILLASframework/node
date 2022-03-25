if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(GO_DOWNLOAD_ARCH "amd64")
    set(GO_DOWNLOAD_HASH "980e65a863377e69fd9b67df9d8395fd8e93858e7a24c9f55803421e453f4f99")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(GO_DOWNLOAD_ARCH "arm64")
    set(GO_DOWNLOAD_HASH "57a9171682e297df1a5bd287be056ed0280195ad079af90af16dcad4f64710cb")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "armv7l")
    set(GO_DOWNLOAD_ARCH "armv6l")
    set(GO_DOWNLOAD_HASH "3287ca2fe6819fa87af95182d5942bf4fa565aff8f145812c6c70c0466ce25ae")
endif()

FetchContent_Declare(
    go
    URL      https://go.dev/dl/go1.17.8.linux-${GO_DOWNLOAD_ARCH}.tar.gz
    URL_HASH SHA256=${GO_DOWNLOAD_HASH}
)

message(STATUS "Downloading Go toolchain for linux/${GO_DOWNLOAD_ARCH}")
FetchContent_MakeAvailable(go)

find_program(GO
    NAMES go
    NO_DEFAULT_PATH
    PATHS
        ${CMAKE_CURRENT_BINARY_DIR}/_deps/go-src/bin
        ${go_SOURCE_DIR}
)

