add_executable(streamer streamer.cpp)

target_link_libraries(streamer PUBLIC
	villas-fpga
)

