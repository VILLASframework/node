add_subdirectory(common)

set(SOURCES
	vlnv.cpp
	card.cpp
	ip.cpp
	ip_node.cpp

	ips/timer.cpp
	ips/switch.cpp
	ips/fifo.cpp
	ips/intc.cpp
	ips/pcie.cpp
	ips/dma.cpp
	ips/bram.cpp

	kernel/kernel.c
	kernel/pci.c
	kernel/vfio.cpp

	utils.c
	list.c
	log.c
	log_config.c
	log_helper.c
)

include(FindPkgConfig)

pkg_check_modules(JANSSON jansson)
pkg_check_modules(XIL libxil)

find_package(Threads)

add_library(villas-fpga SHARED ${SOURCES})

target_link_libraries(villas-fpga PUBLIC villas-common)

# GPU library is optional, check for CUDA presence
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
	add_subdirectory(gpu)
	target_link_libraries(villas-fpga PUBLIC villas-gpu)
else()
	message("No CUDA support, not building GPU library")
endif()

target_compile_definitions(villas-fpga PRIVATE
	BUILDID=\"abc\"
	_GNU_SOURCE
)

target_include_directories(villas-fpga PUBLIC
	../include/villas
	${XIL_INCLUDE_DIRS}
	${JANSSON_INCLUDE_DIRS}
)

target_link_libraries(villas-fpga PUBLIC
	${XIL_LIBRARIES}
	${JANSSON_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	m
)

include(GNUInstallDirs)

install(TARGETS villas-fpga
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
)

install(DIRECTORY ../include/villas DESTINATION include)


