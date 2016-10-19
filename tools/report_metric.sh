#!/bin/bash

if [ "$#" -lt 2 ]; then
	echo "Usage: $0 measurement_name field1=value1 field2=value2 ... -- tag1=tag1value tag2=tag2value"
	echo "Example: $0 builds success=true warnings=235 time=123 -- branch=master comitter=stv0g"
	exit -1
fi

HOST=${INFLUXDB_HOST:-localhost}
PORT=${INFLUXDB_PORT:-8086}
DB=${INFLUXDB_DATABASE:-builds}

CURL_OPTS="-i -XPOST"

if [ -n "${INFLUXDB_USER}" -a -n "${INFLUXDB_PASSWORD}" ]; then
	CURL_OPTS+=" -u ${INFLUXDB_USER}:${INFLUXDB_PASSWORD}"
fi

function join_by {
	local IFS="$1"
	shift
	echo "$*"
}

search() {
	local i=0;
	for str in ${@:2}; do
		if [ "$str" = "$1" ]; then
			echo $i
			return
		else
			((i++))
		fi
	done
	echo ""
}

I=$(search "--" $@)

if [ -n "$I" ]; then
	TAGS=",$(join_by , ${@:(($I+2))})"
	VALUES=${@:2:(($I-1))}
else
	VALUES=${@:2}
fi

MEASUREMENT=$1
TS=$(date +%s%N)

curl ${CURL_OPTS} "http://${HOST}:${PORT}/write?db=${DB}" --data-binary "${MEASUREMENT}${TAGS} ${VALUES} ${TS}"
