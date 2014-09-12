#!/bin/sh

# die on error
set -e

if [ "$(hostname)" != "acs-s2ss" ]; then
	echo "This script has to be run only acs-s2ss!" 1>&2
	exit 1
fi

if [ "$(id -u)" != "0" ]; then
	echo -e "This script must be run as root" 1>&2
	exit 1
fi

IP=78.91.103.24
PORT=12010
IPT=iptables
RULE1="-p udp --dport $PORT -s $IP -j REJECT"
RULE2="-p tcp --dport $PORT -s $IP -j REJECT"

case $1 in
    block)
	$IPT -I INPUT 1 $RULE1 
	$IPT -I INPUT 1 $RULE2
	service tincd restart
	;;

    unblock)
	$IPT -D INPUT $RULE1
	$IPT -D INPUT $RULE2
	service tincd restart
	;;

    status)
	$IPT -C INPUT $RULE1 && echo "Tinc UDP is blocked"
	$IPT -C INPUT $RULE2 && echo "Tinc TCP is blocked"

	echo -n "Sintef "
	tinc -n s2ss info sintef | grep "Reachability"

	echo -n "Frankfurt "
	tinc -n s2ss info fra | grep "Reachability"
	;;
esac
