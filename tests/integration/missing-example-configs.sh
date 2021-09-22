
#!/bin/bash
#
# Test example configurations
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
##################################################################################

NODE_TYPES=$(villas node -C | jq -r '.nodes | join(" ")')
HOOKS_TYPES=$(villas node -C | jq -r '.["hooks"] | join(" ")')

CONFIGS=$(find  -name '*.conf' -o -name '*.json')

MISSING=0

for NODE in ${NODE_TYPES}; do
	[ ${NODE} == "loopback_internal" ] && continue

	if [ ! -f "${SRCDIR}/etc/examples/nodes/${NODE}.conf" ]; then
		echo "Missing example config for node-type: ${NODE}"
		((MISSING++))
	fi
done

for HOOK in ${HOOK_TYPES}; do
	[ ${NODE} == "loopback_internal" ] && continue

	if [ ! -f "${SRCDIR}/etc/examples/hooks/${HOOK}.conf" ]; then
		echo "Missing example config for hook-type: ${HOOK}"
		((MISSING++))
	fi
done

(( ${MISSING} == 0 ))
