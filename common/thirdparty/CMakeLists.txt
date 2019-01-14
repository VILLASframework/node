# We dont want to install the spdlog headers
#set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "Build spdlog tests" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Build spdlog benchmarks" FORCE)
add_subdirectory(spdlog)

set(FMT_DOC OFF CACHE BOOL "Generate the doc target." FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Generate the install target." FORCE)
set(FMT_TEST OFF CACHE BOOL "Generate the test target." FORCE)
add_subdirectory(fmtlib)

target_compile_options(fmt PUBLIC -fPIC)
