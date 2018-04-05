set(SOURCES
	bench-datamovers.c
	bench-jitter.c
	bench-latency.c
	bench-memcpy.c
	bench.c
	fpga.c
)

add_executable(fpga ${SOURCES})

target_include_directories(fpga PUBLIC
	../include/villas
	../include
)

target_link_libraries(fpga PUBLIC
	villas-fpga
)

find_package(LAPACK)

if(LAPACK_FOUND)
	target_sources(fpga PUBLIC bench-overruns.c)
	target_link_libraries(fpga PUBLIC ${LAPACK_LIBRARIES})
	target_include_directories(fpga PUBLIC ${LAPACK_INCLUDE_DIRS})
	target_compile_definitions(fpga PUBLIC WITH_LAPACK)
endif()

include(GNUInstallDirs)

install(TARGETS fpga
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
)
