#!/bin/bash
#
# Remove LGPL licence from all files
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#sed -i -e '/@license GNU Lesser General Public License/d' \
#       -e '/This application is free software;/,+12d' \
#       -e '/ * VILLASnode - connecting real-time simulation equipment/,+1d' $1

find . \( -name "*.c" -or -name "*.h" -or -name "Dockerfile*" -or -name "Makefile*" -or -name "*.conf" -or -name "*.sh" -or -name "*.js" \) \
	-not -path thirdparty \
	-not -path build \
	-exec test -e {} \; \
	-print0 | xargs -0 \
	sed -i -e '/@license GNU Lesser General Public License v2.1/,/Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA/d'
