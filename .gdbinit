# SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0
# Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>

python

import os
import gdb

if nlohmann_json_src := os.environ.get('NLOHMANN_JSON_SRC'):
    gdb.execute(f"source {nlohmann_json_src}/tools/gdb_pretty_printer/nlohmann-json.py")
end
