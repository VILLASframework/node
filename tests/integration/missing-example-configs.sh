
#!/bin/bash
#
# Test example configurations
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

NODE_TYPES=$(villas node -C 2>/dev/null | jq -r '.nodes | join(" ")')
HOOK_TYPES=$(villas node -C 2>/dev/null | jq -r '.hooks | join(" ")')
FORMAT_TYPES=$(villas node -C 2>/dev/null | jq -r '.formats | join(" ")')

MISSING=0

for NODE in ${NODE_TYPES}; do
	NODE=${NODE/./-}
	[ ${NODE} == "loopback_internal" ] && continue

	if [ ! -f "${SRCDIR}/etc/examples/nodes/${NODE}.conf" ]; then
		echo "Missing example config for node-type: ${NODE}"
		((MISSING++))
	fi
done

for HOOK in ${HOOK_TYPES}; do
	[ ${HOOK} == "restart" ] || \
	[ ${HOOK} == "drop" ] || \
	[ ${HOOK} == "fix" ] && continue

	if [ ! -f "${SRCDIR}/etc/examples/hooks/${HOOK}.conf" ]; then
		echo "Missing example config for hook-type: ${HOOK}"
		((MISSING++))
	fi
done

for FORMAT in ${FORMAT_TYPES}; do
	FORMAT=${FORMAT/./-}
	if [ ! -f "${SRCDIR}/etc/examples/formats/${FORMAT}.conf" ]; then
		echo "Missing example config for format-type: ${FORMAT}"
		((MISSING++))
	fi
done

(( ${MISSING} == 0 ))
