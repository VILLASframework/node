set(SOURCES
	ip.cpp
	ip.c
	vlnv.cpp
	vlnv.c
	card.c
	card.cpp

	ips/timer.c
	ips/timer.cpp
	ips/model.c
	ips/switch.c
	ips/dft.c
	ips/fifo.c
	ips/dma.c
	ips/intc.cpp
	ips/intc.c
	ips/gpio.c
	ips/rtds_axis.c

	kernel/kernel.c
	kernel/pci.c
	kernel/vfio.c

	plugin.c
	plugin.cpp
	utils.c
	utils.cpp
	list.c
	log.c
	log_config.c
	log_helper.c
	log.cpp
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
