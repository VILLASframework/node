#!/bin/bash
#
# Transparent Network Emulation Proxy for Layer 2 & 3
#
# Dependencies: iptables, ebtables and iproute2
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

set -e # Abort on error
die() { echo "$1"; exit -1; }

# Apply netem qdisc also for reverse path
REVERSE=0

# This scripts also supports L3 IP packets. Simply replace the MAC by IP addresses
LAYER=2

SRC=00:50:C2:4F:92:D5
DST=00:50:C2:4F:92:D6

#DST=01:0C:CD:01:01:FF
MY=$SRC

SRC_IF=p5p1.10
DST_IF=p5p1.11

# Network emulation settings (see http://stuff.onse.fi/man?program=tc-netem&section=8)
NETEM="delay 1000000 20000 distribution normal duplicate 4 loss 20"

##############################################################################################
# Do not change something below this line!

MARK=$RANDOM

# Tools
TC=/sbin/tc
if   [ "$LAYER" -eq "2" ]; then
	NF=/sbin/ebtables
elif [ "$LAYER" -eq "3" ]; then
	NF=/sbin/iptables
fi

# Test permissions
(( $(id -u) == 0 )) || die "This script must be executed as root (run: sudo $0)"

# Test if tools are available
[ -x $NF ] || die "Please install $NF (run: sudo apt-get install ebtables iptables)"
[ -x $TC ] || die "Please install $TC (run: sudo apt-get install iproute2)"

# Load kernel modules
modprobe sch_netem || die "The netem qdisc is not compiled in this kernel!"

# Clear tables and chains to get a clean state
$NF -t nat -F
$NF -t nat -X

exit

# Add new chain, mark packets from $SRC and redirect them to $DEST

# Insert new chain into flow
$NF -t nat -A PREROUTING -i $SRC_IF -s $SRC -j mark --mark-set $MARK --mark-target CONTINUE
$NF -t nat -A PREROUTING -i $SRC_IF -s $SRC -j dnat --to-dst $DST --dnat-target ACCEPT

$NF -t nat -A PREROUTING -i $DST_IF -s $DST -j mark --mark-set $MARK --mark-target CONTINUE
$NF -t nat -A PREROUTING -i $DST_IF -s $DST -j dnat --to-dst $SRC --dnat-target ACCEPT

exit

# Clean traffic control
$TC qdisc delete dev $DST_IF root || true

# Add classful qdisc to egress (outgoing) network device
$TC qdisc  replace dev $DST_IF root handle 4000 prio bands 4 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
$TC qdisc  add     dev $DST_IF parent 4000:4 handle 4020 netem $NETEM
$TC filter add     dev $DST_IF protocol ip handle $MARK fw flowid 4000:4

echo -e "Successfully configured network emulation:"

echo -e "Netem Settings for $SRC ($SRC_IF) => $DST ($DST_IF):"
echo -e "   $NETEM"
if (( $REVERSE )); then
	echo -e "Netem Settings for $DST ($DST_IF) => $SRC ($SRC_IF):"
	echo -e "   $NETEM_REV"
fi

if [ -n $DEBUG ]; then
	if [ "$SRC_IF" == "$DST_IF" ]; then
		IFNS="$SRC_IF"
	else
		IFNS="$SRC_IF $DST_IF"
	fi

	for inf in $IFNS; do
		for cmd in qdisc filter class; do
			echo -e "\nTC ==> $if: $cmd"
			tc -d -p $cmd show dev $inf
		done
	done

	echo -e "\nTABLES ==>"
	$NF -t nat -L
fi
