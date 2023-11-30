#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

# Safety
set -o errexit
set -o nounset
set -o pipefail
set -x

# Configuration
tsa="${TSA:-https://freetsa.org/tsr}"
cacert="${TSA_CACERT:-cacert.pem}"
cert="${TSA_CERT:-tsa.crt}"
out_dir="${OUT_DIR:-tsr}"
digests="${1}"
mqtt_broker="${MQTT_BROKER:-localhost}"
mqtt_topic="${MQTT_TOPIC:-tsr}"
tmp_dir="$(mktemp --directory villas-ts-sign-XXXXXX)"

# Validation
test -e "${cacert}"
test -e "${cert}"
test -e "${out_dir}/" || mkdir -p "${out_dir}/"
test -d "${out_dir}/"
test -e "${digests}"

# Main Loop
while read -r _first _last algorithm digest ; do
	tsq="${tmp_dir}/${digest}.${algorithm}.tsq"
	tsr="${tmp_dir}/${digest}.${algorithm}.tsr"

	openssl ts -query -"${algorithm}" -digest "${digest}" -out "${tsq}"

	curl --silent > "${tsr}" \
		--header 'Content-Type: application/timestamp-query' \
		--data-binary "@${tsq}" \
		"${tsa}"

	if openssl ts -verify -"${algorithm}" -digest "${digest}" -in "${tsr}" -CAfile "${cacert}" -untrusted "${cert}" ; then
		mosquitto_pub -h "${mqtt_broker}" -t "${mqtt_topic}" -f "${tsr}"
		mv "${tsr}" "${out_dir}"
	else
		{
			echo "Could not verify TSA reply:"
			openssl ts -reply -in "${tsr}" -text
		} >&2
	fi
done < "${digests}"

# Cleanup
rm -r "${tmp_dir}"
