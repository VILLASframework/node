#!/bin/sh
#
# Append a Signed-off-by line to all git commit messages
#
# Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

MESSAGE_FILE="$1"
GIT_AUTHOR="$(git var GIT_AUTHOR_IDENT | sed -n 's|^\(.*>\).*$|\1|p')"
SIGNOFF="Signed-off-by: $GIT_AUTHOR"

if ! grep --quiet --fixed-strings --line-regexp "$SIGNOFF" "$MESSAGE_FILE" ; then
  printf "\n%s\n" "$SIGNOFF" >> "$MESSAGE_FILE"
fi

exit 0
