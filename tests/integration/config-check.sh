
#!/bin/bash
#
# Test example configurations
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

set -e

cd ${SRCDIR}/etc/

CONFIGS=$(find . -name '*.conf' -o -name '*.json')

for CONFIG in ${CONFIGS}; do
    if [ "$(basename ${CONFIG})" == "opal.conf" ] ||
       [ "$(basename ${CONFIG})" == "paths.conf" ] ||
       [ "$(basename ${CONFIG})" == "global.conf" ]; then
        echo "=== Skipping config: ${CONFIG}"
        continue
    fi

    echo "=== Testing config: ${CONFIG}"
    villas-config-check ${CONFIG}
done
