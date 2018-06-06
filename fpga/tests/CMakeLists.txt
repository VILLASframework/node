set(SOURCES
	main.cpp
	fpga.cpp
	logging.cpp
	dma.cpp
	fifo.cpp
	rtds.cpp
	rtds2gpu.cpp
	timer.cpp
	graph.cpp
)

if(CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
	list(APPEND SOURCES gpu.cpp gpu_kernels.cu)
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
