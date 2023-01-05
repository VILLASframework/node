## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2018-2022, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASfpga
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
##############################################################################

add_executable(villas-fpga-ctrl villas-fpga-ctrl.cpp)

target_link_libraries(villas-fpga-ctrl PUBLIC
	villas-fpga
)

add_executable(pcimem pcimem.c)

configure_file(villas-fpga-cat.sh villas-fpga-cat.sh COPYONLY)
configure_file(villas-fpga-xbar-select.sh villas-fpga-xbar-select.sh COPYONLY)
