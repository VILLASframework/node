set(SOURCES
	main.cpp
	fpga.cpp
	logging.cpp
	dma.cpp
    fifo.cpp
	gpu.cpp
#	hls.c
#	intc.c
#	rtds_rtt.c
	timer.cpp
#	xsg.c
    graph.cpp
)

add_executable(unit-tests ${SOURCES})

find_package(Criterion REQUIRED)

target_include_directories(unit-tests PUBLIC
	../include
	${CRITERION_INCLUDE_DIRECTORIES}
)

target_link_libraries(unit-tests PUBLIC
	villas-fpga
	${CRITERION_LIBRARIES}
)
