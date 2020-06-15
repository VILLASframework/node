# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

add_custom_target(run-integration-tests
	COMMAND
		SRCDIR=${PROJECT_SOURCE_DIR}
		BUILDDIR=${PROJECT_BINARY_DIR}
		LD_PRELOAD=/usr/lib64/libSegFault.so
		SEGFAULT_SIGNALS=all
		SEGFAULT_SIGNALS="bus abrt"
		${PROJECT_SOURCE_DIR}/tools/integration-tests.sh 2>&1 | c++filt
	USES_TERMINAL
	DEPENDS
		villas-node
		villas-pipe
		villas-signal
		villas-hook
)

add_dependencies(run-tests run-integration-tests)
