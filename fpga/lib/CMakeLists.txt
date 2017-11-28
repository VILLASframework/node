set(SOURCES
	ip.c
	vlnv.c
	card.c

	ips/timer.c
	ips/model.c
	ips/switch.c
	ips/dft.c
	ips/fifo.c
	ips/dma.c
	ips/intc.cpp
	ips/gpio.c
	ips/rtds_axis.c

	kernel/kernel.c
	kernel/pci.c
	kernel/vfio.c

	plugin.c
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
