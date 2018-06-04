set(SOURCES
	main.cpp
	fpga.cpp
	logging.cpp
	dma.cpp
	fifo.cpp
	rtds.cpp
	timer.cpp
	graph.cpp
)

if(CMAKE_CUDA_COMPILER)
	list(APPEND SOURCES gpu.cpp)
endif()

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
