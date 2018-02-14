set(SOURCES
	ip.c
	vlnv.c
	card.c

	vlnv.cpp
	card.cpp
	ip.cpp
	ip_node.cpp

	ips/timer.c
	ips/model.c
	ips/switch.c
	ips/dft.c
	ips/fifo.c
	ips/dma.c
	ips/intc.c
	ips/gpio.c
	ips/rtds_axis.c

	ips/timer.cpp
	ips/switch.cpp
	ips/fifo.cpp
	ips/intc.cpp
	ips/pcie.cpp

	kernel/kernel.c
	kernel/pci.c
	kernel/vfio.c

	plugin.c
	utils.c
	list.c
	log.c
	log_config.c
	log_helper.c

	plugin.cpp
	utils.cpp
	memory_manager.cpp
)

include(FindPkgConfig)

pkg_check_modules(JANSSON jansson)
pkg_check_modules(XIL libxil)

find_package(Threads)

add_library(villas-fpga SHARED ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

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
