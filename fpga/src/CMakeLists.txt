add_executable(streamer streamer.cpp)

add_executable(pcimem pcimem.c)

target_link_libraries(streamer PUBLIC
	villas-fpga
)

