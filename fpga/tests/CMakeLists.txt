set(SOURCES
	main.cpp
#	dma.c
    fifo.cpp
#	hls.c
#	intc.c
#	rtds_rtt.c
#	tmrctr.c
#	xsg.c
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
