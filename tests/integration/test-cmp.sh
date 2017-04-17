#!/bin/bash

INPUT_FILE=$(mktemp)

function fail() {
	rm ${INPUT_FILE}
	exit $1;
}

cat > ${INPUT_FILE} <<EOF
1491095596.645159701(0)	0.000000
1491095596.745159701(1)	0.587785
1491095596.845159701(2)	0.951057
1491095596.945159701(3)	0.951057
1491095597.045159701(4)	0.587785
1491095597.145159701(5)	0.000000
1491095597.245159701(6)	-0.587785
1491095597.345159701(7)	-0.951057
1491095597.445159701(8)	-0.951057
1491095597.545159701(9)	-0.587785
EOF

villas-test-cmp ${INPUT_FILE} ${INPUT_FILE}
(( $? == 0 )) || fail 1

villas-test-cmp ${INPUT_FILE} <(head -n5 ${INPUT_FILE}; sleep 1; tail -n5 ${INPUT_FILE})
(( $? == 0 )) || fail 8

villas-test-cmp ${INPUT_FILE} <(head -n-1 ${INPUT_FILE})
(( $? == 1 )) || fail 2

villas-test-cmp ${INPUT_FILE} <(cat       ${INPUT_FILE}; echo "1491095597.545159701(9)	-0.587785")
(( $? == 1 )) || fail 7

villas-test-cmp ${INPUT_FILE} <(head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(55)	-0.587785")
(( $? == 2 )) || fail 3

villas-test-cmp ${INPUT_FILE} <(head -n-1 ${INPUT_FILE}; echo "1491095598.545159701(9)	-0.587785")
(( $? == 3 )) || fail 4

villas-test-cmp ${INPUT_FILE} <(head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(9)	-0.587785 -0.587785")
(( $? == 4 )) || fail 5

villas-test-cmp ${INPUT_FILE} <(head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(9)	-1.587785")
(( $? == 5 )) || fail 6

rm ${INPUT_FILE}