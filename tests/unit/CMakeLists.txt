# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

if(CRITERION_FOUND)

	set(TEST_SRC
		advio.c
		bitset.c
		config_json.c
		hist.c
		io.c
		json.c
		kernel.c
		list.c
		log.c
		main.c
		mapping.c
		memory.c
		pool.c
		queue.c
		queue_signalled.c
		task.c
		timing.c
		utils.c
		tsc.c
		hash_table.c
	)

	add_executable(unit-tests ${TEST_SRC})
	target_link_libraries(unit-tests PUBLIC
		PkgConfig::CRITERION
		villas
		Threads::Threads
	)

	add_custom_target(run-unit-tests
		COMMAND $<TARGET_FILE:unit-tests> ${CRITERION_OPTS}
		USES_TERMINAL
	)

	add_dependencies(tests unit-tests)
	add_dependencies(run-tests run-unit-tests)

	add_dependencies(run-unit-tests unit-tests)
endif()
