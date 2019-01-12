#!/bin/bash
#
# Transparent Network Emulation Proxy for Layer 2 & 3
#
# Dependencies: iptables, ebtables and iproute2
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

set -e # Abort on error
die() { echo "$1"; exit -1; }

# Apply netem qdisc also for reverse path
REVERSE=0

# This scripts also supports L3 IP packets. Simply replace the MAC by IP addresses
# Dont forget to adjust FILTER accordingly!
LAYER=2
MY="00:15:17:2e:fb:e8"
SRC="00:15:17:2e:f4:52"
DST="00:15:17:2e:f4:52"
SRC_IF=eth2
DST_IF=eth2

# Apply network emulation only to packets which match those {ip,eb}table rules
# Ethertype of GOOSE is 0x88b8
FILTER="-s $SRC -p ipv4"
FILTER_REV="-s $DST -p ipv4"

# Network emulation settings (see http://stuff.onse.fi/man?program=tc-netem&section=8)
NETEM="delay 1000000 20000 distribution normal duplicate 4 loss 20"
NETEM_REV=$NETEM

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
[ -x $TC  ] || die "Please install $TC (run: sudo apt-get install iproute2)"

# Load kernel modules
modprobe sch_netem || die "The netem qdisc is not compiled in this kernel!"

# Clear tables and chains to get a clean state
$NF -t nat -F
$NF -t nat -X

# Add new chain, mark packets from $SRC and redirect them to $DEST

# Insert new chain into flow
$NF -t nat -I PREROUTING  $FILTER    -j mark --mark-set 123 --mark-target CONTINUE
$NF -t nat -I PREROUTING  $FILTER    -j dnat --to-dst $DST --dnat-target CONTINUE

$NF -t nat -I POSTROUTING --mark 123 -j snat --to-src $MY

# Clean traffic control
$TC qdisc delete dev $DST_IF root || true
$TC qdisc delete dev $SRC_IF root || true

# Add classful qdisc to egress (outgoing) network device
$TC qdisc  replace dev $DST_IF root handle 4000 prio bands 4 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
$TC qdisc  add     dev $DST_IF parent 4000:3 handle 4020 netem $NETEM
$TC filter add     dev $DST_IF protocol ip handle 123 fw flowid 4000:3

if (( $REVERSE )); then
	$NF -t nat -I PREROUTING $FILTER_REV -j mark --mark-set 124 --mark-target CONTINUE
	$NF -t nat -I PREROUTING $FILTER_REV -j dnat --to-dst $SRC --dnat-target CONTINUE

	$NF -t nat -I POSTROUTING --mark 123 -j snat --to-src $MY

	# Add classful qdisc to egress (outgoing) network device
	$TC qdisc  replace dev $SRC_IF root handle 4000 prio bands 4 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
	$TC qdisc  add     dev $SRC_IF parent 4000:4 handle 4021 netem $NETEM_REV
	$TC filter add     dev $SRC_IF protocol ip handle 124 fw flowid 4000:4
fi

echo -e "Successfully configured network emulation:\n"
echo -e "Netem Settings for $SRC ($SRC_IF) => $DST ($DST_IF):"
echo -e "   $NETEM\n"
if (( $REVERSE )); then
	echo -e "Netem Settings for $DST ($DST_IF) => $SRC ($SRC_IF):"
	echo -e "   $NETEM_REV"
fi

# Some debug and status output
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
