# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

set(TEST_SRC
	config_json.cpp
	config.cpp
	format.cpp
	helpers.cpp
	json.cpp
	main.cpp
	mapping.cpp
	memory.cpp
	pool.cpp
	queue_signalled.cpp
	queue.cpp
	signal.cpp
)

add_executable(unit-tests ${TEST_SRC})
target_link_libraries(unit-tests PUBLIC
	PkgConfig::CRITERION
	Threads::Threads
	villas
)

add_custom_target(run-unit-tests
	COMMAND
		/bin/bash -o pipefail -c \"
			LD_PRELOAD=/usr/lib64/libSegFault.so
			SEGFAULT_SIGNALS=all
			SEGFAULT_SIGNALS='bus abrt'
			$<TARGET_FILE:unit-tests> 2>&1 | c++filt\"
	DEPENDS
		unit-tests
	USES_TERMINAL
)

add_dependencies(tests unit-tests)
add_dependencies(run-tests run-unit-tests)
